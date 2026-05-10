#include <linux/errno.h>
#include <linux/string.h>

#include "vpm_mock_bus.h"
#include "vpm_regs.h"

static int vpm_mock_read_u8(void *ctx, u8 reg, u8 *val)
{
    struct vpm_mock_hw *hw = ctx;

    if (!hw || !val)
        return -EINVAL;

    if (reg >= VPM_REG_MAX)
        return -EINVAL;

    *val = hw->regs[reg];
    return 0;
}

static int vpm_mock_write_u8(void *ctx, u8 reg, u8 val)
{
    struct vpm_mock_hw *hw = ctx;

    if (!hw)
        return -EINVAL;

    if (reg >= VPM_REG_MAX)
        return -EINVAL;

    if (reg == VPM_REG_WHOAMI)
        return -EINVAL;

    hw->regs[reg] = val;
    return 0;
}

static int vpm_mock_bulk_read(void *ctx, u8 reg, u8 *buf, size_t len)
{
    struct vpm_mock_hw *hw = ctx;

    if (!hw || !buf)
        return -EINVAL;

    if (reg >= VPM_REG_MAX || len > VPM_REG_MAX - reg)
        return -EINVAL;

    memcpy(buf, &hw->regs[reg], len);
    return 0;
}

static int vpm_mock_bulk_write(void *ctx, u8 reg, const u8 *buf, size_t len)
{
    struct vpm_mock_hw *hw = ctx;

    if (!hw || !buf)
        return -EINVAL;

    if (reg >= VPM_REG_MAX || len > VPM_REG_MAX - reg)
        return -EINVAL;

    memcpy(&hw->regs[reg], buf, len);
    return 0;
}

const struct vpm_bus_ops vpm_mock_bus_ops = {
    .read_u8 = vpm_mock_read_u8,
    .write_u8 = vpm_mock_write_u8,
    .bulk_read = vpm_mock_bulk_read,
    .bulk_write = vpm_mock_bulk_write,
};

void vpm_mock_bus_init_hw(struct vpm_mock_hw *hw,
                          unsigned int mock_revision,
                          unsigned int default_odr_hz,
                          bool low_power_default)
{
    u8 ctrl = VPM_CTRL_ENABLE;

    if (!hw)
        return;

    memset(hw->regs, 0, sizeof(hw->regs));

    hw->regs[VPM_REG_WHOAMI] = VPM_WHOAMI_VALUE;
    hw->regs[VPM_REG_REVISION] = mock_revision & 0xff;
    hw->regs[VPM_REG_ODR] = default_odr_hz & 0xff;
    hw->regs[VPM_REG_PM_STATE] = VPM_PM_ACTIVE;
    hw->regs[VPM_REG_FAULT_INJECT] = 0x00;

    if (low_power_default)
        ctrl |= VPM_CTRL_LOW_POWER;

    hw->regs[VPM_REG_CTRL] = ctrl;
}