#include "rctl_link.h"

struct _rctl_link {
	int ml_sock;
	int js_sock;
};


int _connect_mavlink();
int _connect_joystick();

int
_connect_mavlink() {
	return 0;
}

int
_connect_joystick() {
	return 0;
}

int
rctl_connect_mav() {
	return 0;
}
