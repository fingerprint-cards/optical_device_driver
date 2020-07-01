// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/pm_wakeup.h>
#include <linux/regulator/consumer.h>
#include <linux/bitmap.h>

#include "fpc16xx.h"

#define FPC_RESET_LOW_US             (10500)
#define FPC_RESET_HIGH1_US           (1000)
#define FPC_RESET_HIGH2_US           (600)
#define NUM_PARAMS_REG_ENABLE_SET    (2)

static int hw_reset(struct fpc_data *fpc)
{
	struct device *dev = fpc->dev;

	fpc->hwabs->set_val(fpc->rst_gpio, 1);
	usleep_range(FPC_RESET_HIGH1_US, FPC_RESET_HIGH1_US<<1);

	fpc->hwabs->set_val(fpc->rst_gpio, 0);
	usleep_range(FPC_RESET_LOW_US, FPC_RESET_LOW_US<<1);

	fpc->hwabs->set_val(fpc->rst_gpio, 1);
	usleep_range(FPC_RESET_HIGH2_US, FPC_RESET_HIGH2_US<<1);

	dev_info(dev, "Using GPIO#%d as RST.\n", fpc->rst_gpio);

	return 0;
}

static ssize_t hw_reset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int rc;
	struct  fpc_data *fpc = dev_get_drvdata(dev);

	if (!strncmp(buf, "reset", strlen("reset"))) {
		dev_info(dev, "calling reset\n");
		rc = hw_reset(fpc);
		rc = rc ? rc : count;
	} else {
		dev_info(dev, "Unknown command\n");
		rc = -EINVAL;
	}

	return rc;
}

static DEVICE_ATTR_WO(hw_reset);

static ssize_t clk_enable_store(struct device *device,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct fpc_data *fpc = dev_get_drvdata(device);

	if (!fpc->hwabs->clk_enable_set) {
		return count;
	}

	return fpc->hwabs->clk_enable_set(fpc, buf, count);
}

static DEVICE_ATTR_WO(clk_enable);

static ssize_t regulator_enable_set_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc_data *fpc = dev_get_drvdata(dev);
	char op;
	char name[16];
	int rc;
	bool enable;

	if (NUM_PARAMS_REG_ENABLE_SET != sscanf(buf, "%15[^,],%c", name, &op))
		return -EINVAL;
	if (op == 'e')
		enable = true;
	else if (op == 'd')
		enable = false;
	else
		return -EINVAL;

	rc = fpc->hwabs->vreg_setup(fpc, name, enable);

	return rc ? rc : count;
}

static DEVICE_ATTR_WO(regulator_enable_set);

static struct attribute *fpc_attributes[] = {
	&dev_attr_hw_reset.attr,
	&dev_attr_clk_enable.attr,
	&dev_attr_regulator_enable_set.attr,
	NULL
};

static const struct attribute_group fpc_attribute_group = {
	.attrs = fpc_attributes,
};

static int fpc_request_named_gpio(struct fpc_data *fpc,
		const char *label, int *gpio)
{
	struct device *dev = fpc->dev;
	struct device_node *node = dev->of_node;

	int rc = of_get_named_gpio(node, label, 0);

	if (rc < 0) {
		dev_err(dev, "failed to get '%s'\n", label);
		return rc;
	}

	*gpio = rc;
	rc = devm_gpio_request(dev, *gpio, label);
	if (rc) {
		dev_err(dev, "failed to request gpio %d\n", *gpio);
		return rc;
	}

	dev_dbg(dev, "%s %d\n", label, *gpio);
	return 0;
}

int fpc_probe(struct platform_device *pldev,
		struct fpc_gpio_info *fpc_gpio_ops)
{
	struct device *dev = &pldev->dev;
	struct device_node *node = dev->of_node;
	struct fpc_data *fpc;
	int rc;

	fpc = devm_kzalloc(dev, sizeof(*fpc), GFP_KERNEL);
	if (!fpc) {
		rc = -ENOMEM;
		goto exit;
	}

	fpc->dev = dev;
	dev_set_drvdata(dev, fpc);
	fpc->pldev = pldev;
	fpc->hwabs = fpc_gpio_ops;

	if (!node) {
		dev_err(dev, "no of node found\n");
		rc = -EINVAL;
		goto exit;
	}

	rc = fpc->hwabs->init(fpc);
	if (rc) {
		dev_err(dev, "hw init error\n");
		goto exit;
	}

	rc = fpc_request_named_gpio(fpc, "fpc,gpio_rst", &fpc->rst_gpio);
	if (rc) {
		dev_err(dev, "Requesting GPIO for RST failed with %d.\n", rc);
		goto exit;
	}

	rc = fpc->hwabs->configure(fpc);
	if (rc < 0) {
		goto exit;
	}

	dev_dbg(dev, "Using GPIO#%d as RST.\n", fpc->rst_gpio);

	rc = sysfs_create_group(&dev->kobj, &fpc_attribute_group);
	if (rc) {
		dev_err(dev, "could not create sysfs\n");
		goto exit;
	}

	(void)hw_reset(fpc);
	dev_info(dev, "%s: ok\n", __func__);

exit:
	return rc;
}

int fpc_remove(struct platform_device *pldev)
{
	sysfs_remove_group(&pldev->dev.kobj, &fpc_attribute_group);
	dev_info(&pldev->dev, "%s\n", __func__);

	return 0;
}

MODULE_AUTHOR("Liang Meng <liang.meng@fingerprints.com>");
MODULE_DESCRIPTION("Provides fingerprint device driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
