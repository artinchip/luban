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

#define MTOP_VERSION "0.1"
#define MTOP_DIR	"/sys/devices/platform/soc/184ff000.mtop/mtop"
#define ENABLE 	1
#define DISABLE 0

struct NodeInfo {
	int id;
	char *name;
	FILE *fp;
	unsigned int count;
};

struct NodeCtrl {
	int id;
	char *name;
	FILE *fp;
};

enum mtop_ctrl {
	MTOP_SWITCH,
	MTOP_PERIOD,
};

const char *groupName[] = {"cpu", "dma", "de", "gvd", "ahb"};

struct NodeInfo nodeInfo_aic[] = {
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

struct NodeCtrl nodeCtrl_aic[] = {
	{ 0, "enable", NULL },
	{ 1, "set_period", NULL },
};

int nodeInfoCnt;
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
	printf("mtop version: %s\n", MTOP_VERSION);
	printf("\n");
}

static int mtop_read(void)
{
	int i;
	char path[256];

	for (i = 0; i < nodeInfoCnt; i++) {
		snprintf(path, sizeof(path), "%s/%s", path_prefix,
			 nodeInfo_aic[i].name);
		nodeInfo_aic[i].fp = fopen(path, "r");
		if (NULL == nodeInfo_aic[i].fp) {
			fprintf(stderr, "Could not open file %s: %s\n",
				path, strerror(errno));
			goto open_error;
		}

		fscanf(nodeInfo_aic[i].fp, "%u", &nodeInfo_aic[i].count);
		fclose(nodeInfo_aic[i].fp);
		nodeInfo_aic[i].fp = NULL;
	}
	return 0;

open_error:
	for (i = 0; i < nodeInfoCnt; i++) {
		if (nodeInfo_aic[i].fp) {
			fclose(nodeInfo_aic[i].fp);
			nodeInfo_aic[i].fp = NULL;
		}
	}
	return -1;
}

static void mtop_update(void)
{
	int i;

	for (i = 0; i < nodeInfoCnt; i++)
		nodeInfo_aic[i].count /= delay;

	/* save the cursor position */
	printf("\033[s");
	/* Clear content from the cursor to the end of line */
	printf("\033[K");
	fprintf(output_fp, "\n\n%12s %16s %16s %16s %16s\n", "name",
		"read", "write", "total", "%percent");
	for (i = 0; i < nodeInfoCnt; i += 2) {
		fprintf(output_fp, "%12s %12.2fMB/s %12.2fMB/s %12.2fMB/s %12.2f\n",
			groupName[i / 2],
			(float)nodeInfo_aic[i].count / 1000000,
			(float)nodeInfo_aic[i + 1].count / 1000000,
			(float)(nodeInfo_aic[i].count +
			nodeInfo_aic[i + 1].count) /1000000,
			(float)(nodeInfo_aic[i].count +
			nodeInfo_aic[i + 1].count) / 1000000 / ddr_bw * 100);
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

	snprintf(path, sizeof(path), "%s/%s", MTOP_DIR,
		 nodeCtrl_aic[ctrl_type].name);
	nodeCtrl_aic[ctrl_type].fp = fopen(path, "w");
	if (NULL == nodeCtrl_aic[ctrl_type].fp) {
		fprintf(stderr, "Could not open file %s: %s\n",
			path, strerror(errno));
		return;
	}

	fwrite(buf, sizeof(buf[0]), sizeof(buf) / sizeof(buf[0]),
	       nodeCtrl_aic[ctrl_type].fp);
	fflush(nodeCtrl_aic[ctrl_type].fp);
	fclose(nodeCtrl_aic[ctrl_type].fp);
	nodeCtrl_aic[ctrl_type].fp = NULL;
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
		printf("Invalid sec parameter: should be in the\
			range of [1, 100]\n");
		return;
	}

	mtop_ctrl(sec, MTOP_PERIOD);
}

static void sigterm(int signo)
{
	int i;
	/* Disable MTOP */
	mtop_switch(DISABLE);
	/* Default period setting is 1 seconds */
	mtop_set_period(1);
	/* Cursor move down */
	if (!output_fn[0])
		printf("\033[9B");
	/* Display the cursor */
	printf("\033[?25h");
	for (i = 0; i < nodeInfoCnt; i++)
		if (nodeInfo_aic[i].fp)
			fclose(nodeInfo_aic[i].fp);

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
		return 1;
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
	/* nodeInfo_aic[6] is to set period, so node index is from 1 to 5*/
	nodeInfoCnt = sizeof(nodeInfo_aic) / sizeof(nodeInfo_aic[0]);
	strncpy(path_prefix, MTOP_DIR, sizeof(path_prefix));

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
			exit(0);
		}
	}

	ret = get_ddr_band_width();
	if (ret) {
		printf("Can't get DDR bandwidth\n");
		return 1;
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
	fprintf(output_fp, "mtop version: %s\n", MTOP_VERSION);
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
