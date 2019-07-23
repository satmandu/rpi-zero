// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2010 Broadcom
 */

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/irqchip.h>
#include <linux/of_address.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include "platsmp.h"

static const char * const bcm2835_compat[] = {
#ifdef CONFIG_ARCH_MULTI_V6
	"brcm,bcm2835",
#endif
#ifdef CONFIG_ARCH_MULTI_V7
	"brcm,bcm2711",
	"brcm,bcm2836",
	"brcm,bcm2837",
#endif
	NULL
};

static int bcm2835_needs_bounce(struct device *dev, dma_addr_t dma_addr, size_t size)
{
	/*
	 * The accepted dma addresses are [0xc0000000, 0xffffffff] which map to
	 * ram's [0x00000000, 0x3fffffff].
	 */
	return dma_addr < 3ULL * SZ_1G;
}

static int bcm2835_platform_notify(struct device *dev)
{
	if (dev->parent && !strcmp("soc", dev_name(dev->parent))) {
		dev->dma_mask = &dev->coherent_dma_mask;
		dev->coherent_dma_mask = DMA_BIT_MASK(30);
		dmabounce_register_dev(dev, 2048, 4096, bcm2835_needs_bounce);
	}

	return 0;
}

void __init bcm2835_init_early(void)
{
	if(of_machine_is_compatible("brcm,bcm2711"))
		platform_notify = bcm2835_platform_notify;
}

DT_MACHINE_START(BCM2835, "BCM2835")
	.dma_zone_size	= SZ_1G,
	.dt_compat = bcm2835_compat,
	.smp = smp_ops(bcm2836_smp_ops),
	.init_early = bcm2835_init_early,
MACHINE_END
