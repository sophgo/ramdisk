/*
 * cvi_pinmux for cv84x6
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "func.h"
#include "devmem.h"

#define NELEMS(x) (sizeof(x) / sizeof((x)[0]))
#define PINMUX_SEL_SHIFT 4
#define SCT_EN_SHIFT  12
#define OEX_EN_SHIFT  13

static group_pinmux_regs regs = {
	.g1_pinmux = G1_PINMUX_REG_REG_BASE,
	.g2_pinmux = G2_PINMUX_REG_REG_BASE,
	.g3_pinmux = G3_PINMUX_REG_REG_BASE,
	.g4_pinmux = G4_PINMUX_REG_REG_BASE,
	.g7_pinmux = G7_PINMUX_REG_REG_BASE,
	.g8_pinmux = G8_PINMUX_REG_REG_BASE,
};

unsigned long convert_group_to_addr(uint32_t group, int index)
{
	switch (group)
	{
	case G1:
		return regs.g1_pinmux + index * 4;
	case G2:
		return regs.g2_pinmux + index * 4;
	case G3:
		return regs.g3_pinmux + index * 4;
	case G4:
		return regs.g4_pinmux + index * 4;
	case G7:
		return regs.g7_pinmux + index * 4;
	case G8:
		return regs.g8_pinmux + index * 4;
	}
	return 0;
}

const pinctrl_entry *convert_group_to_pins(uint32_t group)
{
	switch (group)
	{
	case G1:
		return g1_pins;
	case G2:
		return g2_pins;
	case G3:
		return g3_pins;
	case G4:
		return g4_pins;
	case G7:
		return g7_pins;
	case G8:
		return g8_pins;
	}
	return NULL;
}

unsigned long get_group_pinmux_base(const char *pin, uint32_t group)
{
	const pinctrl_entry *group_pinlist = convert_group_to_pins(group);
	if (!group_pinlist)
	{
		printf("get group pinlist failed!\n");
		return -1;
	}
	int i = 0;
	for(; group_pinlist[i].name != NULL; ++i)
	{
		if (strcmp(group_pinlist[i].name, pin) == 0)
		{
			break;
		}
	}
	if (group_pinlist[i].name == NULL) {
		printf("\ninvalid pin name %s!\n", pin);
		return -1;
	}
	unsigned long addr = convert_group_to_addr(group, i);
	if (!addr) {
		printf("\ninvalid pin name:%s\n", pin);
		return -1;
	}
	return addr;
}

uint32_t check_pinmux(const char *pin, uint32_t group)
{
	unsigned long addr = get_group_pinmux_base(pin, group);
	if (addr == (unsigned long)-1)
		return -1;
	printf("pinctrl reg: 0x%lX\n", addr);
	return (devmem_readl(addr) & 0xf0) >> PINMUX_SEL_SHIFT;
}

void set_pinmux(const char *pin, const char *func, uint32_t group, int index)
{
	int i;
	uint32_t max_fun_num = NELEMS(pinlist_map[index].funcs);
	uint32_t mux_val = 0;

	for (i = 0; i < max_fun_num; ++i) {
		if (strcmp(func, pinlist_map[index].funcs[i]) == 0)
			break;
	}
	if (i == max_fun_num) {
		printf("Invalid func %s\n", func);
		return;
	}
	mux_val = pinlist_map[index].func_vals[i];

	unsigned long addr = get_group_pinmux_base(pin, group);
	if (addr == (unsigned long)-1)
		return;
	uint32_t val = devmem_readl(addr);
	val &= ~(0xf << PINMUX_SEL_SHIFT);
	val |= (mux_val << PINMUX_SEL_SHIFT);
	devmem_writel(addr, val);
	printf("set pinmux reg:0x%lX, value: %u\n", addr, mux_val);
}

/* cv84x2: PU_EN=bit2, PD_EN=bit3 (pull_shift=2) */
void set_pin_pd(const char *pin, int pull_down, uint32_t group)
{
	const pinctrl_entry *group_pinlist = convert_group_to_pins(group);
	unsigned long addr = get_group_pinmux_base(pin, group);
	if (addr == (unsigned long)-1)
		return;
	uint32_t val = devmem_readl(addr);
	uint8_t shift = group_pinlist->pull_shift;  /* 2 for cv84x2 */

	val &= ~(1 << shift) & ~(1 << (shift + 1));
	if (pull_down != 2) {
		if (pull_down)
			val |= (1 << shift);      /* PU_EN */
		else
			val |= (1 << (shift + 1)); /* PD_EN */
	}
	devmem_writel(addr, val);
	printf("set pin pull-up/down reg:0x%lx, shift:%d\n", addr, shift);
}

