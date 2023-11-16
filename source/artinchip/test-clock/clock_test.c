#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdbool.h>

#define CLOCK_VERSION "0.1"
#define DIR_CLK "/sys/kernel/debug/clk"
#define GET_PARENT	0
#define GET_ALL_INFO 	1
#define GET_NAME	2
#define GET_MAJOR_INFO  3


char clk_info_type_aic[3][20] = {
	"clk_rate",
	"clk_enable_count",
	"clk_parent"
};

static void usage(char *program)
{
	printf("\n");
	printf("Usage: %s [a] [-a name] [-p name] [-g name] [-v] [-h]\n", program);
	printf("   -a Get all info for clocks.\n");
	printf("   -a NAME  Get all info for clock with this name.\n");
	printf("   -f NAME  Get all major info for clock with this name.\n");
	printf("   -p NAME  Get all parent for clock with this name.\n");
	printf("   -v Display clock_test version.\n");
	printf("   -h Display this help screen\n");
	printf("\n");
}


static char *clk_info_get(char *path, char *filename)
{
	FILE *file;
	char *buffer = NULL;
	char *temp = (char *)malloc(64 * sizeof(char));

	sprintf(temp, "%s/%s", path, filename);

	file = fopen(temp, "r");
	free(temp);
	temp =NULL;

	if (file == NULL)
		return NULL;

	buffer = (char *)malloc(32 * sizeof(char));

	fscanf(file, "%s\\0", buffer);
	fclose(file);

	return buffer;
}


char *clk_get_parent_rate(char *clk_name)
{
	char *path = (char *)malloc(64 * sizeof(char));
	char *parent_name = NULL;
	char *parent_rate = NULL;
	sprintf(path, "%s/%s", DIR_CLK, clk_name);

	/*get parent clock*/
	parent_name = clk_info_get(path, "clk_parent");
	if (parent_name == NULL)
		goto __out;

	/*get parent rate*/
	sprintf(path, "%s/%s", DIR_CLK, parent_name);
	parent_rate = clk_info_get(path, "clk_rate");

	free(parent_name);
	parent_name = NULL;
__out:
	free(path);
	path = NULL;
	return parent_rate;
}


static int clk_get_parent(char *clk_name)
{
	char *path = (char *)malloc(64 * sizeof(char));
	char *parent_name = NULL;

	sprintf(path, "%s/%s", DIR_CLK, clk_name);

	parent_name = clk_info_get(path, "clk_parent");
	if (parent_name == NULL) {
		printf("%s", clk_name);
		goto __out;
	}

	clk_get_parent(parent_name);
	printf(" -> %s", clk_name);

__out:
	free(path);
	path = NULL;
	free(parent_name);
	parent_name = NULL;
	return 0;
}


void clk_each_file(char *clk_name)
{
	char dirname[64];
	DIR *dir;
	struct dirent *entry;
	char *buffer = NULL;
	char *filename = (char *)malloc(512 * sizeof(char));

	printf("\n-----\t%s\t-----\n", clk_name);
	sprintf(dirname, "%s/%s", DIR_CLK, clk_name);

	dir = opendir(dirname);
	printf("dirname:%s\n", dirname);
	if (dir == NULL) {
		free(filename);
		filename = NULL;
	printf("Unable to open directory");
	return;
}

	/*each all files in this clock dir*/
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_type != DT_REG)
			continue;

		buffer = clk_info_get(dirname, entry->d_name);
		printf("%-20s%s", entry->d_name, buffer);
		printf("\n");
	};

	free(filename);
	filename = NULL;
	free(buffer);
	buffer = NULL;
	closedir(dir);
}

void clk_major_info(char *clk_name)
{
	int num = sizeof(clk_info_type_aic)/sizeof(clk_info_type_aic[0]);
	char *path = (char *)malloc(64 * sizeof(char));
	char *parent_rate = NULL;

	printf("%8s  ", clk_name);

	sprintf(path, "%s/%s", DIR_CLK, clk_name);
	for (int i= 0; i < num; i++) {
		char *buffer = clk_info_get(path, clk_info_type_aic[i]);
		printf("%14s",buffer);
		free(buffer);
		buffer = NULL;
	}
	parent_rate = clk_get_parent_rate(clk_name);
	printf("%16s",parent_rate);
	free(parent_rate);
	parent_rate = NULL;

	printf("\n");
}

void get_byname(char *name, int ch)
{
	DIR *dir = opendir(DIR_CLK);
	struct dirent *entry;
	int ret;

	if (dir == NULL) {
		printf("Unable to open directory");
		return;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_type != DT_DIR || strcmp(entry->d_name,".") == 0\
			|| strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		ret = strncmp(entry->d_name, name, strlen(name));
		if (ret == 0) {
			switch (ch)
			{
			case GET_PARENT:
				clk_get_parent(entry->d_name);
				printf("\n");
				break;
			case GET_ALL_INFO:
				clk_each_file(entry->d_name);
				printf("\n");
				break;
			case GET_MAJOR_INFO:
				clk_major_info(entry->d_name);
				printf("\n");
			default:
				break;
			}
		}
	}
}


void get_all_info_task(char *clk[], int opt)
{
	int i = 2;
	if (clk[i]) {
		while (clk[i]) {
			get_byname(clk[i], GET_ALL_INFO);
			i++;
			}
		printf("\n");
	} else
		system("cat /sys/kernel/debug/clk/clk_summary");
}

void get_parent_task(char *clk[])
{
	int i = 2;
	printf("------------ get parent --------------\n");
	while (clk[i]) {
		get_byname(clk[i], GET_PARENT);
		i++;
	}
	printf("\n");
}

void get_major_info_task(char *clk[])
{
	int i = 2;

	printf( "---------------------------------------------------------------------\n");
	printf( "   clock         rate        enable count  parent parent      rate   \n");
	printf( "------------  -----------  --------------  -------------  -----------\n");

	while (clk[i]) {
		get_byname(clk[i], GET_MAJOR_INFO);
		i++;
	}
	printf("\n");
}

int main(int argc, char *argv[])
{
	int opt;
	extern int optind, opterr, optopt;
	extern char *optarg;
	char buffer[256];

	if (access("/sys/kernel/debug/clk/", F_OK) != 0)
		system("mount -t debugfs none /sys/kernel/debug/");

	system("clear");

	while ((opt = getopt(argc, argv, "a::p:f:s:vh")) != -1) {
		switch (opt) {
		case 'a':
			get_all_info_task(argv, optind);
			exit(0);
			break;
		case 'p':
			get_parent_task(argv);
			exit(0);
			break;
		case 'f':
			get_major_info_task(argv);
			exit(0);
			break;
		case 's':
			sprintf(buffer, "echo %s > /sys/kernel/debug/clk\
					/%s/clk_rate", argv[3], argv[2]);
			system(buffer);
			get_byname(argv[2], GET_ALL_INFO);
			exit(0);
			break;
		case 'v':
			printf("clock_test version:0.1\n");
		case 'h':
		default:
			goto __out;
		}
	}

__out:
	usage(argv[0]);
	exit(0);
	return 0;
}
