/*
 * This file is part of the depthcharge project.
 *
 * Copyright (C) 2014 - 2015 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <libpayload.h>
#include <arch/io.h>
#include "base/init_funcs.h"
#include "drivers/gpio/ipq40xx.h"
#include "drivers/bus/spi/ipq40xx.h"
#include "ipq40xx_blsp.h"

#define SCL_GPIO	20
#define SDA_GPIO	21
#define GPIO_FUNC_SCL	0x1
#define GPIO_FUNC_SDA	0x1

#define SCL_GPIO_I2C1       34
#define SDA_GPIO_I2C1       35
#define GPIO_I2C1_FUNC_SCL  0x1
#define GPIO_I2C1_FUNC_SDA  0x1

int blsp_init_board(blsp_qup_id_t id)
{
	switch (id) {
	case BLSP_QUP_ID_0:
	case BLSP_QUP_ID_1:
	case BLSP_QUP_ID_2:
	case BLSP_QUP_ID_3:
		/* Configure GPIOs 20 - SCL, 21 - SDA, 2mA gpio_en */
		gpio_tlmm_config_set(SDA_GPIO, GPIO_FUNC_SDA,
			GPIO_NO_PULL, GPIO_2MA, 1);
		gpio_tlmm_config_set(SCL_GPIO, GPIO_FUNC_SCL,
			GPIO_NO_PULL, GPIO_2MA, 1);

		/* Configure GPIOs 34 - SCL, 35 - SDA, 2mA gpio_en */
		gpio_tlmm_config_set(SDA_GPIO_I2C1, GPIO_I2C1_FUNC_SDA,
			GPIO_NO_PULL, GPIO_2MA, 1);
		gpio_tlmm_config_set(SCL_GPIO_I2C1, GPIO_I2C1_FUNC_SCL,
			GPIO_NO_PULL, GPIO_2MA, 1);

		break;
	default:
		return 1;
	}

	return 0;
}

blsp_return_t blsp_init(blsp_qup_id_t id, blsp_protocol_t protocol)
{
	void *base = blsp_qup_base(id);

	if (!base)
		return BLSP_ID_ERROR;

	/* Configure Mini core to I2C core */
	clrsetbits_le32(base, QUP_CONFIG_MINI_CORE_MSK,
				QUP_CONFIG_MINI_CORE_I2C);

	return BLSP_SUCCESS;
}
