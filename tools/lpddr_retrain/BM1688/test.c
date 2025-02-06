#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include "test.h"
#include <sys/stat.h>
#include <errno.h>

// extern uint8_t uVO_en;
// extern uint8_t retrain_everytime;
uint8_t count;
uint32_t rddata;
uint32_t ddrc_0x10;

uint32_t tctdelay_init_sys0;
uint32_t tctdelay_init_sys1;
int delay_code_init_dq_sys0[2][4][9];
int delay_code_init_dq_sys1[2][4][9];

// DEBUG_SET_LEVEL(DEBUG_LEVEL_ERR);
#define ERR printf
#define DEBUG //printf

static int devmem_fd;

void *devm_map(unsigned long addr, int len)
{
	off_t offset;
	void *map_base;

	devmem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (devmem_fd == -1) {
		ERR("cannot open '/dev/mem'\n");
		goto open_err;
	}
	DEBUG("/dev/mem opened.\n");

	offset = addr & ~(sysconf(_SC_PAGE_SIZE) - 1);

	map_base = mmap(NULL, len + addr - offset, PROT_READ | PROT_WRITE,
					MAP_SHARED, devmem_fd, offset);
	if (map_base == MAP_FAILED) {
		ERR("mmap failed\n");
		goto mmap_err;
	}

	DEBUG("Memory mapped at address %p.\n", map_base + addr - offset);

	return map_base + addr - offset;

mmap_err:
	close(devmem_fd);

open_err:
	return NULL;
}

void devm_unmap(void *virt_addr, int len)
{
	unsigned long addr;

	if (devmem_fd == -1) {
		ERR("'/dev/mem' is closed\n");
		return;
	}

	/* page align */
	addr = (((unsigned long) virt_addr) & ~(sysconf(_SC_PAGE_SIZE) - 1));
	munmap((void *) addr, len + (unsigned long) virt_addr - addr);
	close(devmem_fd);
}

/* read/write 32bit data*/
uint32_t devmem_readl(unsigned long addr)
{
	uint32_t val;
	void *virt_addr;

	virt_addr = devm_map(addr, 4);
	if (virt_addr == NULL) {

		ERR("readl addr map failed\n");
		return 0;
	}

	val = *(uint32_t *) virt_addr;

	devm_unmap(virt_addr, 4);

	return val;
}

void devmem_writel(unsigned long addr, uint32_t val)
{
	void *virt_addr;

	virt_addr = devm_map(addr, 4);
	if (virt_addr == NULL) {
		ERR("writel addr map failed\n");
		return;
	}

	*(uint32_t *) virt_addr = val;

	devm_unmap(virt_addr, 4);
}

void delay(void)
{
	// delay 20ms
	for (int i = 0; i < 55408; i++) {
	}
}

// void mmio_wr32(uintptr_t addr, uint32_t value)
// {
//		*(volatile uint32_t*)addr = value;
// }

// uint32_t mmio_rd32(uintptr_t addr)
// {
//		return *(volatile uint32_t*)addr;
// }

uint32_t modified_bits_by_value(uint32_t orig, uint32_t value, uint32_t msb, uint32_t lsb)
{
	uint32_t bitmask = GENMASK(msb, lsb);

	orig &= ~bitmask;
	return (orig | ((value << lsb) & bitmask));
}

uint32_t get_bits_from_value(uint32_t value, uint32_t msb, uint32_t lsb)
{
	return ((value & GENMASK(msb, lsb)) >> lsb);
}

void cvx32_dfi_phymstr_req(unsigned long pyhd_base_addr)
{
	rddata = 0x00000001;
	devmem_writel(0x0178 + pyhd_base_addr, rddata);

	while (1) {
		rddata = devmem_readl(0x3030 + pyhd_base_addr);
		if (get_bits_from_value(rddata, 1, 0) == 0x3) {
			break;
		}
	}
}

void cvx32_dfi_phymstr_req_clr(unsigned long pyhd_base_addr)
{
	rddata = 0x00000010;
	devmem_writel(0x0178 + pyhd_base_addr, rddata);

	while (1) {
		rddata = devmem_readl(0x3030 + pyhd_base_addr);
		if (get_bits_from_value(rddata, 1, 0) == 0x0) {
			break;
		}
	}
}

void cvx32_dll_sw_clr(unsigned long ddr_ctrl, unsigned long pyhd_base_addr)
{
	uint32_t phyd_stop_clk;
	uint32_t phyd_rd_wr_clk_stop;

	phyd_stop_clk = devmem_readl(ddr_ctrl + 0x30); //phyd_stop_clk
	rddata = modified_bits_by_value(phyd_stop_clk, 0, 9, 9);

	devmem_writel(ddr_ctrl + 0x30, rddata);

	phyd_rd_wr_clk_stop = devmem_readl(ddr_ctrl + 0x148);
	rddata = phyd_rd_wr_clk_stop & 0x7F7FFFFF;// PATCH4.phyd_wr_clk_stop:31:1=0x0, PATCH4.phyd_rd_clk_stop:23:1=0x0
	devmem_writel(ddr_ctrl + 0x148, rddata);

	//param_phyd_sw_dfi_phyupd_req
	rddata = devmem_readl(0x0174 + pyhd_base_addr);
	rddata = modified_bits_by_value(rddata, 1, 0, 0);
	rddata = modified_bits_by_value(rddata, 1, 8, 8);
	devmem_writel(0x0174 + pyhd_base_addr, rddata);
	while (1) {
		// param_phyd_to_reg_sw_phyupd_dline_done
		rddata = devmem_readl(0x3030 + pyhd_base_addr);
		if (get_bits_from_value(rddata, 24, 24) == 0x1) {
			break;
		}
	}
	devmem_writel(ddr_ctrl + 0x30, phyd_stop_clk);

	rddata = phyd_rd_wr_clk_stop | 0x80800000;// PATCH4.phyd_wr_clk_stop:31:1=0x0, PATCH4.phyd_rd_clk_stop:23:1=0x0
	devmem_writel(ddr_ctrl + 0x148, rddata);
}

void cvx32_synp_mrw_lp4(uint32_t ddr_ctrl, uint32_t addr, uint32_t data, uint32_t rank)
{
    //`test_stream = "-1f- cvx32_synp_mrw_lp4";
		devmem_writel(ddr_ctrl + 0x18, 0x80000000);
		//`test_stream = "assert gmr_req";
		// poll MRSTAT.gmr_ack until it is 1
		// Poll MRSTAT.mr_wr_busy until it is 0
		//`test_stream = "Poll MRSTAT.mr_wr_busy until it is 0";
		rddata = 0;
		while (1) {
			rddata = devmem_readl(ddr_ctrl + 0x18);
			if ((get_bits_from_value(rddata, 0, 0) == 0) && (get_bits_from_value(rddata, 16, 16) == 1)) {
				break;
			}
		}
		//`test_stream = "lp4 Poll MRSTAT.mr_wr_busy finish";
		// Write the MRCTRL0.mr_type, MRCTRL0.mr_addr, MRCTRL0.mr_rank and (for MRWs) MRCTRL1.mr_data
		//rddata = 0;
		//rddata[0] = 0;       // mr_type  0:write   1:read
		//rddata[5:4] = rank;  // mr_rank
		rddata = devmem_readl(ddr_ctrl + 0x10);
		rddata = modified_bits_by_value(rddata, 0, 0, 0);
		rddata = modified_bits_by_value(rddata, rank, 5, 4);
		devmem_writel(ddr_ctrl + 0x10, rddata);
		//`test_stream = "lp4 Write the MRCTRL0";
		//    rddata[31:0] = 0;
		//    rddata[15:8] = addr;     // mr_addr
		//    rddata[ 7:0] = data;     // mr_data
		rddata = 0x00000000;
		rddata = modified_bits_by_value(rddata, addr, 15, 8);
		rddata = modified_bits_by_value(rddata, data, 7, 0);
		devmem_writel(ddr_ctrl + 0x14, rddata);
		//`test_stream = "lp4 Write the MRCTRL1";
		// Write MRCTRL0.mr_wr to 1
		rddata = devmem_readl(ddr_ctrl + 0x10);
		//rddata[31] = 1;
		rddata = modified_bits_by_value(rddata, 1, 31, 31);
		devmem_writel(ddr_ctrl + 0x10, rddata);
		//`test_stream = "lp4 Write MRCTRL0.mr_wr to 1";
		while (1) {
			rddata = devmem_readl(ddr_ctrl + 0x18);
			//end while (rddata[0] != 0);
			if (get_bits_from_value(rddata, 0, 0) == 0) {
				break;
			}
		}
		// deassert gmr_req
		devmem_writel(ddr_ctrl + 0x18, 0x00000000);
		//`test_stream = "de-assert gmr_req";
		//`test_stream = "lp4 Poll MRSTAT.mr_wr_busy finish";
	//end
//}}}
}

uint32_t get_adc3_vol(void)
{
	int fd, len;
	char buffer[256];
	uint32_t adc_val_ddr, vol_val_ddr;

	// system("insmod /mnt/system/ko/soph_saradc.ko");
    /* Open the command for reading. */
	fd = open("/sys/bus/iio/devices/iio:device0/in_voltage3_raw", O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd < 0) {
		printf("open adc err!\n");
	}

	write(fd, "1", 1);
	lseek(fd, -1, SEEK_CUR);

	len = read(fd, buffer, 5);
	if (len != 0) {
		// printf("read buf: %s\n", buffer);
		adc_val_ddr = atoi(buffer);
		// printf("adc value is %d\n", adc_val_ddr);
	}
	write(fd, "0", 1);
	close(fd);

	vol_val_ddr = (adc_val_ddr*1500)/4096;
	return vol_val_ddr;
}

