#define _BSD_SOURCE

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <fcntl.h>
#include <time.h>
#include <mhash.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <linux/joystick.h>
#include <mavlink/v1.0/common/mavlink.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>

#include "js_packet.h"

/*
 * Socket Setup
 */
#define UAV_ADDRESS	"192.168.201.46"
#define JOYSTICK_PORT	56000
#define MAVLINK_PORT    56001


/*
 * Joystick Setup
 */
#define JS_DEVICE	"/dev/input/js0"

/*
 * Mavlink Setup
 */
#define SYS_ID		255
#define SYS_COMP	0
#define TARGET_ID	1


enum {
	YAW=0,
	THROTTLE,
	ROLL,
	LEFTBUT,
	RIGHTBUT,
	PITCH
} axis_index_t;

static bool mav_armed = false;

uint32_t
crc32(unsigned char const *p, uint32_t len) {
	int i;
	unsigned int crc = 0;
	while (len--) {
		crc ^= *p++;
		for (i = 0; i < 8; i++)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
	}
	return crc;
}

uint64_t
microsSinceEpoch() {
	struct timeval tv;
	uint64_t micros = 0;

	gettimeofday(&tv, NULL);
	micros =  ((uint64_t)tv.tv_sec) * 1000000 + tv.tv_usec;

	return micros;
}

void
die(const char *s, ...) {
	va_list ap;
	va_start(ap, s);
	vfprintf(stderr, s, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}


int
connect_mavlink() {
	int ml_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (ml_sock < 0)
		die("Could create socket for mavlink endpoint.\n");

	struct sockaddr_in ml_addr;
	memset(&ml_addr, 0, sizeof(ml_addr));
	ml_addr.sin_family = AF_INET;
	ml_addr.sin_port   = htons(MAVLINK_PORT);
	ml_addr.sin_addr.s_addr = inet_addr(UAV_ADDRESS);

	if (connect(ml_sock, (struct sockaddr*)&ml_addr, sizeof(ml_addr)) < 0)
		die("Could not connect mavlink socket.\n");

	// set nonblocking
	int flags = fcntl(ml_sock, F_GETFL, 0);
	flags |= O_NONBLOCK;
	fcntl(ml_sock, F_SETFL, flags);

	return ml_sock;
}


int
connect_joystick() {
	int js_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (js_sock < 0)
		die("Could not create socket for joystick endpoint.\n");

	struct sockaddr_in js_addr;
	memset(&js_addr, 0, sizeof(js_addr));
	js_addr.sin_family = AF_INET;
	js_addr.sin_port = htons(JOYSTICK_PORT);
	js_addr.sin_addr.s_addr = inet_addr(UAV_ADDRESS);

	if (connect(js_sock, (struct sockaddr*)&js_addr, sizeof(js_addr)) < 0)
		die("Could not connect joystick socket.\n");

	return js_sock;
}

int
open_joystick() {
	int js_fd = open(JS_DEVICE, O_RDONLY);
	if (js_fd == -1)
		die("Could not open joystick.\n");
	fcntl(js_fd, F_SETFL, O_NONBLOCK);

	return js_fd;
}

#define SET_MODE(BASE_MODE, CUSTOM_MODE) { \
	unsigned char buf[MAVLINK_MAX_PACKET_LEN]; \
	mavlink_message_t msg; \
	mavlink_msg_set_mode_pack(SYS_ID, SYS_COMP, &msg, TARGET_ID, \
			(BASE_MODE), (CUSTOM_MODE)); \
	int len = mavlink_msg_to_send_buffer(buf, &msg); \
	send(sockfd, buf, len, 0); } \


static volatile uint8_t baseMode = 0;
static volatile uint32_t navMode = 0;

void
mavlink_setmode(int sockfd) {
	printf("set mode\n");
	uint8_t mode = MAV_MODE_FLAG_MANUAL_INPUT_ENABLED;
	SET_MODE(mode & ~MAV_MODE_FLAG_SAFETY_ARMED, baseMode);
}


void
mavlink_arm(int sockfd) {
	if (mav_armed) return;
	printf("arming\n");
	SET_MODE(baseMode | MAV_MODE_FLAG_SAFETY_ARMED, navMode);
	mav_armed = true;
}

void
mavlink_disarm(int sockfd) {
	if (!mav_armed) return;
	printf("disarming\n");
	SET_MODE(baseMode & ~MAV_MODE_FLAG_SAFETY_ARMED, navMode);
	mav_armed = false;
}

static bool mrt_running = true;

void*
mavlink_recv_thread(void *ptr) {
	unsigned char buf[MAVLINK_MAX_PACKET_LEN];
	int *sockfd = (int*)ptr;

	fprintf(stdout, "recv thread started...\n");
	while (mrt_running) {
		ssize_t r = recv(*sockfd, buf, MAVLINK_MAX_PACKET_LEN, 0);
		if ((r == -1) && (errno != EAGAIN && errno != EWOULDBLOCK))
			fprintf(stderr, "recv error: %d\n", errno);
		else {
			// handle message
			uint8_t link_id = 0;
			mavlink_message_t msg;
			mavlink_status_t status;
			unsigned int decode_state;

			for (ssize_t i = 0; i < r; i++) {
				decode_state = mavlink_parse_char(link_id, (uint8_t)(buf[i]), &msg, &status);
				if (decode_state == 1) {
					if (msg.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
						mavlink_heartbeat_t hb;
						mavlink_msg_heartbeat_decode(&msg, &hb);

						printf("%d base mode: %u custom mode: %u\n", MAV_MODE_FLAG_MANUAL_INPUT_ENABLED, hb.base_mode, hb.custom_mode);
						// TODO: mutexes?
						baseMode = hb.base_mode;
						navMode = hb.custom_mode;
					}
				}
			}
		}
	}

	return NULL;
}


