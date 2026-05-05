// SPDX-License-Identifier: GPL-2.0
/*
 * VPM Sensor Lab - Kernel Module Skeleton
 *
 * M1 goal:
 *   - Define a mock register map
 *   - Initialize fake hardware registers
 *   - Read/write mock registers through helper functions
 *   - Validate WHOAMI like a real hardware driver
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>

#include "vpm_regs.h"

static unsigned int mock_revision = 1;
static unsigned int default_odr_hz = 10;
static bool low_power_default = false;

module_param(mock_revision, uint, 0644);
MODULE_PARM_DESC(mock_revision, "Mock silicon revision");

module_param(default_odr_hz, uint, 0644);
MODULE_PARM_DESC(default_odr_hz, "Default output data rate in Hz");

module_param(low_power_default, bool, 0644);
MODULE_PARM_DESC(low_power_default, "Start device in low-power mode");

struct vpm_mock_chip {
    u8 regs[VPM_REG_MAX];
};

static struct vpm_mock_chip vpm_chip;

static int vpm_mock_read_reg(struct vpm_mock_chip *chip, u8 reg, u8 *val)
{
    if (!chip || !val)
        return -EINVAL;

    if (reg >= VPM_REG_MAX)
        return -EINVAL;

    *val = chip->regs[reg];

    return 0;
}

static int vpm_mock_write_reg(struct vpm_mock_chip *chip, u8 reg, u8 val)
{
    if (!chip)
        return -EINVAL;

    if (reg >= VPM_REG_MAX)
        return -EINVAL;

    /*
     * WHOAMI is read-only in real hardware.
     * Keep the same behavior in the mock model.
     */
    if (reg == VPM_REG_WHOAMI)
        return -EINVAL;

    chip->regs[reg] = val;

    return 0;
}

static void vpm_mock_init_regs(struct vpm_mock_chip *chip)
{
    u8 ctrl = VPM_CTRL_ENABLE;

    memset(chip->regs, 0, sizeof(chip->regs));

    chip->regs[VPM_REG_WHOAMI] = VPM_WHOAMI_VALUE;
    chip->regs[VPM_REG_REVISION] = mock_revision & 0xff;
    chip->regs[VPM_REG_ODR] = default_odr_hz & 0xff;
    chip->regs[VPM_REG_PM_STATE] = VPM_PM_ACTIVE;

    if (low_power_default)
        ctrl |= VPM_CTRL_LOW_POWER;

    chip->regs[VPM_REG_CTRL] = ctrl;
}

static int vpm_mock_validate_chip(struct vpm_mock_chip *chip)
{
    int ret;
    u8 val;

    ret = vpm_mock_read_reg(chip, VPM_REG_WHOAMI, &val);
    if (ret)
        return ret;

    if (val != VPM_WHOAMI_VALUE) {
        pr_err("vpm_skeleton: invalid WHOAMI: 0x%02x\n", val);
        return -ENODEV;
    }

    return 0;
}

static void vpm_mock_dump_regs(struct vpm_mock_chip *chip)
{
    u8 whoami;
    u8 revision;
    u8 ctrl;
    u8 odr;
    u8 status;
    u8 pm_state;

    vpm_mock_read_reg(chip, VPM_REG_WHOAMI, &whoami);
    vpm_mock_read_reg(chip, VPM_REG_REVISION, &revision);
    vpm_mock_read_reg(chip, VPM_REG_CTRL, &ctrl);
    vpm_mock_read_reg(chip, VPM_REG_ODR, &odr);
    vpm_mock_read_reg(chip, VPM_REG_STATUS, &status);
    vpm_mock_read_reg(chip, VPM_REG_PM_STATE, &pm_state);

    pr_info("vpm_skeleton: WHOAMI=0x%02x\n", whoami);
    pr_info("vpm_skeleton: REVISION=%u\n", revision);
    pr_info("vpm_skeleton: CTRL=0x%02x\n", ctrl);
    pr_info("vpm_skeleton: ODR=%u Hz\n", odr);
    pr_info("vpm_skeleton: STATUS=0x%02x\n", status);
    pr_info("vpm_skeleton: PM_STATE=%u\n", pm_state);
}

static int __init vpm_skeleton_init(void)
{
    int ret;

    pr_info("vpm_skeleton: init\n");
    pr_info("vpm_skeleton: initializing mock register map\n");

    vpm_mock_init_regs(&vpm_chip);
    vpm_mock_dump_regs(&vpm_chip);

    ret = vpm_mock_validate_chip(&vpm_chip);
    if (ret) {
        pr_err("vpm_skeleton: mock chip validation failed: %d\n", ret);
        return ret;
    }

    pr_info("vpm_skeleton: mock chip initialized successfully\n");

    return 0;
}

static void __exit vpm_skeleton_exit(void)
{
    pr_info("vpm_skeleton: exit\n");
}

module_init(vpm_skeleton_init);
module_exit(vpm_skeleton_exit);

MODULE_AUTHOR("timatm");
MODULE_DESCRIPTION("VPM Sensor Lab - M1 mock register map");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2");