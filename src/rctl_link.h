#ifndef __RCTL_LINK_H__F7623AA0_2423_44A9_B5D2_7715A67DA5D7
#define __RCTL_LINK_H__F7623AA0_2423_44A9_B5D2_7715A67DA5D7

#include <stdint.h>
#include "rctl_config.h"

typedef struct _rctl_link rctl_link_t;

int rctl_connect_mav(rctl_config_t *cfg, rctl_link_t *link);
int rctl_disconnect_mav(rctl_link_t *link);
int rctl_set_rpyt(rctl_link_t *link, int16_t r, int16_t p, int16_t y, int16_t t);
void rctl_alloc_link(rctl_link_t **link);
void rctl_free_link(rctl_link_t **link);
void rctl_arm(rctl_link_t *link);
void rctl_disarm(rctl_link_t *link);
void rctl_toggle_armed(rctl_link_t *link);

#endif /* __RCTL_LINK_H__F7623AA0_2423_44A9_B5D2_7715A67DA5D7 */

