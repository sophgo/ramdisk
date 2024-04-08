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

static group_pinmux_regs regs = {
	.g1_pinmux = G1_PINMUX_REG_REG_BASE,	
	.g5_pinmux = G5_PINMUX_REG_REG_BASE,
	.g6_pinmux = G6_PINMUX_REG_REG_BASE,
	.g7_pinmux = G7_PINMUX_REG_REG_BASE,
	.g8_pinmux = G8_PINMUX_REG_REG_BASE,
	.g9_pinmux = G9_PINMUX_REG_REG_BASE,
	.g11_pinmux = G11_PINMUX_REG_REG_BASE,
	.g12_pinmux = G12_PINMUX_REG_REG_BASE,
	.phy_pinmux = PHY_PINMUX_REG_REG_BASE,
};

unsigned long convert_group_to_addr(uint32_t group, int index)
{
	switch (group)
	{
	case PHY:
		return regs.phy_pinmux + index * 4;
	case G1:
		return regs.g1_pinmux + index * 4;
	case G5:
		return regs.g5_pinmux + index * 4;
	case G6:
		return regs.g6_pinmux + index * 4;
	case G7:
		return regs.g7_pinmux + index * 4;
	case G8:
		return regs.g8_pinmux + index * 4;
	case G9:
		return regs.g9_pinmux + index * 4;
	case G11:
		return regs.g11_pinmux + index * 4;
	case G12:
		return regs.g12_pinmux + index * 4;
	}
	return 0;
}

