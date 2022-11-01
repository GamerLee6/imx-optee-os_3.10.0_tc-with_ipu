

#include <initcall.h>
#include <inttypes.h>
#include <io.h>
#include <kernel/interrupt.h>
#include <kernel/panic.h>
#include <kernel/spinlock.h>
#include <kernel/thread.h>
#include <malloc.h>
#include <printk.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <tee_api_types.h>
#include <trace.h>
#include <utee_defines.h>
#include <util.h>

#include "include/ipu_prv.h"
#include "include/ipu_regs.h"
#include "include/ipu-v3.h"
#include "include/ipu.h"

#include "ipu_ch_param.h"

#define IPU1_BASE 0x2400000
#define IPU1_SIZE 0x400000

register_phys_mem(MEM_AREA_IO_SEC, IPU1_BASE, IPU1_SIZE);

struct ipu_devtype {
	const char *name;
	unsigned long cm_ofs;
	unsigned long idmac_ofs;
	unsigned long ic_ofs;
	unsigned long csi0_ofs;
	unsigned long csi1_ofs;
	unsigned long di0_ofs;
	unsigned long di1_ofs;
	unsigned long smfc_ofs;
	unsigned long dc_ofs;
	unsigned long dmfc_ofs;
	unsigned long vdi_ofs;
	unsigned long cpmem_ofs;
	unsigned long srm_ofs;
	unsigned long tpm_ofs;
	unsigned long dc_tmpl_ofs;
	enum ipuv3_type type;
	bool idmac_used_bufs_present;
};

struct ipu_platform_type {
	struct ipu_devtype devtype;
};

static struct ipu_platform_type ipu_type_imx6q = {
	.devtype = {
		.name = "IPUv3H",
		.cm_ofs =	0x00200000,
		.idmac_ofs =	0x00208000,
		.ic_ofs =	0x00220000,
		.csi0_ofs =	0x00230000,
		.csi1_ofs =	0x00238000,
		.di0_ofs =	0x00240000,
		.di1_ofs =	0x00248000,
		.smfc_ofs =	0x00250000,
		.dc_ofs =	0x00258000,
		.dmfc_ofs =	0x00260000,
		.vdi_ofs =	0x00268000,
		.cpmem_ofs =	0x00300000,
		.srm_ofs =	0x00340000,
		.tpm_ofs =	0x00360000,
		.dc_tmpl_ofs =	0x00380000,
		.type =		IPUv3H,
		.idmac_used_bufs_present = true,
	},
};

struct ipu_soc ipu1; /* ipu@2400000 */

void ipu_dump_registers(struct ipu_soc *ipu)
{
	IMSG("Dumping IPU@%08x registers:", IPU1_BASE);
	IMSG("IPU_CONF = \t0x%08X\n", ipu_cm_read(ipu, IPU_CONF));
	IMSG("IDMAC_CONF = \t0x%08X\n", ipu_idmac_read(ipu, IDMAC_CONF));
	IMSG("IDMAC_CHA_EN1 = \t0x%08X\n",
	       ipu_idmac_read(ipu, IDMAC_CHA_EN(0)));
	IMSG("IDMAC_CHA_EN2 = \t0x%08X\n",
	       ipu_idmac_read(ipu, IDMAC_CHA_EN(32)));
	IMSG("IDMAC_CHA_PRI1 = \t0x%08X\n",
	       ipu_idmac_read(ipu, IDMAC_CHA_PRI(0)));
	IMSG("IDMAC_CHA_PRI2 = \t0x%08X\n",
	       ipu_idmac_read(ipu, IDMAC_CHA_PRI(32)));
	IMSG("IDMAC_BAND_EN1 = \t0x%08X\n",
	       ipu_idmac_read(ipu, IDMAC_BAND_EN(ipu->devtype, 0)));
	IMSG("IDMAC_BAND_EN2 = \t0x%08X\n",
	       ipu_idmac_read(ipu, IDMAC_BAND_EN(ipu->devtype, 32)));
	IMSG("IPU_CHA_DB_MODE_SEL0 = \t0x%08X\n",
	       ipu_cm_read(ipu, IPU_CHA_DB_MODE_SEL(0)));
	IMSG("IPU_CHA_DB_MODE_SEL1 = \t0x%08X\n",
	       ipu_cm_read(ipu, IPU_CHA_DB_MODE_SEL(32)));
	if (ipu->devtype >= IPUv3EX) {
		IMSG("IPU_CHA_TRB_MODE_SEL0 = \t0x%08X\n",
		       ipu_cm_read(ipu, IPU_CHA_TRB_MODE_SEL(ipu->devtype, 0)));
		IMSG("IPU_CHA_TRB_MODE_SEL1 = \t0x%08X\n",
		       ipu_cm_read(ipu,
				IPU_CHA_TRB_MODE_SEL(ipu->devtype, 32)));
	}
	IMSG("DMFC_WR_CHAN = \t0x%08X\n",
	       ipu_dmfc_read(ipu, DMFC_WR_CHAN));
	IMSG("DMFC_WR_CHAN_DEF = \t0x%08X\n",
	       ipu_dmfc_read(ipu, DMFC_WR_CHAN_DEF));
	IMSG("DMFC_DP_CHAN = \t0x%08X\n",
	       ipu_dmfc_read(ipu, DMFC_DP_CHAN));
	IMSG("DMFC_DP_CHAN_DEF = \t0x%08X\n",
	       ipu_dmfc_read(ipu, DMFC_DP_CHAN_DEF));
	IMSG("DMFC_IC_CTRL = \t0x%08X\n",
	       ipu_dmfc_read(ipu, DMFC_IC_CTRL));
	IMSG("IPU_FS_PROC_FLOW1 = \t0x%08X\n",
	       ipu_cm_read(ipu, IPU_FS_PROC_FLOW1));
	IMSG("IPU_FS_PROC_FLOW2 = \t0x%08X\n",
	       ipu_cm_read(ipu, IPU_FS_PROC_FLOW2));
	IMSG("IPU_FS_PROC_FLOW3 = \t0x%08X\n",
	       ipu_cm_read(ipu, IPU_FS_PROC_FLOW3));
	IMSG("IPU_FS_DISP_FLOW1 = \t0x%08X\n",
	       ipu_cm_read(ipu, IPU_FS_DISP_FLOW1));
	IMSG("IPU_VDIC_VDI_FSIZE = \t0x%08X\n",
	       ipu_vdi_read(ipu, VDI_FSIZE));
	IMSG("IPU_VDIC_VDI_C = \t0x%08X\n",
	       ipu_vdi_read(ipu, VDI_C));
	IMSG("IPU_IC_CONF = \t0x%08X\n",
	       ipu_ic_read(ipu, IC_CONF));
}

void fast_smc_ipu_dump_regs(void) {
	ipu_dump_registers(&ipu1);
}

#define PRP_ENC_CSI1 1

