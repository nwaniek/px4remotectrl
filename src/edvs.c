#include "edvs.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
	int uart_fd;
} edvs_thread_data_t;

static bool running = true;
pthread_t *recv_t = NULL;
edvs_thread_data_t _thread_data;

void*
_edvs_recv_thread(void *ptr) {
	edvs_thread_data_t *data = (edvs_thread_data_t*)ptr;

	while (running) {
		edvs_datagram_t dgram;
		if (read(data->uart_fd, &dgram, sizeof(dgram)) > 0) {
			// compute everything here, somehow transmit
			// message to main application
		}
	}

	return NULL;
}


void
edvs_start(int uart_fd) {
	if (uart_fd < 0) return;
	_thread_data.uart_fd = uart_fd;
	recv_t = malloc(sizeof(pthread_t));
	pthread_create(recv_t, NULL, _edvs_recv_thread, (void*)&_thread_data);
}


void
edvs_stop() {
	running = false;
	if (recv_t) pthread_join(*recv_t, NULL);
}
