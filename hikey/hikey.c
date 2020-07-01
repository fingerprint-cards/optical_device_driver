// SPDX-License-Identifier: GPL-2.0
/*
 * FPC Fingerprint sensor device driver
 *
 * This driver will control the platform resources that the FPC fingerprint
 * sensor needs to operate. The major things are probing the sensor to check
 * that it is actually connected and let the Kernel know this and with that also
 * enabling and disabling of regulators, enabling and disabling of platform
 * clocks, controlling GPIOs such as SPI chip select, sensor reset line, MISO
 * and MOSI lines.
 *
 * The driver will expose most of its available functionality in sysfs which
 * enables dynamic control of these features from eg. a user space process.
 *
 * This driver will NOT send any SPI commands to the sensor it only controls the
 * electrical parts.
 *
 *
 * Copyright (c) 2020 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

#include "fpc16xx.h"


static int hikey_init(struct fpc_data *fpc)
{
	return 0;
}

static int vreg_setup(struct fpc_data *fpc16xx, const char *name,
	bool enable)
{
	//TO DO
	return 0;
}

static int hikey_configure(struct fpc_data *fpc)
{
	struct device *dev = fpc->dev;
	int rc = 0;

	dev_info(dev, "%s", __func__);

	rc = gpio_direction_output(fpc->rst_gpio, 1);
	if (rc != 0) {
		dev_err(dev, "gpio_direction_output failed for RST.\n");
		return rc;
	}

	return rc;
}

static struct fpc_gpio_info fpc_ops = {
	.init           = hikey_init,
	.vreg_setup     = vreg_setup,
	.configure      = hikey_configure,
	.get_val        = gpio_get_value,
	.set_val        = gpio_set_value,
	.clk_enable_set = NULL,
};

static struct of_device_id fpc_of_match[] = {
	{ .compatible = "fpc,fpc16xx", },
	{}
};
MODULE_DEVICE_TABLE(of, fpc_of_match);

static int hikey960_probe(struct platform_device *pldev)
{
	return fpc_probe(pldev, &fpc_ops);
}

static struct platform_driver fpc_driver = {
	.driver = {
		.name           = "fpc16xx",
		.owner          = THIS_MODULE,
		.of_match_table = fpc_of_match,
	},
	.probe  = hikey960_probe,
	.remove = fpc_remove
};

static int __init fpc_dev_init(void)
{
	printk("%s\n", __func__);
	return platform_driver_register(&fpc_driver);
}

late_initcall(fpc_dev_init);

static void __exit fpc_dev_exit(void)
{
	printk("%s\n", __func__);
	platform_driver_unregister(&fpc_driver);
}
module_exit(fpc_dev_exit);

MODULE_AUTHOR("Liang Meng <liang.meng@fingerprints.com>");
MODULE_DESCRIPTION("Provides fingerprint device driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
