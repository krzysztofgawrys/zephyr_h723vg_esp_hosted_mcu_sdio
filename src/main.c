/*
 * Copyright (c) 2023, Bjarki Arge Andreasen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led), gpios);

int main(void)
{
	gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

	while (true) {
		gpio_pin_set(led.port, led.pin, true);
		k_msleep(100);
		gpio_pin_set(led.port, led.pin, false);
		k_msleep(100);
	}

	return 0;
}
