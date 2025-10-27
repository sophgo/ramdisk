#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "canread.h"
#include "devmem.h"

#define NELEMS(x) (sizeof(x) / sizeof((x)[0]))

uint32_t flag_l = 0 ;
uint32_t flag_r = 0 ;
uint32_t flag_v = 0 ;
uint32_t flag_class = 0;
char name[32] = "can0";
uint32_t base = CAN0_BASE;
uint32_t reg_index = 0;
char reg_class_list[9][20] = {
							"CANT_READ ",
							"CONTROL   ",
							"ACCEP_MASK",
							"BITRATE   ",
							"ARB       ",
							"FIFO_CTRL ",
							"ERR       ",
							"IRQ       ",
							"STATUS    ",
							};

void print_usage(void)
{
	printf("canread				i:lr:IEFABSCMvV\n");
	printf("./canread -i dev	<== can0 / can1; defult can0\n");
	printf("./canread -l		<== List all regs\n");
	printf("./canread -r index	<== show reg e.g 0x12\n");
	printf("./canread -v		<== show func\n");
	printf("./canread -I		<== show interrupt reg\n");
	printf("./canread -E		<== show ERR reg\n");
	printf("./canread -F		<== show FIFO reg\n");
	printf("./canread -A		<== show ARB reg\n");
	printf("./canread -B		<== show BITRATE reg\n");
	printf("./canread -S		<== show STATUS reg\n");
	printf("./canread -C		<== show CTRL reg\n");
	printf("./canread -M		<== show ACCEP MASK reg\n");
	exit(-1);
}
void print_v(uint32_t index, uint32_t val)
{
	int func_val = 0;
	
	for (size_t i = 0; i <= 8; i++)
	{
		if (can_reg_map[index].funcs[i].mask != 0 && can_reg_map[index].funcs[i].mask !=0xff)
		{
			func_val = (can_reg_map[index].funcs[i].mask & val) >> can_reg_map[index].funcs[i].end;

			printf("   ·[%d:%d]%-26s [0x%x]\n",
												can_reg_map[index].funcs[i].start,
												can_reg_map[index].funcs[i].end,
												can_reg_map[index].funcs[i].name,
												func_val);
		} else {
			break;
		}
		
	}
	if (can_reg_map[index].funcs[0].mask !=0xff)
		printf("\n");
	
	
	
	
}

void print_reg(uint32_t i)
{
	uint32_t val = 0;
	uint32_t reg = base;
	char *rw;

	reg = base + can_reg_map[i].index * 4;
	// printf("reg 0x%08x\n", reg);
	if (can_reg_map[i].rw < 3 && can_reg_map[i].reg_flag_list != 1)
	{
		val = devmem_readl(reg);
		rw = can_reg_map[i].rw==0 ? "RW" : "R";
		printf("[%02d][%-2s]:%-26s 0x%08x = 0x%02x  ", 
														can_reg_map[i].index, 
														rw,
														can_reg_map[i].name, 
														reg, 
														val);
		for (size_t j = 0; j <= 8; j++)
		{
			if (can_reg_map[i].reg_class & BIT(j))
			{
				printf("|%s",reg_class_list[j]); 
			}
		}
		printf("\n");
		if (flag_v == 1)
		{
			print_v(can_reg_map[i].index, val);
		}
		
	} else{
		rw = can_reg_map[i].rw==3 ? "W" : "R";
		printf("[%02d][%-2s]:%-26s                    ", 
			can_reg_map[i].index, 
			rw,
			can_reg_map[i].name);
		
		for (size_t j = 0; j <= 8; j++)
		{
			if (can_reg_map[i].reg_class & BIT(j))
			{
				printf("|%s",reg_class_list[j]); 
			}
		}
		printf("\n");
	}
}
void canread(void)
{
	int num_regs = NELEMS(can_reg_map);
	
	if(flag_l != 1)
		return;

	printf("canread %s:\n", name);
	for (uint32_t i = 0; i < num_regs; i++)
	{
		print_reg(i);
	}
	
}

void print_single_reg(void)
{
	int num_regs = NELEMS(can_reg_map);
	if(flag_r != 1)
		return;

	if (reg_index > num_regs)
	{
		printf("-r > 0x%x", num_regs);
		return;
	}
	
	print_reg(reg_index);
	reg_index = 0;
	
}

void print_flag_reg(void)
{
	int num_regs = NELEMS(can_reg_map);

	if (flag_class)
	{
		printf("canread flag %s:\n", name);
		for (int i = 0; i < num_regs; i++)
		{
			if (can_reg_map[i].reg_class & flag_class)
			{
				print_reg(i);
				
			} 
		}
	}
	
	
}

int main(int argc, char *argv[])
{
	int opt = 0;
	flag_l = 0 ;
	flag_r = 0 ;
	flag_v = 0 ;
	flag_class = 0;
	if (argc == 1)
	{
		print_usage();
	}

	while ((opt = getopt(argc, argv, "i:lr:IEFABSCMvV")) != -1)
	{
		switch (opt)
		{
		case 'i':
			if (strcmp(optarg, "can0") == 0){
				base = CAN0_BASE;
			} else if (strcmp(optarg, "can1") == 0)
			{
				base = CAN1_BASE;
			}else {
				printf("-i  must can0 or can1\n");
				exit(-1);
			}
			break;
		case 'l':
			flag_l = 1;
			break;

		case 'r':
			if (sscanf(optarg, "0x%x", &reg_index) != 1) {
				printf("-r: \n");
				print_usage();
				break;
			}
			flag_r = 1;
			break;
		case 'C':
		flag_class = flag_class|BIT(1);
			break;
		case 'M':
		flag_class = flag_class|BIT(2);
			break;
		case 'B':
		flag_class = flag_class|BIT(3);
			break;
		case 'A':
		flag_class = flag_class|BIT(4);
			break;
		case 'F':
		flag_class = flag_class|BIT(5);
			break;
		case 'E':
		flag_class = flag_class|BIT(6);
			break;
		case 'I':
		flag_class = flag_class|BIT(7);
			break;
		case 'S':
		flag_class = flag_class|BIT(8);
			break;
		case 'v':
		flag_v = 1;
			break;
		case 'V':
		flag_v = 1;
			break;
		
		default:
			printf("default:\n");
			print_usage();
			break;
		}
	}
	
    if (optind < argc) {
        printf("Non-option arguments:\n");
        for (int i = optind; i < argc; i++)
            printf("  %s\n", argv[i]);
    }

	canread();
	print_single_reg();
	print_flag_reg();
	return 0;
}
