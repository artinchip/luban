// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>

#include "artinchip/sample_base.h"

#define VERSION_STRING "1.0"
#define TEST_MODE_NORMAL 0
#define TEST_MODE_PRESSURE 1
#define TEST_MODE_STABILITY 2
#define TEST_MODE_CIRCLE 3
#define TEST_MODE_CIRCLE_X1 4

#define SIMPLE_ARGC_NUM 4
#define NORMAL_ARGC_NUM 5

#define uart_printf(fmt, ...) printf("%s " fmt, get_cur_time(), ##__VA_ARGS__)

static const char *m_send_device = "/dev/ttyS2";
static const char *m_receive_device = "/dev/ttyS3";
static int m_baudrate = 9600;

char *test_msg1 = "1234567890artinchip0987654321";
char *test_msg2 = "1234567890abcdefghijklmnopqurstuvwxyzartinchipzyxwvutsruqponmlkjihgfedcba0987654321";
char *str_null = "no";

static int m_test_mode = -1;

static char *get_cur_time()
{
	static char s[24];
	time_t t;
	struct tm *ltime;

	time(&t);
	ltime = localtime(&t);
	strftime(s, 24, "[%Y-%m-%d %H:%M:%S]", ltime);
	return s;
}

static const struct option lopts[] = {
	{"normal", optional_argument, 0, 'N'},
	{"pressure", optional_argument, 0, 'P'},
	{"stability", optional_argument, 0, 'S'},
	{"circle", optional_argument, 0, 'C'},
	{"circle x1", optional_argument, 0, 'c'},
	{NULL, 0, 0, 0},
};

static void print_usage(const char *prog)
{
	printf("Usage: %s -N/P/S/C dev1 dev2 baudrate\n", prog);
	printf("  -N --normal    Normal test\n"
		   "  -P --pressure  Pressure test with Speed test\n"
		   "  -S --stability Stability test\n"
		   "  -C --circle    Continue circle test within one port\n"
		   "  -c --circle    Circle test within one port for one time\n"
		   "  dev1  send device, %s means no \n"
		   "  dev2  receive device, %s means no \n",
		   str_null, str_null);
	printf("%s -N /dev/ttyS1 /dev/ttyS2 115200\n", prog);
	printf("%s -P %s /dev/ttys2 115200\n", prog, str_null);
	printf("%s -S /dev/ttyS1 %s 115200\n", prog, str_null);
	printf("%s -C /dev/ttyS1 115200\n", prog);
	exit(1);
}

static char *get_test_mode_string(int mode)
{
	switch (mode) {
	case TEST_MODE_NORMAL:
		return "Normal Test";
	case TEST_MODE_PRESSURE:
		return "Pressure Test";
	case TEST_MODE_STABILITY:
		return "Stability";
	case TEST_MODE_CIRCLE:
		return "Circle";
	case TEST_MODE_CIRCLE_X1:
		return "circle";
	default:
		return "UNKNOWN";
	}
}

static void print_parameter(int argc)
{
	printf("Test Mode: %d(%s) \n", m_test_mode, get_test_mode_string(m_test_mode));
	printf("Sender   : %s \n", m_send_device);
	if (argc == SIMPLE_ARGC_NUM)
		printf("Receiver : %s \n", m_send_device);
	else
		printf("Receiver : %s \n", m_receive_device);
	printf("Baudrate : %d \n\n", m_baudrate);
}

static int parse_opts(int argc, char *argv[])
{
	int c;
	int ret = 0;

	while (1) {
		c = getopt_long(argc, argv, "NPSCc", lopts, NULL);

		if (c == -1)
			break;
		switch (c) {
		case 'N':
			m_test_mode = TEST_MODE_NORMAL;
			break;
		case 'P':
			m_test_mode = TEST_MODE_PRESSURE;
			break;
		case 'S':
			m_test_mode = TEST_MODE_STABILITY;
			break;
		case 'C':
			m_test_mode = TEST_MODE_CIRCLE;
			break;
		case 'c':
			m_test_mode = TEST_MODE_CIRCLE_X1;
			break;
		default:
			print_usage(argv[0]);
			ret = -1;
			break;
		}
	}

	m_send_device = argv[2];
	if (argc == SIMPLE_ARGC_NUM) {
		m_receive_device = str_null;
		m_baudrate = atoi(argv[3]);
	} else if (argc == NORMAL_ARGC_NUM) {
		if (m_test_mode == TEST_MODE_CIRCLE 
			|| m_test_mode == TEST_MODE_CIRCLE_X1) {
			printf("circle test just need 3 parameters \n");
			printf("%s -C /dev/ttyS1 115200\n", argv[0]);
			return -1;
		}
		m_receive_device = argv[3];
		m_baudrate = atoi(argv[4]);
	} else {
		print_usage(argv[0]);
		return -1;
	}

	return ret;
}

