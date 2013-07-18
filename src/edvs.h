#ifndef __EDVS_H__F1FBDD4A_AEBF_435F_BA71_29BF5A73C9D8
#define __EDVS_H__F1FBDD4A_AEBF_435F_BA71_29BF5A73C9D8

typedef struct edvs_datagram {
	unsigned char x, y, p;
	// uint64_t t;
} edvs_datagram_t;


void edvs_start(int uart_fd);
void edvs_stop();

#endif /* __EDVS_H__F1FBDD4A_AEBF_435F_BA71_29BF5A73C9D8 */

