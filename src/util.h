#ifndef __UTIL_H__B07C4188_6B8A_4162_9303_48BCFC037276
#define __UTIL_H__B07C4188_6B8A_4162_9303_48BCFC037276

#include <stdint.h>
#include <stdarg.h>

uint32_t crc32(unsigned char const *p, uint32_t len);
uint64_t microsSinceEpoch();
void die(const char *s, ...);

#endif /* __UTIL_H__B07C4188_6B8A_4162_9303_48BCFC037276 */

