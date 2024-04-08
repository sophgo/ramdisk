#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "dfi_monitor.h"
#include "devmem.h"
#include "argvtoint.h"

#define DDR_TOP_SYS1_BASE		0x7000A000
#define DDR_TOP_SYS2_BASE		0x7800A000
#define DDRC_BASE				0x70004000
#define CLKMON_MEANS_CLKIN		0x03000140

#define ERR	printf
#define DEBUG	//printf
#define INFO	printf

uint64_t dfi_r0_mw_cnt[4] = {0, 0, 0, 0};		//dfi_r0_mw_cnt[31:0]
uint64_t dfi_r0_wr2_cnt[4] = {0, 0, 0, 0};	//dfi_r0_wr2_cnt[31:0]
uint64_t dfi_r0_wr_cnt[4] = {0, 0, 0, 0};		//dfi_r0_wr_cnt[31:0]
uint64_t dfi_r0_rd2_cnt[4] = {0, 0, 0, 0};	//dfi_r0_rd2_cnt[31:0]
uint64_t dfi_r0_rd_cnt[4] = {0, 0, 0, 0};		//dfi_r0_rd_cnt[31:0]
uint64_t dfi_r0_act_cnt[4] = {0, 0, 0, 0};	//dfi_r0_act_cnt[31:0]
uint64_t dfi_r0_preab_cnt[4] = {0, 0, 0, 0};	//dfi_r0_preab_cnt[31:0]
uint64_t dfi_r0_prepb_cnt[4] = {0, 0, 0, 0};	//dfi_r0_prepb_cnt[31:0]

uint64_t dfi_r1_mw_cnt[4] = {0, 0, 0, 0};		//dfi_r0_mw_cnt[31:0]
uint64_t dfi_r1_wr2_cnt[4] = {0, 0, 0, 0};	//dfi_r0_wr2_cnt[31:0]
uint64_t dfi_r1_wr_cnt[4] = {0, 0, 0, 0};		//dfi_r0_wr_cnt[31:0]
uint64_t dfi_r1_rd2_cnt[4] = {0, 0, 0, 0};	//dfi_r0_rd2_cnt[31:0]
uint64_t dfi_r1_rd_cnt[4] = {0, 0, 0, 0};		//dfi_r0_rd_cnt[31:0]
uint64_t dfi_r1_act_cnt[4] = {0, 0, 0, 0};	//dfi_r0_act_cnt[31:0]
uint64_t dfi_r1_preab_cnt[4] = {0, 0, 0, 0};	//dfi_r0_preab_cnt[31:0]
uint64_t dfi_r1_prepb_cnt[4] = {0, 0, 0, 0};	//dfi_r0_prepb_cnt[31:0]

uint64_t dfi_w2r_cnt[4] = {0, 0, 0, 0};		//dfi_w2r_cnt[31:0]
uint64_t dfi_rpt_cycle_cnt[4] = {0, 0, 0, 0};	//dfi_rpt_cycle_cnt[31:0]

uint64_t total_dfi_w_cnt[4] = {0, 0, 0, 0};
uint64_t total_dfi_r_cnt[4] = {0, 0, 0, 0};
uint64_t total_dfi_mw_cnt[4] = {0, 0, 0, 0};
uint64_t total_dfi_act_cnt[4] = {0, 0, 0, 0};

uint32_t ch_num = 0;

#define TOP_FABRIC 0x6fff0000
#define OFFSET_AXI_CG_EN 0x4c
#define AXIMON_BIT 0x7

#define AXIMON_SNAPSHOT_REGVALUE_1 0x40004
#define AXIMON_SNAPSHOT_REGVALUE_2 0x40000

#define AXIMON_START_REGVALUE 0x30001
#define AXIMON_STOP_REGVALUE 0x30002

#define AXIMON_OFFLINE_BASE 0x6fff4000

#define AXIMON_offline_M1_WRITE	0x0

uint64_t axi_offline_m1_cycle_cnt = 0;
uint64_t axi_offline_m1_hit_cnt = 0;
uint64_t axi_offline_m1_byte_cnt = 0;

static void axi_mon_cg_en(uint8_t enable)
{
	uint32_t value;

	value = devmem_readl(TOP_FABRIC + OFFSET_AXI_CG_EN);
	if (enable)
		devmem_writel((TOP_FABRIC + OFFSET_AXI_CG_EN), (value | AXIMON_BIT));
	else
		devmem_writel((TOP_FABRIC + OFFSET_AXI_CG_EN), (value & ~AXIMON_BIT));
}

