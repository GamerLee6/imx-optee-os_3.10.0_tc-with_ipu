
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

int _ipu_csi_set_mipi_di(struct ipu_soc *ipu, uint32_t num, uint32_t di_val, uint32_t csi)
{
	uint32_t temp;
	int retval = 0;

	if (di_val > 0xFFL) {
		retval = TEE_ERROR_BAD_PARAMETERS;
		goto err;
	}
	IMSG("In %s(), DI#: %d\n", __func__, num);

	temp = ipu_csi_read(ipu, csi, CSI_MIPI_DI);

	switch (num) {
	case IPU_CSI_MIPI_DI0: // DI0 is used
		temp &= ~CSI_MIPI_DI0_MASK; // clear original DI0 value
		temp |= (di_val << CSI_MIPI_DI0_SHIFT);
		ipu_csi_write(ipu, csi, temp, CSI_MIPI_DI);
		break;
	case IPU_CSI_MIPI_DI1:
		temp &= ~CSI_MIPI_DI1_MASK;
		temp |= (di_val << CSI_MIPI_DI1_SHIFT);
		ipu_csi_write(ipu, csi, temp, CSI_MIPI_DI);
		break;
	case IPU_CSI_MIPI_DI2:
		temp &= ~CSI_MIPI_DI2_MASK;
		temp |= (di_val << CSI_MIPI_DI2_SHIFT);
		ipu_csi_write(ipu, csi, temp, CSI_MIPI_DI);
		break;
	case IPU_CSI_MIPI_DI3:
		temp &= ~CSI_MIPI_DI3_MASK;
		temp |= (di_val << CSI_MIPI_DI3_SHIFT);
		ipu_csi_write(ipu, csi, temp, CSI_MIPI_DI);
		break;
	default:
		retval = -TEE_ERROR_BAD_PARAMETERS;
	}

err:
	return retval;
}

int _ipu_csi_init(struct ipu_soc *ipu, ipu_channel_t channel, uint32_t csi)
{
	uint32_t csi_sens_conf, csi_dest;
	int retval = 0;

	switch (channel) {
	// case CSI_MEM0:
	// case CSI_MEM1:
	// case CSI_MEM2:
	// case CSI_MEM3:
	// 	csi_dest = CSI_DATA_DEST_IDMAC;
	// 	break;
	case CSI_PRP_ENC_MEM:
	// case CSI_PRP_VF_MEM:
		csi_dest = CSI_DATA_DEST_IC;
		break;
	default:
		retval = TEE_ERROR_BAD_PARAMETERS;
		goto err;
	}

	csi_sens_conf = ipu_csi_read(ipu, csi, CSI_SENS_CONF);
	csi_sens_conf &= ~CSI_SENS_CONF_DATA_DEST_MASK; // clear CSI1_DATA_DEST
	ipu_csi_write(ipu, csi, csi_sens_conf | (csi_dest <<
		CSI_SENS_CONF_DATA_DEST_SHIFT), CSI_SENS_CONF);
err:
	return retval;
}