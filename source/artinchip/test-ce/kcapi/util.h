/*
 * Code from kcapi-main.c
 */

#ifndef _UTIL_H_
#define _UTIL_H_

#ifdef __cplusplus
extern "C"
{
#endif

void bin2hex(const uint8_t *bin, size_t binlen, char *hex, size_t hexlen,
	     int u);
int bin2hex_alloc(const uint8_t *bin, uint32_t binlen, char **hex,
		  uint32_t *hexlen);
void bin2print(const uint8_t *bin, size_t binlen);
void hex2bin(const char *hex, size_t hexlen, uint8_t *bin, size_t binlen);
int hex2bin_m(const char *hex, size_t hexlen, uint8_t **bin, size_t binlen);

#ifdef __cplusplus
}
#endif

#endif