static void axi_mon_offline_start(uint32_t base_register)
{
	devmem_writel((AXIMON_OFFLINE_BASE + base_register), AXIMON_START_REGVALUE);
}

static void axi_mon_offline_stop(uint32_t base_register)
{
	devmem_writel((AXIMON_OFFLINE_BASE + base_register), AXIMON_STOP_REGVALUE);
}

static void axi_mon_offline_snapshot(uint32_t base_register)
{
	devmem_writel((AXIMON_OFFLINE_BASE + base_register), AXIMON_SNAPSHOT_REGVALUE_1);
	devmem_writel((AXIMON_OFFLINE_BASE + base_register), AXIMON_SNAPSHOT_REGVALUE_2);
}

static uint32_t get_ddr_type(void)
{
	uint32_t reg_mstr;

	// read MSTR for ddr type
	reg_mstr = devmem_readl(DDRC_BASE + 0x00);
	if (reg_mstr & 0x1) {
		DEBUG("DDR3\n");
		return 0; // DDR3
	} else if (reg_mstr & 0x10) {
		DEBUG("DDR4\n");
		ch_num = 2;
		return 1; // DDR4
	} else if (reg_mstr & 0x20) {
		DEBUG("LPDDR4\n");
		ch_num = 4;
		return 2; // LPDDR4
	}

	ERR("Wrong DDR type (0x%08X)\n", reg_mstr);
	return 0;
}

static uint32_t get_ddr_bus_width(void)
{
	uint32_t reg_mstr;

	// read MSTR
	reg_mstr = devmem_readl(DDRC_BASE + 0x00);

	if (((reg_mstr >> 12) & 0x3) == 0) {
		DEBUG("Bus width is X32\n");
		return 32;
	} else if (((reg_mstr >> 12) & 0x3) == 1) {
		DEBUG("Bus width is X16\n");
		return 16;
	}
	ERR("Wrong DDR bus width (0x%08X)\n", reg_mstr);
	return 0;
}

static void dfi_monitor_start(uint32_t ddr_type)
{
	unsigned long DDR_TOP_BASE[4] = {0x7000A000, 0x7000A100, 0x7800A000, 0x7800A100};

	// dfi_mon_en[0], dfi_mon_tphy_wrdata[7:4], dfi_mon_mem_type[9:8]
	// dfi_mon_mem_type[9:8] = 0 (DDR3)
	// dfi_mon_mem_type[9:8] = 1 (DDR4)
	// dfi_mon_mem_type[9:8] = 2 (LPDDR4)
	devmem_writel(DDR_TOP_BASE[0] + 0x40, 0x001 | (ddr_type << 8));
	devmem_writel(DDR_TOP_BASE[1] + 0x40, 0x001 | (ddr_type << 8));
	devmem_writel(DDR_TOP_BASE[2] + 0x40, 0x001 | (ddr_type << 8));
	devmem_writel(DDR_TOP_BASE[3] + 0x40, 0x001 | (ddr_type << 8));
}

static void dfi_monitor_end(void)
{
	unsigned long DDR_TOP_BASE[4] = {0x7000A000, 0x7000A100, 0x7800A000, 0x7800A100};

	devmem_writel(DDR_TOP_BASE[0] + 0x40, 0x0); //dfi_mon_en[0], dfi_mon_tphy_wrdata[7:4], dfi_mon_mem_type[9:8]
	devmem_writel(DDR_TOP_BASE[1] + 0x40, 0x0); //dfi_mon_en[0], dfi_mon_tphy_wrdata[7:4], dfi_mon_mem_type[9:8]
	devmem_writel(DDR_TOP_BASE[2] + 0x40, 0x0); //dfi_mon_en[0], dfi_mon_tphy_wrdata[7:4], dfi_mon_mem_type[9:8]
	devmem_writel(DDR_TOP_BASE[3] + 0x40, 0x0); //dfi_mon_en[0], dfi_mon_tphy_wrdata[7:4], dfi_mon_mem_type[9:8]
}

static void dfi_monitor_rpt_trig(void)
{
	unsigned long DDR_TOP_BASE[4] = {0x7000A000, 0x7000A100, 0x7800A000, 0x7800A100};

	devmem_writel(DDR_TOP_BASE[0] + 0x44, 0x1); //dfi_mon_rpt_trig[0]
	devmem_writel(DDR_TOP_BASE[1] + 0x44, 0x1); //dfi_mon_rpt_trig[0]
	devmem_writel(DDR_TOP_BASE[2] + 0x44, 0x1); //dfi_mon_rpt_trig[0]
	devmem_writel(DDR_TOP_BASE[3] + 0x44, 0x1); //dfi_mon_rpt_trig[0]
}

