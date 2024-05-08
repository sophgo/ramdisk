#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define GENMASK(msb, lsb)	((2U << (msb))-(1U << (lsb)))

// uint32_t mmio_rd32(uintptr_t addr);
// void mmio_wr32(uintptr_t addr, uint32_t value);
void *devm_map(unsigned long addr, int len);
void devm_unmap(void *virt_addr, int len);
uint32_t devmem_readl(unsigned long addr);
void devmem_writel(unsigned long addr, uint32_t val);
void delay(void);
void test_log(void);

uint32_t modified_bits_by_value(uint32_t orig, uint32_t value, uint32_t msb, uint32_t lsb);
uint32_t get_bits_from_value(uint32_t value, uint32_t msb, uint32_t lsb);

void cvx32_dfi_phymstr_req(unsigned long pyhd_base_addr);
void cvx32_dfi_phymstr_req_clr(unsigned long pyhd_base_addr);
void cvx32_dll_sw_clr(unsigned long ddr_ctrl, unsigned long pyhd_base_addr);
void cvx32_synp_mrw_lp4(uint32_t ddr_ctrl, uint32_t addr, uint32_t data, uint32_t rank);
uint32_t get_sys_num(void);
uint32_t get_adc3_vol(void);
uint32_t dline_code_convertion_patch(uint32_t dline_code_in, uint32_t dll_master_latch);
uint32_t dline_code_convertion_patch_v3(uint32_t dline_code_in, uint32_t dll_code_sum);
void cvx32_synp_mrr_lp4_out(unsigned long addr, uint32_t rank, unsigned long ddr_ctrl, uint32_t *mr_out);
void cvx32_synp_mpcosc_lp4(uint32_t rank, unsigned long ddr_ctrl);
uint32_t cvx32_synp_lp4_tracking(uint32_t rank, unsigned long pyhd_base_addr, unsigned long ddr_ctrl,
					uint32_t subsys, uint32_t mask_code_init_sys[2][4], uint32_t tctdelay_pre);
void rdglvl_retrain_lp4_mpc(uint16_t temp_inc, uint32_t phyd_base_addr, uint32_t DDR_CTRL,
					uint32_t CV_DDR_PHYD_APB);
uint32_t rdglvl_retrain_osc_comp(uint8_t uSys_id, uint32_t rank, uint32_t tctdelay_pre,
					uint32_t phyd_base_addr, uint32_t ddr_ctrl, uint32_t CV_DDR_PHYD_APB);