int32_t ipu_init_channel(struct ipu_soc *ipu, ipu_channel_t cha, ipu_channel_params_t *params)
{
	TEE_Result ret = 0;
	bool bad_pixfmt;
	uint32_t ipu_conf, reg;
	ipu_channel_t channel = CSI_PRP_ENC_MEM;

	IMSG("init channel = %d\n", IPU_CHAN_ID(channel));

	// ret = pm_runtime_get_sync(ipu->dev);
	// if (ret < 0) {
	// 	EMSG( "ch = %d, pm_runtime_get failed:%d!\n",
	// 			IPU_CHAN_ID(channel), ret);
	// 	dump_stack();
	// 	return ret;
	// }
	// /*
	//  * Here, ret could be 1 if the device's runtime PM status was
	//  * already 'active', so clear it to be 0.
	//  */
	// ret = 0;

	// _ipu_get(ipu);

	// mutex_lock(&ipu->mutex_lock);

	/* Re-enable error interrupts every time a channel is initialized */
	ipu_cm_write(ipu, 0xFFFFFFFF, IPU_INT_CTRL(5));
	ipu_cm_write(ipu, 0xFFFFFFFF, IPU_INT_CTRL(6));
	ipu_cm_write(ipu, 0xFFFFFFFF, IPU_INT_CTRL(9));
	ipu_cm_write(ipu, 0xFFFFFFFF, IPU_INT_CTRL(10));

	if (ipu->channel_init_mask & (1L << IPU_CHAN_ID(channel))) {
		IMSG("Warning: channel already initialized %d\n",
			IPU_CHAN_ID(channel));
	}

	ipu_conf = ipu_cm_read(ipu, IPU_CONF);

	if (channel == CSI_PRP_ENC_MEM) {
		// if (params->csi_prp_enc_mem.csi > 1) {
		// 	ret = TEE_ERROR_BAD_PARAMETERS;
		// 	goto err;
		// }
		// if ((ipu->using_ic_dirct_ch == MEM_VDI_PRP_VF_MEM) ||
		// 	(ipu->using_ic_dirct_ch == MEM_VDI_MEM)) {
		// 	ret = TEE_ERROR_BAD_PARAMETERS;
		// 	goto err;
		// }
		ipu->using_ic_dirct_ch = CSI_PRP_ENC_MEM;

		ipu->ic_use_count++;
		ipu->csi_channel[PRP_ENC_CSI1] = channel;

		IMSG("In %s(), csi: %d\n", __func__, 
			PRP_ENC_CSI1);

		if (1) { // params->csi_prp_enc_mem.mipi_en
			ipu_conf |= IPU_CONF_CSI1_DATA_SOURCE;
			_ipu_csi_set_mipi_di(ipu,
				IPU_CSI_MIPI_DI0, // params->csi_prp_enc_mem.mipi_vc,
				params->csi_prp_enc_mem.mipi_id,
				PRP_ENC_CSI1);
		} else {
			EMSG("UN-Support parallel CSI port.");
		}

		/*CSI0/1 feed into IC*/
		ipu_conf &= ~IPU_CONF_IC_INPUT; // IC input from CSI
		ipu_conf |= IPU_CONF_CSI_SEL; // select CSI1

		/*PRP skip buffer in memory, only valid when RWS_EN is true*/
		// reg = ipu_cm_read(ipu, IPU_FS_PROC_FLOW1);
		// ipu_cm_write(ipu, reg & ~FS_ENC_IN_VALID, IPU_FS_PROC_FLOW1);

		/*CSI data (include compander) dest*/
		_ipu_csi_init(ipu, channel, PRP_ENC_CSI1);
		_ipu_ic_init_prpenc(ipu, params, true);
	} else {
		EMSG("UN-Supported channel: optee only support CSI_PRP_ENC_MEM \
		      channel.");
	}

	ipu->channel_init_mask |= 1L << IPU_CHAN_ID(channel);

	ipu_cm_write(ipu, ipu_conf, IPU_CONF);

err:
	// mutex_unlock(&ipu->mutex_lock);
	return ret;
}

/**
 * IPU_INIT_CHANNEL
 * 
 * Initilize IPU CSI_PRP_ENC_MEM channel.
 * 
 * Parameter arrangement:
 * a1 - in_width
 * a2 - in_height
 * a3 - in_pixel_fmt
 * a4 - out_width
 * a5 - out_height
 * a6 - out_pixel_fmt
 * a7 - mipi_id
*/
uint32_t fast_smc_ipu_init_channel(struct thread_smc_args *args) {
	struct ipu_soc *ipu = &ipu1;
	ipu_channel_params_t params;

	params.mem_prp_enc_mem.in_width = args->a1;
	params.mem_prp_enc_mem.in_height = args->a2;
	params.mem_prp_enc_mem.in_pixel_fmt = args->a3;
	params.mem_prp_enc_mem.out_width = args->a4;
	params.mem_prp_enc_mem.out_height = args->a5;
	params.mem_prp_enc_mem.out_pixel_fmt = args->a6;

	params.csi_prp_enc_mem.mipi_id = args->a7;

	params.mem_prp_enc_mem.outv_resize_ratio = 0;
	params.mem_prp_enc_mem.outh_resize_ratio = 0;

	return ipu_init_channel(ipu, CSI_PRP_ENC_MEM, &params);
}

ipu_color_space_t format_to_colorspace(uint32_t fmt)
{
	switch (fmt) {
	case IPU_PIX_FMT_RGB666:
	case IPU_PIX_FMT_RGB565:
	case IPU_PIX_FMT_BGRA4444:
	case IPU_PIX_FMT_BGRA5551:
	case IPU_PIX_FMT_BGR24:
	case IPU_PIX_FMT_RGB24:
	case IPU_PIX_FMT_GBR24:
	case IPU_PIX_FMT_BGR32:
	case IPU_PIX_FMT_BGRA32:
	case IPU_PIX_FMT_RGB32:
	case IPU_PIX_FMT_RGBA32:
	case IPU_PIX_FMT_ABGR32:
	case IPU_PIX_FMT_LVDS666:
	case IPU_PIX_FMT_LVDS888:
	case IPU_PIX_FMT_GPU32_SB_ST:
	case IPU_PIX_FMT_GPU32_SB_SRT:
	case IPU_PIX_FMT_GPU32_ST:
	case IPU_PIX_FMT_GPU32_SRT:
	case IPU_PIX_FMT_GPU16_SB_ST:
	case IPU_PIX_FMT_GPU16_SB_SRT:
	case IPU_PIX_FMT_GPU16_ST:
	case IPU_PIX_FMT_GPU16_SRT:
		return RGB;
		break;

	default:
		return YCbCr;
		break;
	}
	return RGB;
}

uint32_t bytes_per_pixel(uint32_t fmt)
{
	switch (fmt) {
	case IPU_PIX_FMT_GENERIC:	/*generic data */
	case IPU_PIX_FMT_RGB332:
	case IPU_PIX_FMT_YUV420P:
	case IPU_PIX_FMT_YVU420P:
	case IPU_PIX_FMT_YUV422P:
	case IPU_PIX_FMT_YUV444P:
	case IPU_PIX_FMT_NV12:
	case PRE_PIX_FMT_NV21:
	case IPU_PIX_FMT_NV16:
	case PRE_PIX_FMT_NV61:
		return 1;
		break;
	case IPU_PIX_FMT_GENERIC_16:	/* generic data */
	case IPU_PIX_FMT_RGB565:
	case IPU_PIX_FMT_BGRA4444:
	case IPU_PIX_FMT_BGRA5551:
	case IPU_PIX_FMT_YUYV:
	case IPU_PIX_FMT_UYVY:
	case IPU_PIX_FMT_GPU16_SB_ST:
	case IPU_PIX_FMT_GPU16_SB_SRT:
	case IPU_PIX_FMT_GPU16_ST:
	case IPU_PIX_FMT_GPU16_SRT:
		return 2;
		break;
	case IPU_PIX_FMT_BGR24:
	case IPU_PIX_FMT_RGB24:
	case IPU_PIX_FMT_YUV444:
		return 3;
		break;
	case IPU_PIX_FMT_GENERIC_32:	/*generic data */
	case IPU_PIX_FMT_BGR32:
	case IPU_PIX_FMT_BGRA32:
	case IPU_PIX_FMT_RGB32:
	case IPU_PIX_FMT_RGBA32:
	case IPU_PIX_FMT_ABGR32:
	case IPU_PIX_FMT_GPU32_SB_ST:
	case IPU_PIX_FMT_GPU32_SB_SRT:
	case IPU_PIX_FMT_GPU32_ST:
	case IPU_PIX_FMT_GPU32_SRT:
	case IPU_PIX_FMT_AYUV:
		return 4;
		break;
	default:
		return 1;
		break;
	}
	return 0;
}