static void dfi_monitor_readrpt(void)
{
	int i;
	unsigned long DDR_TOP_BASE[4] = {0x7000A000, 0x7000A100, 0x7800A000, 0x7800A100};

	for(i = 0; i < ch_num; i++)
	{
		dfi_r0_mw_cnt[i] += devmem_readl(DDR_TOP_BASE[i] + 0x68); //dfi_r0_mw_cnt[31:0]
		dfi_r0_wr2_cnt[i] += devmem_readl(DDR_TOP_BASE[i] + 0x60); //dfi_r0_wr2_cnt[31:0]
		dfi_r0_wr_cnt[i] += devmem_readl(DDR_TOP_BASE[i] + 0x58); //dfi_r0_wr_cnt[31:0]
		dfi_r0_rd2_cnt[i] += devmem_readl(DDR_TOP_BASE[i] + 0x50); //dfi_r0_rd2_cnt[31:0]
		dfi_r0_rd_cnt[i] += devmem_readl(DDR_TOP_BASE[i] + 0x48); //dfi_r0_rd_cnt[31:0]
		//sys1_dfi_r0_act_cnt += devmem_readl(DDR_TOP_SYS1_BASE + 0x38); //dfi_r0_act_cnt[31:0]
		dfi_r0_preab_cnt[i] += devmem_readl(DDR_TOP_BASE[i] + 0x70); //dfi_r0_preab_cnt[31:0]
		dfi_r0_prepb_cnt[i] += devmem_readl(DDR_TOP_BASE[i] + 0x78); //dfi_r0_prepb_cnt[31:0]
		dfi_r0_act_cnt[i] += devmem_readl(DDR_TOP_BASE[i] + 0x90);

		dfi_r1_mw_cnt[i] += devmem_readl(DDR_TOP_BASE[i] + 0x6c); //dfi_r1_mw_cnt[31:0]
		dfi_r1_wr2_cnt[i] += devmem_readl(DDR_TOP_BASE[i] + 0x64); //dfi_r1_wr2_cnt[31:0]
		dfi_r1_wr_cnt[i] += devmem_readl(DDR_TOP_BASE[i] + 0x5c); //dfi_r1_wr_cnt[31:0]
		dfi_r1_rd2_cnt[i] += devmem_readl(DDR_TOP_BASE[i] + 0x54); //dfi_r1_rd2_cnt[31:0]
		dfi_r1_rd_cnt[i] += devmem_readl(DDR_TOP_BASE[i] + 0x4c); //dfi_r1_rd_cnt[31:0]
		//sys1_dfi_r1_act_cnt += devmem_readl(DDR_TOP_SYS1_BASE + 0x38); //dfi_r1_act_cnt[31:0]
		dfi_r1_preab_cnt[i] += devmem_readl(DDR_TOP_BASE[i] + 0x74); //dfi_r1_preab_cnt[31:0]
		dfi_r1_prepb_cnt[i] += devmem_readl(DDR_TOP_BASE[i] + 0x7c); //dfi_r1_prepb_cnt[31:0]
		dfi_r1_act_cnt[i] += devmem_readl(DDR_TOP_BASE[i] + 0x94);

		dfi_rpt_cycle_cnt[i] += devmem_readl(DDR_TOP_BASE[i] + 0xc4); //dfi_rpt_cycle_cnt[31:0]
		dfi_w2r_cnt[i] += devmem_readl(DDR_TOP_BASE[i] + 0xc0); //dfi_w2r_cnt[31:0]

		total_dfi_mw_cnt[i] += dfi_r0_mw_cnt[i];
		total_dfi_mw_cnt[i] += dfi_r1_mw_cnt[i];

		total_dfi_w_cnt[i] += dfi_r0_wr_cnt[i];
		total_dfi_w_cnt[i] += dfi_r0_wr2_cnt[i];
		total_dfi_w_cnt[i] += dfi_r1_wr_cnt[i];
		total_dfi_w_cnt[i] += dfi_r1_wr2_cnt[i];

		total_dfi_r_cnt[i] += dfi_r0_rd_cnt[i];
		total_dfi_r_cnt[i] += dfi_r0_rd2_cnt[i];
		total_dfi_r_cnt[i] += dfi_r1_rd_cnt[i];
		total_dfi_r_cnt[i] += dfi_r1_rd2_cnt[i];

		total_dfi_act_cnt[i] += dfi_r0_act_cnt[i];
		total_dfi_act_cnt[i] += dfi_r1_act_cnt[i];
		INFO("DFI monitor information: ch%d\n", i);
		INFO("dfi_r0_mw_cnt[%d]    = %lld\n", i, dfi_r0_mw_cnt[i]);
		INFO("dfi_r0_wr2_cnt[%d]   = %lld\n", i, dfi_r0_wr2_cnt[i]);
		INFO("dfi_r0_wr_cnt[%d]    = %lld\n", i, dfi_r0_wr_cnt[i]);
		INFO("dfi_r0_rd2_cnt[%d]   = %lld\n", i, dfi_r0_rd2_cnt[i]);
		INFO("dfi_r0_rd_cnt[%d]    = %lld\n", i, dfi_r0_rd_cnt[i]);
		//DEBUG("dfi_r0_act_cnt=0x%08X\n", sys1_dfi_r0_act_cnt);
		INFO("dfi_r0_preab_cnt[%d] = %lld\n", i, dfi_r0_preab_cnt[i]);
		INFO("dfi_r0_prepb_cnt[%d] = %lld\n", i, dfi_r0_prepb_cnt[i]);
		INFO("dfi_r0_act_cnt[%d]   = %lld\n", i, dfi_r0_act_cnt[i]);

		INFO("dfi_r1_mw_cnt[%d]    = %lld\n", i, dfi_r1_mw_cnt[i]);
		INFO("dfi_r1_wr2_cnt[%d]   = %lld\n", i, dfi_r1_wr2_cnt[i]);
		INFO("dfi_r1_wr_cnt[%d]    = %lld\n", i, dfi_r1_wr_cnt[i]);
		INFO("dfi_r1_rd2_cnt[%d]   = %lld\n", i, dfi_r1_rd2_cnt[i]);
		INFO("dfi_r1_rd_cnt[%d]    = %lld\n", i, dfi_r1_rd_cnt[i]);
		//DEBUG("dfi_r1_act_cnt=0x%08X\n", sys1_dfi_r1_act_cnt);
		INFO("dfi_r1_preab_cnt[%d] = %lld\n", i, dfi_r1_preab_cnt[i]);
		INFO("dfi_r1_prepb_cnt[%d] = %lld\n", i, dfi_r1_prepb_cnt[i]);
		INFO("dfi_r1_act_cnt[%d]   = %lld\n", i, dfi_r1_act_cnt[i]);

		INFO("dfi_rpt_cycle_cnt[%d]= %lld\n", i, dfi_rpt_cycle_cnt[i]);
		INFO("dfi_w2r_cnt[%d]      = %lld\n", i, dfi_w2r_cnt[i]);

		INFO("total_dfi_mw_cnt[%d] = %lld\n", i, total_dfi_mw_cnt[i]);
		INFO("total_dfi_w_cnt[%d]  = %lld\n", i, total_dfi_w_cnt[i]);
		INFO("total_dfi_r_cnt[%d]  = %lld\n", i, total_dfi_r_cnt[i]);
		INFO("total_dfi_act_cnt[%d]= %lld\n", i, total_dfi_act_cnt[i]);
		INFO("\n");
	}
}

