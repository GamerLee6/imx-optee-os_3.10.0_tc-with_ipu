/*
 * Copyright 2005-2016 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#ifndef IPU_PRV_H
#define IPU_PRV_H

#include <assert.h>
#include <drivers/tzc380.h>
#include <io.h>
#include <kernel/panic.h>
#include <mm/core_memprot.h>
#include <mm/core_mmu.h>
#include <stddef.h>
#include <trace.h>
#include <util.h>

#include "ipu-v3.h"

#define MXC_IPU_MAX_NUM		2
#define MXC_DI_NUM_PER_IPU	2

/* Globals */
// extern int dmfc_type_setup;

#define IDMA_CHAN_INVALID	0xFF
#define HIGH_RESOLUTION_WIDTH	1024

enum ipuv3_type {
	IPUv3D,		/* i.MX37 */
	IPUv3EX,	/* i.MX51 */
	IPUv3M,		/* i.MX53 */
	IPUv3H,		/* i.MX6Q/SDL */
};

// #define IPU_MAX_VDI_IN_WIDTH(type)	({ (type) >= IPUv3M ? 968 : 720; })

// struct ipu_irq_node {
// 	irqreturn_t(*handler) (int, void *);	/*!< the ISR */
// 	const char *name;	/*!< device associated with the interrupt */
// 	void *dev_id;		/*!< some unique information for the ISR */
// 	__u32 flags;		/*!< not used */
// };

// enum csc_type_t {
// 	RGB2YUV = 0,
// 	YUV2RGB,
// 	RGB2RGB,
// 	YUV2YUV,
// 	CSC_NONE,
// 	CSC_NUM
// };

struct ipu_soc {
	// unsigned int id;
	unsigned int devtype;
	// bool online;

	// /*clk*/
	// struct clk *ipu_clk;
	// struct clk *di_clk[2];
	// struct clk *di_clk_sel[2];
	// struct clk *pixel_clk[2];
	// bool pixel_clk_en[2];
	// struct clk *pixel_clk_sel[2];
	// struct clk *csi_clk[2];
	// struct clk *prg_clk;

	// /*irq*/
	// int irq_sync;
	// int irq_err;
	// struct ipu_irq_node irq_list[IPU_IRQ_COUNT];

	/*reg*/
	vaddr_t cm_reg;
	vaddr_t idmac_reg;
	vaddr_t dp_reg;
	vaddr_t ic_reg;
	vaddr_t dc_reg;
	vaddr_t dc_tmpl_reg;
	vaddr_t dmfc_reg;
	vaddr_t di_reg[2];
	vaddr_t smfc_reg;
	vaddr_t csi_reg[2];
	vaddr_t cpmem_base;
	vaddr_t tpmem_base;
	vaddr_t vdi_reg;

	// struct device *dev;

	ipu_channel_t csi_channel[2];
	ipu_channel_t using_ic_dirct_ch;
	// unsigned char dc_di_assignment[10];
	bool sec_chan_en[IPU_MAX_CH];
	bool thrd_chan_en[IPU_MAX_CH];
	// bool chan_is_interlaced[52];
	uint32_t channel_init_mask;
	uint32_t channel_enable_mask;

	// /*use count*/
	// int dc_use_count;
	// int dp_use_count;
	// int dmfc_use_count;
	// int smfc_use_count;
	int ic_use_count;
	// int rot_use_count;
	// int vdi_use_count;
	// int di_use_count[2];
	int csi_use_count[2];

	// struct mutex mutex_lock;
	// spinlock_t int_reg_spin_lock;
	// spinlock_t rdy_reg_spin_lock;

	// int dmfc_size_28;
	// int dmfc_size_29;
	// int dmfc_size_24;
	// int dmfc_size_27;
	// int dmfc_size_23;

	// enum csc_type_t fg_csc_type;
	// enum csc_type_t bg_csc_type;
	// bool color_key_4rgb;
	// bool dc_swap;
	// struct completion dc_comp;
	// struct completion csi_comp;

	// int	vdoa_en;
	unsigned int normal_axi;
};

struct ipu_channel {
	uint8_t video_in_dma;
	uint8_t alpha_in_dma;
	uint8_t graph_in_dma;
	uint8_t out_dma;
};

// enum ipu_dmfc_type {
// 	DMFC_NORMAL = 0,
// 	DMFC_HIGH_RESOLUTION_DC,
// 	DMFC_HIGH_RESOLUTION_DP,
// 	DMFC_HIGH_RESOLUTION_ONLY_DP,
// };


static inline uint32_t ipu_cm_read(struct ipu_soc *ipu, unsigned int offset)
{
	return io_read32(ipu->cm_reg + offset);
}

static inline void ipu_cm_write(struct ipu_soc *ipu,
		uint32_t value,  unsigned int offset)
{
	io_write32(ipu->cm_reg + offset, value);
}

