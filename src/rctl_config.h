#ifndef __CONFIG_H__3DEFF63B_7562_4EFC_94DD_3660976C49FA
#define __CONFIG_H__3DEFF63B_7562_4EFC_94DD_3660976C49FA

#include <stdint.h>

/*
 * struct to save configuration of a connection
 */
typedef struct {
	// connectivity
	char *target_ip4;
	int joystick_port;
	int mavlink_port;

	// joystick device setup
	char *joystick_dev;

	// mavlink setup
	uint8_t system_id;
	uint8_t system_comp;
	uint8_t target_id;
	uint8_t target_comp;
} rctl_config_t;

void rctl_parse_argv(rctl_config_t *cfg, int argc, char *argv[]);

void rctl_alloc_config(rctl_config_t **cfg);
void rctl_free_config(rctl_config_t **cfg);


#endif /* __CONFIG_H__3DEFF63B_7562_4EFC_94DD_3660976C49FA */

