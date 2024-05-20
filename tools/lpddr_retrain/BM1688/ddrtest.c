#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include "test.h"

uint32_t addr_ddr_ctrl;
uint32_t phyd_base_addr, cv_ddr_phyd_apb;
uint32_t ddrc_value;
uint32_t rankid;
uint32_t tctdelay_pre_sys0;
uint32_t tctdelay_pre_sys1;

uint32_t rddata;

//uint32_t mask_code_init_sys0[2][4];
//uint32_t mask_code_init_sys1[2][4];

uint8_t uVO_en;
uint8_t retrain_everytime;
uint8_t temp_cnt;
uint8_t urtc_status = 0, uap_status = 0;

int main(int argc, char *argv[])
{
	time_t current_time;
	char *c_time_string;
	int second;

	// uint8_t uSys_num = get_sys_num();//uSys_num;
	uint8_t uSys_num = 2;//uSys_num;
	// printf("sys num ==== %d\n", uSys_num);
	// test_log();

	uVO_en = 0;
	retrain_everytime = 0;
	temp_cnt = 0;
	tctdelay_pre_sys0 = 0;
	tctdelay_pre_sys1 = 0;

	// mrw
	cvx32_synp_mrw_lp4(0x70004000, 23, 0x40, 1);
	cvx32_synp_mrw_lp4(0x70004000, 23, 0x40, 2);
	cvx32_synp_mrw_lp4(0x78004000, 23, 0x40, 1);
	cvx32_synp_mrw_lp4(0x78004000, 23, 0x40, 2);

	// disable rdg tracking
	rddata = devmem_readl(0x70000070);
	rddata = modified_bits_by_value(rddata, 0xf, 19, 16);
	devmem_writel(0x70000070, rddata);

	rddata = devmem_readl(0x78000070);
	rddata = modified_bits_by_value(rddata, 0xf, 19, 16);
	devmem_writel(0x78000070, rddata);

	// enable ctrlupd
	devmem_writel(0x700041a0, 0x00400018);
	devmem_writel(0x780041a0, 0x00400018);

	// clear phyd mrw ca_cmd
	rddata = devmem_readl(0x700001ac);
	rddata = modified_bits_by_value(rddata, 0x0, 25, 24);
	devmem_writel(0x700001ac, rddata);

	rddata = devmem_readl(0x780001ac);
	rddata = modified_bits_by_value(rddata, 0x0, 25, 24);
	devmem_writel(0x780001ac, rddata);

	devmem_writel(0x281000f4, 0); //AP 0x281000f4 clear to 0;

	rddata = devmem_readl(0x05026028);
	rddata = modified_bits_by_value(rddata, 0, 3, 0);//rtc 0x05026028 bit3~0 to 0
	devmem_writel(0x05026028, rddata);

	if (argc == 1) {
		second = 6;
	} else {
		second = atoi(argv[1]);
		retrain_everytime = 1;
	}

	while (1) {
		//
		if ((get_bits_from_value(devmem_readl(0x67004000), 7, 7) == 1) ||
							(get_bits_from_value(devmem_readl(0x67005000), 7, 7) == 1)) {
			uVO_en = 1;
		} else {
			uVO_en = 0;
		}
		urtc_status = get_bits_from_value(devmem_readl(0x05026028), 3, 0);//Get rtc_status
		uap_status  = get_bits_from_value(devmem_readl(0x281000f4), 3, 0);//Get AP status

		for (uint8_t i = 0; i < uSys_num; i++) {  // subsys
			// printf("retrain0\n");
			if (((uVO_en == 0) && (urtc_status == 0)) || ((uVO_en == 1) &&
								(uap_status == 0) && (urtc_status == 0))) {
				if (i == 0) {
					//sys0 base address init
					addr_ddr_ctrl = 0x70004000;
					phyd_base_addr = 0x70000000;
					cv_ddr_phyd_apb = 0x70006000;

					ddrc_value = devmem_readl(addr_ddr_ctrl);
					rankid = (get_bits_from_value(ddrc_value, 25, 24) == 0x1 ? 1 : 2);

					tctdelay_pre_sys0 = rdglvl_retrain_osc_comp(i, rankid, tctdelay_pre_sys0,
								phyd_base_addr, addr_ddr_ctrl, cv_ddr_phyd_apb);

				} else if (i == 1) {
					//sys1 base address init
					addr_ddr_ctrl = 0x78004000;
					phyd_base_addr = 0x78000000;
					cv_ddr_phyd_apb = 0x78006000;

					ddrc_value = devmem_readl(addr_ddr_ctrl);
					rankid = (get_bits_from_value(ddrc_value, 25, 24) == 0x1 ? 1 : 2);

					tctdelay_pre_sys1 = rdglvl_retrain_osc_comp(i, rankid, tctdelay_pre_sys1,
								phyd_base_addr, addr_ddr_ctrl, cv_ddr_phyd_apb);
				}
			}
		}

		usleep(second*1000000);
	}
	return 0;
}