static inline uint32_t channel_2_dma(ipu_channel_t ch, ipu_buffer_t type)
{
	return ((uint32_t) ch >> (6 * type)) & 0x3F;
};

static inline int _ipu_is_ic_chan(uint32_t dma_chan)
{
	return (((dma_chan >= 11) && (dma_chan <= 22) && (dma_chan != 17) &&
		(dma_chan != 18)));
}

#define idma_is_valid(ch)	((ch) != NO_DMA)
#define idma_mask(ch)		(idma_is_valid(ch) ? (1UL << ((ch) & 0x1F)) : 0)

static inline bool idma_is_set(struct ipu_soc *ipu, uint32_t reg, uint32_t dma)
{
	return !!(ipu_idmac_read(ipu, reg) & idma_mask(dma));
}

int32_t ipu_init_channel_buffer(struct ipu_soc *ipu,
				uint32_t pixel_fmt,
				uint16_t width, uint16_t height,
				uint32_t stride,
				dma_addr_t phyaddr_0, dma_addr_t phyaddr_1)
{
	uint32_t reg;
	uint32_t dma_chan;
	uint32_t burst_size;

	ipu_rotate_mode_t rot_mode = IPU_ROTATE_NONE;
	ipu_channel_t channel = CSI_PRP_ENC_MEM;
	ipu_buffer_t type = IPU_OUTPUT_BUFFER;
	dma_addr_t phyaddr_2 = 0;
	uint32_t u = 0;
	uint32_t v = 0;

	DMSG("width: %d\n", width);
	DMSG("height: %d\n", height);
	DMSG("stride: %d\n", stride);
	DMSG("pa0: 0x%x\n", phyaddr_0);
	DMSG("pa1: 0x%x\n", phyaddr_1);

	dma_chan = channel_2_dma(channel, type);
	if (!idma_is_valid(dma_chan))
		return TEE_ERROR_BAD_PARAMETERS;
	DMSG("dma channel: %d\n", dma_chan);

	if (stride < width * bytes_per_pixel(pixel_fmt))
		stride = width * bytes_per_pixel(pixel_fmt);

	if (stride % 4) {
		EMSG("Stride not 32-bit aligned, stride = %d\n", stride);
		return TEE_ERROR_BAD_PARAMETERS;
	}
	/* IC channel's width must be multiple of 8 pixels */
	if (_ipu_is_ic_chan(dma_chan) && (width % 8)) {
		EMSG("Width must be 8 pixel multiple\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	// if (_ipu_is_vdi_out_chan(dma_chan) &&
	// 	((width < 16) || (height < 16) || (width % 2) || (height % 4))) {
	// 	EMSG( "vdi width/height limited err\n");
	// 	return TEE_ERROR_BAD_PARAMETERS;
	// }

	// /* IPUv3EX and IPUv3M support triple buffer */
	// if ((!_ipu_is_trb_chan(ipu, dma_chan)) && phyaddr_2) {
	// 	EMSG( "Chan%d doesn't support triple buffer "
	// 			   "mode\n", dma_chan);
	// 	return TEE_ERROR_BAD_PARAMETERS;
	// }
	// if (!phyaddr_1 && phyaddr_2) {
	// 	EMSG( "Chan%d's buf1 physical addr is NULL for "
	// 			   "triple buffer mode\n", dma_chan);
	// 	return TEE_ERROR_BAD_PARAMETERS;
	// }

	// mutex_lock(&ipu->mutex_lock);

	/* Build parameter memory data for DMA channel */
	_ipu_ch_param_init(ipu, dma_chan, pixel_fmt, width, height, stride, u, v, 0,
			   phyaddr_0, phyaddr_1, phyaddr_2);
	
	DMSG("IPU cp initialized successfully.");

	// /* Set correlative channel parameter of local alpha channel */
	// if ((_ipu_is_ic_graphic_chan(dma_chan) ||
	//      _ipu_is_dp_graphic_chan(dma_chan)) &&
	//     (ipu->thrd_chan_en[IPU_CHAN_ID(channel)] == true)) { // false
	// 	_ipu_ch_param_set_alpha_use_separate_channel(ipu, dma_chan, true);
	// 	_ipu_ch_param_set_alpha_buffer_memory(ipu, dma_chan);
	// 	_ipu_ch_param_set_alpha_condition_read(ipu, dma_chan);
	// 	/* fix alpha width as 8 and burst size as 16*/
	// 	_ipu_ch_params_set_alpha_width(ipu, dma_chan, 8);
	// 	_ipu_ch_param_set_burst_size(ipu, dma_chan, 16);
	// } else if (_ipu_is_ic_graphic_chan(dma_chan) &&
	// 	   ipu_pixel_format_has_alpha(pixel_fmt))
	// 	_ipu_ch_param_set_alpha_use_separate_channel(ipu, dma_chan, false);

	// if (rot_mode) // false
	// 	_ipu_ch_param_set_rotation(ipu, dma_chan, rot_mode);

	/* IC and ROT channels have restriction of 8 or 16 pix burst length */
	if (_ipu_is_ic_chan(dma_chan)) { // true
		if ((width % 16) == 0) /* TBD */
			_ipu_ch_param_set_burst_size(ipu, dma_chan, 16);
		else
			_ipu_ch_param_set_burst_size(ipu, dma_chan, 8);
	}
	DMSG("IPU cp burst size set successfully.");
	// } else if (_ipu_is_irt_chan(dma_chan)) {
	// 	_ipu_ch_param_set_burst_size(ipu, dma_chan, 8);
	// 	_ipu_ch_param_set_block_mode(ipu, dma_chan);
	// } else if (_ipu_is_dmfc_chan(dma_chan)) {
	// 	burst_size = _ipu_ch_param_get_burst_size(ipu, dma_chan);
	// 	_ipu_dmfc_set_wait4eot(ipu, dma_chan, width);
	// 	_ipu_dmfc_set_burst_size(ipu, dma_chan, burst_size);
	// }

	// if (_ipu_disp_chan_is_interlaced(ipu, channel) ||
	// 	ipu->chan_is_interlaced[dma_chan])
	// 	_ipu_ch_param_set_interlaced_scan(ipu, dma_chan);

	if (_ipu_is_ic_chan(dma_chan)) { // true
		burst_size = _ipu_ch_param_get_burst_size(ipu, dma_chan);
		_ipu_ic_idma_init(ipu, dma_chan, width, height, burst_size,
			rot_mode); //
	}
	DMSG("IPU ic idma initialized successfully.");
	// } else if (_ipu_is_smfc_chan(dma_chan)) {
	// 	burst_size = _ipu_ch_param_get_burst_size(ipu, dma_chan);
	// 	/*
	// 	 * This is different from IPUv3 spec, but it is confirmed
	// 	 * in IPUforum that SMFC burst size should be NPB[6:3]
	// 	 * when IDMAC works in 16-bit generic data mode.
	// 	 */
	// 	if (pixel_fmt == IPU_PIX_FMT_GENERIC)
	// 		/* 8 bits per pixel */
	// 		burst_size = burst_size >> 4;
	// 	else if (pixel_fmt == IPU_PIX_FMT_GENERIC_16)
	// 		/* 16 bits per pixel */
	// 		burst_size = burst_size >> 3;
	// 	else
	// 		burst_size = burst_size >> 2;
	// 	_ipu_smfc_set_burst_size(ipu, channel, burst_size-1);
	// }

	switch (dma_chan) {
	// case 0:
	// case 1:
	// case 2:
	// case 3:
	// 	_ipu_ch_param_set_axi_id(ipu, dma_chan, ipu->ch0123_axi);
	// 	break;
	// case 23:
	// 	_ipu_ch_param_set_axi_id(ipu, dma_chan, ipu->ch23_axi);
	// 	break;
	// case 27:
	// 	_ipu_ch_param_set_axi_id(ipu, dma_chan, ipu->ch27_axi);
	// 	break;
	// case 28:
	// 	_ipu_ch_param_set_axi_id(ipu, dma_chan, ipu->ch28_axi);
	// 	break;
	default:
		_ipu_ch_param_set_axi_id(ipu, dma_chan, ipu->normal_axi);
		break;
	}
	DMSG("IPU cp axi id set successfully.");

	if (idma_is_set(ipu, IDMAC_CHA_PRI(dma_chan), dma_chan) &&
	    ipu->devtype == IPUv3H) { // IPUv3H for imx6q
		uint32_t reg = IDMAC_CH_LOCK_EN_1(ipu->devtype);
		uint32_t value = 0;

		if (dma_chan != 20) {
			EMSG("ERROR: dma channel is not 20.");
		}

		value = 0x3 << 10;
		value |= ipu_idmac_read(ipu, reg);
		ipu_idmac_write(ipu, value, reg);
	}

	_ipu_ch_param_dump(ipu, dma_chan);

	reg = ipu_cm_read(ipu,
			IPU_CHA_TRB_MODE_SEL(ipu->devtype, dma_chan));
	reg &= ~idma_mask(dma_chan);
	ipu_cm_write(ipu, reg,
			IPU_CHA_TRB_MODE_SEL(ipu->devtype, dma_chan));

	reg = ipu_cm_read(ipu, IPU_CHA_DB_MODE_SEL(dma_chan));
	if (phyaddr_1)
		reg |= idma_mask(dma_chan);
	else
		EMSG("ERROR: NULL phyaddr_1");
	ipu_cm_write(ipu, reg, IPU_CHA_DB_MODE_SEL(dma_chan));

	/* Reset to buffer 0 */
	ipu_cm_write(ipu, idma_mask(dma_chan),
			IPU_CHA_CUR_BUF(ipu->devtype, dma_chan));

	// mutex_unlock(&ipu->mutex_lock);

	return 0;
}

/**
 * IPU_INIT_CHANNEL_BUFFER
 * 
 * Initilize IPU CSI_PRP_ENC_MEM channel buffer.
 * 
 * Parameter arrangement:
 * a1 - pixel_fmt
 * a2 - width
 * a3 - height
 * a4 - stride
 * a5 - phyaddr_0
 * a6 - phyaddr_1
*/
uint32_t fast_smc_ipu_init_channel_buffer(struct thread_smc_args *args) {
	struct ipu_soc *ipu = &ipu1;

	return ipu_init_channel_buffer(ipu, args->a1, args->a2, args->a3,
				       args->a4, args->a5, args->a6);
}

int32_t ipu_enable_channel(struct ipu_soc *ipu, ipu_channel_t cha)
{
	uint32_t reg;
	uint32_t ipu_conf;
	uint32_t out_dma;

	ipu_channel_t channel = CSI_PRP_ENC_MEM;

	// mutex_lock(&ipu->mutex_lock);

	// if (ipu->channel_enable_mask & (1L << IPU_CHAN_ID(channel))) {
	// 	EMSG( "Warning: channel already enabled %d\n",
	// 		IPU_CHAN_ID(channel));
	// 	// mutex_unlock(&ipu->mutex_lock);
	// 	return TEE_ERROR_BAD_STATE;
	// }

	/* Get output dma channels */
	out_dma = channel_2_dma(channel, IPU_OUTPUT_BUFFER);

	ipu_conf = ipu_cm_read(ipu, IPU_CONF);
	if (ipu->ic_use_count > 0)
		ipu_conf |= IPU_CONF_IC_EN;

	ipu_cm_write(ipu, ipu_conf, IPU_CONF);

	if (idma_is_valid(out_dma)) {
		reg = ipu_idmac_read(ipu, IDMAC_CHA_EN(out_dma));
		ipu_idmac_write(ipu, reg | idma_mask(out_dma), IDMAC_CHA_EN(out_dma));
	}

	_ipu_ic_enable_task(ipu, channel);
	ipu->channel_enable_mask |= 1L << IPU_CHAN_ID(channel);

	// mutex_unlock(&ipu->mutex_lock);

	return 0;
}

/**
 * IPU_ENABLE_CHANNEL
 * 
 * Enable IPU CSI_PRP_ENC_MEM channel from the SW
*/
uint32_t fast_smc_ipu_enable_channel(struct thread_smc_args *args) {
	struct ipu_soc *ipu = &ipu1;

	return ipu_enable_channel(ipu, CSI_PRP_ENC_MEM);
}

int32_t ipu_update_channel_buffer(struct ipu_soc *ipu, uint32_t bufNum, dma_addr_t phyaddr)
{
	ipu_channel_t channel = CSI_PRP_ENC_MEM;
	ipu_buffer_t type = IPU_OUTPUT_BUFFER;

	int ret = 0;
	uint32_t dma_chan = channel_2_dma(channel, type);
	uint32_t reg = idma_mask(dma_chan);

	DMSG("dma_chan: %d\n", dma_chan);

	if (dma_chan == IDMA_CHAN_INVALID)
		return TEE_ERROR_BAD_PARAMETERS;

	// spin_lock_irqsave(&ipu->rdy_reg_spin_lock, lock_flags);
	if (bufNum == 0)
		reg = ipu_cm_read(ipu,
				IPU_CHA_BUF0_RDY(ipu->devtype, dma_chan));
	else if (bufNum == 1)
		reg = ipu_cm_read(ipu,
				IPU_CHA_BUF1_RDY(ipu->devtype, dma_chan));
	else
		EMSG("ERROR: only support double buffering.");

	if ((reg & idma_mask(dma_chan)) == 0) /* buffer is not ready */
		_ipu_ch_param_set_buffer(ipu, dma_chan, bufNum, phyaddr);
	else {/* the buffer cannot be ready when updating the buffer */
		EMSG("Error: trying to update an initialized buffer");
		ret = TEE_ERROR_BAD_STATE;
	}
	// spin_unlock_irqrestore(&ipu->rdy_reg_spin_lock, lock_flags);

	return ret;
}

/**
 * IPU_UPDATE_CHANNEL_BUFFER
 * 
 * Update IPU CSI_PRP_ENC_MEM channel buffer.
 * 
 * Parameter arrangement:
 * a1 - bufNum
 * a2 - phyaddr
*/
uint32_t fast_smc_ipu_update_channel_buffer(struct thread_smc_args *args) {
	struct ipu_soc *ipu = &ipu1;

	return ipu_update_channel_buffer(ipu, args->a1, args->a2);
}

int32_t ipu_select_buffer(struct ipu_soc *ipu, uint32_t bufNum)
{
	ipu_channel_t channel = CSI_PRP_ENC_MEM;
	ipu_buffer_t type = IPU_OUTPUT_BUFFER;

	uint32_t *ipu_cha_buf0_rdy;
	uint32_t *ipu_cha_buf1_rdy;

	uint32_t reg = 0;
	uint32_t dma_chan = channel_2_dma(channel, type);

	DMSG("dma channel: %d, bufNum: %d", dma_chan, bufNum);

	if (dma_chan == IDMA_CHAN_INVALID)
		return TEE_ERROR_BAD_PARAMETERS;

	// spin_lock_irqsave(&ipu->rdy_reg_spin_lock, lock_flags);

	// if (bufNum == 0)
	// 	reg = ipu_cm_read(ipu,
	// 			IPU_CHA_BUF0_RDY(ipu->devtype, dma_chan));
	// else if (bufNum == 1)
	// 	reg = ipu_cm_read(ipu,
	// 			IPU_CHA_BUF1_RDY(ipu->devtype, dma_chan));
	// if ((reg & idma_mask(dma_chan)) != 0) /* buffer is ready */
	// 	EMSG("ERROR: BUF%d ready before set.", bufNum);

	/* Mark buffer to be ready. */
	if (bufNum == 0) {
		ipu_cm_write(ipu, reg | idma_mask(dma_chan),
			     IPU_CHA_BUF0_RDY(ipu->devtype, dma_chan));
		// ipu_cha_buf0_rdy = ipu->cm_reg + IPU_CHA_BUF0_RDY(ipu->devtype, dma_chan);
		// DMSG("ipu->cm_reg: 0x%08x", ipu->cm_reg);
		// DMSG("ipu_cha_buf0_rdy: 0x%08x", ipu_cha_buf0_rdy);
		// *ipu_cha_buf0_rdy = reg | idma_mask(dma_chan);
	} else if (bufNum == 1) {
		ipu_cm_write(ipu, reg | idma_mask(dma_chan),
			     IPU_CHA_BUF1_RDY(ipu->devtype, dma_chan));
		// ipu_cha_buf1_rdy = ipu->cm_reg + IPU_CHA_BUF1_RDY(ipu->devtype, dma_chan);
		// DMSG("ipu->cm_reg: 0x%08x", ipu->cm_reg);
		// DMSG("ipu_cha_buf1_rdy: 0x%08x", ipu_cha_buf1_rdy);
		// *ipu_cha_buf1_rdy = reg | idma_mask(dma_chan);
	} else
		EMSG("ERROR: only support double buffering.");

	// /* make sure the buffer is ready */
	// if (bufNum == 0)
	// 	reg = ipu_cha_buf0_rdy;
	// else if (bufNum == 1)
	// 	reg = ipu_cha_buf1_rdy;

	// if ((reg & idma_mask(dma_chan)) == 0) { /* buffer is not ready */
	// 	EMSG("ERROR: BUF%d set ready fails.", bufNum);
	// 	return TEE_ERROR_BAD_STATE;
	// }

	// spin_unlock_irqrestore(&ipu->rdy_reg_spin_lock, lock_flags);

	return 0;
}

/**
 * IPU_SELECT_BUFFER
 * 
 * Select IPU CSI_PRP_ENC_MEM channel buffer number 0 or 1.
 * 
 * Parameter arrangement:
 * a1 - bufNum
*/
uint32_t fast_smc_ipu_select_buffer(struct thread_smc_args *args) {
	struct ipu_soc *ipu = &ipu1;

	return ipu_select_buffer(ipu, args->a1);
}

int32_t ipu_enable_csi(struct ipu_soc *ipu)
{
	uint32_t reg;
	uint32_t csi = 1;

	DMSG("enable csi: %d\n", csi);

	// _ipu_get(ipu);
	// mutex_lock(&ipu->mutex_lock);
	ipu->csi_use_count[csi]++;

	// if (ipu->csi_use_count[csi] == 1) {
		reg = ipu_cm_read(ipu, IPU_CONF);
		if (csi == 0)
			ipu_cm_write(ipu, reg | IPU_CONF_CSI0_EN, IPU_CONF);
		else
			ipu_cm_write(ipu, reg | IPU_CONF_CSI1_EN, IPU_CONF);
	// }
	// mutex_unlock(&ipu->mutex_lock);
	// _ipu_put(ipu);

	return 0;
}

/**
 * IPU_ENABLE_CSI
 * 
 * Enable IPU CSI1 interface from the SW
 */
uint32_t fast_smc_ipu_enable_csi(struct thread_smc_args *args) {
	struct ipu_soc *ipu = &ipu1;

	return ipu_enable_csi(ipu);
}

int32_t ipu_disable_csi(struct ipu_soc *ipu, uint32_t csi_port)
{
	uint32_t reg;
	uint32_t csi = 1;

	// if (csi > 1) {
	// 	dev_err(ipu->dev, "Wrong csi num_%d\n", csi);
	// 	return -EINVAL;
	// }
	// _ipu_get(ipu);
	// mutex_lock(&ipu->mutex_lock);
	// ipu->csi_use_count[csi]--;
	// if (ipu->csi_use_count[csi] == 0) {
	// _ipu_csi_wait4eof(ipu, ipu->csi_channel[csi]);
	reg = ipu_cm_read(ipu, IPU_CONF);
	if (csi == 0)
		ipu_cm_write(ipu, reg & ~IPU_CONF_CSI0_EN, IPU_CONF);
	else
		ipu_cm_write(ipu, reg & ~IPU_CONF_CSI1_EN, IPU_CONF);
	// }
	// mutex_unlock(&ipu->mutex_lock);
	// _ipu_put(ipu);
	return 0;
}

/**
 * IPU_DISABLE_CSI
 * 
 * Disable IPU CSI1 interface from the SW
 */
uint32_t fast_smc_ipu_disable_csi(struct thread_smc_args *args) {
	struct ipu_soc *ipu = &ipu1;

	return ipu_disable_csi(ipu, 1);
}

void _ipu_clear_buffer_ready(struct ipu_soc *ipu, ipu_channel_t channel, ipu_buffer_t type,
		uint32_t bufNum)
{
	uint32_t dma_ch = channel_2_dma(channel, type);

	if (!idma_is_valid(dma_ch))
		return;

	ipu_cm_write(ipu, 0xF0300000, IPU_GPR); /* write one to clear */
	if (bufNum == 0)
		ipu_cm_write(ipu, idma_mask(dma_ch),
				IPU_CHA_BUF0_RDY(ipu->devtype, dma_ch));
	else if (bufNum == 1)
		ipu_cm_write(ipu, idma_mask(dma_ch),
				IPU_CHA_BUF1_RDY(ipu->devtype, dma_ch));
	// else
	// 	ipu_cm_write(ipu, idma_mask(dma_ch),
	// 			IPU_CHA_BUF2_RDY(ipu->devtype, dma_ch));
	ipu_cm_write(ipu, 0x0, IPU_GPR); /* write one to set */
}

int32_t ipu_disable_channel(struct ipu_soc *ipu, ipu_channel_t cha, bool wait_for_stop)
{
	uint32_t reg;
	// uint32_t in_dma;
	uint32_t out_dma;
	uint32_t sec_dma = NO_DMA;
	uint32_t thrd_dma = NO_DMA;
	// uint16_t fg_pos_x, fg_pos_y;
	unsigned long lock_flags;
	ipu_channel_t channel = CSI_PRP_ENC_MEM;

	// mutex_lock(&ipu->mutex_lock);

	// if ((ipu->channel_enable_mask & (1L << IPU_CHAN_ID(channel))) == 0) {
	// 	dev_dbg(ipu->dev, "Channel already disabled %d\n",
	// 		IPU_CHAN_ID(channel));
	// 	mutex_unlock(&ipu->mutex_lock);
	// 	return -EACCES;
	// }

	/* Get input and output dma channels */
	out_dma = channel_2_dma(channel, IPU_OUTPUT_BUFFER);
	// in_dma = channel_2_dma(channel, IPU_VIDEO_IN_BUFFER);

	if (
		// idma_is_valid(in_dma) &&
		// !idma_is_set(ipu, IDMAC_CHA_EN(in_dma), in_dma))
		// && 
		(idma_is_valid(out_dma) &&
		!idma_is_set(ipu, IDMAC_CHA_EN(out_dma), out_dma))) {
		// mutex_unlock(&ipu->mutex_lock);
		return TEE_ERROR_BAD_STATE;
	}

	// if (ipu->sec_chan_en[IPU_CHAN_ID(channel)])
	// 	sec_dma = channel_2_dma(channel, IPU_GRAPH_IN_BUFFER);
	// if (ipu->thrd_chan_en[IPU_CHAN_ID(channel)]) {
	// 	sec_dma = channel_2_dma(channel, IPU_GRAPH_IN_BUFFER);
	// 	thrd_dma = channel_2_dma(channel, IPU_ALPHA_IN_BUFFER);
	// }

	// if ((channel == MEM_BG_SYNC) || (channel == MEM_FG_SYNC) ||
	//     (channel == MEM_DC_SYNC)) {
	// 	if (channel == MEM_FG_SYNC) {
	// 		_ipu_disp_get_window_pos(ipu, channel, &fg_pos_x, &fg_pos_y);
	// 		_ipu_disp_set_window_pos(ipu, channel, 0, 0);
	// 	}

	// 	_ipu_dp_dc_disable(ipu, channel, false);

	// 	/*
	// 	 * wait for BG channel EOF then disable FG-IDMAC,
	// 	 * it avoid FG NFB4EOF error.
	// 	 */
	// 	if ((channel == MEM_FG_SYNC) && (ipu_is_channel_busy(ipu, MEM_BG_SYNC))) {
	// 		int timeout = 50;

	// 		ipu_cm_write(ipu, IPUIRQ_2_MASK(IPU_IRQ_BG_SYNC_EOF),
	// 				IPUIRQ_2_STATREG(ipu->devtype,
	// 						IPU_IRQ_BG_SYNC_EOF));
	// 		while ((ipu_cm_read(ipu,
	// 			IPUIRQ_2_STATREG(ipu->devtype,
	// 			IPU_IRQ_BG_SYNC_EOF)) &
	// 			IPUIRQ_2_MASK(IPU_IRQ_BG_SYNC_EOF)) == 0) {
	// 			msleep(10);
	// 			timeout -= 10;
	// 			if (timeout <= 0) {
	// 				dev_err(ipu->dev, "warning: wait for bg sync eof timeout\n");
	// 				break;
	// 			}
	// 		}
	// 	}
	// } else if (wait_for_stop && !_ipu_is_smfc_chan(out_dma) &&
	// 	   channel != CSI_PRP_VF_MEM && channel != CSI_PRP_ENC_MEM) {
	// 	while (idma_is_set(ipu, IDMAC_CHA_BUSY(ipu->devtype, in_dma),
	// 				in_dma) ||
	// 	       idma_is_set(ipu, IDMAC_CHA_BUSY(ipu->devtype, out_dma),
	// 				out_dma) ||
	// 		(ipu->sec_chan_en[IPU_CHAN_ID(channel)] &&
	// 		idma_is_set(ipu, IDMAC_CHA_BUSY(ipu->devtype, sec_dma),
	// 				sec_dma)) ||
	// 		(ipu->thrd_chan_en[IPU_CHAN_ID(channel)] &&
	// 		idma_is_set(ipu, IDMAC_CHA_BUSY(ipu->devtype, thrd_dma),
	// 				thrd_dma))) {
	// 		uint32_t irq = 0xffffffff;
	// 		int timeout = 50000;

	// 		if (idma_is_set(ipu, IDMAC_CHA_BUSY(ipu->devtype,
	// 				out_dma), out_dma))
	// 			irq = out_dma;
	// 		if (ipu->sec_chan_en[IPU_CHAN_ID(channel)] &&
	// 			idma_is_set(ipu, IDMAC_CHA_BUSY(ipu->devtype,
	// 					sec_dma), sec_dma))
	// 			irq = sec_dma;
	// 		if (ipu->thrd_chan_en[IPU_CHAN_ID(channel)] &&
	// 			idma_is_set(ipu, IDMAC_CHA_BUSY(ipu->devtype,
	// 					thrd_dma), thrd_dma))
	// 			irq = thrd_dma;
	// 		if (idma_is_set(ipu, IDMAC_CHA_BUSY(ipu->devtype,
	// 					in_dma), in_dma))
	// 			irq = in_dma;

	// 		if (irq == 0xffffffff) {
	// 			dev_dbg(ipu->dev, "warning: no channel busy, break\n");
	// 			break;
	// 		}

	// 		ipu_cm_write(ipu, IPUIRQ_2_MASK(irq),
	// 				IPUIRQ_2_STATREG(ipu->devtype, irq));

	// 		dev_dbg(ipu->dev, "warning: channel %d busy, need wait\n", irq);

	// 		while (((ipu_cm_read(ipu,
	// 				IPUIRQ_2_STATREG(ipu->devtype, irq))
	// 			& IPUIRQ_2_MASK(irq)) == 0) &&
	// 			(idma_is_set(ipu, IDMAC_CHA_BUSY(ipu->devtype,
	// 						irq), irq))) {
	// 			udelay(10);
	// 			timeout -= 10;
	// 			if (timeout <= 0) {
	// 				ipu_dump_registers(ipu);
	// 				dev_err(ipu->dev, "warning: disable ipu dma channel %d during its busy state\n", irq);
	// 				break;
	// 			}
	// 		}
	// 		dev_dbg(ipu->dev, "wait_time:%d\n", 50000 - timeout);

	// 	}
	// }

	// if ((channel == MEM_BG_SYNC) || (channel == MEM_FG_SYNC) ||
	//     (channel == MEM_DC_SYNC)) {
	// 	reg = ipu_idmac_read(ipu, IDMAC_WM_EN(in_dma));
	// 	ipu_idmac_write(ipu, reg & ~idma_mask(in_dma), IDMAC_WM_EN(in_dma));
	// }

	/* Disable IC task */
	if (_ipu_is_ic_chan(out_dma))
		// _ipu_is_ic_chan(in_dma) ||
		// _ipu_is_irt_chan(in_dma) || _ipu_is_irt_chan(out_dma) ||
		// _ipu_is_vdi_out_chan(out_dma))
		_ipu_ic_disable_task(ipu, channel);

	/* Disable DMA channel(s) */
	// if (idma_is_valid(in_dma)) {
	// 	reg = ipu_idmac_read(ipu, IDMAC_CHA_EN(in_dma));
	// 	ipu_idmac_write(ipu, reg & ~idma_mask(in_dma), IDMAC_CHA_EN(in_dma));
	// 	ipu_cm_write(ipu, idma_mask(in_dma),
	// 			IPU_CHA_CUR_BUF(ipu->devtype, in_dma));
	// 	ipu_cm_write(ipu, tri_cur_buf_mask(in_dma),
	// 			IPU_CHA_TRIPLE_CUR_BUF(ipu->devtype, in_dma));
	// }
	if (idma_is_valid(out_dma)) {
		reg = ipu_idmac_read(ipu, IDMAC_CHA_EN(out_dma));
		ipu_idmac_write(ipu, reg & ~idma_mask(out_dma), IDMAC_CHA_EN(out_dma));
		ipu_cm_write(ipu, idma_mask(out_dma),
				IPU_CHA_CUR_BUF(ipu->devtype, out_dma));
		// ipu_cm_write(ipu, tri_cur_buf_mask(out_dma),
		// 		IPU_CHA_TRIPLE_CUR_BUF(ipu->devtype, out_dma));
	}
	// if (ipu->sec_chan_en[IPU_CHAN_ID(channel)] && idma_is_valid(sec_dma)) {
	// 	reg = ipu_idmac_read(ipu, IDMAC_CHA_EN(sec_dma));
	// 	ipu_idmac_write(ipu, reg & ~idma_mask(sec_dma), IDMAC_CHA_EN(sec_dma));
	// 	ipu_cm_write(ipu, idma_mask(sec_dma),
	// 			IPU_CHA_CUR_BUF(ipu->devtype, sec_dma));
	// }
	// if (ipu->thrd_chan_en[IPU_CHAN_ID(channel)] && idma_is_valid(thrd_dma)) {
	// 	reg = ipu_idmac_read(ipu, IDMAC_CHA_EN(thrd_dma));
	// 	ipu_idmac_write(ipu, reg & ~idma_mask(thrd_dma), IDMAC_CHA_EN(thrd_dma));
	// 	if (channel == MEM_BG_SYNC || channel == MEM_FG_SYNC) {
	// 		reg = ipu_idmac_read(ipu, IDMAC_SEP_ALPHA);
	// 		ipu_idmac_write(ipu, reg & ~idma_mask(in_dma), IDMAC_SEP_ALPHA);
	// 	} else {
	// 		reg = ipu_idmac_read(ipu, IDMAC_SEP_ALPHA);
	// 		ipu_idmac_write(ipu, reg & ~idma_mask(sec_dma), IDMAC_SEP_ALPHA);
	// 	}
	// 	ipu_cm_write(ipu, idma_mask(thrd_dma),
	// 			IPU_CHA_CUR_BUF(ipu->devtype, thrd_dma));
	// }

	// if (channel == MEM_FG_SYNC)
	// 	_ipu_disp_set_window_pos(ipu, channel, fg_pos_x, fg_pos_y);

	// spin_lock_irqsave(&ipu->rdy_reg_spin_lock, lock_flags);
	/* Set channel buffers NOT to be ready */
	// if (idma_is_valid(in_dma)) {
	// 	_ipu_clear_buffer_ready(ipu, channel, IPU_VIDEO_IN_BUFFER, 0);
	// 	_ipu_clear_buffer_ready(ipu, channel, IPU_VIDEO_IN_BUFFER, 1);
	// 	_ipu_clear_buffer_ready(ipu, channel, IPU_VIDEO_IN_BUFFER, 2);
	// }
	if (idma_is_valid(out_dma)) {
		_ipu_clear_buffer_ready(ipu, channel, IPU_OUTPUT_BUFFER, 0);
		_ipu_clear_buffer_ready(ipu, channel, IPU_OUTPUT_BUFFER, 1);
	}
	// if (ipu->sec_chan_en[IPU_CHAN_ID(channel)] && idma_is_valid(sec_dma)) {
	// 	_ipu_clear_buffer_ready(ipu, channel, IPU_GRAPH_IN_BUFFER, 0);
	// 	_ipu_clear_buffer_ready(ipu, channel, IPU_GRAPH_IN_BUFFER, 1);
	// }
	// if (ipu->thrd_chan_en[IPU_CHAN_ID(channel)] && idma_is_valid(thrd_dma)) {
	// 	_ipu_clear_buffer_ready(ipu, channel, IPU_ALPHA_IN_BUFFER, 0);
	// 	_ipu_clear_buffer_ready(ipu, channel, IPU_ALPHA_IN_BUFFER, 1);
	// }
	// spin_unlock_irqrestore(&ipu->rdy_reg_spin_lock, lock_flags);

	ipu->channel_enable_mask &= ~(1L << IPU_CHAN_ID(channel));

	// if (ipu->prg_clk)
	// 	clk_disable_unprepare(ipu->prg_clk);

	// mutex_unlock(&ipu->mutex_lock);

	return 0;
}

/**
 * IPU_DISABLE_CHANNEL
 * 
 * Disable IPU CSI_PRP_ENC_MEM channel.
 * 
 * Parameter arrangement:
*/
uint32_t fast_smc_ipu_disable_channel(struct thread_smc_args *args) {
	struct ipu_soc *ipu = &ipu1;

	ipu_disable_channel(ipu, CSI_PRP_ENC_MEM, false);

	return 0;
}


void ipu_uninit_channel(struct ipu_soc *ipu, ipu_channel_t cha)
{
	uint32_t reg;
	uint32_t in_dma, out_dma = 0;
	uint32_t ipu_conf;
	uint32_t dc_chan = 0;
	int ret;
	ipu_channel_t channel = CSI_PRP_ENC_MEM;

	// mutex_lock(&ipu->mutex_lock);

	if ((ipu->channel_init_mask & (1L << IPU_CHAN_ID(channel))) == 0) {
		DMSG("Channel already uninitialized %d\n",
			IPU_CHAN_ID(channel));
		// mutex_unlock(&ipu->mutex_lock);
		return;
	}

	/* Make sure channel is disabled */
	/* Get input and output dma channels */
	in_dma = channel_2_dma(channel, IPU_VIDEO_IN_BUFFER);
	out_dma = channel_2_dma(channel, IPU_OUTPUT_BUFFER);

	if (idma_is_set(ipu, IDMAC_CHA_EN(in_dma), in_dma) ||
	    idma_is_set(ipu, IDMAC_CHA_EN(out_dma), out_dma)) {
		EMSG("Channel %d is not disabled, disable first\n",
			IPU_CHAN_ID(channel));
		// mutex_unlock(&ipu->mutex_lock);
		return;
	}

	ipu_conf = ipu_cm_read(ipu, IPU_CONF);

	/* Reset the double buffer */
	reg = ipu_cm_read(ipu, IPU_CHA_DB_MODE_SEL(in_dma));
	ipu_cm_write(ipu, reg & ~idma_mask(in_dma), IPU_CHA_DB_MODE_SEL(in_dma));
	reg = ipu_cm_read(ipu, IPU_CHA_DB_MODE_SEL(out_dma));
	ipu_cm_write(ipu, reg & ~idma_mask(out_dma), IPU_CHA_DB_MODE_SEL(out_dma));

	/* Reset the triple buffer */
	// reg = ipu_cm_read(ipu, IPU_CHA_TRB_MODE_SEL(ipu->devtype, in_dma));
	// ipu_cm_write(ipu, reg & ~idma_mask(in_dma),
	// 			IPU_CHA_TRB_MODE_SEL(ipu->devtype, in_dma));
	// reg = ipu_cm_read(ipu, IPU_CHA_TRB_MODE_SEL(ipu->devtype, out_dma));
	// ipu_cm_write(ipu, reg & ~idma_mask(out_dma),
	// 			IPU_CHA_TRB_MODE_SEL(ipu->devtype, out_dma));

	if (_ipu_is_ic_chan(in_dma)) { // || _ipu_is_dp_graphic_chan(in_dma)
		ipu->sec_chan_en[IPU_CHAN_ID(channel)] = false;
		ipu->thrd_chan_en[IPU_CHAN_ID(channel)] = false;
	}

	switch (channel) {
	case CSI_PRP_ENC_MEM:
		ipu->ic_use_count--;
		if (ipu->using_ic_dirct_ch == CSI_PRP_ENC_MEM)
			ipu->using_ic_dirct_ch = 0;
		_ipu_ic_uninit_prpenc(ipu);
		if (ipu->csi_channel[0] == channel) {
			ipu->csi_channel[0] = CHAN_NONE;
		} else if (ipu->csi_channel[1] == channel) {
			ipu->csi_channel[1] = CHAN_NONE;
		}
		break;
	default:
		break;
	}

	if (ipu->ic_use_count == 0)
		ipu_conf &= ~IPU_CONF_IC_EN;
	// if (ipu->vdi_use_count == 0) {
	// 	ipu_conf &= ~IPU_CONF_ISP_EN;
	// 	ipu_conf &= ~IPU_CONF_VDI_EN;
	// 	ipu_conf &= ~IPU_CONF_IC_INPUT;
	// }
	// if (ipu->rot_use_count == 0)
	// 	ipu_conf &= ~IPU_CONF_ROT_EN;
	// if (ipu->dc_use_count == 0)
	// 	ipu_conf &= ~IPU_CONF_DC_EN;
	// if (ipu->dp_use_count == 0)
	// 	ipu_conf &= ~IPU_CONF_DP_EN;
	// if (ipu->dmfc_use_count == 0)
	// 	ipu_conf &= ~IPU_CONF_DMFC_EN;
	// if (ipu->di_use_count[0] == 0) {
	// 	ipu_conf &= ~IPU_CONF_DI0_EN;
	// }
	// if (ipu->di_use_count[1] == 0) {
	// 	ipu_conf &= ~IPU_CONF_DI1_EN;
	// }
	// if (ipu->smfc_use_count == 0)
	// 	ipu_conf &= ~IPU_CONF_SMFC_EN;

	ipu_cm_write(ipu, ipu_conf, IPU_CONF);

	ipu->channel_init_mask &= ~(1L << IPU_CHAN_ID(channel));

	// /*
	//  * Disable pixel clk and its parent clock(if the parent clock
	//  * usecount is 1) after clearing DC/DP/DI bits in IPU_CONF
	//  * register to prevent LVDS display channel starvation.
	//  */
	// if (_ipu_is_primary_disp_chan(in_dma) &&
	//     ipu->pixel_clk_en[ipu->dc_di_assignment[dc_chan]]) {
	// 	clk_disable_unprepare(ipu->pixel_clk[ipu->dc_di_assignment[dc_chan]]);
	// 	ipu->pixel_clk_en[ipu->dc_di_assignment[dc_chan]] = false;
	// }

	// mutex_unlock(&ipu->mutex_lock);

	// _ipu_put(ipu);

	// ret = pm_runtime_put_sync_suspend(ipu->dev);
	// if (ret < 0) {
	// 	dev_err(ipu->dev, "ch = %d, pm_runtime_put failed:%d!\n",
	// 			IPU_CHAN_ID(channel), ret);
	// 	dump_stack();
	// }

	// WARN_ON(ipu->ic_use_count < 0);
	// WARN_ON(ipu->vdi_use_count < 0);
	// WARN_ON(ipu->rot_use_count < 0);
	// WARN_ON(ipu->dc_use_count < 0);
	// WARN_ON(ipu->dp_use_count < 0);
	// WARN_ON(ipu->dmfc_use_count < 0);
	// WARN_ON(ipu->smfc_use_count < 0);
}

/**
 * IPU_UNINIT_CHANNEL
 * 
 * Uninitilize IPU CSI_PRP_ENC_MEM channel.
 * 
 * Parameter arrangement:
*/
uint32_t fast_smc_ipu_uninit_channel(struct thread_smc_args *args) {
	struct ipu_soc *ipu = &ipu1;

	ipu_uninit_channel(ipu, CSI_PRP_ENC_MEM);

	return 0;
}

#define IPU1_ERR_IRQ	37
#define IPU1_SYNC_IRQ	38

static enum itr_return ipu1_err_handler(struct itr_handler *handler __unused) {
	IMSG("IPU err from the SW");

	return ITRR_HANDLED;
}

static struct itr_handler ipu1_err_itr_handler = {
	.it = IPU1_ERR_IRQ,
	.handler = ipu1_err_handler,
};

static enum itr_return ipu1_sync_handler(struct itr_handler *handler __unused) {
	IMSG("frame captured from the SW");

	return ITRR_HANDLED;
}

static struct itr_handler ipu1_sync_itr_handler = {
	.it = IPU1_SYNC_IRQ,
	.handler = ipu1_sync_handler,
};


static TEE_Result ipu_init(void) {
	struct ipu_soc *ipu = &ipu1;
	unsigned long ipu_base;
	struct ipu_devtype *devtype = &ipu_type_imx6q.devtype;

	/* map ipu1 registers into the optee core va */
	ipu_base = core_mmu_get_va(IPU1_BASE, MEM_AREA_IO_SEC);
	
	ipu->cm_reg = ipu_base + devtype->cm_ofs;
	ipu->ic_reg = ipu_base + devtype->ic_ofs;
	ipu->idmac_reg = ipu_base + devtype->idmac_ofs;
	/* DP Registers are accessed thru the SRM */
	ipu->dp_reg = ipu_base + devtype->srm_ofs;
	ipu->dc_reg = ipu_base + devtype->dc_ofs;
	ipu->dmfc_reg = ipu_base + devtype->dmfc_ofs;
	ipu->di_reg[0] = ipu_base + devtype->di0_ofs;
	ipu->di_reg[1] = ipu_base + devtype->di1_ofs;
	ipu->smfc_reg = ipu_base + devtype->smfc_ofs;
	ipu->csi_reg[0] = ipu_base + devtype->csi0_ofs;
	ipu->csi_reg[1] = ipu_base + devtype->csi1_ofs;
	ipu->cpmem_base = ipu_base + devtype->cpmem_ofs;
	ipu->tpmem_base = ipu_base + devtype->tpm_ofs;
	ipu->dc_tmpl_reg = ipu_base + devtype->dc_tmpl_ofs;
	ipu->vdi_reg = ipu_base + devtype->vdi_ofs;

	// IMSG(" CM Regs = 0x%08lx\n", ipu->cm_reg);
	// IMSG(" IC Regs = 0x%08lx\n", ipu->ic_reg);
	// IMSG(" IDMAC Regs = 0x%08lx\n", ipu->idmac_reg);
	// IMSG(" DP Regs = 0x%08lx\n", ipu->dp_reg);
	// IMSG(" DC Regs = 0x%08lx\n", ipu->dc_reg);
	// IMSG(" DMFC Regs = 0x%08lx\n", ipu->dmfc_reg);
	// IMSG(" DI0 Regs = 0x%08lx\n", ipu->di_reg[0]);
	// IMSG(" DI1 Regs = 0x%08lx\n", ipu->di_reg[1]);
	// IMSG(" SMFC Regs = 0x%08lx\n", ipu->smfc_reg);
	// IMSG(" CSI0 Regs = 0x%08lx\n", ipu->csi_reg[0]);
	// IMSG(" CSI1 Regs = 0x%08lx\n", ipu->csi_reg[1]);
	// IMSG(" CPMem = 0x%08lx\n", ipu->cpmem_base);
	// IMSG(" TPMem = 0x%08lx\n", ipu->tpmem_base);
	// IMSG(" DC Template Mem = 0x%08lx\n", ipu->dc_tmpl_reg);
	// IMSG(" VDI Regs = 0x%08lx\n", ipu->vdi_reg);

	// ipu_dump_registers(ipu);

	ipu->channel_enable_mask = 0;
	ipu->channel_init_mask = 0;

	ipu->normal_axi = 1; // imx6q

	ipu->devtype = devtype->type;

	// register IPU1 error(37) and sync(38) interrupt handler
	// itr_add(&ipu1_err_itr_handler);
	// itr_enable(ipu1_err_itr_handler.it);
	// itr_add(&ipu1_sync_itr_handler);
	// itr_enable(ipu1_sync_itr_handler.it);

	return TEE_SUCCESS;
}
driver_init(ipu_init);