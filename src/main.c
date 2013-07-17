#define _XOPEN_SOURCE	700
#define _POSIX_C_SOURCE 200809L

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
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>

#include "js_packet.h"
#include "rctl_config.h"
#include "rctl_link.h"
#include "util.h"

static bool running = true;

#define JS_DEVICE "/dev/input/js0"

int
open_joystick(const char *js_device) {
	int js_fd = open(js_device, O_RDONLY);
	if (js_fd == -1)
		die("Could not open joystick '%s'.\n", js_device);
	fcntl(js_fd, F_SETFL, O_NONBLOCK);
	return js_fd;
}

void
sigint_handler(int signum) {
	if (signum == SIGINT)
		running = false;
}

typedef enum {
	BUTTON_UNCHANGED,
	BUTTON_PRESSED,
	BUTTON_RELEASED
} button_state_t;


/*
 * mavlink_msg_handler is triggered by a separate thread within the rctl
 * functions! make sure to use mutexes if necessary
 */
void
mavlink_msg_handler(mavlink_message_t msg) {
	// printf("ding %d\n", msg.msgid);
	if (msg.msgid == MAVLINK_MSG_ID_HIGHRES_IMU) {
		mavlink_highres_imu_t imu;
		mavlink_msg_highres_imu_decode(&msg, &imu);
		printf("%12ld %9.4f %9.4f %9.4f %9.4f %9.4f %9.4f %9.4f %9.4f %9.4f\n",
				microsSinceEpoch(),
				imu.xacc,
				imu.yacc,
				imu.zacc,
				imu.xgyro,
				imu.ygyro,
				imu.zgyro,
				imu.xmag,
				imu.ymag,
				imu.zmag);
	}
}

#define BUTTON1_UNCHANGED (1 << 0)
#define BUTTON1_PRESSED   (1 << 1)
#define BUTTON1_RELEASED  (1 << 2)
#define BUTTON2_UNCHANGED (1 << 3)
#define BUTTON2_PRESSED   (1 << 4)
#define BUTTON2_RELEASED  (1 << 5)

int
parse_buttons(int lb, int rb) {
	static int old_lb = INT16_MIN;
	static int old_rb = INT16_MIN;

	int tmp1 = lb - old_lb;
	int tmp2 = rb - old_rb;

	old_lb = lb;
	old_rb = rb;

	int result = 0;
	if (tmp1 > 0) {
		result |= BUTTON1_PRESSED;
	}
	else if (tmp1 < 0) {
		result |= BUTTON1_RELEASED;
	}
	else {
		result |= BUTTON1_UNCHANGED;
	}

	if (tmp2 > 0) {
		result |= BUTTON2_PRESSED;
	}
	else if (tmp2 < 0) {
		result |= BUTTON2_RELEASED;
	}
	else {
		result |= BUTTON2_UNCHANGED;
	}

	return result;
}

void
mainloop(int js_fd, rctl_link_t *link) {
	int16_t r, p, y, t, lb, rb;
	struct js_event js;
	uint64_t last_time_stamp = microsSinceEpoch();

	memset(&js, 0, sizeof(js));
	r = p = y = t = 0;
	lb = rb = INT16_MIN;

	// empty the joystick file buffer
	while (read(js_fd, &js, sizeof(js)) > 0);

	// run main loop
	while (running) {
		read(js_fd, &js, sizeof(js));
		switch (js.type & ~JS_EVENT_INIT) {
		case JS_EVENT_AXIS:
			switch (js.number) {
			case YAW:
				y = (int16_t)js.value;
				break;
			case ROLL:
				r = (int16_t)js.value;
				break;
			case PITCH:
				p = (int16_t)js.value;
				break;
			case THROTTLE:
				t = (int16_t)js.value;
				break;
			case LEFTBUT:
				lb = (int16_t)js.value;
				break;
			case RIGHTBUT:
				rb = (int16_t)js.value;
				break;
			default:
				break;
			}
			break;
		}
		int bstate = parse_buttons(lb, rb);
		if (bstate & BUTTON1_PRESSED) {
			rctl_toggle_armed(link);
		}
		if (bstate & BUTTON2_PRESSED) {
			rctl_disarm(link);
			running = false;
		}

		// send commands to drone
		if (running && (microsSinceEpoch() - last_time_stamp > 40000)) {
			last_time_stamp = microsSinceEpoch();
			rctl_set_rpyt(link, r, p, y, t);
		}
	}
}

int
main(int argc, char *argv[]) {
	/*
	 * register sigint handler to make it possible close all open
	 * sockets and exit in a sane state
	 */
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = sigint_handler;
	sigaction(SIGINT, &sa, NULL);

	/*
	 * allocate required remote control variables
	 */
	rctl_config_t *cfg = NULL;
	rctl_link_t *link = NULL;
	rctl_alloc_config(&cfg);
	rctl_alloc_link(&link);

	/*
	 * open joystick device
	 */
	const char *js_device = JS_DEVICE;
	if (argc > 1)
		js_device = argv[1];
	int js_fd = open_joystick(js_device);

	/*
	 * configuration setup
	 */
	cfg->target_ip4		= "192.168.201.46";
	cfg->joystick_port	= 56000;
	cfg->mavlink_port	= 56001;
	cfg->system_id		= 255;
	cfg->system_comp	= 0;
	cfg->target_id		= 1;
	cfg->target_comp	= 0;
	cfg->mavlink_handler	= mavlink_msg_handler;

	/*
	 */
	rctl_connect_mav(cfg, link);
	mainloop(js_fd, link);
	rctl_disarm(link);
	rctl_disconnect_mav(link);

	rctl_free_link(&link);
	rctl_free_config(&cfg);
}

