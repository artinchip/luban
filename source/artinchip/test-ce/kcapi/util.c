/*
 * Code from kcapi-main.c
 */

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include "util.h"

static char hex_char_map_l[] = { '0', '1', '2', '3', '4', '5', '6', '7',
				 '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
static char hex_char_map_u[] = { '0', '1', '2', '3', '4', '5', '6', '7',
				 '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
static char hex_char(uint32_t bin, int u)
{
	if (bin < sizeof(hex_char_map_l))
		return (u) ? hex_char_map_u[bin] : hex_char_map_l[bin];
	return 'X';
}

/**
 * Convert binary string into hex representation
 * @bin input buffer with binary data
 * @binlen length of bin
 * @hex output buffer to store hex data
 * @hexlen length of already allocated hex buffer (should be at least
 *	   twice binlen -- if not, only a fraction of binlen is converted)
 * @u case of hex characters (0=>lower case, 1=>upper case)
 */
void bin2hex(const uint8_t *bin, size_t binlen, char *hex, size_t hexlen, int u)
{
	size_t i = 0;
	size_t chars = (binlen > (hexlen / 2)) ? (hexlen / 2) : binlen;

	for (i = 0; i < chars; i++) {
		hex[(i*2)] = hex_char((bin[i] >> 4), u);
		hex[((i*2)+1)] = hex_char((bin[i] & 0x0f), u);
	}
}

/**
 * Allocate sufficient space for hex representation of bin
 * and convert bin into hex
 *
 * Caller must free hex
 * @bin [in] input buffer with bin representation
 * @binlen [in] length of bin
 * @hex [out] return value holding the pointer to the newly allocated buffer
 * @hexlen [out] return value holding the allocated size of hex
 *
 * return: 0 on success, !0 otherwise
 */
int bin2hex_alloc(const uint8_t *bin, uint32_t binlen, char **hex,
		  uint32_t *hexlen)
{
	char *out = NULL;
	uint32_t outlen = 0;

	if (!binlen)
		return -EINVAL;

	outlen = (binlen) * 2;

	out = (char *)calloc(1, outlen + 1);
	if (!out)
		return -errno;

	bin2hex(bin, binlen, out, outlen, 0);
	*hex = out;
	*hexlen = outlen;
	return 0;
}

void bin2print(const uint8_t *bin, size_t binlen)
{
	char *hex;
	size_t hexlen = binlen * 2 + 1;

	hex = (char *)calloc(1, hexlen);
	if (!hex)
		return;
	bin2hex(bin, binlen, hex, hexlen - 1 , 0);
	fprintf(stdout, "%s", hex);
	free(hex);
}

static int bin_char(uint8_t hex)
{
	if (48 <= hex && 57 >= hex)
		return (hex - 48);
	if (65 <= hex && 70 >= hex)
		return (hex - 55);
	if (97 <= hex && 102 >= hex)
		return (hex - 87);
	return 0;
}

/**
 * Convert hex representation into binary string
 * @hex input buffer with hex representation
 * @hexlen length of hex
 * @bin output buffer with binary data
 * @binlen length of already allocated bin buffer (should be at least
 *	   half of hexlen -- if not, only a fraction of hexlen is converted)
 */
void hex2bin(const char *hex, size_t hexlen, uint8_t *bin, size_t binlen)
{
	size_t i = 0;
	size_t chars = (binlen > (hexlen / 2)) ? (hexlen / 2) : binlen;

	for (i = 0; i < chars; i++) {
		bin[i] = (uint8_t)(bin_char((uint8_t)hex[(i*2)]) << 4);
		bin[i] |= (uint8_t)bin_char((uint8_t)hex[((i*2)+1)]);
	}
}

int hex2bin_m(const char *hex, size_t hexlen, uint8_t **bin, size_t binlen)
{
	uint8_t *buf = NULL;

	if(1 == hexlen) {
		*bin = NULL;
		return 0;
	}

	buf = (uint8_t *)calloc(1, binlen);
	if (!buf)
		return -ENOMEM;

	hex2bin(hex, hexlen, buf, binlen);
	*bin = buf;
	return 0;
}