uint32_t get_sys_num(void)
{
	uint32_t opt_reg, gpio117_reg;
	uint32_t vol_val_ddr;

	opt_reg = devmem_readl(0x27102014);
	gpio117_reg = devmem_readl(0x27013050);

	vol_val_ddr = get_adc3_vol();

	if ((get_bits_from_value(opt_reg, 8, 0) == 0x11)
	|| (get_bits_from_value(opt_reg, 8, 0) == 0xF9)
	|| (get_bits_from_value(opt_reg, 8, 0) == 0xC1)) {
		// cv186ah
		if (get_bits_from_value(gpio117_reg, 21, 21) == 0b0) {
			// gpio 117 low
			if (vol_val_ddr >= 0 && vol_val_ddr < 105) {
				return 1;
				// ddr_rank_num = DUAL_RANK;
				// ddr_sys_num  = SINGLE_SYS;
				// board_ddr_type = BOARD_LPDDR4;
				// printf("Auto: board info: dual rank, single sys, lpddr4, 4266.\n");
			} else if (vol_val_ddr >= 195 && vol_val_ddr < 405) {
				return 2;
				// ddr_rank_num = SINGLE_RANK;
				// ddr_sys_num  = DUAL_SYS;
				// board_ddr_type = BOARD_LPDDR4;
				// printf("Auto: board info: single rank, dual sys, lpddr4, 4266.\n");
			} else if (vol_val_ddr >= 495 && vol_val_ddr < 705) {
				return 2;
				// ddr_rank_num = DUAL_RANK;
				// ddr_sys_num  = DUAL_SYS;
				// board_ddr_type = BOARD_LPDDR4;
				// printf("Auto: board info: dual rank, dual sys, lpddr4, 4266.\n");
			} else if (vol_val_ddr >= 795 && vol_val_ddr < 1005) {
				return 1;
				// ddr_rank_num = SINGLE_RANK;
				// ddr_sys_num  = SINGLE_SYS;
				// board_ddr_type = BOARD_LPDDR4;
				// printf("Auto: board info: single rank, single sys, lpddr4, 4266.\n");
			} else if (vol_val_ddr >= 1095 && vol_val_ddr < 1305) {
				return 2;
				// ddr_rank_num = DUAL_RANK;
				// ddr_sys_num  = DUAL_SYS;
				// board_ddr_type = BOARD_LPDDR4X;
				// printf("Auto: board info: dual rank, dual sys, lpddr4x, 4266.\n");
			} else if (vol_val_ddr >= 1395 && vol_val_ddr <= 1500) {
				return 2;
				// ddr_rank_num = SINGLE_RANK;
				// ddr_sys_num  = DUAL_SYS;
				// board_ddr_type = BOARD_LPDDR4X;
				// printf("Auto: board info: single rank, dual sys, lpddr4x, 4266.\n");
			}
		} else {
			// gpio 117 high
			if (vol_val_ddr >= 0 && vol_val_ddr < 105) {
				return 2;
				// ddr_rank_num = SINGLE_RANK;
				// ddr_sys_num  = DUAL_SYS;
				// board_ddr_type = BOARD_DDR4_X16;
				// printf("Auto: board info: single rank, dual sys, ddr4, 3200.\n");
			} else if (vol_val_ddr >= 195 && vol_val_ddr < 405) {
				return 1;
				// ddr_rank_num = SINGLE_RANK;
				// ddr_sys_num  = SINGLE_SYS;
				// board_ddr_type = BOARD_LPDDR4X;
				// printf("Auto: board info: single rank, single sys, lpddr4x, 4266.\n");
			} else if (vol_val_ddr >= 495 && vol_val_ddr < 705) {
				return 1;
				// ddr_rank_num = SINGLE_RANK;
				// ddr_sys_num  = SINGLE_SYS;
				// board_ddr_type = BOARD_DDR4_X16;
				// printf("Auto: board info: single rank, single sys, ddr4, 3200.\n");
			} else if (vol_val_ddr >= 795 && vol_val_ddr < 1005) {
				return 1;
				// ddr_rank_num = DUAL_RANK;
				// ddr_sys_num  = SINGLE_SYS;
				// board_ddr_type = BOARD_LPDDR4X;
				// printf("Auto: board info: dual rank, single sys, lpddr4x, 4266.\n");
			}
		}
	} else if (get_bits_from_value(opt_reg, 5, 0) == 0x00 || get_bits_from_value(opt_reg, 5, 0) == 0x3f) {
		// bm1688
		if (get_bits_from_value(gpio117_reg, 21, 21) == 0b0) {
			// gpio 117 low
			if (vol_val_ddr >= 0 && vol_val_ddr < 105) {
				return 2;
				// ddr_rank_num = DUAL_RANK;
				// ddr_sys_num  = DUAL_SYS;
				// board_ddr_type = BOARD_LPDDR4;
				// printf("Auto: board info: dual rank, dual sys, lpddr4, 4266.\n");
			} else if (vol_val_ddr >= 195 && vol_val_ddr < 405) {
				return 2;
				// ddr_rank_num = SINGLE_RANK;
				// ddr_sys_num  = DUAL_SYS;
				// board_ddr_type = BOARD_LPDDR4;
				// printf("Auto: board info: single rank, dual sys, lpddr4, 4266.\n");
			} else if (vol_val_ddr >= 495 && vol_val_ddr < 705) {
				return 1;
				// ddr_rank_num = DUAL_RANK;
				// ddr_sys_num  = SINGLE_SYS;
				// board_ddr_type = BOARD_LPDDR4;
				// printf("Auto: board info: dual rank, single sys, lpddr4, 4266.\n");
			} else if (vol_val_ddr >= 795 && vol_val_ddr < 1005) {
				return 1;
				// ddr_rank_num = SINGLE_RANK;
				// ddr_sys_num  = SINGLE_SYS;
				// board_ddr_type = BOARD_LPDDR4;
				// printf("Auto: board info: single rank, single sys, lpddr4, 4266.\n");
			} else if (vol_val_ddr >= 1095 && vol_val_ddr < 1305) {
				return 2;
				// ddr_rank_num = DUAL_RANK;
				// ddr_sys_num  = DUAL_SYS;
				// board_ddr_type = BOARD_LPDDR4X;
				// printf("Auto: board info: dual rank, dual sys, lpddr4x, 4266.\n");
			} else if (vol_val_ddr >= 1395 && vol_val_ddr <= 1500) {
				return 2;
				// ddr_rank_num = SINGLE_RANK;
				// ddr_sys_num  = DUAL_SYS;
				// board_ddr_type = BOARD_LPDDR4X;
				// printf("Auto: board info: single rank, dual sys, lpddr4x, 4266.\n");
			}
		} else {
			// gpio 117 high
			if (vol_val_ddr >= 0 && vol_val_ddr < 105) {
				return 2;
				// ddr_rank_num = SINGLE_RANK;
				// ddr_sys_num  = DUAL_SYS;
				// board_ddr_type = BOARD_DDR4_X16;
				// printf("Auto: board info: single rank, dual sys, ddr4, 3200.\n");
			} else if (vol_val_ddr >= 195 && vol_val_ddr < 405) {
				return 1;
				// ddr_rank_num = SINGLE_RANK;
				// ddr_sys_num  = SINGLE_SYS;
				// board_ddr_type = BOARD_LPDDR4X;
				// printf("Auto: board info: single rank, single sys, lpddr4x, 4266.\n");
			} else if (vol_val_ddr >= 495 && vol_val_ddr < 705) {
				return 1;
				// ddr_rank_num = SINGLE_RANK;
				// ddr_sys_num  = SINGLE_SYS;
				// board_ddr_type = BOARD_DDR4_X16;
				// printf("Auto: board info: single rank, single sys, ddr4, 3200.\n");
			} else if (vol_val_ddr >= 795 && vol_val_ddr < 1005) {
				return 1;
				// ddr_rank_num = DUAL_RANK;
				// ddr_sys_num  = SINGLE_SYS;
				// board_ddr_type = BOARD_LPDDR4X;
				// printf("Auto: board info: dual rank, single sys, lpddr4x, 4266.\n");
			}
		}
	}
}

uint32_t dline_code_convertion_patch(uint32_t dline_code_in, uint32_t dll_master_latch)
{
	uint32_t dll_mult;
	uint32_t logical_dline_code;
	uint32_t dline_code_out_to_aphy;

	if (get_bits_from_value(dline_code_in, 9, 7) > 3) {
		dline_code_out_to_aphy = modified_bits_by_value(dline_code_out_to_aphy, 3, 7, 6);
		logical_dline_code = ((get_bits_from_value(dline_code_in, 9, 7) - 3) << 7) + dline_code_in;
	} else {
		dline_code_out_to_aphy = modified_bits_by_value(dline_code_out_to_aphy,
								get_bits_from_value(dline_code_in, 8, 7), 7, 6);
	}

	dll_mult = get_bits_from_value(logical_dline_code, 6, 0) * dll_master_latch; //dll_master_latch is 4*dll_code
	dll_mult = dll_mult >> 10; //devide by (256*4)

	if (get_bits_from_value(dll_mult, 6, 6) == 1) {
		dll_mult = 0x3f;
	}

	dline_code_out_to_aphy = modified_bits_by_value(dline_code_out_to_aphy,
							get_bits_from_value(dll_mult, 5, 0), 5, 0);
	return dline_code_out_to_aphy;
}

uint32_t dline_code_convertion_patch_v3(uint32_t dline_code_in, uint32_t dll_code_sum)
{
	uint32_t UI;
	// uint32_t real_dll;
	uint32_t ideal_dll;
	uint32_t real_160;
	uint32_t fraction_logic;
	// uint32_t mid_logic;
	uint32_t logical_dline_code;
	uint32_t ph;
	uint32_t dll_mult;
	uint32_t dline_code_out_to_aphy;

	// real_dll = dll_code_sum >> 2;
	UI = 234;
	ideal_dll = 0x2f;
	real_160 = (160 * dll_code_sum / ideal_dll) >> 2;
	fraction_logic = (real_160 - UI/4) * 256 / UI; // 160 - 60 = 100ps => logical code
	// mid_logic = 0x8c0 - fraction_logic;

	logical_dline_code = dline_code_in;
	//overflow detect
	if (logical_dline_code > (0x980 + fraction_logic)) {
		logical_dline_code = 0x980 + fraction_logic;
	} else if (logical_dline_code < (0x800 - fraction_logic)) {
		logical_dline_code = 0x800 - fraction_logic;
	} else {
		//logical_dline_code = logical_dline_code;
	}

	dll_mult = get_bits_from_value(logical_dline_code, 6, 0) * dll_code_sum;
	dll_mult = dll_mult >> 10;

	if (get_bits_from_value(logical_dline_code, 16, 7) > 0x10) {
		ph = get_bits_from_value(logical_dline_code, 16, 7) - 0x10;
	} else {
		ph = 3;
		// dll_mult = dll_mult - (real_dll >> 1);
	}

	if (get_bits_from_value(dll_mult, 31, 6) != 0) {
		dll_mult = 0x3f;
	}

	dline_code_out_to_aphy = (ph << 6) | dll_mult;

	return dline_code_out_to_aphy;
}

