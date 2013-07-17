#ifndef __CONFIG_H__3DEFF63B_7562_4EFC_94DD_3660976C49FA
#define __CONFIG_H__3DEFF63B_7562_4EFC_94DD_3660976C49FA

#include <stdint.h>
#include <math.h>
#include <mavlink/v1.0/common/mavlink.h>

/*
 * struct to save configuration of a connection. In order to use the
 * connection, pass either an IPv4 or an IPv6 address. Set the other one
 * to NULL.
 */
typedef struct {
	// connectivity
	char *target_ip4;
	char *target_ip6;
	int joystick_port;
	int mavlink_port;

	// mavlink setup
	uint8_t system_id;
	uint8_t system_comp;
	uint8_t target_id;
	uint8_t target_comp;

	// callback routines
	void (*mavlink_handler)(mavlink_message_t msg);
} rctl_config_t;

void rctl_alloc_config(rctl_config_t **cfg);
void rctl_free_config(rctl_config_t **cfg);

// void rctl_parse_argv(rctl_config_t *cfg, int argc, char *argv[]);

#endif /* __CONFIG_H__3DEFF63B_7562_4EFC_94DD_3660976C49FA */

