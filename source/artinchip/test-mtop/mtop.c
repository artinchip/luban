// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 ArtInChip Technology Co., Ltd.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ENABLE 	1
#define DISABLE 0

struct node_info {
	int id;
	char *name;
	FILE *fp;
	unsigned int count;
};

struct node_ctrl {
	int id;
	char *name;
	FILE *fp;
};

enum mtop_ctrl {
	MTOP_SWITCH,
	MTOP_PERIOD,
};

struct mtop_info {
	char *version;
	const char **group_name;
	struct node_info *node_info_aic;
	int node_num;
};

const char *group_name_v10[] = {"cpu", "dma", "de", "gvd", "ahb"};

const char *group_name_v13[] = {"cpu", \
			"usb", "gmac", "sdmc0", "sdmc1", "qspi", "usb_h0", "usb_h1", \
			"dvp", "de", "dma0", "dma1", "dma2", \
			"img", "print", "scan", "ahb"};

struct node_info node_info_aic_v10[] = {
	{ 1, "cpu_rd", NULL, 0 },
	{ 2, "cpu_wr", NULL, 0 },
	{ 3, "dma_rd", NULL, 0 },
	{ 4, "dma_wr", NULL, 0 },
	{ 5, "de_rd", NULL, 0 },
	{ 6, "de_wr", NULL, 0 },
	{ 7, "gvd_rd", NULL, 0 },
	{ 8, "gvd_wr", NULL, 0 },
	{ 9, "ahb_rd", NULL, 0 },
	{ 10, "ahb_wr", NULL, 0 },
};

struct node_info node_info_aic_v13[] = {
	{ 1, "cpu_rd", NULL, 0 },
	{ 2, "cpu_wr", NULL, 0 },
	{ 3, "axi1_rd", NULL, 0 },
	{ 4, "axi1_wr", NULL, 0 },
	{ 5, "dvp_rd", NULL, 0 },
	{ 6 , "dvp_wr", NULL, 0 },
	{ 7, "de_rd", NULL, 0 },
	{ 8, "de_wr", NULL, 0 },
	{ 9, "dma0_rd", NULL, 0 },
	{ 10, "dma0_wr", NULL, 0 },
	{ 11, "dma1_rd", NULL, 0 },
	{ 12, "dma1_wr", NULL, 0 },
	{ 13, "dma2_rd", NULL, 0 },
	{ 14, "dma2_wr", NULL, 0 },
	{ 15, "img_rd", NULL, 0 },
	{ 16, "img_wr", NULL, 0 },
	{ 17, "print_rd", NULL, 0 },
	{ 18, "print_wr", NULL, 0 },
	{ 19, "scan_rd", NULL, 0 },
	{ 20, "scan_wr", NULL, 0 },
	{ 21, "ahb_rd", NULL, 0 },
	{ 22, "ahb_wr", NULL, 0 },
};

struct node_ctrl node_ctrl_aic[] = {
	{ 0, "enable", NULL },
	{ 1, "set_period", NULL },
};

static struct mtop_info mtop_data[] = {
	{ .version = "0.0",
	  .node_num = 10,
	  .group_name = group_name_v10,
	  .node_info_aic = node_info_aic_v10,},
	{ .version = "3.0",
	  .node_num = 22,
	  .group_name = group_name_v13,
	  .node_info_aic = node_info_aic_v13, },
	{},
};

static struct mtop_info *mtop = NULL;
char mtop_dir[128];
int node_info_cnt;
int delay = 1;
int iter;
char output_fn[256];
FILE *output_fp = NULL;


char path_prefix[128];
unsigned int ddr_bw;

static void usage(char *program)
{
	printf("\n");
	printf("Usage: %s [-n iter] [-d delay] [-m] [-o FILE] [-h]\n", program);
	printf("   -n NUM   Number of updates before this program exiting.\n");
	printf("   -d NUM   Seconds to wait between update.\n");
	printf("   -o FILE  Output to a file.\n");
	printf("   -v Display mtop version.\n");
	printf("   -h Display this help screen.\n");
	printf("\n");
}

static void version(void)
{
	printf("\n");
	printf("mtop version: %s\n", mtop->version);
	printf("\n");
}