void cvx32_synp_mrr_lp4_out(unsigned long addr, uint32_t rank, unsigned long ddr_ctrl, uint32_t *mr_out)
{
	devmem_writel(ddr_ctrl+0x18, 0x80000000);

	rddata = 0;
	while (1) {
		rddata = devmem_readl(ddr_ctrl + 0x18);
		if (get_bits_from_value(rddata, 0, 0) == 0 && get_bits_from_value(rddata, 16, 16) == 1) {
			break;
		}
	}

	rddata = devmem_readl(ddr_ctrl + 0x10);
	rddata = modified_bits_by_value(rddata, 1, 0, 0);
	rddata = modified_bits_by_value(rddata, rank, 5, 4);
	devmem_writel(ddr_ctrl+0x10, rddata);

	rddata = 0x0;
	rddata = modified_bits_by_value(rddata, addr, 15, 8);
	rddata = modified_bits_by_value(rddata, 0, 7, 0);
	devmem_writel(ddr_ctrl + 0x14, rddata);

	rddata = devmem_readl(ddr_ctrl + 0x10);
	rddata = modified_bits_by_value(rddata, 1, 31, 31);
	devmem_writel(ddr_ctrl + 0x10, rddata);

	while (1) {
		rddata = devmem_readl(ddr_ctrl + 0x18);
		if (get_bits_from_value(rddata, 0, 0) == 0) {
			break;
		}
	}

	devmem_writel(ddr_ctrl + 0x18, 0x0);
	rddata = devmem_readl(ddr_ctrl + 0xC10);
	mr_out[0] = (get_bits_from_value(rddata, 23, 16) << 8) | get_bits_from_value(rddata, 7, 0);
	rddata = devmem_readl(ddr_ctrl + 0xC14);
	mr_out[1] = (get_bits_from_value(rddata, 23, 16) << 8) | get_bits_from_value(rddata, 7, 0);
	rddata = devmem_readl(ddr_ctrl + 0xC18);
	mr_out[2] = (get_bits_from_value(rddata, 23, 16) << 8) | get_bits_from_value(rddata, 7, 0);
	rddata = devmem_readl(ddr_ctrl + 0xC1C);
	mr_out[3] = (get_bits_from_value(rddata, 23, 16) << 8) | get_bits_from_value(rddata, 7, 0);
}

void cvx32_synp_mpcosc_lp4(uint32_t rank, unsigned long ddr_ctrl)
{
	ddrc_0x10 = 0;

	devmem_writel(ddr_ctrl + 0x18, 0x80000000);
	rddata = 0;

	while (1) {
		rddata = devmem_readl(ddr_ctrl + 0x18);
		if (get_bits_from_value(rddata, 0, 0) == 0 && get_bits_from_value(rddata, 16, 16) == 1) {
			break;
		}
	}

	rddata = devmem_readl(ddr_ctrl + 0x10);
	rddata = modified_bits_by_value(rddata, 0, 0, 0);
	rddata = modified_bits_by_value(rddata, rank, 5, 4);
	devmem_writel(ddr_ctrl + 0x10, rddata);

	rddata = 0x0;
	rddata = modified_bits_by_value(rddata, 0xff, 15, 8);
	rddata = modified_bits_by_value(rddata, 0x4b, 7, 0);
	devmem_writel(ddr_ctrl + 0x14, rddata);

	rddata = devmem_readl(ddr_ctrl + 0x10);
	rddata = modified_bits_by_value(rddata, 1, 31, 31);
	ddrc_0x10 = rddata;
	devmem_writel(ddr_ctrl + 0x10, rddata);

	while (1) {
		rddata = devmem_readl(ddr_ctrl + 0x18);
		if (get_bits_from_value(rddata, 0, 0) == 0) {
			break;
		}
	}

	devmem_writel(ddr_ctrl + 0x18, 0x0);
}

