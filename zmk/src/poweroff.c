/*
 * Copyright (c) 2025 scholzri
 * SPDX-License-Identifier: Apache-2.0
 *
 * Deep sleep (power off) implementation for Telink B91 (TLSR951x).
 *
 * Called by Zephyr's sys_poweroff() when ZMK's idle sleep triggers
 * (CONFIG_ZMK_SLEEP). Enters deep sleep with GPIO keypress wakeup.
 *
 * Uses DEEPSLEEP_MODE (no SRAM retention) — on wakeup, the MCU does a
 * full cold boot through MCUboot. Retention mode (0x03) doesn't work
 * with MCUboot because the boot ROM reloads MCUboot into ILM on reset,
 * overwriting retained app code.
 *
 * On wakeup, the MCU resets from the reset vector — this function
 * never returns.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(poweroff_b91, LOG_LEVEL_INF);

/* -------------------------------------------------------------------------
 * Hardware register addresses (direct access, avoiding SDK headers)
 * ----------------------------------------------------------------------- */

#define GPIO_PC_OUT     0x80140313UL

/* Analog register serial interface */
#define ANA_ADDR_REG    0x80140180UL
#define ANA_CTRL_REG    0x80140182UL
#define ANA_LEN_REG     0x80140183UL
#define ANA_DATA_REG    0x80140184UL
#define ANA_CTRL_CYC    BIT(6)
#define ANA_CTRL_RW     BIT(5)  /* 1=write, 0=read */
#define ANA_CTRL_BUSY   BIT(7)

/* PM sleep modes and wakeup sources (from SDK pm.h) */
#define DEEPSLEEP_MODE  0x30
#define PM_WAKEUP_PAD   BIT(3)
#define PM_TICK_STIMER_16M 0

/* GPIO pin encoding (from SDK gpio.h): group | bit */
#define GPIO_GROUPD     0x300
#define GPIO_PD2        (GPIO_GROUPD | BIT(2))
#define GPIO_PD3        (GPIO_GROUPD | BIT(3))
#define GPIO_PD4        (GPIO_GROUPD | BIT(4))
#define GPIO_PD5        (GPIO_GROUPD | BIT(5))
#define GPIO_PD6        (GPIO_GROUPD | BIT(6))
#define GPIO_PD7        (GPIO_GROUPD | BIT(7))

#define WAKEUP_LEVEL_LOW 0

/* -------------------------------------------------------------------------
 * SDK function declarations (from blob / HAL)
 * ----------------------------------------------------------------------- */
typedef unsigned int gpio_pin_e;
typedef unsigned int pm_gpio_wakeup_level_e;
typedef unsigned int pm_sleep_mode_e;
typedef unsigned int pm_sleep_wakeup_src_e;
typedef unsigned int pm_wakeup_tick_type_e;

extern void pm_set_gpio_wakeup(gpio_pin_e pin, pm_gpio_wakeup_level_e pol,
			       int en);
extern int pm_sleep_wakeup(pm_sleep_mode_e sleep_mode,
			   pm_sleep_wakeup_src_e wakeup_src,
			   pm_wakeup_tick_type_e wakeup_tick_type,
			   unsigned int wakeup_tick);

/* -------------------------------------------------------------------------
 * Analog register read/write (analog domain survives deep sleep)
 * ----------------------------------------------------------------------- */
static uint8_t analog_read(uint8_t addr)
{
	unsigned int key = irq_lock();

	sys_write8(addr, ANA_ADDR_REG);
	sys_write8(1, ANA_LEN_REG);
	sys_write8(ANA_CTRL_CYC, ANA_CTRL_REG);  /* read: RW bit clear */
	while (sys_read8(ANA_CTRL_REG) & ANA_CTRL_BUSY) {
	}
	uint8_t val = sys_read8(ANA_DATA_REG);

	irq_unlock(key);
	return val;
}

static void analog_write(uint8_t addr, uint8_t val)
{
	unsigned int key = irq_lock();

	sys_write8(addr, ANA_ADDR_REG);
	sys_write8(val, ANA_DATA_REG);
	sys_write8(1, ANA_LEN_REG);
	sys_write8(ANA_CTRL_CYC | ANA_CTRL_RW, ANA_CTRL_REG);
	while (sys_read8(ANA_CTRL_REG) & ANA_CTRL_BUSY) {
	}

	irq_unlock(key);
}

/* -------------------------------------------------------------------------
 * z_sys_poweroff — called by Zephyr's sys_poweroff() with IRQs locked.
 *
 * Prepares GPIO matrix for keypress wakeup detection, turns off LEDs,
 * and enters deep sleep. The MCU cold-boots on wakeup.
 * ----------------------------------------------------------------------- */
