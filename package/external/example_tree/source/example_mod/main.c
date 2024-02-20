#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

int main(int argc, char *argv[])
{
	int i;

	printf("argc: %d\n", argc);
	for (i = 0; i < argc; i++) {
		printf(" %s\n", argv[i]);
	}
	return 0;
}
