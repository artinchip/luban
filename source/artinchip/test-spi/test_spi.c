// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2023 ArtInChip Technology Co., Ltd.
 */

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define MAX_SIZE 0x100000

struct test_config {
	char *device;
	char *input_file;
	char *output_file;
	char *input_tx;
	int speed;
	int delay;
	int bits;
	int mode;
	int verbose;
	int transfer_size;
	int tx_only;
	int iterations;
};

#define STAT_INTERVAL 5

struct test_stat {
	uint64_t prev_read_count;
	uint64_t prev_write_count;
	uint64_t cur_read_count;
	uint64_t cur_write_count;
};

struct test_config test_cfg = { 0 };
struct test_stat test_st = { 0 };

static void print_usage(const char *prog)
{
	printf("Usage: %s\n", prog);
	puts("  -D --device   device to use (default /dev/spidev1.1)\n"
	     "  -s --speed    max speed (Hz)\n"
	     "  -d --delay    delay (usec)\n"
	     "  -b --bpw      bits per word\n"
	     "  -i --input    input data from a file (e.g. \"test.bin\")\n"
	     "  -o --output   output data to a file (e.g. \"results.bin\")\n"
	     "  -l --loop     loopback\n"
	     "  -H --cpha     clock phase\n"
	     "  -O --cpol     clock polarity\n"
	     "  -L --lsb      least significant bit first\n"
	     "  -C --cs-high  chip select active high\n"
	     "  -3 --3wire    SI/SO signals shared\n"
	     "  -v --verbose  Verbose (show tx buffer)\n"
	     "  -p            Send data (e.g. \"1234\\xde\\xad\")\n"
	     "  -N --no-cs    no chip select\n"
	     "  -R --ready    slave pulls low to pause\n"
	     "  -2 --dual     dual transfer\n"
	     "  -4 --quad     quad transfer\n"
	     "  -S --size     transfer size\n"
	     "  -T --tx-only  send data only\n"
	     "  -I --iter     iterations\n");
	exit(1);
}

static void dump_config(struct test_config *cfg)
{
	printf("configuration:\n");
	printf("\tdevice        :%s\n", cfg->device);
	printf("\tinput_file    :%s\n", cfg->input_file);
	printf("\toutput_file   :%s\n", cfg->output_file);
	printf("\tinput_tx      :%s\n", cfg->input_tx);
	printf("\tspeed         :%d\n", cfg->speed);
	printf("\tdelay         :%d\n", cfg->delay);
	printf("\tbits          :%d\n", cfg->bits);
	printf("\tmode          :0x%x\n", cfg->mode);
	printf("\tverbose       :%d\n", cfg->verbose);
	printf("\ttransfer_size :%d\n", cfg->transfer_size);
	printf("\ttx_only       :%d\n", cfg->tx_only);
	printf("\titerations    :%d\n", cfg->iterations);
}