void z_sys_poweroff(void)
{
	LOG_INF("Entering deep sleep (GPIO wakeup)");

	/* Turn off RGB LED power (PC2 = MOSFET gate, active-high) */
	sys_write8(sys_read8(GPIO_PC_OUT) & ~BIT(2), GPIO_PC_OUT);

	/* Disable USB DP pull-up to cleanly detach from host.
	 * Analog register 0x0B bit 7 = DP pull-up enable. */
	analog_write(0x0B, 0x00);

	/* Configure 100K pull-downs on column pins and 1M pull-ups on row pins
	 * via analog registers. These survive deep sleep (digital GPIO doesn't).
	 *
	 * B91 pull registers: 2 bits per pin, 4 pins per register.
	 *   reg = 0x0E + (port * 2) + (pin >= 4 ? 1 : 0)
	 *   pin N → bits [(N&3)*2+1 : (N&3)*2]
	 *   Values: 0=none, 1=1M pull-up, 2=100K pull-down, 3=10K pull-up
	 *
	 * With columns held LOW (100K pull-down) and rows HIGH (1M pull-up),
	 * a keypress forward-biases the matrix diode → row goes LOW → wakeup.
	 *
	 * Crush 80 Matrix:
	 *   Columns (16): PE0-PE2, PE4-PE6, PB0-PB6, PC0, PC1, PC4
	 *   Rows (6):     PD7 (Row 0), PD2 (Row 1), PD3 (Row 2), PD4 (Row 3), PD5 (Row 4), PD6 (Row 5) */
#define PULL_DOWN  2  /* 100K pull-down */
#define PULL_UP    1  /* 1M pull-up */

	/* 1. Pull-downs on Column pins */
	/* 0x10: PB0-PB3 all 100K down */
	analog_write(0x10, (PULL_DOWN << 0) | (PULL_DOWN << 2) |
			   (PULL_DOWN << 4) | (PULL_DOWN << 6));
	/* 0x11: PB4-PB6 100K down, PB7 (LED MOSI) unchanged */
	analog_write(0x11, (analog_read(0x11) & 0xC0) |
			   (PULL_DOWN << 0) | (PULL_DOWN << 2) | (PULL_DOWN << 4));
	/* 0x12: PC0-PC1 100K down, PC2 (LED MOSFET) and PC3 unchanged */
	analog_write(0x12, (analog_read(0x12) & 0xF0) |
			   (PULL_DOWN << 0) | (PULL_DOWN << 2));
	/* 0x13: PC4 100K down, PC5-PC7 unchanged */
	analog_write(0x13, (analog_read(0x13) & 0xFC) | (PULL_DOWN << 0));
	/* 0x16: PE0-PE2 100K down, PE3 unchanged */
	analog_write(0x16, (analog_read(0x16) & 0xC0) |
			   (PULL_DOWN << 0) | (PULL_DOWN << 2) | (PULL_DOWN << 4));
	/* 0x17: PE4-PE6 100K down, PE7 unchanged */
	analog_write(0x17, (analog_read(0x17) & 0xC0) |
			   (PULL_DOWN << 0) | (PULL_DOWN << 2) | (PULL_DOWN << 4));

	/* 2. Pull-ups on Row pins */
	/* 0x14: PD2-PD3 1M up, PD0-PD1 unchanged */
	analog_write(0x14, (analog_read(0x14) & 0x0F) |
			   (PULL_UP << 4) | (PULL_UP << 6));
	/* 0x15: PD4-PD7 all 1M up (PD4, PD5, PD6, and PD7 Row 0) */
	analog_write(0x15, (PULL_UP << 0) | (PULL_UP << 2) |
			   (PULL_UP << 4) | (PULL_UP << 6));

	/* Configure all 6 row pins as wakeup sources (LOW level = key pressed). */
	pm_set_gpio_wakeup(GPIO_PD7, WAKEUP_LEVEL_LOW, 1);
	pm_set_gpio_wakeup(GPIO_PD2, WAKEUP_LEVEL_LOW, 1);
	pm_set_gpio_wakeup(GPIO_PD3, WAKEUP_LEVEL_LOW, 1);
	pm_set_gpio_wakeup(GPIO_PD4, WAKEUP_LEVEL_LOW, 1);
	pm_set_gpio_wakeup(GPIO_PD5, WAKEUP_LEVEL_LOW, 1);
	pm_set_gpio_wakeup(GPIO_PD6, WAKEUP_LEVEL_LOW, 1);

	/* Enter deep sleep — MCU cold-boots on wakeup via reset vector. */
	pm_sleep_wakeup(DEEPSLEEP_MODE, PM_WAKEUP_PAD,
			PM_TICK_STIMER_16M, 0);

	/* Never reached — MCU resets on wakeup */
	CODE_UNREACHABLE;
}
