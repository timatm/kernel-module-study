/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _VPM_REGS_H_
#define _VPM_REGS_H_

#include <linux/bits.h>

/*
 * VPM Sensor mock register map
 *
 * This register map models a small embedded sensor device.
 * It will later be reused by the mock hardware model, userspace
 * validation tool, and eventually the real IIO driver.
 */

#define VPM_REG_WHOAMI          0x00
#define VPM_REG_REVISION        0x01
#define VPM_REG_CTRL            0x02
#define VPM_REG_ODR             0x03
#define VPM_REG_STATUS          0x04

#define VPM_REG_TEMP_L          0x10
#define VPM_REG_TEMP_H          0x11
#define VPM_REG_ACCEL_X_L       0x12
#define VPM_REG_ACCEL_X_H       0x13
#define VPM_REG_ACCEL_Y_L       0x14
#define VPM_REG_ACCEL_Y_H       0x15
#define VPM_REG_ACCEL_Z_L       0x16
#define VPM_REG_ACCEL_Z_H       0x17

#define VPM_REG_PM_STATE        0x30

/*
 * Mock-only validation register.
 *
 * This register does not represent a production hardware register.
 * It is used by the virtual hardware model to inject deterministic
 * error conditions for pre-silicon driver validation.
 */
#define VPM_REG_FAULT_INJECT    0x7e

#define VPM_REG_MAX             0x80

#define VPM_WHOAMI_VALUE        0xa5

/* FAULT_INJECT register bits */
#define VPM_FAULT_INVALID_STATUS    BIT(0)
#define VPM_FAULT_DEVICE_BUSY       BIT(1)

/* CTRL register bits */
#define VPM_CTRL_ENABLE         BIT(0)
#define VPM_CTRL_LOW_POWER      BIT(1)
#define VPM_CTRL_FIFO_EN        BIT(2)
#define VPM_CTRL_IRQ_EN         BIT(3)
#define VPM_CTRL_WAKEUP_EN      BIT(4)

/* STATUS register bits */
#define VPM_STATUS_DATA_READY   BIT(0)
#define VPM_STATUS_FIFO_FULL    BIT(1)
#define VPM_STATUS_FAULT        BIT(2)
#define VPM_STATUS_PM_SUSPENDED BIT(3)

/* PM_STATE values */
#define VPM_PM_ACTIVE           0
#define VPM_PM_IDLE             1
#define VPM_PM_RUNTIME_SUSPEND  2
#define VPM_PM_SYSTEM_SUSPEND   3

#define VPM_FAULT_INVALID_STATUS    BIT(0)
#define VPM_FAULT_DEVICE_BUSY       BIT(1)

#endif /* _VPM_REGS_H_ */