static int is_null_device(const char *device)
{
	return !strcasecmp(device, str_null);
}

static int open_device(const char *device)
{
	int fd;
	int ret;

	fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd < 0) {
		printf("can't open device %s!\n", device);
		return -1;
	}

	ret = fcntl(fd, F_SETFL, 0);
	if (ret < 0)
		printf("fcntl for failed!\n");

	ret = isatty(fd);
	if (ret == 0) {
		printf("standard input is not a terminal device for %s\n", device);
		close(fd);
		return -1;
	}

	return fd;
}

static int config_device(int fd, int nSpeed, int nBits, int nParity, int nStop)
{
	struct termios newtio, oldtio;

	if (tcgetattr(fd, &oldtio) != 0) {
		perror("tcgetattr");
		return -1;
	}

	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag |= CLOCAL | CREAD;
	newtio.c_cflag &= ~CSIZE;
	switch (nBits) {
	case 7:
		newtio.c_cflag |= CS7;
		break;
	case 8:
		newtio.c_cflag |= CS8;
		break;
	default:
		printf("Unsupported data size\n");
		return -1;
	}

	switch (nParity) {
	case 'o':
	case 'O':
		newtio.c_cflag |= PARENB;
		newtio.c_cflag |= PARODD;
		newtio.c_iflag |= (INPCK | ISTRIP);
		break;
	case 'e':
	case 'E':
		newtio.c_iflag |= (INPCK | ISTRIP);
		newtio.c_cflag |= PARENB;
		newtio.c_cflag &= ~PARODD;
		break;
	case 'n':
	case 'N':
		newtio.c_cflag &= ~PARENB;
		break;
	default:
		printf("Unsupported parity\n");
		return -1;
	}

	switch (nStop) {
	case 1:
		newtio.c_cflag &= ~CSTOPB;
		break;
	case 2:
		newtio.c_cflag |= CSTOPB;
		break;
	default:
		printf("Unsupported stop bits\n");
		return -1;
	}
	switch (nSpeed) {
	case 2400:
		cfsetispeed(&newtio, B2400);
		cfsetospeed(&newtio, B2400);
		break;
	case 4800:
		cfsetispeed(&newtio, B4800);
		cfsetospeed(&newtio, B4800);
		break;
	case 9600:
		cfsetispeed(&newtio, B9600);
		cfsetospeed(&newtio, B9600);
		break;
	case 19200:
		cfsetispeed(&newtio, B19200);
		cfsetospeed(&newtio, B19200);
		break;
	case 38400:
		cfsetispeed(&newtio, B38400);
		cfsetospeed(&newtio, B38400);
		break;
	case 57600:
		cfsetispeed(&newtio, B57600);
		cfsetospeed(&newtio, B57600);
		break;
	case 115200:
		cfsetispeed(&newtio, B115200);
		cfsetospeed(&newtio, B115200);
		break;
	case 230400:
		cfsetispeed(&newtio, B230400);
		cfsetospeed(&newtio, B230400);
		break;
	case 460800:
		cfsetispeed(&newtio, B460800);
		cfsetospeed(&newtio, B460800);
		break;
	case 500000:
		cfsetispeed(&newtio, B500000);
		cfsetospeed(&newtio, B500000);
		break;
	case 576000:
		cfsetispeed(&newtio, B576000);
		cfsetospeed(&newtio, B576000);
		break;
	case 921600:
		cfsetispeed(&newtio, B921600);
		cfsetospeed(&newtio, B921600);
		break;
	case 1000000:
		cfsetispeed(&newtio, B1000000);
		cfsetospeed(&newtio, B1000000);
		break;
	case 1152000:
		cfsetispeed(&newtio, B1152000);
		cfsetospeed(&newtio, B1152000);
		break;
	case 1500000:
		cfsetispeed(&newtio, B1500000);
		cfsetospeed(&newtio, B1500000);
		break;
	case 2500000:
		cfsetispeed(&newtio, B2500000);
		cfsetospeed(&newtio, B2500000);
		break;
	case 3000000:
		cfsetispeed(&newtio, B3000000);
		cfsetospeed(&newtio, B3000000);
		break;
	default:
		printf("\tSorry, Unsupported baud rate, set default 115200!\n");
		cfsetispeed(&newtio, B115200);
		cfsetospeed(&newtio, B115200);
		break;
	}

	newtio.c_cc[VTIME] = 1;
	newtio.c_cc[VMIN] = 1;

	tcflush(fd, TCIFLUSH);
	if (tcsetattr(fd, TCSANOW, &newtio) != 0) {
		perror("tcsetattr");
		return -1;
	}
	return 0;
}