static int get_version_data()
{
	int ret = -1;
	FILE *file;
	char buffer[16] = " ";
	char path[64] = "ln -sf /sys/devices/platform/soc/*.mtop/mtop";

	system(path);
	chdir("/mtop");
	getcwd(path, 64);

	snprintf(mtop_dir, sizeof(mtop_dir), "%s/%s", path, "version");

	file = fopen(mtop_dir, "r");
	if (!file) {
		fprintf(stderr, "Failed to open %s errno: %d[%s]\n",
			mtop_dir, errno, strerror(errno));
		return -1;
	}

	ret = fread(buffer, sizeof(char), 16, file);
	if (ret <= 0) {
		fprintf(stderr, "fread() return %d, errno: %d[%s]\n",
			ret, errno, strerror(errno));
		fclose(file);
		return -1;
	}
	fclose(file);

	for (int i = 0; i < 2; i++) {
		if (strlen(mtop_data[i].version) != (strlen(buffer) - 1))
			continue;
		ret = strncmp(mtop_data[i].version, buffer,
				strlen(mtop_data[i].version));
		if (ret == 0) {
			mtop = &mtop_data[i];
			node_info_cnt = mtop->node_num;
			break;
		}
	}

	if (ret == -1) {
		printf("Error! no version !");
		return -1;
	}
	snprintf(mtop_dir, sizeof(mtop_dir), path);
	return 0;
}

static int mtop_read(void)
{
	int i;
	char path[256];
	struct node_info *node_info_aic = mtop->node_info_aic;

	for (i = 0; i < node_info_cnt; i++) {
		snprintf(path, sizeof(path), "%s/%s", path_prefix,
			 node_info_aic[i].name);
		node_info_aic[i].fp = fopen(path, "r");
		if (NULL == node_info_aic[i].fp) {
			fprintf(stderr, "Could not open file %s: %s\n",
				path, strerror(errno));
			goto open_error;
		}

		fscanf(node_info_aic[i].fp, "%u", &node_info_aic[i].count);
		fclose(node_info_aic[i].fp);
		node_info_aic[i].fp = NULL;
	}
	return 0;

open_error:
	for (i = 0; i < node_info_cnt; i++) {
		if (node_info_aic[i].fp) {
			fclose(node_info_aic[i].fp);
			node_info_aic[i].fp = NULL;
		}
	}
	return -1;
}

static void mtop_update(void)
{
	int i;
	const char **group_name = mtop->group_name;
	struct node_info *node_info_aic = mtop->node_info_aic;

	for (i = 0; i < node_info_cnt; i++)
		node_info_aic[i].count /= delay;

	/* save the cursor position */
	printf("\033[s");
	/* Clear content from the cursor to the end of line */
	printf("\033[K");
	fprintf(output_fp, "\n\n%12s %16s %16s %16s %16s\n", "name",
		"read", "write", "total", "%percent");
	for (i = 0; i < node_info_cnt; i += 2) {
		fprintf(output_fp, "%12s %12.2fMB/s %12.2fMB/s %12.2fMB/s %12.2f\n",
			group_name[i / 2],
			(float)node_info_aic[i].count / 1000000,
			(float)node_info_aic[i + 1].count / 1000000,
			(float)(node_info_aic[i].count +
			node_info_aic[i + 1].count) /1000000,
			(float)(node_info_aic[i].count +
			node_info_aic[i + 1].count) / 1000000 / ddr_bw * 100);
	}
	fprintf(output_fp, "\n");
	/* restore cursor position */
	if (iter != 0)
		printf("\033[u");
}

static void mtop_ctrl(int val, enum mtop_ctrl ctrl_type)
{
	char path[256];
	char buf[8];

	switch (ctrl_type) {
	case MTOP_SWITCH:
	case MTOP_PERIOD:
		break;
	default:
		fprintf(stderr, "ctrl_type is error\n");
		return;
	}

	snprintf(buf, sizeof(buf), "%d", val);

	snprintf(path, sizeof(path), "%s/%s", mtop_dir,
		 node_ctrl_aic[ctrl_type].name);
	node_ctrl_aic[ctrl_type].fp = fopen(path, "w");
	if (NULL == node_ctrl_aic[ctrl_type].fp) {
		fprintf(stderr, "Could not open file %s: %s\n",
			path, strerror(errno));
		return;
	}

	fwrite(buf, sizeof(buf[0]), sizeof(buf) / sizeof(buf[0]),
	       node_ctrl_aic[ctrl_type].fp);
	fflush(node_ctrl_aic[ctrl_type].fp);
	fclose(node_ctrl_aic[ctrl_type].fp);
	node_ctrl_aic[ctrl_type].fp = NULL;
}

