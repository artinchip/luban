#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

void usage()
{
	fprintf(stderr,
			"Usage: reg-dump [[-a addr [value]] | [-m module]] [-c count] [-h]\n\n"
			"	-a addr		dump address\n"
			"	-m module	dump module register\n"
			"	-c count	dump register count, default value is 4\n"
			"	-h		show this help usage\n\n"
			"\tFor example:\n"
			"\t\tShow register value of specified address:\n"
			"\t\t\tregister_dump -a 0x18020000\n"
			"\t\tShow 48 register values from the specified address:\n"
			"\t\t\tregister_dump -a 0x18020000 -c 48\n"
			"\t\tWrite data to specified address:\n"
			"\t\t\tregister_dump -a 0x18020000 0x12345678\n"
			"\t\tShow module register values:\n"
			"\t\t\tregister_dump -m Module_name\n"
			"\t\tModule_name can be:\n"
			"\t\t\ttwi0, serial0, ethernet0, adcim, cir, rgb, etc\n"
			"\t\tModule_name can be found in directory /sys/firmware/devicetree/base/soc/\n\n");
}

int dump_core(unsigned int phy_addr, unsigned int count, unsigned int write_val, int write_flag)
{
	void *map_base, *virt_addr;
	unsigned int read_result;
	unsigned int page_size, mapped_size, offset_in_page;
	int fd, i;

	fd = open("/dev/mem", O_RDWR);
	mapped_size = page_size = getpagesize();
	offset_in_page = phy_addr & (page_size - 1);
	/* read or write unit is 4 bytes */
	if (offset_in_page + 4 > page_size) {
		mapped_size *= 2;
	}

	map_base = mmap(NULL, mapped_size, PROT_READ | PROT_WRITE,
					MAP_SHARED, fd, phy_addr & ~(page_size - 1));
	if (!map_base) {
		printf("mmap failed\n");
		close(fd);
		exit(1);
	}

	virt_addr = (char *)map_base + offset_in_page;

	if (write_flag) {
		/* write data to register */
		*(unsigned int *)virt_addr = write_val;
	} else {
		/* Read data from register */
		for (i = 0; i < count; i++) {
			if ((i % 4) == 0)
				printf("\n0x%08x: ", phy_addr);
			read_result = *(volatile unsigned int *)virt_addr;
			printf("%08x ", read_result);
			virt_addr += 4;
			phy_addr += 4;
		}
		printf("\n");
	}

	munmap(map_base, mapped_size);
	close(fd);
	return 0;
}

int main(int argc, char *argv[])
{
	char *module = NULL;
	char *p;
	char command[100];
	char read_data[9] = {0};
	unsigned int count = 4, dump_addr = 0, write_val = 0;
	int ret;
	int write_flag = 0;
	int module_index = 0;
	int fd;
	char *dts_path = "/sys/firmware/devicetree/base/soc/";

	if (argc == 1) {
		usage();
		exit(1);
	}

	while ((ret = getopt(argc, argv, "a:c:m:h")) != -1) {
		switch (ret) {
		case 'a':
			dump_addr = strtoul(optarg, &p, 16);
			if (argv[optind] && isdigit(argv[optind][0])) {
				write_flag = 1;
				write_val = strtoul(argv[optind], &p, 16);
			}
			break;
		case 'c':
			count = strtoul(optarg, &p, 10);
			break;
		case 'm':
			module = optarg;
			break;
		default:
			usage();
			exit(1);
		}
	}

	if (module && dump_addr) {
		fprintf(stderr, "option parameter -a and -m could not be specified simultaneously\n");
		usage();
		exit(1);
	}

	if (!module && !dump_addr) {
		fprintf(stderr, "Error: Both address and module_name are not specified\n");
		usage();
		exit(1);
	}

	if (module) {
		if (isdigit(module[strlen(module) - 1]) &&
			isdigit(module[strlen(module) - 2])) {
				/* module name such as twi23 is invalid */
				printf("module name is error!!!\n");
				exit(1);
		} else if (isdigit(module[strlen(module) - 1])) {
			/* module name such as twi3, serial2, etc */
			module_index = module[strlen(module) - 1] - '0' + 1;
			module[strlen(module) - 1] = '\0';
		} else {
			/* module name such as adcim, rtc, etc */
			module_index = 1;
		}

		sprintf(command, "ls %s | grep ^%s@ | sed -e 's/.*@//; %d!d' > /tmp/.tmp_reg\n", dts_path, module, module_index);
		system(command);

		fd = open("/tmp/.tmp_reg", O_RDWR);
		if (fd == -1) {
			printf("register_dump run error!\n");
			exit(1);
		}

		ret = read(fd, (void *)read_data, 8);
		if (ret != 8) {
			printf("Module not exist!\n");
			exit(1);
		}
		dump_addr = strtoul(read_data, &p, 16);
		close(fd);

		/* delete .tmp_reg file */
		sprintf(command, "rm -f /tmp/.tmp_reg\n");
		system(command);
	}

	return dump_core(dump_addr, count, write_val, write_flag);
}