static inline uint32_t ipu_idmac_read(struct ipu_soc *ipu,  unsigned int offset)
{
	return io_read32(ipu->idmac_reg + offset);
}

static inline void ipu_idmac_write(struct ipu_soc *ipu,
		uint32_t value,  unsigned int offset)
{
	io_write32(ipu->idmac_reg + offset, value);
}

static inline uint32_t ipu_dc_read(struct ipu_soc *ipu,  unsigned int offset)
{
	return io_read32(ipu->dc_reg + offset);
}

static inline void ipu_dc_write(struct ipu_soc *ipu,
		uint32_t value,  unsigned int offset)
{
	io_write32(ipu->dc_reg + offset, value);
}

static inline uint32_t ipu_dc_tmpl_read(struct ipu_soc *ipu,  unsigned int offset)
{
	return io_read32(ipu->dc_tmpl_reg + offset);
}

static inline void ipu_dc_tmpl_write(struct ipu_soc *ipu,
		uint32_t value,  unsigned int offset)
{
	io_write32(ipu->dc_tmpl_reg + offset, value);
}

static inline uint32_t ipu_dmfc_read(struct ipu_soc *ipu,  unsigned int offset)
{
	return io_read32(ipu->dmfc_reg + offset);
}

static inline void ipu_dmfc_write(struct ipu_soc *ipu,
		uint32_t value,  unsigned int offset)
{
	io_write32(ipu->dmfc_reg + offset, value);
}

static inline uint32_t ipu_dp_read(struct ipu_soc *ipu,  unsigned int offset)
{
	return io_read32(ipu->dp_reg + offset);
}

static inline void ipu_dp_write(struct ipu_soc *ipu,
		uint32_t value,  unsigned int offset)
{
	io_write32(ipu->dp_reg + offset, value);
}

static inline uint32_t ipu_di_read(struct ipu_soc *ipu, int di,  unsigned int offset)
{
	return io_read32(ipu->di_reg[di] + offset);
}

static inline void ipu_di_write(struct ipu_soc *ipu, int di,
		uint32_t value,  unsigned int offset)
{
	io_write32(ipu->di_reg[di] + offset, value);
}

static inline uint32_t ipu_csi_read(struct ipu_soc *ipu, int csi,  unsigned int offset)
{
	return io_read32(ipu->csi_reg[csi] + offset);
}

static inline void ipu_csi_write(struct ipu_soc *ipu, int csi,
		uint32_t value,  unsigned int offset)
{
	io_write32(ipu->csi_reg[csi] + offset, value);
}

static inline uint32_t ipu_smfc_read(struct ipu_soc *ipu,  unsigned int offset)
{
	return io_read32(ipu->smfc_reg + offset);
}

static inline void ipu_smfc_write(struct ipu_soc *ipu,
		uint32_t value,  unsigned int offset)
{
	io_write32(ipu->smfc_reg + offset, value);
}

static inline uint32_t ipu_vdi_read(struct ipu_soc *ipu,  unsigned int offset)
{
	return io_read32(ipu->vdi_reg + offset);
}

static inline void ipu_vdi_write(struct ipu_soc *ipu,
		uint32_t value,  unsigned int offset)
{
	io_write32(ipu->vdi_reg + offset, value);
}

static inline uint32_t ipu_ic_read(struct ipu_soc *ipu,  unsigned int offset)
{
	return io_read32(ipu->ic_reg + offset);
}

static inline void ipu_ic_write(struct ipu_soc *ipu,
		uint32_t value,  unsigned int offset)
{
	io_write32(ipu->ic_reg + offset, value);
}

// int register_ipu_device(struct ipu_soc *ipu, int id);
// void unregister_ipu_device(struct ipu_soc *ipu, int id);
ipu_color_space_t format_to_colorspace(uint32_t fmt);
// bool ipu_pixel_format_has_alpha(uint32_t fmt);

void ipu_dump_registers(struct ipu_soc *ipu);

// uint32_t _ipu_channel_status(struct ipu_soc *ipu, ipu_channel_t channel);

// void ipu_disp_init(struct ipu_soc *ipu);
// void _ipu_init_dc_mappings(struct ipu_soc *ipu);
// int _ipu_dp_init(struct ipu_soc *ipu, ipu_channel_t channel, uint32_t in_pixel_fmt,
// 		 uint32_t out_pixel_fmt);
// void _ipu_dp_uninit(struct ipu_soc *ipu, ipu_channel_t channel);
// void _ipu_dc_init(struct ipu_soc *ipu, int dc_chan, int di, bool interlaced, uint32_t pixel_fmt);
// void _ipu_dc_uninit(struct ipu_soc *ipu, int dc_chan);
// void _ipu_dp_dc_enable(struct ipu_soc *ipu, ipu_channel_t channel);
// void _ipu_dp_dc_disable(struct ipu_soc *ipu, ipu_channel_t channel, bool swap);
// void _ipu_dmfc_init(struct ipu_soc *ipu, int dmfc_type, int first);
// void _ipu_dmfc_set_wait4eot(struct ipu_soc *ipu, int dma_chan, int width);
// void _ipu_dmfc_set_burst_size(struct ipu_soc *ipu, int dma_chan, int burst_size);
// int _ipu_disp_chan_is_interlaced(struct ipu_soc *ipu, ipu_channel_t channel);