uint32_t cvx32_synp_lp4_tracking(uint32_t rank, unsigned long pyhd_base_addr, unsigned long ddr_ctrl, uint32_t subsys,
						uint32_t mask_code_init_sys[2][4], uint32_t tctdelay_pre)
{
	uint32_t mr18_out[2][4];
	uint32_t mr19_out[2][4];
	uint64_t ddrc_mpc_osc_cnt;
	uint32_t tctdelay_new;
	uint32_t temp_inc;
	uint32_t temp_dec;
	uint32_t diff_code_rdg;
	uint32_t diff_code_wdq;
	uint32_t tctdelay_cur;
	uint32_t tctdelay_init;
	uint32_t dll_code_sum;
	uint32_t dline_code_out_to_aphy;
	char str[80] = "";

	devmem_writel(ddr_ctrl + 0x30, 0x00000100);
	// mmio_wr32(ddr_ctrl + 0x30, 0x00000100);

	for (int i = 0; i < rank; i = i + 1) {
		cvx32_synp_mpcosc_lp4(rank, ddr_ctrl);
		cvx32_synp_mrr_lp4_out(18, 1 << i, ddr_ctrl, mr18_out[i]);
		cvx32_synp_mrr_lp4_out(19, 1 << i, ddr_ctrl, mr19_out[i]);
	}

	// mmio_wr32(ddr_ctrl + 0x30, 0x0000010b);
	devmem_writel(ddr_ctrl + 0x30, 0x0000010b);

	//param_phyd_to_reg_rx_dll_code3-0
	rddata = devmem_readl(0x3018 + pyhd_base_addr);
	//average dll code
	dll_code_sum = get_bits_from_value(rddata, 31, 24) + get_bits_from_value(rddata, 23, 16) +
					get_bits_from_value(rddata, 15, 8) + get_bits_from_value(rddata, 7, 0);
	//$display("ave_dll_code = %h", ave_dll_code);

	// ddrc_mpc_osc_cnt = devmem_readl(ddr_ctrl + 0xC00);

	if (temp_cnt == 0 && subsys == 0) {
		//tctdelay_new = (ddrc_mpc_osc_cnt * 938)/(get_bits_from_value(mr19_out[1][0], 7, 0)*256 +
				//get_bits_from_value(mr18_out[1][0], 7, 0));
		tctdelay_new = (2048 * 470)/(get_bits_from_value(mr19_out[1][0], 7, 0)*256 +
									get_bits_from_value(mr18_out[1][0], 7, 0));
		tctdelay_init_sys0 = tctdelay_new;
		tctdelay_cur = tctdelay_new;
		temp_cnt += 1;
	} else if (temp_cnt == 1 && subsys == 1) {
		tctdelay_new = (2048 * 470)/(get_bits_from_value(mr19_out[1][0], 7, 0)*256 +
									get_bits_from_value(mr18_out[1][0], 7, 0));
		tctdelay_init_sys1 = tctdelay_new;
		tctdelay_cur = tctdelay_new;
		temp_cnt += 1;
	} else {
		// tctdelay_cur = tctdelay_pre;
		// tctdelay_new = (ddrc_mpc_osc_cnt * 938)/(get_bits_from_value(mr19_out[1][0], 7, 0)*256 +
		//		 get_bits_from_value(mr18_out[1][0], 7, 0));
		// tctdelay_pre = (subsys == 0 ? tctdelay_init_sys0 : tctdelay_init_sys1);
		tctdelay_new = (2048 * 470)/(get_bits_from_value(mr19_out[1][0], 7, 0)*256 +
									get_bits_from_value(mr18_out[1][0], 7, 0));
		printf("mr19_out[1][0] = 0x%x, mr18_out[1][0] = 0x%x\n", mr19_out[1][0], mr18_out[1][0]);
		printf("tctdelay_pre = 0x%x, tctdelay_new = 0x%x\n", tctdelay_pre, tctdelay_new);
		if ((tctdelay_new > tctdelay_pre) && ((tctdelay_new - tctdelay_pre) > 1)) {
			//tctdelay_old - tctdelay_new) > margin  TBD
			tctdelay_cur = tctdelay_new;
			temp_inc = 1;
			temp_dec = 0;
		} else if ((tctdelay_new < tctdelay_pre) && ((tctdelay_pre - tctdelay_new) > 1)) {
			//tctdelay_new - tctdelay_old) > margin  TBD
			tctdelay_cur = tctdelay_new;
			temp_inc = 0;
			temp_dec = 1;
		} else {
			temp_inc = 0;
			temp_dec = 0;
			tctdelay_cur = tctdelay_pre;
		}
		printf("66666\n");

		if (temp_inc == 1) {
			tctdelay_init = (subsys == 0 ? tctdelay_init_sys0 : tctdelay_init_sys1);
			diff_code_rdg = (tctdelay_new - tctdelay_init) * 1; //(tctdelay_new - tctdelay_old) * coef  TBD
			printf("diff_code_rdg = 0x%x\n", diff_code_rdg);
			printf("tctdelay_new = 0x%x, tctdelay_init = 0x%x", tctdelay_new, tctdelay_init);
			usleep(1000);
			diff_code_wdq = (tctdelay_new - tctdelay_init) * 0.1;
			// printf("tctdelay_pre = 0x%x, tctdelay_new = 0x%x\n", tctdelay_pre, tctdelay_new);
			// printf("temp_inc = 0x%x, diff_code_rdg = 0x%x, diff_code_wdq = 0x%x\n",
			//			temp_inc, diff_code_rdg, diff_code_wdq);
			// usleep(1000);

			for (int i = 0; i < 2; i++) {
				for (int j = 0; j < 4; j++) {
					// rddata = devmem_readl(0x818 + i*0x600 + j*0x60 + pyhd_base_addr);
					// rddata = mask_code_init_sys[i][j] + diff_code_rdg;
					rddata = get_bits_from_value(mask_code_init_sys[i][j], 27, 16) + diff_code_rdg;
					printf("rddata = 0x%x\n", rddata);
					// rddata = modified_bits_by_value(rddata,
					//	get_bits_from_value(rddata, 13, 0) + diff_code_rdg, 13, 0);
					// printf("rddata = 0x%x\n", rddata);
					// usleep(1000);
					devmem_writel(0x0818 + i*0x600 + j*0x60 + pyhd_base_addr, rddata);

					dline_code_out_to_aphy = dline_code_convertion_patch_v3(rddata, dll_code_sum);
					rddata = devmem_readl(0x840 + 0x600*i + 0x60*j + pyhd_base_addr);
					rddata = modified_bits_by_value(rddata,
						get_bits_from_value(dline_code_out_to_aphy, 7, 0), 7, 0);
					devmem_writel(0x840 + 0x600*i + 0x60*j + pyhd_base_addr,  rddata);

					//write eye
					// rddata = devmem_readl(0x600 + 0x600 * i + 0x60*j + pyhd_base_addr);
					// rddata = modified_bits_by_value(rddata,
					//		get_bits_from_value(rddata, 13, 0) + diff_code_wdq, 13, 0);
					// rddata = modified_bits_by_value(rddata,
					//		get_bits_from_value(rddata, 29, 16) + diff_code_wdq, 29, 16);
					// pyhd_base_addr += (0x600 + i*0x600 + j*0x60);
					// // str[80] = "";
					// sprintf(str, "devmem 0x%x 32 0x%x", pyhd_base_addr, rddata);
					// system(str);
					// pyhd_base_addr -= (0x600 + i*0x600 + j*0x60);
					// // devmem_writel(0x600 + 0x600 * i + 0x60*j + pyhd_base_addr, rddata);

					// rddata = devmem_readl(0x604 + 0x600 * i + 0x60*j + pyhd_base_addr);
					// rddata = modified_bits_by_value(rddata,
					//		get_bits_from_value(rddata, 13, 0) + diff_code_wdq, 13, 0);
					// rddata = modified_bits_by_value(rddata,
					//		get_bits_from_value(rddata, 29, 16) + diff_code_wdq, 29, 16);
					// pyhd_base_addr += (0x604 + i*0x600 + j*0x60);
					// // str[80] = "";
					// sprintf(str, "devmem 0x%x 32 0x%x", pyhd_base_addr, rddata);
					// system(str);
					// pyhd_base_addr -= (0x604 + i*0x600 + j*0x60);
					// // devmem_writel(0x604 + 0x600 * i + 0x60*j + pyhd_base_addr, rddata);

					// rddata = devmem_readl(0x608 + 0x600 * i + 0x60*j + pyhd_base_addr);
					// rddata = modified_bits_by_value(rddata,
					//		get_bits_from_value(rddata, 13, 0) + diff_code_wdq, 13, 0);
					// rddata = modified_bits_by_value(rddata,
					//		get_bits_from_value(rddata, 29, 16) + diff_code_wdq, 29, 16);
					// pyhd_base_addr += (0x608 + i*0x600 + j*0x60);
					// // str[80] = "";
					// sprintf(str, "devmem 0x%x 32 0x%x", pyhd_base_addr, rddata);
					// system(str);
					// pyhd_base_addr -= (0x608 + i*0x600 + j*0x60);
					// // devmem_writel(0x608 + 0x600 * i + 0x60*j + pyhd_base_addr, rddata);

					// rddata = devmem_readl(0x60C + 0x600 * i + 0x60*j + pyhd_base_addr);
					// rddata = modified_bits_by_value(rddata,
					//		get_bits_from_value(rddata, 13, 0) + diff_code_wdq, 13, 0);
					// rddata = modified_bits_by_value(rddata,
					//		get_bits_from_value(rddata, 29, 16) + diff_code_wdq, 29, 16);
					// pyhd_base_addr += (0x60C + i*0x600 + j*0x60);
					// // str[80] = "";
					// sprintf(str, "devmem 0x%x 32 0x%x", pyhd_base_addr, rddata);
					// system(str);
					// pyhd_base_addr -= (0x60C + i*0x600 + j*0x60);
					// // devmem_writel(0X60C + 0X600 * i + 0X60*j + pyhd_base_addr, rddata);

					// rddata = devmem_readl(0X610 + 0X600 * i + 0X60*j + pyhd_base_addr);
					// rddata = modified_bits_by_value(rddata,
					//		get_bits_from_value(rddata, 13, 0) + diff_code_wdq, 13, 0);
					// pyhd_base_addr += (0x610 + i*0x600 + j*0x60);
					// // str[80] = "";
					// sprintf(str, "devmem 0x%x 32 0x%x", pyhd_base_addr, rddata);
					// system(str);
					// pyhd_base_addr -= (0x610 + i*0x600 + j*0x60);
					// // devmem_writel(0X610 + 0X600 * i + 0X60*j + pyhd_base_addr, rddata);
				}
			}
		} else if (temp_dec == 1) {
			tctdelay_init = (subsys == 0 ? tctdelay_init_sys0 : tctdelay_init_sys1);
			// diff_code_rdg = (tctdelay_pre - tctdelay_new) * 3.5;  //(tctdelay_old - tctdelay_new) * coef
			// diff_code_wdq = (tctdelay_pre - tctdelay_new) * 0.8;
			diff_code_rdg = (tctdelay_new - tctdelay_init) * 1;  //(tctdelay_old - tctdelay_new) * coef
			printf("tctdelay_new = 0x%x, tctdelay_init = 0x%x", tctdelay_new, tctdelay_init);
			printf("diff_code_rdg = 0x%x\n", diff_code_rdg);
			usleep(1000);
			diff_code_wdq = (tctdelay_new - tctdelay_init) * 0.1;
			// printf("tctdelay_pre = 0x%x, tctdelay_new = 0x%x\n", tctdelay_pre, tctdelay_new);
			// printf("temp_dec = 0x%x, diff_code_rdg = 0x%x, diff_code_wdq = 0x%x\n",
			//			temp_dec, diff_code_rdg, diff_code_wdq);
			// usleep(10000);

			for (int i = 0; i < 2; i++) {
				for (int j = 0; j < 4; j++) {
					// printf("entering for!!!!!!!!!!!!!!!!\n");
					// usleep(20000);
					// rddata = devmem_readl(0x818 + i*0x600 + j*0x60 + pyhd_base_addr);
					// rddata = mask_code_init_sys[i][j] - diff_code_rdg;
					rddata = get_bits_from_value(mask_code_init_sys[i][j], 27, 16) - diff_code_rdg;
					// printf("0x818 + i*0x600 + j*0x60 = 0x%x\n", 0x818 + i*0x600 + j*0x60);
					// printf("rddata = 0x%x\n", rddata);
					// usleep(20000);
					// rddata = modified_bits_by_value(rddata,
					//	get_bits_from_value(rddata, 13, 0) - diff_code_rdg, 13, 0);
					// printf("rddata = 0x%x\n", rddata);
					// usleep(20000);

					devmem_writel(0x818 + i*0x600 + j*0x60 + pyhd_base_addr, rddata);

					dline_code_out_to_aphy = dline_code_convertion_patch_v3(rddata, dll_code_sum);
					rddata = devmem_readl(0x840 + 0x600*i + 0x60*j + pyhd_base_addr);
					rddata = modified_bits_by_value(rddata,
							get_bits_from_value(dline_code_out_to_aphy, 7, 0), 7, 0);
					devmem_writel(0x840 + 0x600*i + 0x60*j + pyhd_base_addr,  rddata);

					//write eye
					// rddata = devmem_readl(0x600 + 0x600 * i + 0x60*j + pyhd_base_addr);
					// rddata = modified_bits_by_value(rddata,
					//		get_bits_from_value(rddata, 13, 0) - diff_code_wdq, 13, 0);
					// rddata = modified_bits_by_value(rddata,
					//		get_bits_from_value(rddata, 29, 16) - diff_code_wdq, 29, 16);
					// pyhd_base_addr += (0x600 + i*0x600 + j*0x60);
					// // str[80] = "";
					// sprintf(str, "devmem 0x%x 32 0x%x", pyhd_base_addr, rddata);
					// system(str);
					// pyhd_base_addr -= (0x600 + i*0x600 + j*0x60);
					// // devmem_writel(0x600 + 0x600 * i + 0x60*j + pyhd_base_addr, rddata);

					// rddata = devmem_readl(0x604 + 0x600 * i + 0x60*j + pyhd_base_addr);
					// rddata = modified_bits_by_value(rddata,
					//		get_bits_from_value(rddata, 13, 0) - diff_code_wdq, 13, 0);
					// rddata = modified_bits_by_value(rddata,
					//		get_bits_from_value(rddata, 29, 16) - diff_code_wdq, 29, 16);
					// pyhd_base_addr += (0x604 + i*0x600 + j*0x60);
					// // str[80] = "";
					// sprintf(str, "devmem 0x%x 32 0x%x", pyhd_base_addr, rddata);
					// system(str);
					// pyhd_base_addr -= (0x604 + i*0x600 + j*0x60);
					// // devmem_writel(0x604 + 0x600 * i + 0x60*j + pyhd_base_addr, rddata);

					// rddata = devmem_readl(0x608 + 0x600 * i + 0x60*j + pyhd_base_addr);
					// rddata = modified_bits_by_value(rddata,
					//		get_bits_from_value(rddata, 13, 0) - diff_code_wdq, 13, 0);
					// rddata = modified_bits_by_value(rddata,
					//		get_bits_from_value(rddata, 29, 16) - diff_code_wdq, 29, 16);
					// pyhd_base_addr += (0x608 + i*0x600 + j*0x60);
					// // str[80] = "";
					// sprintf(str, "devmem 0x%x 32 0x%x", pyhd_base_addr, rddata);
					// system(str);
					// pyhd_base_addr -= (0x608 + i*0x600 + j*0x60);
					// // devmem_writel(0x608 + 0x600 * i + 0x60*j + pyhd_base_addr, rddata);

					// rddata = devmem_readl(0x60C + 0x600 * i + 0x60*j + pyhd_base_addr);
					// rddata = modified_bits_by_value(rddata,
					//		get_bits_from_value(rddata, 13, 0) - diff_code_wdq, 13, 0);
					// rddata = modified_bits_by_value(rddata,
					//		get_bits_from_value(rddata, 29, 16) - diff_code_wdq, 29, 16);
					// pyhd_base_addr += (0x60C + i*0x600 + j*0x60);
					// // str[80] = "";
					// sprintf(str, "devmem 0x%x 32 0x%x", pyhd_base_addr, rddata);
					// system(str);
					// pyhd_base_addr -= (0x60C + i*0x600 + j*0x60);
					// // devmem_writel(0x60C + 0x600 * i + 0x60*j + pyhd_base_addr, rddata);

					// rddata = devmem_readl(0x610 + 0x600 * i + 0x60*j + pyhd_base_addr);
					// rddata = modified_bits_by_value(rddata,
					//		get_bits_from_value(rddata, 13, 0) - diff_code_wdq, 13, 0);
					// pyhd_base_addr += (0x610 + i*0x600 + j*0x60);
					// // str[80] = "";
					// sprintf(str, "devmem 0x%x 32 0x%x", pyhd_base_addr, rddata);
					// system(str);
					// pyhd_base_addr -= (0x610 + i*0x600 + j*0x60);
					// // devmem_writel(0x610 + 0x600 * i + 0x60*j + pyhd_base_addr, rddata);
				}
			}
		}
	}
	return tctdelay_cur;
}

uint32_t Data_A53_2_8051(uint32_t data_a53)
{
	uint32_t data_8051;

	data_8051 = (data_a53 & 0xff) << 24 | (data_a53 & 0xff00) << 8 |
				(data_a53 & 0xff0000) >> 8 | (data_a53&0xff000000) >> 24;
	printf("data_a53:0x%x data_8051:0x%x\n", data_a53, data_8051);
	return data_8051;
}

