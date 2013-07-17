#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

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
