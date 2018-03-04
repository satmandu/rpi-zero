#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <dt-bindings/pinctrl/bcm2835.h>

#define SLEW_SHIFT	4
#define HYST_SHIFT	3
#define DRIVE_MASK	0x7

#define PASSWORD	0x5a000000

static const struct of_device_id bcm2835_pads_of_match[] = {
	{ .compatible = "brcm,bcm2835-pm-pads", },
	{},
};
MODULE_DEVICE_TABLE(of, bcm2835_pads_of_match);

static int bcm2835_dt_setup_slew_rate(struct device *dev, void __iomem *base)
{
	struct device_node *np = dev->of_node;
	int i, count, ret = 0;
	u32 *vals;
	u32 reg;

	count = of_property_count_u32_elems(np, "brcm,slew-rate");
	if (count <= 0) {
		dev_info(dev, "No slew rate defined\n");
		return 0;
	}

	vals = kcalloc(count, sizeof(*vals), GFP_KERNEL);
	if (!vals)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, "brcm,slew-rate", vals, count);
	if (ret < 0) {
		dev_err(dev, "Unable to read slew rate %d\n", ret);
		goto err;
	}

	for (i = 0; i < count; i += 2) {
		if (vals[i] > BCM2835_PAD_GPIO_46_53) {
			dev_err(dev, "Bank is out of range: %u\n", vals[i]);
			goto err;
		}

		if (vals[i+1] > 1) {
			dev_err(dev, "Slew rate is out of range: %u\n",
				vals[i+1]);
			goto err;
		}

		reg = readl(base + vals[i]) & ~(1 << SLEW_SHIFT);
		reg |= vals[i+1] << SLEW_SHIFT;
		writel(reg | PASSWORD, base + vals[i]);
	}

err:
	kfree(vals);

	return ret;
}

static int bcm2835_pads_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	void __iomem *base;
	int i, ret = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		dev_err(dev, "Unable to map resource %ld\n", PTR_ERR(base));
		return PTR_ERR(base);
	}

	ret = bcm2835_dt_setup_slew_rate(dev, base);

	for (i = 0; i <= BCM2835_PAD_GPIO_46_53; i++)
		dev_info(dev, "Bank%d : %08x\n", i, readl(base + i * 4));

	return ret;
}

static struct platform_driver bcm2835_pads_driver = {
	.probe	= bcm2835_pads_probe,
	.driver = {
		.name = "bcm2835-pads",
		.of_match_table = of_match_ptr(bcm2835_pads_of_match),
	},
};
module_platform_driver(bcm2835_pads_driver);

MODULE_DESCRIPTION("BCM2835 pads control");
MODULE_AUTHOR("Stefan Wahren <stefan.wahren@i2se.com>");
MODULE_LICENSE("GPL v2");
