// SPDX-License-Identifier: GPL-2.0+
/*
 * Broadcom 2711 thermal sensor driver
 *
 * based on brcmstb_thermal
 *
 * Copyright (C) 2019 Stefan Wahren
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/thermal.h>

#define AVS_TMON_STATUS			0x00
 #define AVS_TMON_STATUS_valid_msk	BIT(10)
 #define AVS_TMON_STATUS_data_msk	GENMASK(9, 0)

/* HW related temperature constants */
#define AVS_TMON_TEMP_MAX		0x3ff
#define AVS_TMON_TEMP_MIN		-88161
#define AVS_TMON_TEMP_MASK		AVS_TMON_TEMP_MAX

struct bcm2711_thermal_priv {
	void __iomem *tmon_base;
	struct device *dev;
	struct thermal_zone_device *thermal;
	struct clk *clk;
};

static void avs_tmon_get_coeffs(struct thermal_zone_device *tz, int *slope,
				int *offset)
{
	*slope = thermal_zone_get_slope(tz);
	*offset = thermal_zone_get_offset(tz);
}

/* Convert a HW code to a temperature reading (millidegree celsius) */
static inline int avs_tmon_code_to_temp(struct thermal_zone_device *tz,
					u32 code)
{
	const int val = code & AVS_TMON_TEMP_MASK;
	int slope, offset;

	avs_tmon_get_coeffs(tz, &slope, &offset);

	return slope * val + offset;
}

static int bcm2711_get_temp(void *data, int *temp)
{
	struct bcm2711_thermal_priv *priv = data;
	u32 val;
	long t;

	val = __raw_readl(priv->tmon_base + AVS_TMON_STATUS);

	if (!(val & AVS_TMON_STATUS_valid_msk)) {
		dev_err(priv->dev, "reading not valid\n");
		return -EIO;
	}

	val &= AVS_TMON_STATUS_data_msk;

	t = avs_tmon_code_to_temp(priv->thermal, val);
	if (t < 0)
		*temp = 0;
	else
		*temp = t;

	return 0;
}

static const struct thermal_zone_of_device_ops bcm2711_thermal_of_ops = {
	.get_temp	= bcm2711_get_temp,
};

static const struct of_device_id bcm2711_thermal_id_table[] = {
	{ .compatible = "brcm,bcm2711-thermal" },
	{},
};
MODULE_DEVICE_TABLE(of, bcm2711_thermal_id_table);

static int bcm2711_thermal_probe(struct platform_device *pdev)
{
	struct thermal_zone_device *thermal;
	struct bcm2711_thermal_priv *priv;
	struct resource *res;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->tmon_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(priv->tmon_base))
		return PTR_ERR(priv->tmon_base);

	priv->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(priv->clk)) {
		ret = PTR_ERR(priv->clk);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "could not get clk: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(priv->clk);
	if (ret)
		return ret;

	priv->dev = &pdev->dev;
	platform_set_drvdata(pdev, priv);

	thermal = thermal_zone_of_sensor_register(&pdev->dev, 0, priv,
						  &bcm2711_thermal_of_ops);
	if (IS_ERR(thermal)) {
		ret = PTR_ERR(thermal);
		dev_err(&pdev->dev, "could not register sensor: %d\n", ret);
		goto err_clk;
	}

	priv->thermal = thermal;

	return 0;

err_clk:
	clk_disable_unprepare(priv->clk);

	return ret;
}

static int bcm2711_thermal_remove(struct platform_device *pdev)
{
	struct bcm2711_thermal_priv *priv = platform_get_drvdata(pdev);
	struct thermal_zone_device *thermal = priv->thermal;

	if (thermal)
		thermal_zone_of_sensor_unregister(&pdev->dev, priv->thermal);

	if (!IS_ERR(priv->clk))
		clk_disable_unprepare(priv->clk);

	return 0;
}

static struct platform_driver bcm2711_thermal_driver = {
	.probe = bcm2711_thermal_probe,
	.remove = bcm2711_thermal_remove,
	.driver = {
		.name = "bcm2711_thermal",
		.of_match_table = bcm2711_thermal_id_table,
	},
};
module_platform_driver(bcm2711_thermal_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Stefan Wahren");
MODULE_DESCRIPTION("Broadcom 2711 thermal sensor driver");