void _ipu_ic_enable_task(struct ipu_soc *ipu, ipu_channel_t channel);
void _ipu_ic_disable_task(struct ipu_soc *ipu, ipu_channel_t channel);
// int  _ipu_ic_init_prpvf(struct ipu_soc *ipu, ipu_channel_params_t *params,
// 			bool src_is_csi);
// void _ipu_vdi_init(struct ipu_soc *ipu, ipu_channel_t channel, ipu_channel_params_t *params);
// void _ipu_vdi_uninit(struct ipu_soc *ipu);
// void _ipu_ic_uninit_prpvf(struct ipu_soc *ipu);
// void _ipu_ic_init_rotate_vf(struct ipu_soc *ipu, ipu_channel_params_t *params);
// void _ipu_ic_uninit_rotate_vf(struct ipu_soc *ipu);
// void _ipu_ic_init_csi(struct ipu_soc *ipu, ipu_channel_params_t *params);
// void _ipu_ic_uninit_csi(struct ipu_soc *ipu);
int  _ipu_ic_init_prpenc(struct ipu_soc *ipu, ipu_channel_params_t *params,
			 bool src_is_csi);
void _ipu_ic_uninit_prpenc(struct ipu_soc *ipu);
// void _ipu_ic_init_rotate_enc(struct ipu_soc *ipu, ipu_channel_params_t *params);
// void _ipu_ic_uninit_rotate_enc(struct ipu_soc *ipu);
// int  _ipu_ic_init_pp(struct ipu_soc *ipu, ipu_channel_params_t *params);
// void _ipu_ic_uninit_pp(struct ipu_soc *ipu);
// void _ipu_ic_init_rotate_pp(struct ipu_soc *ipu, ipu_channel_params_t *params);
// void _ipu_ic_uninit_rotate_pp(struct ipu_soc *ipu);
int _ipu_ic_idma_init(struct ipu_soc *ipu, int dma_chan, uint16_t width, uint16_t height,
		      int burst_size, ipu_rotate_mode_t rot);
// void _ipu_vdi_toggle_top_field_man(struct ipu_soc *ipu);
int _ipu_csi_init(struct ipu_soc *ipu, ipu_channel_t channel, uint32_t csi);
int _ipu_csi_set_mipi_di(struct ipu_soc *ipu, uint32_t num, uint32_t di_val, uint32_t csi);
// void ipu_csi_set_test_generator(struct ipu_soc *ipu, bool active, uint32_t r_value,
// 		uint32_t g_value, uint32_t b_value,
// 		uint32_t pix_clk, uint32_t csi);
// void _ipu_csi_ccir_err_detection_enable(struct ipu_soc *ipu, uint32_t csi);
// void _ipu_csi_ccir_err_detection_disable(struct ipu_soc *ipu, uint32_t csi);
// void _ipu_csi_wait4eof(struct ipu_soc *ipu, ipu_channel_t channel);
// void _ipu_smfc_init(struct ipu_soc *ipu, ipu_channel_t channel, uint32_t mipi_id, uint32_t csi);
// void _ipu_smfc_set_burst_size(struct ipu_soc *ipu, ipu_channel_t channel, uint32_t bs);
// void _ipu_dp_set_csc_coefficients(struct ipu_soc *ipu, ipu_channel_t channel, int32_t param[][3]);
// int32_t _ipu_disp_set_window_pos(struct ipu_soc *ipu, ipu_channel_t channel,
// 		int16_t x_pos, int16_t y_pos);
// int32_t _ipu_disp_get_window_pos(struct ipu_soc *ipu, ipu_channel_t channel,
// 		int16_t *x_pos, int16_t *y_pos);
// void _ipu_get(struct ipu_soc *ipu);
// void _ipu_put(struct ipu_soc *ipu);

// struct clk *clk_register_mux_pix_clk(struct device *dev, const char *name,
// 		const char **parent_names, u8 num_parents, unsigned long flags,
// 		u8 ipu_id, u8 di_id, u8 clk_mux_flags);
// struct clk *clk_register_div_pix_clk(struct device *dev, const char *name,
// 		const char *parent_name, unsigned long flags,
// 		u8 ipu_id, u8 di_id, u8 clk_div_flags);
// struct clk *clk_register_gate_pix_clk(struct device *dev, const char *name,
// 		const char *parent_name, unsigned long flags,
// 		u8 ipu_id, u8 di_id, u8 clk_gate_flags);

#endif	/* IPU_PRV_H */