static void mtop_switch(int sw)
{
	if (sw != 0 && sw != 1) {
		printf("Invalid parameter sw: should be 0 or 1\n");
		return;
	}

	mtop_ctrl(sw, MTOP_SWITCH);
}

static void mtop_set_period(int sec)
{
	if (sec <= 0 || sec > 100) {
		printf("Invalid sec parameter: should be in the"
			"range of [1, 100]\n");
		return;
	}

	mtop_ctrl(sec, MTOP_PERIOD);
}

static void sigterm(int signo)
{
	int i;
	struct node_info *node_info_aic = mtop->node_info_aic;

	/* Disable MTOP */
	mtop_switch(DISABLE);
	/* Default period setting is 1 seconds */
	mtop_set_period(1);
	/* Cursor move down */
	if (!output_fn[0])
		printf("\033[9B");
	/* Display the cursor */
	printf("\033[?25h");
	for (i = 0; i < node_info_cnt; i++)
		if (node_info_aic[i].fp)
			fclose(node_info_aic[i].fp);

	if (output_fp)
		fclose(output_fp);
	exit(0);
}

static int get_ddr_band_width(void)
{
	FILE *fp;

	system("mount -t debugfs none /sys/kernel/debug/");
	fp = fopen("/sys/kernel/debug/clk/ddr/clk_rate", "r");
	if (!fp) {
		printf("open ddr clock node error\n");
		return -1;
	}
	/* Get DDR controller frequency */
	fscanf(fp, "%u", &ddr_bw);
	/* Get DDR band width */
	ddr_bw = ddr_bw * 8 / 1000000;
	fclose(fp);
	system("umount -t debufs /sys/kernel/debug/");
	return 0;
}

int main(int argc, char *argv[])
{
	int opt, ret;
	extern int optind, opterr, optopt;
	extern char *optarg;

	ret = get_version_data();
	if (ret) {
		printf("Can't get mtop version data\n");
		return -1;
	}
	/* node_info_aic[6] is to set period, so node index is from 1 to 5*/
	strncpy(path_prefix, mtop_dir, sizeof(path_prefix));

	memset(output_fn, 0, sizeof(output_fn));
	output_fp = stdout;
	iter = -1;

	while ((opt = getopt(argc, argv, "n:d:kmo:hv")) != -1) {
		switch (opt) {
		case 'n':
			if (!optarg) {
				usage(argv[0]);
				exit(0);
			}
			iter = strtoul(optarg, (char **)NULL, 10);
			break;
		case 'd':
			if (!optarg) {
				usage(argv[0]);
				exit(0);
			}
			delay = strtoul(optarg, (char **)NULL, 10);
			break;
		case 'o':
			if (!optarg) {
				usage(argv[0]);
				exit(0);
			}
			strncpy(output_fn, optarg, sizeof(output_fn));
			break;
		case 'h':
			usage(argv[0]);
			exit(0);
		case 'v':
			version();
			exit(0);
		default:
			usage(argv[0]);
			exit(-1);
		}
	}

	ret = get_ddr_band_width();
	if (ret) {
		printf("Can't get DDR bandwidth\n");
		return -1;
	}

	mtop_set_period(delay);
	mtop_switch(ENABLE);

	signal(SIGTERM, sigterm);
	signal(SIGHUP, sigterm);
	signal(SIGINT, sigterm);

	if (output_fn[0]) {
		output_fp = fopen(output_fn, "wt+");
		if (stdout == output_fp) {
			fprintf(stderr, "Could not open file %s: %s\n",
				output_fn, strerror(errno));
			exit(-1);
		}
	}

	if (!output_fn[0])
		system("clear");
	fprintf(output_fp, "mtop version: %s\n", mtop->version);
	fprintf(output_fp, "mtop is used to show DDR bandwidth.\n");
	fprintf(output_fp, "DDR total band width: %uMB/s\n", ddr_bw);
	if (output_fn[0])
		fprintf(output_fp, "output: %s\n", output_fn);
	/* Hide the cursor */
	printf("\033[?25l");
	while (iter == -1 || iter-- > 0) {
		mtop_read();
		mtop_update();
		sleep(delay);
	}

	mtop_switch(DISABLE);
	mtop_set_period(1);
	/* Display the cursor */
	printf("\033[?25h");
	if (output_fp)
		fclose(output_fp);

	return 0;
}
