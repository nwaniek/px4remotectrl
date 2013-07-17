#define _XOPEN_SOURCE	700
#define _POSIX_C_SOURCE 200809L

#include "rctl_config.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*
void
rctl_parse_argv(rctl_config_t *cfg, int argc, char *argv[]) {
	while (getopt(argc, argv, "i:") != -1) {
		printf("found it\n");
	}
}
*/

void
rctl_alloc_config(rctl_config_t **cfg) {
	if ((*cfg)) rctl_free_config(cfg);
	*cfg = malloc(sizeof(**cfg));
}

void
rctl_free_config(rctl_config_t **cfg) {
	if (!cfg && !(*cfg)) return;
	// free((*cfg)->target_ip4);
	free((*cfg));
	*cfg = NULL;
}
