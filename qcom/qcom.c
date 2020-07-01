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
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/bitmap.h>

#include "fpc16xx.h"

const char * const pctl_names[] = {
	"fpc16xx_reset_reset",
	"fpc16xx_reset_active",
};

struct priv_data {
	struct clk *spi_main;
};

#define PWR_ON_SLEEP_MIN_US        10000
#define PWR_ON_SLEEP_MAX_US        (PWR_ON_SLEEP_MIN_US<<1)

/**
 * Will try to select the set of pins (GPIOS) defined in a pin control node of
 * the device tree named @p name.
 *
 * The node can contain several eg. GPIOs that is controlled when selecting it.
 * The node may activate or deactivate the pins it contains, the action is
 * defined in the device tree node itself and not here. The states used
 * internally is fetched at probe time.
 *
 */
static int select_pin_ctl(struct fpc_data *fpc16xx, const char *name)
{
	size_t i;
	int rc;
	int size;
	struct device *dev = fpc16xx->dev;

	size = ARRAY_SIZE(fpc16xx->pinctrl_state);
	for (i = 0; i < size; i++) {
		const char *n = pctl_names[i];

		if (!strncmp(n, name, strlen(n))) {
			rc = pinctrl_select_state(fpc16xx->fingerprint_pinctrl,
					fpc16xx->pinctrl_state[i]);
			if (rc) {
				dev_err(dev, "cannot select '%s'\n", name);
			} else {
				dev_dbg(dev, "Selected '%s'\n", name);
			}
			goto exit;
		}
	}

	rc = -EINVAL;
	dev_err(dev, "%s:'%s' not found\n", __func__, name);

exit:
	return rc;
}

static int vreg_setup(struct fpc_data *fpc16xx, const char *name,
		 bool enable)
{
	size_t i;
	int rc;
	struct regulator *vreg;
	struct device *dev = fpc16xx->dev;
	int size;

	size = ARRAY_SIZE(vreg_conf);
	for (i = 0; i < size; i++) {
		const char *n = vreg_conf[i].name;

		if (!memcmp(n, name, strlen(n))) {
			goto found;
		}
	}

	dev_err(dev, "Regulator %s not found\n", name);

	return -EINVAL;

found:
	vreg = fpc16xx->vreg[i];
	if (enable) {
		if (!vreg) {
			vreg = devm_regulator_get(dev, name);
			if (IS_ERR_OR_NULL(vreg)) {
				dev_err(dev, "Unable to get %s\n", name);
				return PTR_ERR(vreg);
			}
		}

		if (regulator_count_voltages(vreg) > 0) {
			rc = regulator_set_voltage(vreg,
				vreg_conf[i].vmin,
				vreg_conf[i].vmax);
			if (rc) {
				dev_err(dev, "Unable to set voltage on %s, %d\n", name, rc);
			}
		}

		rc = regulator_set_load(vreg, vreg_conf[i].ua_load);
		if (rc < 0) {
			dev_err(dev, "Unable to set current on %s, %d\n", name, rc);
		}

		rc = regulator_enable(vreg);
		if (rc) {
			dev_err(dev, "error enabling %s: %d\n", name, rc);
			vreg = NULL;
		}
		fpc16xx->vreg[i] = vreg;
	} else {
		if (vreg) {
			if (regulator_is_enabled(vreg)) {
				regulator_disable(vreg);
				dev_dbg(dev, "disabled %s\n", name);
			}
			fpc16xx->vreg[i] = NULL;
		}
		rc = 0;
	}

	return rc;
}

/**
 * Will setup GPIOs, and regulators to correctly initialize the fingerprint
 * sensor to be ready for work.
 *
 * In the correct order according to the sensor spec this function will
 * enable/disable regulators, and reset line, all to set the sensor in a
 * correct power on or off state "electrical" wise.
 *
 * @note This function will not send any commands to the sensor it will only
 *       control it "electrically".
 */
