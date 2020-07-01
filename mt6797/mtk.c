// SPDX-License-Identifier: GPL-2.0
/*
 * FPC Fingerprint sensor device driver
 *
 * This driver will control the platform resources that the FPC fingerprint
 * sensor needs to operate. The major things are probing the sensor to check
 * that it is actually connected and let the Kernel know this and with that also
 * enabling and disabling of regulators, enabling and disabling of platform
 * clocks.
 * *
 * The driver will expose most of its available functionality in sysfs which
 * enables dynamic control of these features from eg. a user space process.
 *
 * Copyright (c) 2020 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include "fpc16xx.h"

struct priv_data {
	struct clk *spi_main;
};

static int set_clks(struct fpc_data *fpc, bool enable)
{
	struct priv_data *priv = (struct priv_data *)fpc->hwabs->priv;
	int rc = 0;

	if (enable) {
		rc = clk_enable(priv->spi_main);
		if (rc) {
			dev_err(fpc->dev,
				"%s: Error enabling spi-main clk: %d\n",
				__func__, rc);
			return rc;
		}
	} else {
		clk_disable(priv->spi_main);
	}

	return rc;
}

static ssize_t clk_enable_set(struct fpc_data *fpc, const char *buf, size_t count)
{
	return set_clks(fpc, (*buf == '1')) ? 1 : count;
}

static int mtk6797_init(struct fpc_data *fpc)
{
	return 0;
}

static int vreg_setup(struct fpc_data *fpc16xx, const char *name,
	bool enable)
{
	//To DO
	return 0;
}

static int mtk6797_configure(struct fpc_data *fpc)
{
	struct device *dev = fpc->dev;
	struct priv_data *priv = (struct priv_data *)fpc->hwabs->priv;
	int rc = 0;

	rc = gpio_direction_output(fpc->rst_gpio, 1);
	if (rc != 0) {
		dev_err(dev, "gpio_direction_output failed for RST.\n");
		return rc;
	}

	priv->spi_main = clk_get(dev, "spi-main");
	if (IS_ERR(priv->spi_main)) {
		dev_err(dev, "%s: failed to get spi-main\n", __func__);
		return -EINVAL;
	}

	clk_prepare(priv->spi_main);
	set_clks(fpc, true);

	return rc;
}

static struct fpc_gpio_info mtk6797_ops = {
	.init = mtk6797_init,
	.vreg_setup = vreg_setup,
	.configure = mtk6797_configure,
	.get_val = gpio_get_value,
	.set_val = gpio_set_value,
	.clk_enable_set = clk_enable_set,
};

static const struct of_device_id mt6797_of_match[] = {
	{ .compatible = "fpc,fpc16xx", },
	{}
};

MODULE_DEVICE_TABLE(of, mt6797_of_match);

static int mtk6797_probe(struct platform_device *pldev)
{
	int rc;

	mtk6797_ops.priv = devm_kzalloc(&pldev->dev, sizeof(struct priv_data), GFP_KERNEL);

	if (!mtk6797_ops.priv) {
		dev_err(&pldev->dev, "failed to allocate memory for struct priv_data\n");
		return -ENOMEM;
	}

	rc = fpc_probe(pldev, &mtk6797_ops);

	return rc;
}

static struct platform_driver mtk6797_driver = {
	.driver = {
		.name           = "fpc16xx",
		.owner          = THIS_MODULE,
		.of_match_table = mt6797_of_match,
	},
	.probe  = mtk6797_probe,
	.remove = fpc_remove
};

module_platform_driver(mtk6797_driver);

MODULE_LICENSE("GPL");
