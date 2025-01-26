#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "devmem.h"

#define PWM2_BASE	0x27052000
#define HLPERIOD0	0x000
#define PERIOD0		0x004
#define POLARITY	0x040
#define PWMSTART	0x044
#define PWMUPDATE	0x04c
#define PWM_OE		0x0D0

#define PERIOD 100

/*
 * Vfb = 0.6V, VDDIO = 1.8V
 * R1 = 50k, R2 = 100k, R3 = 200k
 * Vout = Vfb + R1 * (Vfb / R2 - (VDDIO * duty_cycle - Vfb) / R3)
 */

void print_usage(void)
{
	printf("vddc_tool for Athena2\n");
	printf("vddc_tool -r				  <== read vddc pwm duty cycle\n");
	printf("vddc_tool -p [duty_cycle](0.05-0.55)	  <== write vddc pwm duty cycle\n");
	printf("vddc_tool -v [voltage/V](0.9/0.95/1.0)    <== write voltage\n");
	exit(-1);
}

int main(int argc, char *argv[])
{
	int opt = 0;
	uint32_t hlperiod = 0, period = 0;
	double duty_cycle = 0, voltage = 0;

	if (argc == 1)
		print_usage();

	while ((opt = getopt(argc, argv, "rp:v:")) != -1) {
		switch (opt) {
		case 'r':
			hlperiod = devmem_readl(PWM2_BASE + HLPERIOD0);
			period = devmem_readl(PWM2_BASE + PERIOD0);
			duty_cycle = (period - hlperiod) * 1.0 / period;
			printf("duty_cycle: %f\n", duty_cycle);
			break;

		case 'p':
			if (sscanf(optarg, "%lf", &duty_cycle) != 1) {
				print_usage();
				break;
			}
			if (duty_cycle > 0.55 || duty_cycle < 0.05) {
				printf("invalid argument.\n");
				break;
			}
			hlperiod = PERIOD - PERIOD * duty_cycle;
			devmem_writel(PWM2_BASE + HLPERIOD0, hlperiod);
			devmem_writel(PWM2_BASE + PERIOD0, PERIOD);
			devmem_writel(PWM2_BASE + PWMUPDATE, 0x1);
			devmem_writel(PWM2_BASE + PWMUPDATE, 0x0);
			break;

		case 'v':
			if (sscanf(optarg, "%lf", &voltage) != 1) {
				print_usage();
				break;
			}
			if (voltage != 0.9 && voltage != 0.95 && voltage != 1.0) {
				printf("invalid argument.\n");
				break;
			}

			duty_cycle = (1.05 - voltage)/0.45;
			hlperiod = PERIOD - PERIOD * duty_cycle;
			devmem_writel(PWM2_BASE + HLPERIOD0, hlperiod);
			devmem_writel(PWM2_BASE + PERIOD0, PERIOD);
			devmem_writel(PWM2_BASE + PWMUPDATE, 0x1);
			devmem_writel(PWM2_BASE + PWMUPDATE, 0x0);
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
