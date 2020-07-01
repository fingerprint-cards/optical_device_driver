/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */
#ifndef _FPC16XX_H_
#define _FPC16XX_H_

#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/pm_wakeup.h>
#include <linux/regulator/consumer.h>

#define MAX_PCTL_NAMNES    2

struct fpc_gpio_info;

struct vreg_config {
	char *name;
	unsigned long vmin;
	unsigned long vmax;
	int ua_load;
};

static struct vreg_config const vreg_conf[] = {
	{ "vdd_pwr", 3000000UL, 3000000UL, 25000, },
};

struct fpc_data {
	struct device *dev;
	struct platform_device *pldev;
	struct pinctrl *fingerprint_pinctrl;
	struct pinctrl_state *pinctrl_state[MAX_PCTL_NAMNES];
	struct regulator *vreg[ARRAY_SIZE(vreg_conf)];
	int rst_gpio;
	struct mutex lock; /* To set/get exported values in sysfs */
	bool prepared;
#ifdef CONFIG_FPC_COMPAT
	bool compatible_enabled;
#endif
	const struct fpc_gpio_info *hwabs;
};

struct fpc_gpio_info {
	int (*init)(struct fpc_data *fpc);
	int (*configure)(struct fpc_data *fpc);
	int (*get_val)(unsigned int gpio);
	void (*set_val)(unsigned int gpio, int val);
	ssize_t (*clk_enable_set)(struct fpc_data *fpc, const char *buf, size_t count);
	int (*vreg_setup)(struct fpc_data *fpc, const char *name, bool enable);
	void *priv;
};

extern int fpc_probe(struct platform_device *pldev, struct fpc_gpio_info *fpc_gpio_ops);

extern int fpc_remove(struct platform_device *pldev);
#endif
