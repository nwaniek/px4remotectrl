#define _XOPEN_SOURCE	700
#define _POSIX_C_SOURCE 200809L

#include "rctl_config.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

void
rctl_alloc_config(rctl_config_t **cfg) {
	if ((*cfg)) rctl_free_config(cfg);
	*cfg = malloc(sizeof(**cfg));
}

void
rctl_free_config(rctl_config_t **cfg) {
	if (!cfg && !(*cfg)) return;
	free((*cfg));
	*cfg = NULL;
}