void set_pin_drv(const char *pin, int driving, uint32_t group)
{
	unsigned long addr = get_group_pinmux_base(pin, group);
	if (addr == (unsigned long)-1)
		return;
	uint32_t val = devmem_readl(addr);
	uint8_t shift = 8;

	val &= ~(0xf << shift);
	val |= driving << shift;
	devmem_writel(addr, val);
	printf("set pin driving reg:0x%lx\n", addr);
}

void set_pin_sct(const char *pin, int enable, uint32_t group)
{
	unsigned long addr = get_group_pinmux_base(pin, group);
	if (addr == (unsigned long)-1)
		return;
	uint32_t val = devmem_readl(addr);
	val &= ~(1 << SCT_EN_SHIFT);
	if (enable)
		val |= (1 << SCT_EN_SHIFT);
	devmem_writel(addr, val);
	printf("set pin Schmitt trigger:0x%lx, %s\n", addr, enable ? "enable" : "disable");
}

void set_pin_oex(const char *pin, int enable, uint32_t group)
{
	unsigned long addr = get_group_pinmux_base(pin, group);
	if (addr == (unsigned long)-1)
		return;
	uint32_t val = devmem_readl(addr);
	val &= ~(1 << OEX_EN_SHIFT);
	if (enable)
		val |= (1 << OEX_EN_SHIFT);
	devmem_writel(addr, val);
	printf("set pin open-drain:0x%lx, %s\n", addr, enable ? "enable" : "disable");
}

void print_fun(const char *pin, uint32_t reg_val, int index, uint32_t group)
{
	uint32_t i = 0;
	uint32_t max_fun_num = NELEMS(pinlist_map[index].funcs);

	printf("%s function:\n", pin);
	for (i = 0; i < max_fun_num; i++)
	{
		if (strncmp(pinlist_map[index].funcs[i], "NULL", 4) != 0)
		{
			int is_current = (pinlist_map[index].func_vals[i] == reg_val);
			printf("[%c] %s\n", is_current ? 'v' : ' ', pinlist_map[index].funcs[i]);
		}
	}
	printf("\n");
}

void print_usage(void)
{
	printf("cvi_pinmux for cv84x2/cv84x6\n");
	printf("./cvi_pinmux -p          <== List all pins\n");
	printf("./cvi_pinmux -l          <== List all pins and its func\n");
	printf("./cvi_pinmux -r pin      <== Get func from pin\n");
	printf("./cvi_pinmux -w pin/func <== Set func to pin\n");
	printf("./cvi_pinmux -c <pin>,<0|1|2>  <== Set pull (0:down; 1:up; 2:off)\n");
	printf("./cvi_pinmux -d <pin>,<0~15>   <== Set driving\n");
	printf("./cvi_pinmux -s <pin>,<0|1>    <== Schmitt trigger (1:enable)\n");
	printf("./cvi_pinmux -o <pin>,<0|1>    <== Open-drain (1:enable)\n");
	exit(-1);
}