static int16_t yaw_integrator = 0;

void
mavlink_set_rpyt(int sockfd, js_packet_t jp) {
	unsigned char buf[MAVLINK_MAX_PACKET_LEN];
	mavlink_message_t msg;

	int16_t r, p, y;
	uint16_t t;

	yaw_integrator += (int16_t)(jp.axis[YAW] / 1000);

	r = (int16_t)jp.axis[ROLL];
	p = (int16_t)jp.axis[PITCH];
	y = yaw_integrator;
	t = (uint16_t)(jp.axis[THROTTLE] + INT16_MAX);

	mavlink_msg_set_quad_swarm_roll_pitch_yaw_thrust_pack(
		SYS_ID, SYS_COMP, &msg, 0, 2,
		&r, &p, &y, &t);
	int len = mavlink_msg_to_send_buffer(buf, &msg);
	send(sockfd, buf, len, 0);
}

static volatile bool running = true;

void
sigint_handler(int signum) {
	printf("SIGINT received\n");
	running = false;
}

typedef enum {
	BUTTON_UNCHANGED,
	BUTTON_PRESSED,
	BUTTON_RELEASED
} button_state_t;

int
main() {
	// TODO: change to sigaction
	signal(SIGINT, sigint_handler);

	pthread_t rcv_t;

	struct js_event js;
	int js_fd = open_joystick();
	js_packet_t js_packet;
	memset(&js_packet, 0, sizeof(js_packet));
	js_packet.joy_id = JOY_ID;

	int js_sock = connect_joystick();
	int ml_sock = connect_mavlink();
	usleep(500);
	mavlink_setmode(ml_sock);
	pthread_create(&rcv_t, NULL, mavlink_recv_thread, &ml_sock);

	printf("Connected...\n");

	// initialize button states
	int button_value[2] = {0};
	button_state_t button_state[2] = {BUTTON_UNCHANGED};

	// empty the joystick file buffer
	while (read(js_fd, &js, sizeof(js)) > 0);

	printf("Running...\n");
	uint64_t last_time_stamp = 0;
	while (running) {
		read(js_fd, &js, sizeof(js));
		switch (js.type & ~JS_EVENT_INIT) {
		case JS_EVENT_AXIS:
			js_packet.axis[js.number] = (int16_t)js.value;
			break;
		}

		/*
		 * parse button values
		 */
		int tmp1 = (int)js_packet.axis[LEFTBUT] - (int)button_value[0];
		int tmp2 = (int)js_packet.axis[RIGHTBUT] - (int)button_value[1];

		button_value[0] = js_packet.axis[LEFTBUT];
		button_value[1] = js_packet.axis[RIGHTBUT];

		if (tmp1 > 0) {
			button_state[0] = BUTTON_PRESSED;
		}
		else if (tmp1 < 0) {
			button_state[0] = BUTTON_RELEASED;
		}
		else {
			button_state[0] = BUTTON_UNCHANGED;
		}

		if (tmp2 > 0) {
			button_state[1] = BUTTON_PRESSED;
		}
		else if (tmp2 < 0) {
			button_state[1] = BUTTON_RELEASED;
		}
		else {
			button_state[1] = BUTTON_UNCHANGED;
		}


		/*
		 * Determine Button Action
		 */
		switch (button_state[0]) {
		case BUTTON_PRESSED:
			if (mav_armed)
				mavlink_disarm(ml_sock);
			else
				mavlink_arm(ml_sock);
			break;
		default:
			break;
		}

		switch (button_state[1]) {
		case BUTTON_PRESSED:
			running = false;
			break;
		default:
			break;
		}

		usleep(1000);
		if (running && mav_armed && (microsSinceEpoch() - last_time_stamp > 40000)) {
			/*
			printf("%+6d %+6d %+6d %6d\n",
					js_packet.axis[ROLL],
					js_packet.axis[PITCH],
					js_packet.axis[YAW],
					js_packet.axis[THROTTLE]);
			*/

			last_time_stamp = microsSinceEpoch();
			js_packet.checksum = crc32((unsigned char *) &js_packet,sizeof(js_packet_t) - sizeof(uint32_t));
			if (write(js_sock, &js_packet, sizeof(js_packet_t)) < 0)
				die("write error\n");
		}
	}

	fprintf(stdout, "Cleaning up...\n");

	// tidy up, close everything, shutdown
	mavlink_disarm(ml_sock);

	mrt_running = false;
	pthread_join(rcv_t, NULL);

	close(ml_sock);
	close(js_sock);
	close(js_fd);
	usleep(1000);
	return 0;

}