const pinctrl_entry *convert_group_to_pins(uint32_t group)
{
	switch (group)
	{
	case PHY:
		return phy_pins;
	case G1:
		return g1_pins;
	case G5:
		return g5_pins;
	case G6:
		return g6_pins;
	case G7:
		return g7_pins;
	case G8:
		return g8_pins;
	case G9:
		return g9_pins;
	case G11:
		return g11_pins;
	case G12:
		return g12_pins;
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

/**
 * @description: check and return the group pinmux reg val
 * @param {char} *pin
 * @param {uint32_t} group
 * @param {char} *
 * @return {*}
 */
uint32_t check_pinmux(const char *pin, uint32_t group)
{
	unsigned long addr = get_group_pinmux_base(pin, group);
	if (addr == -1)
		return -1;
	printf("pinctrl reg: 0x%lX\n", addr);
	return (devmem_readl(addr) & 0xf0) >> PINMUX_SEL_SHIFT;
}

int check_set_ipmux(const char **ip_mux, const char *func, uint32_t group, unsigned long base, bool set)
{
	int i = 0, ret = 0;
	for (; ip_mux[i] != NULL; ++i)
	{
		if (strcmp(func, ip_mux[i]) == 0)
			break;
	}
	if (ip_mux[i] == NULL)
		return -1;
	ret = ((devmem_readl(base + i * 4) & 0xf) == group);
	if (set) {
		devmem_writel(base + i * 4, group);
		printf("set ip_mux reg:0x%lX, val: %d\n", base + i * 4, devmem_readl(base + i * 4));
	}
	return ret;
}

/**
 * @description: check weather the group ip mux set correct
 * @param {char*} func
 * @param {uint32_t} group
 * @param {int} set 0: don't set ipmux; 1: set ipmux 
 * @return {*} -1: not found in this group, 0: ip mux reg value incorrect, 1: correct
 */
int check_set_ip_mux(const char *func, uint32_t group, bool set)
{
	if (group == PHY)
	{
		return check_set_ipmux(phy_ip_mux, func, 4, PHY_IP_MUX_REG_GRP_BASE, set);
	}
	else if (group == G7)
	{
		return check_set_ipmux(rtc_ip_mux, func, 7, RTC_IP_MUX_REG_GRP_BASE, set);
	}

	return check_set_ipmux(core_ip_mux, func, group, CORE_IP_MUX_REG_GRP_BASE, set);
}

void set_pinmux(const char *pin, const char *func, uint32_t group, int index) 
{
	int i;
	uint32_t max_fun_num = NELEMS(pinlist_map[index].funcs);
	check_set_ip_mux(func, group, 1);

 	unsigned long addr = get_group_pinmux_base(pin, group);
	for (i = 0; i < max_fun_num; ++i) {
		if (strcmp(func, pinlist_map[index].funcs[i]) == 0) 
			break;
	}
	if (i == max_fun_num) {
		printf("Invalid func %s \n", func);
	}
	uint32_t val = devmem_readl(addr);
	val &= ~(0xf << PINMUX_SEL_SHIFT);
	val |= (i << PINMUX_SEL_SHIFT);
	devmem_writel(addr, val);
	printf("set pinmux reg:0x%lX, shift: %d\n", addr, PINMUX_SEL_SHIFT);
}

void set_pin_pd(const char *pin, int pull_down, uint32_t group)
{
	const pinctrl_entry *group_pinlist = convert_group_to_pins(group);
	unsigned long addr = get_group_pinmux_base(pin, group);
	uint32_t val = devmem_readl(addr);
	uint8_t shift = group_pinlist->pull_shift;
	if (shift == 0) {
		if (pull_down == 2)
			val &= (~(1 << shift) & ~(1 << (shift + 1)));
		else {
			val |= (1 << shift);
			if (pull_down == 1)
				val |= (1 << (shift + 1));
			if (pull_down == 0)
				val &= ~(1 << (shift + 1));
		}
	} else {
		val &= (~(1 << shift) & ~(1 << (shift + 1)));
		if (pull_down != 2)
			val |= pull_down ?
				       ((1 << shift) & ~(1 << (shift + 1))) :
				       ((1 << (1 + shift)) & ~(1 << shift));
	}

	devmem_writel(addr, val);
	printf("set pin pull-up/down reg:0x%lx, shift:%d\n", addr, shift);
}

void set_pin_drv(const char *pin, int driving, uint32_t group)
{
	unsigned long addr = get_group_pinmux_base(pin, group);
	uint32_t val = devmem_readl(addr);
	uint8_t shift = 8;

	val &= ~(0xf << shift);
	val |= driving << shift;
	devmem_writel(addr, val);
	printf("set pin driving reg:0x%lx, shift:%d\n", addr, shift);
}

/**
 * @description: print the funcs of the index(th) pin of pinlist_map
 * @param {char} *pin
 * @param {uint32_t} value
 * @param {int} index
 * @param {uint32_t} group
 * @return {*}
 */
void print_fun(const char *pin, uint32_t value, int index, uint32_t group)
{
	uint32_t i = 0;
	int ip_mux_val;	
	uint32_t max_fun_num = NELEMS(pinlist_map[index].funcs);

	printf("%s function:\n", pin);
	for (i = 0; i < max_fun_num; i++)
	{
		if (strncmp(pinlist_map[index].funcs[i], "NULL", strlen("NULL") != 0))
		{
			if (i == value) {
				ip_mux_val = check_set_ip_mux(pinlist_map[index].funcs[i], group, 0);
				if (ip_mux_val == 1 || ip_mux_val == -1) 
					printf("[v] %s\n", pinlist_map[index].funcs[i]);
				else 
					printf("[o] %s. Warnning: group %d ip_mux not set\n", pinlist_map[index].funcs[i], group);
			}
			else
				printf("[ ] %s\n", pinlist_map[index].funcs[i]);
		}
	}
	printf("\n");
}

/**
 * @brief list all power domains' voltage
 * 
 */
void list_power_domain(void)
{
	int i;
	int max_domain_length = 32;

	printf(" ** Athena2 Power Domain Info ** \n");
	printf(" %-*s | Voltage \n", max_domain_length, "Power Domain");
	for (i = 0; i < max_domain_length + 10; ++i)
		printf("-");
	printf("\n");

	for (i = 0; i < NELEMS(pds); ++i) {
		pds[i].voltage = devmem_readl(POWER_DOMAIN_REG) & pds[i].mask;
		printf(" %-*s | %s \n", max_domain_length, pds[i].domain,
		       pds[i].voltage ? "1.8V" : "3.3V");
	}
}

int set_power_domain(const char* power_domain, bool vol)
{
	int i = 0;
	uint32_t pd_nums = NELEMS(pds);
	uint32_t val;

	for (; i < pd_nums; ++i)
		if (strcmp(pds[i].domain, power_domain) == 0)
			break;
	
	if (i == pd_nums)
		return -1;
	
	val = devmem_readl(POWER_DOMAIN_REG);
	pds[i].voltage = vol;
	devmem_writel(POWER_DOMAIN_REG,
		      vol ? (val | pds[i].mask) : (val & (~pds[i].mask)));

	printf("Power domain(%s) : %s \n", power_domain, vol ? "1.8V" : "3.3V");
	return 0;
}

void print_usage(void)
{
	printf("cvi_pinmux for Athena2\n");
	printf("./cvi_pinmux -p          <== List all pins\n");
	printf("./cvi_pinmux -l          <== List all pins and its func\n");
	printf("./cvi_pinmux -r pin      <== Get func from pin\n");
	printf("./cvi_pinmux -w pin/func <== Set func to pin\n");
	printf("./cvi_pinmux -c <pin name>,<0 or 1 or 2>  <== Set pin pull up/down (0:pull down; 1:pull up; 2:pull off)\n");
	printf("./cvi_pinmux -d <pin name>,<0 ~ 15>  <== Set pin driving\n");
	printf("./cvi_pinmux -D all <== List all power domains' voltage \n");
	printf("./cvi_pinmux -D <power domain>,<0 or 1> <== Set the <power domain> voltage to 1(1.8V)/0(3.3V) \n");
	exit(-1);
}

int main(int argc, char *argv[])
{
	int opt = 0;
	uint32_t i = 0;
	uint32_t pinmux_val;
	char pin[32];
	char func[32];
	int pull_down, driving;
	char power_domain[32];
	int vol;

	if (argc == 1)
	{
		print_usage();
	}

	while ((opt = getopt(argc, argv, "hplr:w:c:d:D:")) != -1)
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
				if (pinmux_val == -1)
					exit(-1);
				print_fun(pinlist_map[i].pin, pinmux_val, i, pinlist_map[i].group);

				printf("value: %d\n", pinmux_val);
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
				if (pinmux_val == -1)
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
				printf("\nInvalid option: pin %s, pull_down %s\n", pin, optarg);
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
				printf("\nInvalid option: pin %s, driving %s\n", pin, optarg);
				break;
			}
			set_pin_drv(pin, driving, pinlist_map[i].group);
			break;

		case 'D':
			if (sscanf(optarg, "%[^,],%d", power_domain, &vol) != 2) {
				if (strcmp(power_domain, "all") != 0) {
					printf("Invalid argument! \n");
					break;
				}
				list_power_domain();
				break;
			}
			if (set_power_domain(power_domain, vol)) {
				printf("Invalid power domain: %s\n", power_domain);
				list_power_domain();
			}
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