static int device_prepare(struct fpc_data *fpc16xx, bool enable)
{
	int rc = 0;

	if (enable && !fpc16xx->prepared) {
		fpc16xx->prepared = true;
		select_pin_ctl(fpc16xx, "fpc16xx_reset_reset");

		rc = vreg_setup(fpc16xx, "vdd_pwr", true);
		if (rc) {
			(void)vreg_setup(fpc16xx, "vdd_pwr", false);
			fpc16xx->prepared = false;
		}

		usleep_range(PWR_ON_SLEEP_MIN_US, PWR_ON_SLEEP_MAX_US);

		/* As we can't control chip select here the other part of the
		 * sensor driver eg. the TEE driver needs to do a _SOFT_ reset
		 * on the sensor after power up to be sure that the sensor is
		 * in a good state after power up. Okeyed by ASIC.
		 */

		(void)select_pin_ctl(fpc16xx, "fpc16xx_reset_active");
	} else if (!enable && fpc16xx->prepared) {
		rc = 0;
		(void)select_pin_ctl(fpc16xx, "fpc16xx_reset_reset");

		usleep_range(PWR_ON_SLEEP_MIN_US, PWR_ON_SLEEP_MAX_US);

		(void)vreg_setup(fpc16xx, "vdd_pwr", false);
		fpc16xx->prepared = false;
	}

	return rc;
}


static int qcom_init(struct fpc_data *fpc)
{
	return 0;
}

static int qcom_configure(struct fpc_data *fpc16xx)
{
	struct device *dev = fpc16xx->dev;
	int rc = 0;
	int i = 0;

	fpc16xx->fingerprint_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(fpc16xx->fingerprint_pinctrl)) {
		if (PTR_ERR(fpc16xx->fingerprint_pinctrl) == -EPROBE_DEFER) {
			dev_info(dev, "pinctrl not ready\n");
			rc = -EPROBE_DEFER;
			goto exit;
		}
		dev_err(dev, "Target does not use pinctrl\n");
		fpc16xx->fingerprint_pinctrl = NULL;
		rc = -EINVAL;
		goto exit;
	}

	for (i = 0; i < ARRAY_SIZE(pctl_names); i++) {
		const char *n = pctl_names[i];
		struct pinctrl_state *state =
			pinctrl_lookup_state(fpc16xx->fingerprint_pinctrl, n);
		if (IS_ERR(state)) {
			dev_err(dev, "cannot find '%s'\n", n);
			rc = -EINVAL;
			goto exit;
		}
		dev_info(dev, "found pin control %s\n", n);
		fpc16xx->pinctrl_state[i] = state;
	}

	(void)device_prepare(fpc16xx, true);
exit:
	return rc;
}

static struct fpc_gpio_info qcom_ops = {
	.init           = qcom_init,
	.vreg_setup     = vreg_setup,
	.configure      = qcom_configure,
	.get_val        = gpio_get_value,
	.set_val        = gpio_set_value,
	.clk_enable_set = NULL,
};

static const struct of_device_id qcom_of_match[] = {
	{ .compatible = "fpc,fpc16xx", },
	{}
};

MODULE_DEVICE_TABLE(of, qcom_of_match);

static int qcom_probe(struct platform_device *pldev)
{
	int rc;

	qcom_ops.priv = devm_kzalloc(&pldev->dev, sizeof(struct priv_data), GFP_KERNEL);

	if (!qcom_ops.priv) {
		dev_err(&pldev->dev, "failed to allocate memory for struct priv_data\n");
		return -ENOMEM;
	}

	rc = fpc_probe(pldev, &qcom_ops);

	return rc;
}

static struct platform_driver qcom_driver = {
	.driver = {
		.name           = "fpc16xx",
		.owner          = THIS_MODULE,
		.of_match_table = qcom_of_match,
	},
	.probe  = qcom_probe,
	.remove = fpc_remove
};

module_platform_driver(qcom_driver);

MODULE_AUTHOR("Liang Meng <liang.meng@fingerprints.com>");
MODULE_DESCRIPTION("Provides fingerprint device driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