void rdglvl_retrain_lp4_mpc(uint16_t temp_inc, uint32_t phyd_base_addr, uint32_t DDR_CTRL, uint32_t CV_DDR_PHYD_APB)
{
	uint16_t  cur_byte0_mask_rank0;
	uint16_t  cur_byte1_mask_rank0;
	uint16_t  cur_byte2_mask_rank0;
	uint16_t  cur_byte3_mask_rank0;
	uint16_t  min_mask_rank0_01;
	uint16_t  min_mask_rank0_23;
	uint16_t  min_mask_rank0_0123;
	uint16_t  max_mask_rank0_01;
	uint16_t  max_mask_rank0_23;
	uint16_t  max_mask_rank0_0123;

	uint16_t  cur_byte0_mask_rank1;
	uint16_t  cur_byte1_mask_rank1;
	uint16_t  cur_byte2_mask_rank1;
	uint16_t  cur_byte3_mask_rank1;
	uint16_t  min_mask_rank1_01;
	uint16_t  min_mask_rank1_23;
	uint16_t  min_mask_rank1_0123;
	uint16_t  max_mask_rank1_01;
	uint16_t  max_mask_rank1_23;
	uint16_t  max_mask_rank1_0123;

	uint32_t  start_point;
	uint32_t  end_point;
	uint16_t  min_mask;
	uint16_t  max_mask;
	uint32_t  MR13_pre;
	uint32_t  MR13;
	uint32_t  rank2_en;
	uint32_t  rank0_rdglvl_req;
	uint32_t  rank1_rdglvl_req;
	uint32_t  Dataflow;
	//uint32_t  phy_184 = phyd_base_addr + 0x184;
	//uint32_t  phy_3444 = phyd_base_addr + 0x3444;
	uint32_t  Dataflow_mpc;
	//uint32_t xdata current_time;
	uint32_t dis_dq_t;

	uint32_t dis_dq_t_pre_ctrl_0304;
	uint32_t dqs_dq_t_ctrl_0304;
	uint32_t ctrl_0030_pre, ctrl_0030;
	uint32_t ctrl_0148_pre, ctrl_0148;
	uint32_t phyd_01a0_pre_r0, phyd_01a0_r0;//retrain
	uint32_t phyd_01a0_pre_r1, phyd_01a0_r1;//retrain

	//read current training rdglvl results
	// rank0 {{{
	rddata = devmem_readl(0x0818 + phyd_base_addr);//bit [13:0] = mask shift/dline
	cur_byte0_mask_rank0 = get_bits_from_value(rddata, 15, 0);
	//printf("1:r=%lx, %x\n", rddata, cur_byte0_mask_rank0);
	rddata = devmem_readl(0x0878 + phyd_base_addr);
	cur_byte1_mask_rank0 = get_bits_from_value(rddata, 15, 0);
	//printf("2:r=%lx, %x\n", rddata, cur_byte1_mask_rank0);
	rddata = devmem_readl(0x08D8 + phyd_base_addr);
	cur_byte2_mask_rank0 = get_bits_from_value(rddata, 15, 0);
	//printf("3:r=%lx, %x\n", rddata, cur_byte2_mask_rank0);
	rddata = devmem_readl(0x0938 + phyd_base_addr);
	cur_byte3_mask_rank0 = get_bits_from_value(rddata, 15, 0);
	//printf("4:r=%lx, %x\n", rddata, cur_byte3_mask_rank0);

	//find the min rdg value
	if (cur_byte0_mask_rank0 >= cur_byte1_mask_rank0) {
		min_mask_rank0_01 = cur_byte1_mask_rank0;
	} else {
		min_mask_rank0_01 = cur_byte0_mask_rank0;
	}
	if (cur_byte2_mask_rank0 >= cur_byte3_mask_rank0) {
		min_mask_rank0_23 = cur_byte3_mask_rank0;
	} else {
		min_mask_rank0_23 = cur_byte2_mask_rank0;
	}
	if (min_mask_rank0_01 >= min_mask_rank0_23) {
		min_mask_rank0_0123 = min_mask_rank0_23;
	} else {
		min_mask_rank0_0123 = min_mask_rank0_01;
	}

	//find the max rdglvl value
	if (cur_byte0_mask_rank0 >= cur_byte1_mask_rank0) {
		max_mask_rank0_01 = cur_byte0_mask_rank0;
	} else {
		max_mask_rank0_01 = cur_byte1_mask_rank0;
	}
	if (cur_byte2_mask_rank0 >= cur_byte3_mask_rank0) {
		max_mask_rank0_23 = cur_byte2_mask_rank0;
	} else {
		max_mask_rank0_23 = cur_byte3_mask_rank0;
	}
	if (max_mask_rank0_01 >= max_mask_rank0_23) {
		max_mask_rank0_0123 = max_mask_rank0_01;
	} else {
		max_mask_rank0_0123 = max_mask_rank0_23;
	}
	//}}}

	//rank1{{{
	rank2_en = devmem_readl(DDR_CTRL + 0x00);
	if (get_bits_from_value(rank2_en, 25, 24) == 3) { //two rank
		rddata = devmem_readl(0x0e18 + phyd_base_addr);//bit [13:0] = mask shift/dline
		cur_byte0_mask_rank1 = get_bits_from_value(rddata, 15, 0);
		//printf("2-1:r=%lx, %x\n", rddata, cur_byte0_mask_rank1);
		rddata = devmem_readl(0x0e78 + phyd_base_addr);
		cur_byte1_mask_rank1 = get_bits_from_value(rddata, 15, 0);
		//printf("2-2:r=%lx, %x\n", rddata, cur_byte1_mask_rank1);
		rddata = devmem_readl(0x0eD8 + phyd_base_addr);
		cur_byte2_mask_rank1 = get_bits_from_value(rddata, 15, 0);
		//printf("2-3:r=%lx, %x\n", rddata, cur_byte2_mask_rank1);
		rddata = devmem_readl(0x0f38 + phyd_base_addr);
		cur_byte3_mask_rank1 = get_bits_from_value(rddata, 15, 0);
		//printf("2-4:r=%lx, %x\n", rddata, cur_byte3_mask_rank1);
		//find the min rdg value
		if (cur_byte0_mask_rank1 >= cur_byte1_mask_rank1) {
			min_mask_rank1_01 = cur_byte1_mask_rank1;
		} else {
			min_mask_rank1_01 = cur_byte0_mask_rank1;
		}
		if (cur_byte2_mask_rank1 >= cur_byte3_mask_rank1) {
			min_mask_rank1_23 = cur_byte3_mask_rank1;
		} else {
			min_mask_rank1_23 = cur_byte2_mask_rank1;
		}
		if (min_mask_rank1_01 >= min_mask_rank1_23) {
			min_mask_rank1_0123 = min_mask_rank1_23;
		} else {
			min_mask_rank1_0123 = min_mask_rank1_01;
		}

		//find the max rdglvl value
		if (cur_byte0_mask_rank1 >= cur_byte1_mask_rank1) {
			max_mask_rank1_01 = cur_byte0_mask_rank1;
		} else {
			max_mask_rank1_01 = cur_byte1_mask_rank1;
		}
		if (cur_byte2_mask_rank1 >= cur_byte3_mask_rank1) {
			max_mask_rank1_23 = cur_byte2_mask_rank1;
		} else {
			max_mask_rank1_23 = cur_byte3_mask_rank1;
		}
		if (max_mask_rank1_01 >= max_mask_rank1_23) {
			max_mask_rank1_0123 = max_mask_rank1_01;
		} else {
			max_mask_rank1_0123 = max_mask_rank1_23;
		}

		if (min_mask_rank1_0123 >= min_mask_rank0_0123) {
			min_mask = min_mask_rank0_0123;
		} else {
			min_mask = min_mask_rank1_0123;
		}
		if (max_mask_rank1_0123 >= max_mask_rank0_0123) {
			max_mask = max_mask_rank1_0123;
		} else {
			max_mask = max_mask_rank0_0123;
		}
	} else {
		min_mask = min_mask_rank0_0123;
		max_mask = max_mask_rank0_0123;
	}
	//}}}

	if (temp_inc) {
		start_point = min_mask + 0x200 - 0x60;
		end_point   = max_mask + 0x200 + 0x160;
	} else {
		start_point = min_mask + 0x200 - 0x40;
		end_point   = max_mask + 0x200 + 0xd9;
	}

	start_point = min_mask - 0x60;
	end_point = max_mask + 0x400;

	rddata = devmem_readl(0x0060 + phyd_base_addr);
		//printf("rd=%lx\n", rddata);
	rddata = modified_bits_by_value(rddata, 8, 27, 24); //param_phyd_pigtlvl_dly_step[3:0] set to 8
	devmem_writel(0x0060 + phyd_base_addr, rddata);
		//printf("rd2=%lx\n", rddata);

	//"rdglvl_LP4_MPC"
	//$display("[KC_DBG] LPDDR4 MPR mode");
	rddata = devmem_readl(0x0060 + phyd_base_addr);
	rddata = modified_bits_by_value(rddata, 0, 20, 20); //param_phyd_pigtlvl_scan_mode
	devmem_writel(0x0060 + phyd_base_addr,  rddata);

	rddata = devmem_readl(0x0184 + phyd_base_addr);
	Dataflow = rddata;
	Dataflow_mpc = modified_bits_by_value(rddata, 4, 6, 4); //Dataflow from MPR,{lp4,ddr4,ddr3}
	//write_robot(0x0184 + phyd_base_addr, rddata);

	//param_phyd_pigtlvl_start_delay_code    7     0
	//param_phyd_pigtlvl_start_shift_code    13    8
	//param_phyd_pigtlvl_end_delay_code    23     16
	//param_phyd_pigtlvl_end_shift_code    29     24
	devmem_writel(0x0064 + phyd_base_addr, ((end_point << 16) | start_point)); //set start/end point

	rddata = devmem_readl(0x0184 + phyd_base_addr);
	rddata = modified_bits_by_value(rddata, 4, 6, 4); //Dataflow from MPR,{lp4,ddr4,ddr3}
	rddata = modified_bits_by_value(rddata, 1, 0, 0); //param_phyd_dfi_rdglvl_req
	rank0_rdglvl_req = modified_bits_by_value(rddata, 1, 25, 24); //sel rank0 param_phyd_dfi_rdglvl_rank_sel
	rank1_rdglvl_req = modified_bits_by_value(rddata, 2, 25, 24); //sel rank1 param_phyd_dfi_rdglvl_rank_sel

	//MR13
	MR13_pre = devmem_readl(DDR_CTRL + 0xe0);
	MR13_pre = modified_bits_by_value(MR13_pre, 0b11, 7, 6);
	MR13 = modified_bits_by_value(MR13_pre, 1, 1, 1); //Read Preamble Training Mode
	//printf("clk1\n");

	//save MR13, MR13_pre, Dataflow, Dataflow_mpc, rank0_rdglvl_req, rank1_rdglvl_req, rank2_en    to rtc sram

	dis_dq_t_pre_ctrl_0304 = devmem_readl(DDR_CTRL + 0x304);
	dqs_dq_t_ctrl_0304 = modified_bits_by_value(dis_dq_t_pre_ctrl_0304, 1, 0, 0);
	ctrl_0030_pre = devmem_readl(DDR_CTRL + 0x30) | 0x200;
	ctrl_0030 = ctrl_0030_pre & ~0x200;

	ctrl_0148_pre = devmem_readl(DDR_CTRL + 0x148) | 0x800000 | 0x80000000;
	ctrl_0148 = (ctrl_0148_pre & ~0x800000) | 0x80000000;

	phyd_01a0_pre_r0 = (1 * 16) + (3 * 64) + (13 * 256) + (MR13_pre * 65536) + 1;
	phyd_01a0_r0 = (1 * 16) + (3 * 64) + (13 * 256) + (MR13 * 65536) + 1;

	phyd_01a0_pre_r1 = (2 * 16) + (3 * 64) + (13 * 256) + (MR13_pre * 65536) + 1;
	phyd_01a0_r1 = (2 * 16) + (3 * 64) + (13 * 256) + (MR13 * 65536) + 1;

	//printf("MR13 = 0x%x\n", MR13);
	//printf("MR13_pre = 0x%x\n", MR13_pre);
	//printf("Dataflow = 0x%x\n", Dataflow);
	//printf("Dataflow_mpc = 0x%x\n", Dataflow_mpc);
	//printf("rank0_rdglvl_req = 0x%x\n", rank0_rdglvl_req);
	//printf("rank1_rdglvl_req = 0x%x\n", rank1_rdglvl_req);
	//printf("rank2_en = 0x%x\n", rank2_en);

	//printf("dis_dq_t_pre_ctrl_0304 = 0x%x\n", dis_dq_t_pre_ctrl_0304);
	//printf("dqs_dq_t_ctrl_0304 = 0x%x\n", dqs_dq_t_ctrl_0304);
	//printf("ctrl_0030_pre = 0x%x\n", ctrl_0030_pre);
	//printf("ctrl_0030 = 0x%x\n", ctrl_0030);
	//printf("ctrl_0148_pre = 0x%x\n", ctrl_0148_pre);
	//printf("ctrl_0148 = 0x%x\n", ctrl_0148);
	//printf("phyd_01a0_pre_r0 = 0x%x\n", phyd_01a0_pre_r0);
	//printf("phyd_01a0_r0 = 0x%x\n", phyd_01a0_r0);
	//printf("phyd_01a0_pre_r1 = 0x%x\n", phyd_01a0_pre_r1);
	//printf("phyd_01a0_r1 = 0x%x\n", phyd_01a0_r1);


	//MR13 = Data_A53_2_8051(MR13);
	//MR13_pre = Data_A53_2_8051(MR13_pre);
	//Dataflow = Data_A53_2_8051(Dataflow);
	//Dataflow_mpc = Data_A53_2_8051(Dataflow_mpc);
	//rank0_rdglvl_req = Data_A53_2_8051(rank0_rdglvl_req);
	//rank1_rdglvl_req = Data_A53_2_8051(rank1_rdglvl_req);
	//rank2_en = Data_A53_2_8051(rank2_en);

	//dis_dq_t_pre_ctrl_0304 = Data_A53_2_8051(dis_dq_t_pre_ctrl_0304);
	//dqs_dq_t_ctrl_0304 = Data_A53_2_8051(dqs_dq_t_ctrl_0304);
	//ctrl_0030_pre = Data_A53_2_8051(ctrl_0030_pre);
	//ctrl_0030 = Data_A53_2_8051(ctrl_0030);
	//ctrl_0148_pre = Data_A53_2_8051(ctrl_0148_pre);
	//ctrl_0148 = Data_A53_2_8051(ctrl_0148);
	//phyd_01a0_pre_r0 = Data_A53_2_8051(phyd_01a0_pre_r0);
	//phyd_01a0_r0 = Data_A53_2_8051(phyd_01a0_r0);
	//phyd_01a0_pre_r1 = Data_A53_2_8051(phyd_01a0_pre_r1);
	//phyd_01a0_r1 = Data_A53_2_8051(phyd_01a0_r1);

	//devmem_writel(0x05207a00 + 0x00, MR13);
	//devmem_writel(0x05207a00 + 0x04, MR13_pre);
	//devmem_writel(0x05207a00 + 0x08, Dataflow);
	//devmem_writel(0x05207a00 + 0x0c, Dataflow_mpc);
	//devmem_writel(0x05207a00 + 0x10, rank0_rdglvl_req);
	//devmem_writel(0x05207a00 + 0x14, rank1_rdglvl_req);
	//devmem_writel(0x05207a00 + 0x18, rank2_en);

	//devmem_writel(0x05207a00 + 0x1c, dis_dq_t_pre_ctrl_0304);
	//devmem_writel(0x05207a00 + 0x20, dqs_dq_t_ctrl_0304);
	//devmem_writel(0x05207a00 + 0x24, ctrl_0030_pre);
	//devmem_writel(0x05207a00 + 0x28, ctrl_0030);
	//devmem_writel(0x05207a00 + 0x2c, ctrl_0148_pre);
	//devmem_writel(0x05207a00 + 0x30, ctrl_0148);
	//devmem_writel(0x05207a00 + 0x34, phyd_01a0_pre_r0);
	//devmem_writel(0x05207a00 + 0x38, phyd_01a0_r0);
	//devmem_writel(0x05207a00 + 0x3c, phyd_01a0_pre_r1);
	//devmem_writel(0x05207a00 + 0x40, phyd_01a0_r1);


	devmem_writel(0x05026800 + 0x00, MR13);
	devmem_writel(0x05026800 + 0x04, MR13_pre);
	devmem_writel(0x05026800 + 0x08, Dataflow);
	devmem_writel(0x05026800 + 0x0c, Dataflow_mpc);
	devmem_writel(0x05026800 + 0x10, rank0_rdglvl_req);
	devmem_writel(0x05026800 + 0x14, rank1_rdglvl_req);
	devmem_writel(0x05026800 + 0x18, rank2_en);

	devmem_writel(0x05026800 + 0x1c, dis_dq_t_pre_ctrl_0304);
	devmem_writel(0x05026800 + 0x20, dqs_dq_t_ctrl_0304);
	devmem_writel(0x05026800 + 0x24, ctrl_0030_pre);
	devmem_writel(0x05026800 + 0x28, ctrl_0030);
	devmem_writel(0x05026800 + 0x2c, ctrl_0148_pre);
	devmem_writel(0x05026800 + 0x30, ctrl_0148);
	devmem_writel(0x05026800 + 0x34, phyd_01a0_pre_r0);
	devmem_writel(0x05026800 + 0x38, phyd_01a0_r0);
	devmem_writel(0x05026800 + 0x3c, phyd_01a0_pre_r1);
	devmem_writel(0x05026800 + 0x40, phyd_01a0_r1);

	/*
	 *printf("ST1:\n");
	 *dw_timer_start(TIMER_ID1, MCU51_CLK_HZ * 1);
	 *dis_dq_t = read_robot(0x0304 + DDR_CTRL);
	 *rddata = dis_dq_t | 0x1;//modified_bits_by_value(dis_dq_t, 1, 0, 0); //dis_dq
	 *write_robot(0x0304 + DDR_CTRL, rddata);
	 *cvx32_clk_gating_disable_retrain(DDR_CTRL, CV_DDR_PHYD_APB);
	 ***bist setting for dfi rdglvl
	 ***cvx32_bist_rdglvl_init();


	 *** wait 8051 interupt, enter retrain flow


	 ***rank0 training{{{
	 ***dfi_phymstr_en

	 *rddata = read_robot(DDR_CTRL + 0x1C4);
	 *rddata = rddata | 0x1;//modified_bits_by_value(rddata, 1, 0, 0); //DFIPHYMSTR.dfi_phymstr_en
	 *write_robot(DDR_CTRL + 0x1C4, rddata);
	 *#if 1
	 *cvx32_dfi_phymstr_req(phyd_base_addr);
	 ***cvx32_clk_gating_disable_retrain(DDR_CTRL, CV_DDR_PHYD_APB);
	 *write_robot(0x0184 + phyd_base_addr, Dataflow_mpc);

	 ***MR13
	 ***read_robot(DDR_CTRL + 0xe0, MR13);
	 ***MR13=modified_bits_by_value(MR13, 1, 1, 1); //Read Preamble Training Mode
	 *cvx32_dfi_sw_mrw_retrain(13, MR13, 1, 3, phyd_base_addr, DDR_CTRL);

	 *write_robot(phy_184, rank0_rdglvl_req); //param_phyd_dfi_rdglvl_req

	 *while (1) {
	 ***[1] param_phyd_dfi_rdglvl_done
	 *rddata = read_robot(phy_3444);
	 ***printf("rd=%lx\n", rddata);
	 *if (rddata & 0x2 == 0x2) {
	 *break;
	 *}
	 *}

	 ***delay_us(30);
	 *cvx32_dfi_sw_mrw_retrain(13, MR13_pre, 1, 3, phyd_base_addr, DDR_CTRL);
	 ***$display("[KC_DBG] cvx32_rdglvl_req rank 0 finish");
	 *

	 *
	 ***read_robot(DDR_CTRL + 0x00, rddata);
	 ***if (get_bits_from_value(rank2_en,25,24) == 3 ) { //two rank
	 *if ((rank2_en & 0x3000000) == 0x3000000 ) { //two rank

	 *cvx32_dfi_sw_mrw_retrain(13, MR13, 2, 3, phyd_base_addr, DDR_CTRL);

	 ***rgdlvl_req
	 *write_robot(phy_184, rank1_rdglvl_req);
	 ***delay_us(30);
	 *while (1) {
	 ***[1] param_phyd_dfi_rdglvl_done
	 *rddata = read_robot(phy_3444);
	 *if (rddata & 0x2 == 0x2) {
	 *break;
	 *}
	 *}
	 ***cvx32_rdglvl_req_rank2();// rank 1
	 ***$display("[KC_DBG] cvx32_rdglvl_req rank 1 finish");
	 *cvx32_dfi_sw_mrw_retrain(13, MR13_pre, 2, 3, phyd_base_addr, DDR_CTRL);
	 *}
	 *
	 *write_robot(phy_184, Dataflow);
	 *#endif
	 *cvx32_dfi_phymstr_req_clr(phyd_base_addr);
	 *cvx32_clk_gating_enable_retrain(DDR_CTRL, CV_DDR_PHYD_APB);

	 *write_robot(0x304 + DDR_CTRL, dis_dq_t);
	 */
	//read_robot(0x0184 + phyd_base_addr,  rddata);
	//rddata=modified_bits_by_value(rddata, 0, 6, 4); //Dataflow normal,{lp4,ddr4,ddr3}
	//write_robot(phy_184, Dataflow);
	//current_time = dw_timer_stop(TIMER_ID1); // ??????
	//c_time_string = ctime(&current_time); // ????????
	//printf("C: %ld\n", current_time); // ????

	//retrain finish
}