static void parse_opts(int argc, char *argv[], struct test_config *cfg)
{
	memset(cfg, 0, sizeof(*cfg));

	cfg->bits = 8;
	cfg->speed = 500000;
	while (1) {
		static const struct option lopts[] = {
			{ "device",  1, 0, 'D' },
			{ "speed",   1, 0, 's' },
			{ "delay",   1, 0, 'd' },
			{ "bpw",     1, 0, 'b' },
			{ "input",   1, 0, 'i' },
			{ "output",  1, 0, 'o' },
			{ "loop",    0, 0, 'l' },
			{ "cpha",    0, 0, 'H' },
			{ "cpol",    0, 0, 'O' },
			{ "lsb",     0, 0, 'L' },
			{ "cs-high", 0, 0, 'C' },
			{ "3wire",   0, 0, '3' },
			{ "no-cs",   0, 0, 'N' },
			{ "ready",   0, 0, 'R' },
			{ "dual",    0, 0, '2' },
			{ "verbose", 0, 0, 'v' },
			{ "quad",    0, 0, '4' },
			{ "size",    1, 0, 'S' },
			{ "iter",    1, 0, 'I' },
			{ "tx-only", 0, 0, 'T' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "D:s:d:b:i:o:lHOLC3NR248p:vS:I:T",
				lopts, NULL);

		if (c == -1)
			break;

		switch (c) {
		case 'D':
			cfg->device = optarg;
			break;
		case 's':
			cfg->speed = atoi(optarg);
			break;
		case 'd':
			cfg->delay = atoi(optarg);
			break;
		case 'b':
			cfg->bits = atoi(optarg);
			break;
		case 'i':
			cfg->input_file = optarg;
			break;
		case 'o':
			cfg->output_file = optarg;
			break;
		case 'l':
			cfg->mode |= SPI_LOOP;
			break;
		case 'H':
			cfg->mode |= SPI_CPHA;
			break;
		case 'O':
			cfg->mode |= SPI_CPOL;
			break;
		case 'L':
			cfg->mode |= SPI_LSB_FIRST;
			break;
		case 'C':
			cfg->mode |= SPI_CS_HIGH;
			break;
		case 'N':
			cfg->mode |= SPI_NO_CS;
			break;
		case 'v':
			cfg->verbose = 1;
			break;
		case 'R':
			cfg->mode |= SPI_READY;
			break;
		case 'p':
			cfg->input_tx = optarg;
			break;
		case '2':
			cfg->mode |= SPI_TX_DUAL;
			break;
		case '3':
			cfg->mode |= SPI_3WIRE;
			break;
		case '4':
			cfg->mode |= SPI_TX_QUAD;
			break;
		case '8':
			cfg->mode |= SPI_TX_OCTAL;
			break;
		case 'S':
			cfg->transfer_size = atoi(optarg);
			break;
		case 'T':
			cfg->tx_only = 1;
			break;
		case 'I':
			cfg->iterations = atoi(optarg);
			break;
		default:
			print_usage(argv[0]);
		}
	}
	if (cfg->mode & SPI_LOOP) {
		if (cfg->mode & SPI_TX_DUAL)
			cfg->mode |= SPI_RX_DUAL;
		if (cfg->mode & SPI_TX_QUAD)
			cfg->mode |= SPI_RX_QUAD;
		if (cfg->mode & SPI_TX_OCTAL)
			cfg->mode |= SPI_RX_OCTAL;
	}
}

static void pabort(const char *s)
{
	if (errno != 0)
		perror(s);
	else
		printf("%s\n", s);

	abort();
}

static void hex_dump(const void *src, size_t length, size_t line_size,
		     char *prefix)
{
	int i = 0;
	const unsigned char *address = src;
	const unsigned char *line = address;
	unsigned char c;

	printf("%s | ", prefix);
	while (length-- > 0) {
		printf("%02X ", *address++);
		if (!(++i % line_size) || (length == 0 && i % line_size)) {
			if (length == 0) {
				while (i++ % line_size)
					printf("__ ");
			}
			printf(" |");
			while (line < address) {
				c = *line++;
				printf("%c", (c < 32 || c > 126) ? '.' : c);
			}
			printf("|\n");
			if (length > 0)
				printf("%s | ", prefix);
		}
	}
}
/*
 *  Unescape - process hexadecimal escape character
 *      converts shell input "\x23" -> 0x23
 */
static int unescape(char *_dst, char *_src, size_t len)
{
	int ret = 0;
	int match;
	char *src = _src;
	char *dst = _dst;
	unsigned int ch;

	while (*src) {
		if (*src == '\\' && *(src+1) == 'x') {
			match = sscanf(src + 2, "%2x", &ch);
			if (!match)
				pabort("malformed input string");

			src += 4;
			*dst++ = (unsigned char)ch;
		} else {
			*dst++ = *src++;
		}
		ret++;
	}
	return ret;
}

static void send_data(struct test_config *cfg, int fd, uint8_t const *tx,
		      size_t len)
{
	int ret;
	struct spi_ioc_transfer tr;

	memset(&tr, 0, sizeof(tr));
	tr.tx_buf = (unsigned long)tx;
	tr.len = len;
	tr.delay_usecs = cfg->delay;
	tr.speed_hz = cfg->speed;
	tr.bits_per_word = cfg->bits;


	if (cfg->mode & SPI_TX_OCTAL)
		tr.tx_nbits = 8;
	else if (cfg->mode & SPI_TX_QUAD)
		tr.tx_nbits = 4;
	else if (cfg->mode & SPI_TX_DUAL)
		tr.tx_nbits = 2;

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		pabort("can't send spi message");

	if (cfg->verbose)
		hex_dump(tx, len, 32, "TX");
}

static void recv_data(struct test_config *cfg, int fd, uint8_t const *rx,
		      size_t len)
{
	int ret;
	int out_fd;
	struct spi_ioc_transfer tr;

	memset(&tr, 0, sizeof(tr));
	tr.rx_buf = (unsigned long)rx;
	tr.len = len;
	tr.delay_usecs = cfg->delay;
	tr.speed_hz = cfg->speed;
	tr.bits_per_word = cfg->bits;

	if (cfg->mode & SPI_RX_OCTAL)
		tr.rx_nbits = 8;
	else if (cfg->mode & SPI_RX_QUAD)
		tr.rx_nbits = 4;
	else if (cfg->mode & SPI_RX_DUAL)
		tr.rx_nbits = 2;

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		pabort("can't send spi message");

	if (cfg->output_file) {
		out_fd = open(cfg->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
		if (out_fd < 0)
			pabort("could not open output file");

		ret = write(out_fd, rx, len);
		if (ret != len)
			pabort("not all bytes written to output file");

		close(out_fd);
	}

	if (cfg->verbose)
		hex_dump(rx, len, 32, "RX");
}

static void show_transfer_rate(struct test_stat *st)
{
	double rx_rate, tx_rate;

	rx_rate = ((st->cur_read_count - st->prev_read_count) * 8) /
		  (STAT_INTERVAL * 1000.0);
	tx_rate = ((st->cur_write_count - st->prev_write_count) * 8) /
		  (STAT_INTERVAL * 1000.0);

	printf("rate: tx %.1fkbps, rx %.1fkbps\n", tx_rate, rx_rate);

	st->prev_read_count  = st->cur_read_count;
	st->prev_write_count = st->cur_write_count;
}


int main(int argc, char *argv[])
{
	struct test_config *cfg = &test_cfg;
	int fd, ret = 0;

	parse_opts(argc, argv, cfg);

	if (cfg->input_tx && cfg->input_file)
		pabort("only one of -p and --input may be selected");
	if (cfg->verbose)
		dump_config(cfg);

	fd = open(cfg->device, O_RDWR);
	if (fd < 0)
		pabort("can't open device");

	/*
	 * spi mode
	 */
	ret = ioctl(fd, SPI_IOC_WR_MODE32, &cfg->mode);
	if (ret == -1)
		pabort("can't set spi mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE32, &cfg->mode);
	if (ret == -1)
		pabort("can't get spi mode");

	/*
	 * bits per word
	 */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &cfg->bits);
	if (ret == -1)
		pabort("can't set bits per word");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &cfg->bits);
	if (ret == -1)
		pabort("can't get bits per word");

	/*
	 * max speed hz
	 */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &cfg->speed);
	if (ret == -1)
		pabort("can't set max speed hz");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &cfg->speed);
	if (ret == -1)
		pabort("can't get max speed hz");

	uint8_t *tx = NULL;
	uint8_t *rx = NULL;
	int tsize = 0;
	size_t size = 0;

	if (cfg->transfer_size)
		tsize = cfg->transfer_size;

	if (cfg->input_tx) {
		/* If the data to be sent is from command line */
		size = strlen(cfg->input_tx);

		tx = malloc(size);
		if (!tx)
			pabort("can't allocate tx buffer");

		size = unescape((char *)tx, cfg->input_tx, size);
	}
	if (cfg->input_file) {
		/* If the data to be sent is from binary file */
		struct stat sb;
		int tx_fd;

		if (stat(cfg->input_file, &sb) == -1)
			pabort("can't stat input file");

		size = sb.st_size;
		if (size > MAX_SIZE)
			size = MAX_SIZE;
		tx = malloc(size);
		if (!tx)
			pabort("can't allocate tx buffer");
		tx_fd = open(cfg->input_file, O_RDONLY);
		if (tx_fd < 0)
			pabort("can't open input file");
		size = read(tx_fd, tx, size);
		if (size <= 0)
			pabort("failed to read input file");
	}
	if (tsize < size)
		tsize = size;
	if (!tx)
		pabort("nothing to be sent");
	if (!cfg->tx_only) {
		rx = malloc(tsize);
		if (!rx)
			pabort("can't allocate tx buffer");
	}

	struct timespec last_stat;
	struct test_stat *st = &test_st;

	clock_gettime(CLOCK_MONOTONIC, &last_stat);
	do {
		struct timespec current;

		send_data(cfg, fd, tx, tsize);
		st->cur_write_count += tsize;
		if (!cfg->tx_only) {
			recv_data(cfg, fd, rx, tsize);
			st->cur_read_count += tsize;
		}
		clock_gettime(CLOCK_MONOTONIC, &current);
		if (current.tv_sec - last_stat.tv_sec > STAT_INTERVAL) {
			show_transfer_rate(st);
			last_stat = current;
		}
	} while ((--cfg->iterations) > 0);

	printf("total: tx %.1fKB, rx %.1fKB\n", st->cur_write_count / 1024.0,
	       st->cur_read_count / 1024.0);
	close(fd);

	if (tx)
		free(tx);
	if (rx)
		free(rx);
	return ret;
}
