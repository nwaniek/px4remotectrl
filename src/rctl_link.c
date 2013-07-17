#define _XOPEN_SOURCE	700
#define _POSIX_C_SOURCE 200809L

#include "rctl_link.h"
#include <math.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdbool.h>
#include <mavlink/v1.0/common/mavlink.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "util.h"
#include "js_packet.h"

struct _rctl_link {
	int js_sock;
	int ml_sock;
	pthread_t recv_t;
	rctl_config_t *cfg;
};

static volatile uint8_t baseMode = 0;
static volatile uint32_t navMode = 0;
static volatile bool mav_armed = false;
static volatile bool mrt_running = true;

/* forward declarations */
int _connect_mavlink();
int _connect_joystick();

int
_connect_socket(char *ip, int port, int af) {
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		die("Could create socket for port %d.\n", port);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(port);
	inet_pton(af, ip, &(addr.sin_addr));

	// addr.sin_addr.s_addr = inet_addr(ip4);

	if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
		die("Could not connect socket %s:%d.\n", ip, port);

	return sock;
}

int
_connect_mavlink(rctl_config_t *cfg) {
	int ml_sock = -1;
	if (cfg->target_ip4)
		ml_sock = _connect_socket(cfg->target_ip4, cfg->mavlink_port, AF_INET);
	else if (cfg->target_ip6)
		ml_sock = _connect_socket(cfg->target_ip6, cfg->mavlink_port, AF_INET6);
	else
		die("No target IP specified!\n");

	// set nonblocking
	int flags = fcntl(ml_sock, F_GETFL, 0);
	flags |= O_NONBLOCK;
	fcntl(ml_sock, F_SETFL, flags);

	return ml_sock;
}

int
_connect_joystick(rctl_config_t *cfg) {
	int js_sock = -1;
	if (cfg->target_ip4)
		js_sock = _connect_socket(cfg->target_ip4, cfg->joystick_port, AF_INET);
	else if (cfg->target_ip6)
		js_sock = _connect_socket(cfg->target_ip6, cfg->joystick_port, AF_INET6);
	else
		die("No target IP specified\n");

	return js_sock;
}

#define SET_MODE(BASE_MODE, CUSTOM_MODE) { \
	unsigned char buf[MAVLINK_MAX_PACKET_LEN]; \
	mavlink_message_t msg; \
	mavlink_msg_set_mode_pack(link->cfg->system_id, \
			link->cfg->system_comp, &msg, \
			link->cfg->target_id, \
			(BASE_MODE), (CUSTOM_MODE)); \
	int len = mavlink_msg_to_send_buffer(buf, &msg); \
	send(link->ml_sock, buf, len, 0); } \

void
_mavlink_set_initial_mode(rctl_link_t *link) {
	printf("set mode\n");
	uint8_t mode = MAV_MODE_FLAG_MANUAL_INPUT_ENABLED;
	SET_MODE(mode & ~MAV_MODE_FLAG_SAFETY_ARMED, baseMode);
}


void*
_mavlink_recv_thread(void *ptr) {
	unsigned char buf[MAVLINK_MAX_PACKET_LEN];
	rctl_link_t *link = (rctl_link_t*)ptr;

	fprintf(stdout, "recv thread started...\n");
	while (mrt_running) {
		ssize_t r = recv(link->ml_sock, buf, MAVLINK_MAX_PACKET_LEN, 0);
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

					// mavlink_highres_imu_t imu;
					mavlink_heartbeat_t hb;
					// mavlink_sys_status_t sys;

					switch (msg.msgid) {

					/*
					case MAVLINK_MSG_ID_HIGHRES_IMU:
						mavlink_msg_highres_imu_decode(&msg, &imu);
						// printf("IMU Data. Accel (%4.4f, %4.4f, %4.4f)\n", imu.xacc, imu.yacc, imu.zacc);
						break;
					*/

					case MAVLINK_MSG_ID_HEARTBEAT:
						mavlink_msg_heartbeat_decode(&msg, &hb);
						// printf("Heartbeat. Base Mode: %u Custom Mode: %u\n", hb.base_mode, hb.custom_mode);
						// TODO: mutexes?
						// printf("HEARTBEAT %d\n", msg.msgid);
						baseMode = hb.base_mode;
						navMode = hb.custom_mode;
						break;

					/*
					case MAVLINK_MSG_ID_SYS_STATUS:
						mavlink_msg_sys_status_decode(&msg, &sys);
						printf("System Status. Battery Voltage: %u [mV] (%d %% remaining)\n", sys.voltage_battery, sys.battery_remaining);
						break;
					*/

					default:
						// printf("Unhandled msgid %d\n", msg.msgid);
						break;
					}

					// invoke callback function
					if (link->cfg->mavlink_handler)
						link->cfg->mavlink_handler(msg);
				}
			}
		}
	}

	return NULL;
}

void
rctl_arm(rctl_link_t *link) {
	if (mav_armed) return;
	printf("arming\n");
	SET_MODE(baseMode | MAV_MODE_FLAG_SAFETY_ARMED, navMode);
	mav_armed = true;
}

void
rctl_disarm(rctl_link_t *link) {
	if (!mav_armed) return;
	printf("disarming\n");
	SET_MODE(baseMode & ~MAV_MODE_FLAG_SAFETY_ARMED, navMode);
	mav_armed = false;
}

void
rctl_toggle_armed(rctl_link_t *link) {
	if (mav_armed)
		rctl_disarm(link);
	else
		rctl_arm(link);
}

int
rctl_connect_mav(rctl_config_t *cfg, rctl_link_t *link) {

	link->js_sock = _connect_joystick(cfg);
	link->ml_sock = _connect_mavlink(cfg);
	link->cfg = cfg;

	_mavlink_set_initial_mode(link);
	pthread_create(&link->recv_t, NULL, _mavlink_recv_thread, link);
	return 0;
}

int
rctl_disconnect_mav(rctl_link_t *link) {
	mrt_running = false;
	pthread_join(link->recv_t, NULL);
	close(link->ml_sock);
	close(link->js_sock);
	return 0;
}

int
rctl_set_rpyt(rctl_link_t *link, int16_t r, int16_t p, int16_t y, int16_t t) {
	js_packet_t jp;
	jp.joy_id = JOY_ID;
	jp.axis[ROLL]     = r;
	jp.axis[PITCH]    = p;
	jp.axis[YAW]      = y;
	jp.axis[THROTTLE] = t;
	jp.checksum       = crc32((unsigned char *)&jp, sizeof(jp) - sizeof(uint32_t));
	if (write(link->js_sock, &jp, sizeof(jp)) < 0)
		fprintf(stderr, "EE: written less bytes than expected\n");
	return 0;
}

void
rctl_alloc_link(rctl_link_t **link) {
	if ((*link)) rctl_free_link(link);
	*link = malloc(sizeof(rctl_link_t));
}

void
rctl_free_link(rctl_link_t **link) {
	free(*link);
	*link = NULL;
}