int main(int argc, char *argv[])
{
	int opt = 0;
	uint32_t i = 0;
	uint32_t pinmux_val;
	char pin[32];
	char func[32];
	int pull_down, driving, sct_en, oex_en;

	if (argc == 1)
	{
		print_usage();
	}

	while ((opt = getopt(argc, argv, "hplr:w:c:d:s:o:")) != -1)
	{
		switch (opt)
		{
		case 'r':
			for (i = 0; i < NELEMS(pinlist_map); i++)
			{
				if (strcmp(optarg, pinlist_map[i].pin) == 0)
					break;
			}
			if (i != NELEMS(pinlist_map)) {
				pinmux_val = check_pinmux(pinlist_map[i].pin, pinlist_map[i].group);
				if (pinmux_val != (uint32_t)-1) {
					print_fun(pinlist_map[i].pin, pinmux_val, i, pinlist_map[i].group);
					printf("value: %u\n", pinmux_val);
				}
			}
			else
			{
				printf("\nInvalid pin: %s\n", optarg);
			}
			break;

		case 'w':
			if (sscanf(optarg, "%[^/]/%s", pin, func) != 2) {
				print_usage();
				break;
			}

			printf("pin %s\n", pin);
			printf("func %s\n", func);

			for (i = 0; i < NELEMS(pinlist_map); i++)
			{
				if (strcmp(pin, pinlist_map[i].pin) == 0)
					break;
			}
			if (i == NELEMS(pinlist_map)) {
				printf("\nInvalid option: %s\n", optarg);
				break;
			}
			set_pinmux(pin, func, pinlist_map[i].group, i);
			break;

		case 'p':
			printf("Pinlist:\n");
			for (i = 0; i < NELEMS(pinlist_map); i++)
				printf("%s\n", pinlist_map[i].pin);
			break;

		case 'l':
			for (i = 0; i < NELEMS(pinlist_map); i++)
			{
				pinmux_val = check_pinmux(pinlist_map[i].pin, pinlist_map[i].group);
				if (pinmux_val == (uint32_t)-1)
					continue;
				print_fun(pinlist_map[i].pin, pinmux_val, i, pinlist_map[i].group);
			}
			break;

		case 'c':
			if (sscanf(optarg, "%[^,],%d", pin, &pull_down) != 2) {
				print_usage();
				break;
			}
			if (pull_down != 0 && pull_down != 1 && pull_down != 2) {
				print_usage();
				break;
			}

			for (i = 0; i < NELEMS(pinlist_map); i++)
			{
				if (strcmp(pin, pinlist_map[i].pin) == 0)
					break;
			}
			if (i == NELEMS(pinlist_map)) {
				printf("\nInvalid option: pin %s\n", pin);
				break;
			}
			set_pin_pd(pin, pull_down, pinlist_map[i].group);
			break;

		case 'd':
			if (sscanf(optarg, "%[^,],%d", pin, &driving) != 2) {
				print_usage();
				break;
			}
			if (driving < 0 || driving > 15) {
				print_usage();
				break;
			}
			for (i = 0; i < NELEMS(pinlist_map); i++)
			{
				if (strcmp(pin, pinlist_map[i].pin) == 0)
					break;
			}
			if (i == NELEMS(pinlist_map)) {
				printf("\nInvalid option: pin %s\n", pin);
				break;
			}
			set_pin_drv(pin, driving, pinlist_map[i].group);
			break;

		case 's':
			if (sscanf(optarg, "%[^,],%d", pin, &sct_en) != 2) {
				print_usage();
				break;
			}
			if (sct_en != 0 && sct_en != 1) {
				print_usage();
				break;
			}
			for (i = 0; i < NELEMS(pinlist_map); i++) {
				if (strcmp(pin, pinlist_map[i].pin) == 0)
					break;
			}
			if (i == NELEMS(pinlist_map)) {
				printf("\nInvalid option: pin %s\n", pin);
				break;
			}
			set_pin_sct(pin, sct_en, pinlist_map[i].group);
			break;

		case 'o':
			if (sscanf(optarg, "%[^,],%d", pin, &oex_en) != 2) {
				print_usage();
				break;
			}
			if (oex_en != 0 && oex_en != 1) {
				print_usage();
				break;
			}
			for (i = 0; i < NELEMS(pinlist_map); i++) {
				if (strcmp(pin, pinlist_map[i].pin) == 0)
					break;
			}
			if (i == NELEMS(pinlist_map)) {
				printf("\nInvalid option: pin %s\n", pin);
				break;
			}
			set_pin_oex(pin, oex_en, pinlist_map[i].group);
			break;

		case 'h':
			print_usage();
			break;

		case '?':
			print_usage();
			break;

		default:
			print_usage();
			break;
		}
	}

	return 0;
}
