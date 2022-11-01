#include <initcall.h>
#include <inttypes.h>
#include <io.h>
#include <kernel/panic.h>
#include <kernel/spinlock.h>
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


static int _calc_resize_coeffs(struct ipu_soc *ipu,
				uint32_t inSize, uint32_t outSize,
				uint32_t *resizeCoeff,
				uint32_t *downsizeCoeff)
{
	uint32_t tempSize;
	uint32_t tempDownsize;

	if (inSize > 4096) {
		EMSG("IC input size(%d) cannot exceed 4096\n",
			inSize);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if (outSize > 1024) {
		EMSG("IC output size(%d) cannot exceed 1024\n",
			outSize);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if ((outSize << 3) < inSize) {
		EMSG("IC cannot downsize more than 8:1\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Compute downsizing coefficient */
	/* Output of downsizing unit cannot be more than 1024 */
	tempDownsize = 0;
	tempSize = inSize;
	while (((tempSize > 1024) || (tempSize >= outSize * 2)) &&
	       (tempDownsize < 2)) {
		tempSize >>= 1;
		tempDownsize++;
	}
	*downsizeCoeff = tempDownsize;

	/* compute resizing coefficient using the following equation:
	   resizeCoeff = M*(SI -1)/(SO - 1)
	   where M = 2^13, SI - input size, SO - output size    */
	*resizeCoeff = (8192L * (tempSize - 1)) / (outSize - 1);
	if (*resizeCoeff >= 16384L) {
		EMSG("Overflow on IC resize coefficient.\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	IMSG("resizing from %u -> %u pixels, "
		"downsize=%u, resize=%u.%lu (reg=%u)\n", inSize, outSize,
		*downsizeCoeff, (*resizeCoeff >= 8192L) ? 1 : 0,
		((*resizeCoeff & 0x1FFF) * 10000L) / 8192L, *resizeCoeff);

	return 0;
}

int _ipu_ic_init_prpenc(struct ipu_soc *ipu, ipu_channel_params_t *params,
			bool src_is_csi)
{
	uint32_t reg, ic_conf;
	uint32_t downsizeCoeff, resizeCoeff;
	ipu_color_space_t in_fmt, out_fmt;
	int ret = 0;

	/* Setup vertical resizing */
	if (!params->mem_prp_enc_mem.outv_resize_ratio) {
		ret = _calc_resize_coeffs(ipu,
					params->mem_prp_enc_mem.in_height,
					params->mem_prp_enc_mem.out_height,
					&resizeCoeff, &downsizeCoeff);
		if (ret < 0) {
			EMSG("failed to calculate prpenc height "
				"scaling coefficients\n");
			return ret;
		}

		reg = (downsizeCoeff << 30) | (resizeCoeff << 16);
	} else
		reg = (params->mem_prp_enc_mem.outv_resize_ratio) << 16;

	/* Setup horizontal resizing */
	if (!params->mem_prp_enc_mem.outh_resize_ratio) {
		ret = _calc_resize_coeffs(ipu, params->mem_prp_enc_mem.in_width,
					params->mem_prp_enc_mem.out_width,
					&resizeCoeff, &downsizeCoeff);
		if (ret < 0) {
			EMSG("failed to calculate prpenc width "
				"scaling coefficients\n");
			return ret;
		}

		reg |= (downsizeCoeff << 14) | resizeCoeff;
	} else
		reg |= params->mem_prp_enc_mem.outh_resize_ratio;

	ipu_ic_write(ipu, reg, IC_PRP_ENC_RSC);

	ic_conf = ipu_ic_read(ipu, IC_CONF);

	/* Setup color space conversion */
	in_fmt = format_to_colorspace(params->mem_prp_enc_mem.in_pixel_fmt);
	out_fmt = format_to_colorspace(params->mem_prp_enc_mem.out_pixel_fmt);
	if (in_fmt == RGB) {
		if ((out_fmt == YCbCr) || (out_fmt == YUV)) {
			// /* Enable RGB->YCBCR CSC1 */
			// _init_csc(ipu, IC_TASK_ENCODER, RGB, out_fmt, 1);
			// ic_conf |= IC_CONF_PRPENC_CSC1;
		}
	}
	if ((in_fmt == YCbCr) || (in_fmt == YUV)) {
		if (out_fmt == RGB) {
			// /* Enable YCBCR->RGB CSC1 */
			// _init_csc(ipu, IC_TASK_ENCODER, YCbCr, RGB, 1);
			// ic_conf |= IC_CONF_PRPENC_CSC1;
		} else {
			/* TODO: Support YUV<->YCbCr conversion? */
			IMSG("in:YUV -> out:YUV, should be no color space conversion");
		}
	}

	if (src_is_csi)
		ic_conf &= ~IC_CONF_RWS_EN;
	else
		ic_conf |= IC_CONF_RWS_EN;

	ipu_ic_write(ipu, ic_conf, IC_CONF);

	return ret;
}

int _ipu_ic_idma_init(struct ipu_soc *ipu, int dma_chan,
		uint16_t width, uint16_t height,
		int burst_size, ipu_rotate_mode_t rot)
{
	uint32_t ic_idmac_1, ic_idmac_2, ic_idmac_3;
	// uint32_t temp_rot = bitrev8(rot) >> 5;
	bool need_hor_flip = false;

	if ((burst_size != 8) && (burst_size != 16)) {
		DMSG("Illegal burst length for IC\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	width--;
	height--;

	// if (temp_rot & 0x2)	/* Need horizontal flip */
	// 	need_hor_flip = true;

	DMSG("burst_size: %d\n", burst_size);

	ic_idmac_1 = ipu_ic_read(ipu, IC_IDMAC_1);
	ic_idmac_2 = ipu_ic_read(ipu, IC_IDMAC_2);
	ic_idmac_3 = ipu_ic_read(ipu, IC_IDMAC_3);
	// if (dma_chan == 22) {	/* PP output - CB2 */
	// 	if (burst_size == 16)
	// 		ic_idmac_1 |= IC_IDMAC_1_CB2_BURST_16;
	// 	else
	// 		ic_idmac_1 &= ~IC_IDMAC_1_CB2_BURST_16;

	// 	if (need_hor_flip)
	// 		ic_idmac_1 |= IC_IDMAC_1_PP_FLIP_RS;
	// 	else
	// 		ic_idmac_1 &= ~IC_IDMAC_1_PP_FLIP_RS;

	// 	ic_idmac_2 &= ~IC_IDMAC_2_PP_HEIGHT_MASK;
	// 	ic_idmac_2 |= height << IC_IDMAC_2_PP_HEIGHT_OFFSET;

	// 	ic_idmac_3 &= ~IC_IDMAC_3_PP_WIDTH_MASK;
	// 	ic_idmac_3 |= width << IC_IDMAC_3_PP_WIDTH_OFFSET;
	// } else if (dma_chan == 11) {	/* PP Input - CB5 */
	// 	if (burst_size == 16)
	// 		ic_idmac_1 |= IC_IDMAC_1_CB5_BURST_16;
	// 	else
	// 		ic_idmac_1 &= ~IC_IDMAC_1_CB5_BURST_16;
	// } else if (dma_chan == 47) {	/* PP Rot input */
	// 	ic_idmac_1 &= ~IC_IDMAC_1_PP_ROT_MASK;
	// 	ic_idmac_1 |= temp_rot << IC_IDMAC_1_PP_ROT_OFFSET;
	// }

	// if (dma_chan == 12) {	/* PRP Input - CB6 */
	// 	if (burst_size == 16)
	// 		ic_idmac_1 |= IC_IDMAC_1_CB6_BURST_16;
	// 	else
	// 		ic_idmac_1 &= ~IC_IDMAC_1_CB6_BURST_16;
	// }

	if (dma_chan == 20) {	/* PRP ENC output - CB0 */
		if (burst_size == 16)
			ic_idmac_1 |= IC_IDMAC_1_CB0_BURST_16;
		else
			ic_idmac_1 &= ~IC_IDMAC_1_CB0_BURST_16;

		if (need_hor_flip)
			ic_idmac_1 |= IC_IDMAC_1_PRPENC_FLIP_RS;
		else
			ic_idmac_1 &= ~IC_IDMAC_1_PRPENC_FLIP_RS;

		ic_idmac_2 &= ~IC_IDMAC_2_PRPENC_HEIGHT_MASK;
		ic_idmac_2 |= height << IC_IDMAC_2_PRPENC_HEIGHT_OFFSET;

		ic_idmac_3 &= ~IC_IDMAC_3_PRPENC_WIDTH_MASK;
		ic_idmac_3 |= width << IC_IDMAC_3_PRPENC_WIDTH_OFFSET;
	}
	// } else if (dma_chan == 45) {	/* PRP ENC Rot input */
	// 	ic_idmac_1 &= ~IC_IDMAC_1_PRPENC_ROT_MASK;
	// 	ic_idmac_1 |= temp_rot << IC_IDMAC_1_PRPENC_ROT_OFFSET;
	// }

	// if (dma_chan == 21) {	/* PRP VF output - CB1 */
	// 	if (burst_size == 16)
	// 		ic_idmac_1 |= IC_IDMAC_1_CB1_BURST_16;
	// 	else
	// 		ic_idmac_1 &= ~IC_IDMAC_1_CB1_BURST_16;

	// 	if (need_hor_flip)
	// 		ic_idmac_1 |= IC_IDMAC_1_PRPVF_FLIP_RS;
	// 	else
	// 		ic_idmac_1 &= ~IC_IDMAC_1_PRPVF_FLIP_RS;

	// 	ic_idmac_2 &= ~IC_IDMAC_2_PRPVF_HEIGHT_MASK;
	// 	ic_idmac_2 |= height << IC_IDMAC_2_PRPVF_HEIGHT_OFFSET;

	// 	ic_idmac_3 &= ~IC_IDMAC_3_PRPVF_WIDTH_MASK;
	// 	ic_idmac_3 |= width << IC_IDMAC_3_PRPVF_WIDTH_OFFSET;

	// } else if (dma_chan == 46) {	/* PRP VF Rot input */
	// 	ic_idmac_1 &= ~IC_IDMAC_1_PRPVF_ROT_MASK;
	// 	ic_idmac_1 |= temp_rot << IC_IDMAC_1_PRPVF_ROT_OFFSET;
	// }

	// if (dma_chan == 14) {	/* PRP VF graphics combining input - CB3 */
	// 	if (burst_size == 16)
	// 		ic_idmac_1 |= IC_IDMAC_1_CB3_BURST_16;
	// 	else
	// 		ic_idmac_1 &= ~IC_IDMAC_1_CB3_BURST_16;
	// } else if (dma_chan == 15) {	/* PP graphics combining input - CB4 */
	// 	if (burst_size == 16)
	// 		ic_idmac_1 |= IC_IDMAC_1_CB4_BURST_16;
	// 	else
	// 		ic_idmac_1 &= ~IC_IDMAC_1_CB4_BURST_16;
	// } else if (dma_chan == 5) {	/* VDIC OUTPUT - CB7 */
	// 	if (burst_size == 16)
	// 		ic_idmac_1 |= IC_IDMAC_1_CB7_BURST_16;
	// 	else
	// 		ic_idmac_1 &= ~IC_IDMAC_1_CB7_BURST_16;
	// }

	ipu_ic_write(ipu, ic_idmac_1, IC_IDMAC_1);
	ipu_ic_write(ipu, ic_idmac_2, IC_IDMAC_2);
	ipu_ic_write(ipu, ic_idmac_3, IC_IDMAC_3);
	return 0;
}

void _ipu_ic_enable_task(struct ipu_soc *ipu, ipu_channel_t channel)
{
	uint32_t ic_conf;

	ic_conf = ipu_ic_read(ipu, IC_CONF);
	switch (channel) {
	// case CSI_PRP_VF_MEM:
	// case MEM_PRP_VF_MEM:
	// 	ic_conf |= IC_CONF_PRPVF_EN;
	// 	break;
	// case MEM_VDI_PRP_VF_MEM:
	// 	ic_conf |= IC_CONF_PRPVF_EN;
	// 	break;
	// case MEM_VDI_MEM:
	// 	ic_conf |= IC_CONF_PRPVF_EN | IC_CONF_RWS_EN ;
	// 	break;
	// case MEM_ROT_VF_MEM:
	// 	ic_conf |= IC_CONF_PRPVF_ROT_EN;
	// 	break;
	case CSI_PRP_ENC_MEM: //
	// case MEM_PRP_ENC_MEM:
		ic_conf |= IC_CONF_PRPENC_EN;
		break;
	// case MEM_ROT_ENC_MEM:
	// 	ic_conf |= IC_CONF_PRPENC_ROT_EN;
	// 	break;
	// case MEM_PP_MEM:
	// 	ic_conf |= IC_CONF_PP_EN;
	// 	break;
	// case MEM_ROT_PP_MEM:
	// 	ic_conf |= IC_CONF_PP_ROT_EN;
	// 	break;
	default:
		break;
	}
	ipu_ic_write(ipu, ic_conf, IC_CONF);
}

void _ipu_ic_disable_task(struct ipu_soc *ipu, ipu_channel_t channel)
{
	uint32_t ic_conf;

	ic_conf = ipu_ic_read(ipu, IC_CONF);
	switch (channel) {
	// case CSI_PRP_VF_MEM:
	// case MEM_PRP_VF_MEM:
	// 	ic_conf &= ~IC_CONF_PRPVF_EN;
	// 	break;
	// case MEM_VDI_PRP_VF_MEM:
	// 	ic_conf &= ~IC_CONF_PRPVF_EN;
	// 	break;
	// case MEM_VDI_MEM:
	// 	ic_conf &= ~(IC_CONF_PRPVF_EN | IC_CONF_RWS_EN);
	// 	break;
	// case MEM_ROT_VF_MEM:
	// 	ic_conf &= ~IC_CONF_PRPVF_ROT_EN;
	// 	break;
	case CSI_PRP_ENC_MEM:
	// case MEM_PRP_ENC_MEM:
		ic_conf &= ~IC_CONF_PRPENC_EN;
		break;
	// case MEM_ROT_ENC_MEM:
	// 	ic_conf &= ~IC_CONF_PRPENC_ROT_EN;
	// 	break;
	// case MEM_PP_MEM:
	// 	ic_conf &= ~IC_CONF_PP_EN;
	// 	break;
	// case MEM_ROT_PP_MEM:
	// 	ic_conf &= ~IC_CONF_PP_ROT_EN;
	// 	break;
	default:
		break;
	}
	ipu_ic_write(ipu, ic_conf, IC_CONF);
}

void _ipu_ic_uninit_prpenc(struct ipu_soc *ipu)
{
	uint32_t reg;

	reg = ipu_ic_read(ipu, IC_CONF);
	reg &= ~(IC_CONF_PRPENC_EN | IC_CONF_PRPENC_CSC1);
	ipu_ic_write(ipu, reg, IC_CONF);
}