static uint32_t meas_clk_in_mhz(uint32_t meas_sel)
{
	uint32_t clk_meas;
	uint32_t meas_count;
	uint32_t clk_in_mhz;
#if 0
	// meas_en[0] = 0
	devmem_writel(CLKMON_MEANS_CLKIN, 0 | meas_sel << 2);
	// meas_en[0] = 1
	devmem_writel(CLKMON_MEANS_CLKIN, 1 | meas_sel << 2);

	clk_meas = devmem_readl(CLKMON_MEANS_CLKIN);
	meas_count = (clk_meas >> 8) & 0x3FFFFF;
	clk_in_mhz = meas_count / 10 * 2;

	// disable clk meas
	devmem_writel(CLKMON_MEANS_CLKIN, 0);
#endif
	//clk_in_mhz = 2133;

	// meas_en[0] = 0
	devmem_writel(0x28102b48, 0x8);
	// meas_en[0] = 1
	devmem_writel(0x28102b40, 0);
	
	devmem_writel(0x28102b40, 0x61);

	clk_meas = devmem_readl(0x28102b40);
	meas_count = (clk_meas >> 8) & 0x3FFFFF;
	clk_in_mhz = meas_count / 10 * 4;
	
	return clk_in_mhz;
}

static void dfi_calc_bw(uint32_t meas_time_ms,
			uint32_t ddr_type,
			uint32_t ddr_bus_width)
{
	uint32_t ddr_clk;
	uint64_t byte_count[4];
	uint64_t cycle_count[4];
	double bw[4];
	double bw_axi[4];
	double ideal_bw;
	double efficiency;

	uint32_t i;

	ddr_clk = meas_clk_in_mhz(1);
	INFO("\nddr_clk = %d MHz\n\n", ddr_clk);

	INFO("BW Results:\n");
	for (i = 0; i < ch_num; i++) {
		// byte count
		byte_count[i] = total_dfi_mw_cnt[i];
		byte_count[i] += total_dfi_w_cnt[i];
		byte_count[i] += total_dfi_r_cnt[i];

		byte_count[i] *= (ddr_bus_width / 8);
		byte_count[i] *= (ddr_type == 2 ? 16 : 8);
		INFO("ch%d byte_count = %u\n", i, byte_count[i]);

		// cycle count
		cycle_count[i] = dfi_rpt_cycle_cnt[i];
		INFO("ch%d dfi_cycle_count = %u\n", i, cycle_count[i]);

		// bw
		bw[i] =  (float) (byte_count[i] * ddr_clk / 2) / (float) cycle_count[i];
		INFO("ch%d bw = %.2f MB/s\n", i, bw[i]);

		bw_axi[i] = (float) (byte_count[i] * 1000) / axi_offline_m1_cycle_cnt;
		INFO("ch%d bw(axi) = %.2f MB/s\n\n", i, bw_axi[i]);
	}

	// ideal bw
	//ideal_bw = (cycle_count[0] + cycle_count[1] + cycle_count[2] + cycle_count[3]) * 4;
	//ideal_bw *= (ddr_bus_width / 8);
	//ideal_bw /= (meas_time_ms * 1000);
	//INFO("ideal_bw = %.2f MB/s\n", ideal_bw);

	// efficiency(%)
	//efficiency = ((bw[0] + bw[1] + bw[2] + bw[3]) * 100)/ideal_bw;
	//INFO("efficiency = %.2f %%\n", efficiency);
}