typedef int (*printf_func_type)(const char *, ...);
printf_func_type retrain_log_func;

#define RETRAIN_LOG(...) (retrain_log_func ? retrain_log_func(__VA_ARGS__) : (void)0)

void test_log(void)
{
	struct stat st = {0};
	// check ddr dir
	// printf("flag ============ %d\n", stat("/mnt/data/ddr", &st));
	if (stat("/mnt/data/ddr", &st) == -1) {
		retrain_log_func = NULL;
	} else {
		retrain_log_func = printf;
	}
}

uint32_t rdglvl_retrain_osc_comp(uint8_t uSys_id, uint32_t rank, uint32_t tctdelay_pre,
	uint32_t phyd_base_addr, uint32_t ddr_ctrl, uint32_t CV_DDR_PHYD_APB)
{
	uint32_t mr18_out[2][4];
	uint32_t mr19_out[2][4];
	uint32_t ddrc_mpc_osc_cnt;
	uint32_t tctdelay_new;
	uint32_t tctdelay_old;
	uint8_t temp_inc;
	uint8_t temp_dec;
	uint32_t tctdelay_cur;
	uint8_t i, cnts;
	uint32_t dll_code_sum;
	uint32_t dll_code_avg;
	uint32_t max_boundary;
	uint32_t tctdelay_init;
	uint32_t base_addr;
	int diff_code_wdq;
	int delay_code;

	// printf("retrain1\n");
	// test_log();

	devmem_writel(ddr_ctrl + 0x30, 0x00000100);//close some lp func

	memset(mr18_out, 0, sizeof(mr18_out));
	memset(mr19_out, 0, sizeof(mr19_out));
	for (i = 0; i < rank; i = i + 1) { //rank = 1/2
		cvx32_synp_mpcosc_lp4(1 << i, ddr_ctrl);//
		cvx32_synp_mrr_lp4_out(18, 1 << i, ddr_ctrl, mr18_out[i]);
		cvx32_synp_mrr_lp4_out(19, 1 << i, ddr_ctrl, mr19_out[i]);
	}
	devmem_writel(ddr_ctrl + 0x30, 0x0000010b);//

	// printf("retrain2\n");
	//param_phyd_to_reg_rx_dll_code3-0
	rddata = devmem_readl(0x3018 + phyd_base_addr);
	//average dll code
	dll_code_sum = get_bits_from_value(rddata, 31, 24) + get_bits_from_value(rddata, 23, 16) +
					get_bits_from_value(rddata, 15, 8) + get_bits_from_value(rddata, 7, 0);
	dll_code_avg = dll_code_sum >> 2;
	//$display("ave_dll_code = %h", ave_dll_code);

	//ddrc_mpc_osc_cnt = devmem_readl(ddr_ctrl + 0xc00);
	/*
	 *tctdelay_old = tctdelay_pre;
	 *tctdelay_new = (uint32_t)((double)(ddrc_mpc_osc_cnt * 938) / (mr19_out[0][0]*256 + mr18_out[0][0]));//

	 *if ((tctdelay_old > tctdelay_new) && ((tctdelay_old - tctdelay_new) > 4.5)) {
	 *	//tctdelay_old - tctdelay_new) > margin  TBD
	 *	tctdelay_cur = tctdelay_new;
	 *	temp_inc = 1;
	 *	temp_dec = 0;
	 *} else if ((tctdelay_old < tctdelay_new) && ((tctdelay_new - tctdelay_old) > 4.5)) {
	 *	//tctdelay_new - tctdelay_old) > margin  TBD
	 *	tctdelay_cur = tctdelay_new;
	 *	temp_dec = 1;
	 *	temp_inc = 0;
	 *} else {
	 *	temp_inc = 0;
	 *	temp_dec = 0;
	 *	tctdelay_cur = tctdelay_old;
	 *}
	 */
	if (temp_cnt == 0 && uSys_id == 0) {
		// printf("retrain3\n");
		// tctdelay_new = (ddrc_mpc_osc_cnt * 938)/(get_bits_from_value(mr19_out[1][0], 7, 0)*256
		//			+ get_bits_from_value(mr18_out[1][0], 7, 0));
		tctdelay_new = (2048 * 470)/(get_bits_from_value(mr19_out[0][0], 7, 0)*256 +
							get_bits_from_value(mr18_out[0][0], 7, 0));
		tctdelay_init_sys0 = tctdelay_new;
		tctdelay_cur = tctdelay_new;
		for (i = 0; i < 2; i = i + 1) { //rank
			// printf("MR18[%d][0] ====================== 0x%x\n", i, mr18_out[i][0]);
			// printf("MR19[%d][0] ====================== 0x%x\n", i, mr19_out[i][0]);
		}
		rddata = devmem_readl(0x168 + phyd_base_addr);
		if (get_bits_from_value(rddata, 4, 4) == 1)
			base_addr = 0x600;
		else
			base_addr = 0x624;
		for (int i = 0; i < rank; i++) {
			for (int j = 0; j < 4; j++) {
				for (int k = 0; k < 4; k++) {
					rddata = devmem_readl(base_addr + 0x600 * i + 0x60 * j + 0x4 * k +
											phyd_base_addr);
					delay_code_init_dq_sys0[i][j][2*k] = get_bits_from_value(rddata, 7, 0);
					delay_code_init_dq_sys0[i][j][2*k+1] = get_bits_from_value(rddata, 23, 16);
				}
				rddata = devmem_readl(base_addr + 0x600 * i + 0x60 * j + 0x10 + phyd_base_addr);
				delay_code_init_dq_sys0[i][j][8] = get_bits_from_value(rddata, 7, 0);
			}
		}
		/*
		 *for (int i = 0; i < 2; i++) {
		 *	for (int j = 0; j < 4; j++) {
		 *		for (int k = 0; k < 4; k++) {
		 *			rddata = devmem_readl(0x600 + 0x600 * i + 0x60 * j + 0x4 * k + phyd_base_addr);
		 *			// printf("rddata = 0x%x\n", rddata);
		 *		}
		 *		rddata = devmem_readl(0x600 + 0x600 * i + 0x60 * j + 0x10 + phyd_base_addr);
		 *		// printf("rddata = 0x%x\n", rddata);
		 *	}
		 *}
		 */
		temp_cnt += 1;
	} else if (temp_cnt == 1 && uSys_id == 1) {
		// printf("retrain4\n");
		tctdelay_new = (2048 * 470)/(get_bits_from_value(mr19_out[0][0], 7, 0)*256 +
							get_bits_from_value(mr18_out[0][0], 7, 0));
		tctdelay_init_sys1 = tctdelay_new;
		tctdelay_cur = tctdelay_new;
		for (i = 0; i < 2; i = i + 1) { //rank
			// printf("MR18[%d][0] ====================== 0x%x\n", i, mr18_out[i][0]);
			// printf("MR19[%d][0] ====================== 0x%x\n", i, mr19_out[i][0]);
		}
		rddata = devmem_readl(0x168 + phyd_base_addr);
		if (get_bits_from_value(rddata, 4, 4) == 1)
			base_addr = 0x600;
		else
			base_addr = 0x624;
		for (int i = 0; i < rank; i++) {
			for (int j = 0; j < 4; j++) {
				for (int k = 0; k < 4; k++) {
					rddata = devmem_readl(base_addr + 0x600 * i + 0x60 * j + 0x4 * k +
											phyd_base_addr);
					delay_code_init_dq_sys1[i][j][2*k] = get_bits_from_value(rddata, 7, 0);
					delay_code_init_dq_sys1[i][j][2*k+1] = get_bits_from_value(rddata, 23, 16);
				}
				rddata = devmem_readl(base_addr + 0x600 * i + 0x60 * j + 0x10 + phyd_base_addr);
				delay_code_init_dq_sys1[i][j][8] = get_bits_from_value(rddata, 7, 0);
			}
		}
		/*
		 *for (int i = 0; i < 2; i++) {
		 *	for (int j = 0; j < 4; j++) {
		 *		for (int k = 0; k < 4; k++) {
		 *			rddata = devmem_readl(0x600 + 0x600 * i + 0x60 * j + 0x4 * k + phyd_base_addr);
		 *			// printf("rddata = 0x%x\n", rddata);
		 *		}
		 *		rddata = devmem_readl(0x600 + 0x600 * i + 0x60 * j + 0x10 + phyd_base_addr);
		 *		// printf("rddata = 0x%x\n", rddata);
		 *	}
		 *}
		 */
		temp_cnt += 1;
	} else {
		// tctdelay_cur = tctdelay_pre;
		// tctdelay_new = (ddrc_mpc_osc_cnt * 938)/(get_bits_from_value(mr19_out[1][0], 7, 0)*256
		//					 + get_bits_from_value(mr18_out[1][0], 7, 0));
		// tctdelay_pre = (subsys == 0 ? tctdelay_init_sys0 : tctdelay_init_sys1);
		tctdelay_new = (2048 * 470)/(get_bits_from_value(mr19_out[0][0], 7, 0)*256 +
						get_bits_from_value(mr18_out[0][0], 7, 0));

		//printf("mr19_out[1][0] = 0x%x, mr18_out[1][0] = 0x%x\n", mr19_out[1][0], mr18_out[1][0]);
		// printf("tctdelay_pre = 0x%x, tctdelay_new = 0x%x\n", tctdelay_pre, tctdelay_new);

		// 2~3 temp change --- 1 tctdelay code
		if ((tctdelay_new > tctdelay_pre) && ((tctdelay_new - tctdelay_pre) > 1)) {
			//tctdelay_old - tctdelay_new) > margin  TBD
			tctdelay_cur = tctdelay_new;
			temp_inc = 1;
			temp_dec = 0;
			RETRAIN_LOG("temp_inc = 1\n");
		} else if ((tctdelay_new < tctdelay_pre) && ((tctdelay_pre - tctdelay_new) > 1)) {
			//tctdelay_new - tctdelay_old) > margin  TBD
			tctdelay_cur = tctdelay_new;
			temp_inc = 0;
			temp_dec = 1;
			RETRAIN_LOG("temp_dec = 1\n");
		} else {
			temp_inc = 0;
			temp_dec = 0;
			tctdelay_cur = tctdelay_pre;
			RETRAIN_LOG("temp no change\n");
		}

		if ((temp_inc == 1) || (temp_dec == 1) || (retrain_everytime == 1)) {
			//wdq
			tctdelay_init = (uSys_id == 0 ? tctdelay_init_sys0 : tctdelay_init_sys1);
			//diff_code_rdg = (tctdelay_new - tctdelay_init) * 3;  //(tctdelay_new - tctdelay_old) * coef
			//diff_code_wdq = (tctdelay_new - tctdelay_init) * 0.5;
			rddata = devmem_readl(0x168 + phyd_base_addr);
			if (get_bits_from_value(rddata, 4, 4) == 1)
				diff_code_wdq = (int)(tctdelay_new - tctdelay_init) * 0.35;
			else
				diff_code_wdq = (int)(tctdelay_new - tctdelay_init) * 0.07;
			//printf("diff_code_rdg = %d\n", diff_code_rdg);
			// printf("diff_code_wdq = %d\n", diff_code_wdq);
			// printf("tctdelay_new = 0x%x, tctdelay_init = 0x%x\n", tctdelay_new, tctdelay_init);

			// usleep(1000);

			// printf("tctdelay_pre = 0x%x, tctdelay_new = 0x%x\n", tctdelay_pre, tctdelay_new);
			// printf("temp_inc = 0x%x, diff_code_rdg = 0x%x, diff_code_wdq = 0x%x\n",
			//			temp_inc, diff_code_rdg, diff_code_wdq);
			// usleep(1000);
			rddata = devmem_readl(0x168 + phyd_base_addr);
			if (get_bits_from_value(rddata, 4, 4) == 1) {
				base_addr = 0x600;
				max_boundary = 0xff;
			} else {
				base_addr = 0x624;
				max_boundary = dll_code_avg;
			}

			for (int i = 0; i < 2; i++) {
				for (int j = 0; j < 4; j++) {
					//write eye
					for (int k = 0; k < 4; k++) {
						rddata = devmem_readl(base_addr + 0x600 * i + 0x60 * j + 0x4 * k +
												phyd_base_addr);
						delay_code = (uSys_id == 0 ? delay_code_init_dq_sys0[i][j][2*k] :
								delay_code_init_dq_sys1[i][j][2*k]) + diff_code_wdq;
					if (delay_code > max_boundary) {
						delay_code = max_boundary;
					} else if (delay_code < 0x0) {
						delay_code = 0x0;
					}
						rddata = modified_bits_by_value(rddata,
									get_bits_from_value(delay_code, 7, 0), 7, 0);

						delay_code = (uSys_id == 0 ? delay_code_init_dq_sys0[i][j][2*k+1] :
							delay_code_init_dq_sys1[i][j][2*k+1]) + diff_code_wdq;
					if (delay_code > max_boundary) {
						delay_code = max_boundary;
					} else if (delay_code < 0x0) {
						delay_code = 0x0;
					}
						rddata = modified_bits_by_value(rddata,
									get_bits_from_value(delay_code, 7, 0), 23, 16);
						devmem_writel(base_addr + 0x600 * i + 0x60*j + 0x4 * k +
										phyd_base_addr, rddata);
					}
					rddata = devmem_readl(base_addr + 0x600 * i + 0x60 * j + 0x10 + phyd_base_addr);
					delay_code = (uSys_id == 0 ? delay_code_init_dq_sys0[i][j][8] :
								delay_code_init_dq_sys1[i][j][8]) + diff_code_wdq;
					if (delay_code > max_boundary) {
						delay_code = max_boundary;
					} else if (delay_code < 0x0) {
						delay_code = 0x0;
					}
					rddata = modified_bits_by_value(rddata,
									get_bits_from_value(delay_code, 7, 0), 7, 0);
					devmem_writel(base_addr + 0x600 * i + 0x60*j + 0x10 + phyd_base_addr, rddata);
				}
			}
			// if (rank == 2) {
				//rdqs retrain flow
				//calculate value for retrain flow needed in 8051
				if ((temp_inc) || (retrain_everytime == 1)) {
					rdglvl_retrain_lp4_mpc(1, phyd_base_addr, ddr_ctrl, CV_DDR_PHYD_APB);
				} else if (temp_dec) {
					rdglvl_retrain_lp4_mpc(0, phyd_base_addr, ddr_ctrl, CV_DDR_PHYD_APB);
				}
				//printf("polling rank0 retrain\n");
				//issue rank0 retrain request;
				//reset rgd and realease reset
				devmem_writel(phyd_base_addr + 0x128, 0x1d);
				devmem_writel(phyd_base_addr + 0x128, 0x1f);
				if (uVI_en == 1) {
					//set vi info reg
					rddata = devmem_readl(0x281000f4);
					rddata = modified_bits_by_value(rddata, 1, 8, 8);
					devmem_writel(0x281000f4, rddata);
					count = 100;
					while (count--) {
						if (get_bits_from_value(devmem_readl(0x281000f4), 9, 9) == 1) {
							rddata = devmem_readl(0x281000f4);
							rddata = modified_bits_by_value(rddata, 0, 9, 9);
							devmem_writel(0x281000f4, rddata);
							break;
						}
						usleep(40 * 1000);
					} //wait vi response and go continue
				}
				if (uVO_en == 1) { //with vo
					//set AP info reg
					rddata = devmem_readl(0x281000f4);
					rddata = modified_bits_by_value(rddata, 1 << (uSys_id * 2), 3, 0);
					devmem_writel(0x281000f4, rddata);

					//set rtc info reg
					rddata = devmem_readl(0x05026028);
					rddata = modified_bits_by_value(rddata, 1 << (uSys_id * 2), 3, 0);
					devmem_writel(0x05026028, rddata);
				} else {//without VO
					rddata = devmem_readl(0x05026028);
					rddata = modified_bits_by_value(rddata, 1 << (uSys_id * 2), 3, 0);
					devmem_writel(0x05026028, rddata);

					// set vo irq and rtc info reg;
					rddata = devmem_readl(0x281000f8);
					rddata = modified_bits_by_value(rddata, 1, 16, 16);
					devmem_writel(0x281000f8, rddata);
				}

				//polling rank0 retrain done
				cnts = 0;
			while (1) {
				cnts++;
				if (uVO_en == 0) {
					RETRAIN_LOG("vo_en:%d, 0x281000f4 = 0x%x, 0x05026028 = 0x%x\n",
					uVO_en, devmem_readl(0x281000f4), devmem_readl(0x05026028));
					if (get_bits_from_value(devmem_readl(0x05026028), 3, 0) == 0) {
						break;
					}
				} else {
					usleep(1000);
					RETRAIN_LOG("vo_en:%d, 0x281000f4 = 0x%x, 0x05026028 = 0x%x\n",
					uVO_en, devmem_readl(0x281000f4), devmem_readl(0x05026028));
					if ((get_bits_from_value(devmem_readl(0x281000f4), 3, 0) == 0) &&
						(get_bits_from_value(devmem_readl(0x05026028), 3, 0) == 0)) {
						break;
					}

				if (cnts > 1000) {
					if ((get_bits_from_value(devmem_readl(0x67004000), 7, 7) == 0) &&
						(get_bits_from_value(devmem_readl(0x67005000), 7, 7) == 0)) {
						RETRAIN_LOG("polling retrain flow done > 1000 times, break.\n");
						break;
					}
				}
				}
				//usleep(1000);
			}
			//printf("0x670040AC = 0x%x, 0x670050AC = 0x%x\n",
			//		devmem_readl(0x670040AC), devmem_readl(0x670050AC));
			RETRAIN_LOG("r0 retrain done\n");
			if (rank == 2) {
				//reset rgd and realease reset
				devmem_writel(phyd_base_addr + 0x128, 0x1d);
				devmem_writel(phyd_base_addr + 0x128, 0x1f);
				//printf("polling rank1 retrain\n");
				//issue rank1 retrain request;
				if (uVI_en == 1) {
					//set vi info reg
					rddata = devmem_readl(0x281000f4);
					rddata = modified_bits_by_value(rddata, 1, 8, 8);
					devmem_writel(0x281000f4, rddata);
					count = 100;
					while (count--) {
						if (get_bits_from_value(devmem_readl(0x281000f4), 9, 9) == 1) {
							rddata = devmem_readl(0x281000f4);
							rddata = modified_bits_by_value(rddata, 0, 9, 9);
							devmem_writel(0x281000f4, rddata);
							break;
						}
						usleep(40 * 1000);
					} //wait vi response and go continue
				}
				if (uVO_en == 1) { //with vo
					//set AP info reg
					rddata = devmem_readl(0x281000f4);
					rddata = modified_bits_by_value(rddata, 2 << (uSys_id * 2), 3, 0);
					devmem_writel(0x281000f4, rddata);

					//set rtc info reg
					rddata = devmem_readl(0x05026028);
					rddata = modified_bits_by_value(rddata, 2 << (uSys_id * 2), 3, 0);
					devmem_writel(0x05026028, rddata);

				} else {//without VO
					rddata = devmem_readl(0x05026028);
					rddata = modified_bits_by_value(rddata, 2 << (uSys_id * 2), 3, 0);
					devmem_writel(0x05026028, rddata);

					// set vo irq and rtc info reg;
					rddata = devmem_readl(0x281000f8);
					rddata = modified_bits_by_value(rddata, 1, 16, 16);
					devmem_writel(0x281000f8, rddata);
				}

				//polling rank1 retrain done
				cnts = 0;
				while (1) {
					cnts++;
				if (uVO_en == 0) {
					RETRAIN_LOG("vo_en:%d, 0x281000f4 = 0x%x, 0x05026028 = 0x%x\n",
					uVO_en, devmem_readl(0x281000f4), devmem_readl(0x05026028));
					if (get_bits_from_value(devmem_readl(0x05026028), 3, 0) == 0) {
						break;
					}
				} else {
					usleep(1000);
					RETRAIN_LOG("vo_en:%d, 0x281000f4 = 0x%x, 0x05026028 = 0x%x\n",
							uVO_en, devmem_readl(0x281000f4), devmem_readl(0x05026028));
					if ((get_bits_from_value(devmem_readl(0x281000f4), 3, 0) == 0) &&
						(get_bits_from_value(devmem_readl(0x05026028), 3, 0) == 0)) {
						break;
					}
				if (cnts > 1000) {
					if ((get_bits_from_value(devmem_readl(0x67004000), 7, 7) == 0) &&
						(get_bits_from_value(devmem_readl(0x67005000), 7, 7) == 0)) {
						RETRAIN_LOG("polling retrain flow done > 1000 times, break.\n");
						break;
					}
				}
				}
				//usleep(1000);
				}
				//printf("0x670040AC = 0x%x, 0x670050AC = 0x%x\n",
				//		devmem_readl(0x670040AC), devmem_readl(0x670050AC));
				RETRAIN_LOG("r1 retrain done\n");
			// }
			}
		}
	}

	return tctdelay_cur;
}