int uart_recv(int fd, char *rcv_buf, int data_len, int timeout)
{
	int len, fs_sel;
	fd_set fs_read;
	struct timeval time;

	time.tv_sec = timeout / 1000;
	time.tv_usec = timeout % 1000 * 1000;

	FD_ZERO(&fs_read);
	FD_SET(fd, &fs_read);

	memset(rcv_buf, 0, data_len);
	fs_sel = select(fd + 1, &fs_read, NULL, NULL, &time);
	if (fs_sel) {
		len = read(fd, rcv_buf, data_len);
		return len;
	}
	else
		return -1;
}

int uart_send(int fd, char *send_buf, int data_len)
{
	ssize_t ret = 0;

	ret = write(fd, send_buf, data_len);
	tcflush(fd, TCOFLUSH);
	if (ret == data_len)
		return ret;
	else
		return -1;
}

/*
	send what you input
	if input nothing, will send the default string
*/
static void uart_send_test(int fd)
{
	char send_buf[10240];
	int send_len;

	uart_printf("=============================================\n");
	uart_printf("Please input messages you want to send\n");
	while (1) {
		scanf("%s", send_buf);
		if (strlen(send_buf) == 0)
			strcpy(send_buf, test_msg1);

		send_len = uart_send(fd, send_buf, strlen(send_buf));
		if (send_len > 0)
			printf("%s send %d: |%s|\n", m_send_device, send_len, send_buf);
		else
			printf("send data failed！\n");
	}
}

/* Receive and re-send the received one */
static void uart_receive_test(int fd, int log)
{
	char rcv_buf[1025];
	int rcv_len = 0;

	while (1) {
		rcv_len = uart_recv(fd, rcv_buf, 1000, 10000);
		if (rcv_len > 0) {
			if (log) {
				rcv_buf[rcv_len] = '\0';
				printf("%s recv %d: |%s|\n", m_receive_device, rcv_len, rcv_buf);
			}
		}
		else
			continue;
		
		usleep(10000);
	}
}

static void uart_stability_receive_test(int fd)
{
	int len;
	char rcv_buf[1025];
	unsigned int count = 0;
	unsigned int total = 0;

	printf("=============== receiving =================== \n");
	while (1) {
		len = uart_recv(fd, rcv_buf, 1000, 10000);
		if (len > 0)
			count += len;
		usleep(10000);
		if (count > 64 * 1024) {
			total += count;
			count = 0;
			uart_printf("%s receive %d B\n", m_receive_device, total);
		}
	}
}

static void cal_speed(long len, struct timeval start, struct timeval end)
{
	long timeused = ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec)) / 1000;

	printf("===========================================\n");
	uart_printf("Len: %ld b, Time: %ldms, Speed: %ld bps\n", len * 8, timeused, (len * 8 * 1000 / timeused));
}

/*
	speed test for send
*/
static void uart_speed_test_send(int fd)
{
	char buf[1024] = {0};
	char send_buf[1025] = {0};
	int send_len;
	long count = 0;
	int i;

	uart_printf("type any key to send 100kB \n");
	for (i = 0; i < 128; i++)
		send_buf[i] = 'A' + i % 10;

	while (1) {
		scanf("%s", buf);
		count = 0;
		uart_printf("start send. \n");
		for (i = 0; i < 800; i++) {
			send_len = uart_send(fd, send_buf, 128);
			if (send_len > 0)
				count += send_len;
		}

		uart_printf("send finish: %ld \n", count);
	}
}

/* speed test for receive */
static void uart_speed_test_receive(int fd)
{
	int rcv_len = 0;
	int receive_begin = 0;
	long count = 0;
	struct timeval start, end;
	char rcv_buf[1025] = {0};

	printf("=============== receiving ===================\n");
	while (1) {
		rcv_len = uart_recv(fd, rcv_buf, 1024, 10000);
		if (rcv_len > 0) {
			if (!receive_begin) {
				receive_begin = 1;
				gettimeofday(&start, NULL);
				count = rcv_len;
			} else {
				count += rcv_len;
			}
		} else {
			if (receive_begin) {
				receive_begin = 0;
				gettimeofday(&end, NULL);
				cal_speed(count, start, end);
				count = 0;
			}
			count = 0;
		}
	}
}

int create_fd(const char *device)
{
	int fd = -1;

	if (is_null_device(device)) {
		DBG("device is invalid: %s\n", device);
		fd = -1;
	} else {
		fd = open_device(device);
		if (config_device(fd, m_baudrate, 8, 'N', 1) < 0) {
			close(fd);
			fd = -1;
		}
	}
	return fd;
}