int main(int argc, char *argv[])
{
	uint32_t meas_time_ms;
	uint32_t remain;
	uint32_t ddr_type;
	uint32_t ddr_bus_width;

	if (argc >= 2) {
		meas_time_ms = StrToInt(argv[1]);
		if (meas_time_ms == 0)
			meas_time_ms = 1000;
	} else {
		meas_time_ms = 10; //1 msec
	}
	INFO("measure time = %d ms\n", meas_time_ms);

	ddr_type = get_ddr_type();
	INFO("DDR Type = %d (0:DDR3, 1:DDR4, 2:LPDDR4)\n", ddr_type);

	ddr_bus_width = get_ddr_bus_width();
	INFO("Bus width = %d\n", ddr_bus_width);

	axi_mon_cg_en(1);

	remain = meas_time_ms;

	axi_mon_offline_start(AXIMON_offline_M1_WRITE);

	while (remain) {
		dfi_monitor_start(ddr_type);
		usleep(1000 * (remain > 500 ? 500 : remain));
		axi_mon_offline_snapshot(AXIMON_offline_M1_WRITE);
		dfi_monitor_end();
		axi_offline_m1_cycle_cnt += devmem_readl(AXIMON_OFFLINE_BASE + 0x24);
		axi_offline_m1_hit_cnt += devmem_readl(AXIMON_OFFLINE_BASE + 0x28);
		axi_offline_m1_byte_cnt += devmem_readl(AXIMON_OFFLINE_BASE + 0x2c);
		INFO("axi_offline_m1_cycle_cnt = %lld.\n", axi_offline_m1_cycle_cnt);
		INFO("axi_offline_m1_hit_cnt = %lld.\n", axi_offline_m1_hit_cnt);
		INFO("axi_offline_m1_byte_cnt = %lld.\n", axi_offline_m1_byte_cnt);
		dfi_monitor_rpt_trig();
		dfi_monitor_readrpt();
		remain -= (remain > 500 ? 500 : remain);
	}
	axi_mon_offline_stop(AXIMON_offline_M1_WRITE);

	dfi_calc_bw(meas_time_ms, ddr_type, ddr_bus_width);

	return 0;
}