void *create_receive_thread(void *arg)
{
	int fd = *(int *)arg;

	if ((fd == -1) || (fd == 0)) {
		ERR("Invalid UART device file: %d\n", fd);
		return NULL;
	}
	if (m_test_mode == TEST_MODE_PRESSURE)
		uart_speed_test_receive(fd);
	else if (m_test_mode == TEST_MODE_STABILITY)
		uart_stability_receive_test(fd);
	else
		uart_receive_test(fd, 1);

	return NULL;
}

void uart_normal_test(int argc)
{
	pthread_t tid;
	int send_fd = create_fd(m_send_device);
	int rcv_fd = create_fd(m_receive_device);

	if (send_fd == -1) {
		send_fd = rcv_fd;
		if (send_fd == -1) {
			uart_printf("send and receive fd are all failed to open \n");
			exit(-1);
		}
		uart_receive_test(send_fd, 1);
	} else {
		if (argc == SIMPLE_ARGC_NUM)
			pthread_create(&tid, NULL, create_receive_thread, &send_fd);
		else
			pthread_create(&tid, NULL, create_receive_thread, &rcv_fd);

		uart_send_test(send_fd);
	}
}

void uart_pressure_test(int argc)
{
	pthread_t tid;
	int send_fd = create_fd(m_send_device);
	int rcv_fd = create_fd(m_receive_device);

	if (send_fd == -1) {
		send_fd = rcv_fd;
		if (send_fd == -1) {
			uart_printf("send and receive fd are all failed to open \n");
			exit(-1);
		}
		uart_speed_test_receive(send_fd);
	} else {
		if (argc == SIMPLE_ARGC_NUM)
			pthread_create(&tid, NULL, create_receive_thread, &send_fd);
		else
			pthread_create(&tid, NULL, create_receive_thread, &rcv_fd);

		uart_speed_test_send(send_fd);
	}
}

void uart_stability_test(void)
{
	pthread_t tid = 0;
	int send_fd = create_fd(m_send_device);
	int recv_fd = create_fd(m_receive_device);

	if (send_fd == -1) {
		send_fd = create_fd(m_receive_device);
		if (send_fd == -1) {
			DBG("send and receive fd are all failed to open \n");
			exit(-1);
		}
		uart_receive_test(send_fd, 0);
	} else {
		int len;
		unsigned int count = 0;
		unsigned int total = 0;

		pthread_create(&tid, NULL, create_receive_thread, &recv_fd);
		printf("=============== sending ===================== \n");
		while (1) {
			len = uart_send(send_fd, test_msg2, strlen(test_msg2));
			if (len > 0)
				count += len;
			usleep(10000);
			if (count > 64 * 1024) {
				total += count;
				count = 0;
				uart_printf("%s send %d B\n", m_send_device, total);
			}
		}
	}
}

/* Circle RX/TX of one Port for send receive test */
static void uart_circle_test(int times)
{
	int send_len = 0;
	int rcv_len = 0;
	int count = 0;
	int fd = create_fd(m_send_device);
	char rcv_buf[1025];

	if (fd == -1)
		return;

	while (1) {
		send_len = uart_send(fd, test_msg1, strlen(test_msg1));
		if (send_len > 0)
			uart_printf("send data is:    %s\n", test_msg1);
		else
			uart_printf("send data failed！\n");

		usleep(20000);
		rcv_len = uart_recv(fd, rcv_buf, 100, 10000);
		if (rcv_len > 0) {
			rcv_buf[rcv_len] = '\0';
			uart_printf("receive data is: %s\n", rcv_buf);
		} else {
			uart_printf("receive data failed！\n");
			continue;
		}

		if (strcmp(test_msg1, rcv_buf) || send_len != rcv_len) {
			uart_printf(" tested failed !\n");
			uart_printf("send: %d [%s]\n", send_len, test_msg1);
			uart_printf("receive: %d [%s]\n", rcv_len, rcv_buf);
		}
		else
			uart_printf("Test Success !\n");

		count ++;
		if(times == count)
			break;

		usleep(5000000);
	}
}

int main(int argc, char *argv[])
{
	if (argc != SIMPLE_ARGC_NUM && argc != NORMAL_ARGC_NUM) {
		print_usage(argv[0]);
		exit(-1);
	}

	if (parse_opts(argc, argv) < 0) {
		print_usage(argv[0]);
		exit(-1);
	}

	print_parameter(argc);

	switch (m_test_mode) {
	case TEST_MODE_NORMAL:
		uart_normal_test(argc);
		break;
	case TEST_MODE_PRESSURE:
		uart_pressure_test(argc);
		break;
	case TEST_MODE_STABILITY:
		uart_stability_test();
		break;
	case TEST_MODE_CIRCLE:
		uart_circle_test(-1);
		break;
	case TEST_MODE_CIRCLE_X1:
		uart_circle_test(1);
		break;
	default:
		ERR("Unsupported mode: %d\n", m_test_mode);
		return -1;
	}

	return 0;
}
