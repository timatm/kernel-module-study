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

#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

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

    struct mutex lock;
    struct dentry *debugfs_dir;

    struct delayed_work data_work;
    bool data_work_enabled;

    u32 sample_counter;

    s16 temp_raw;
    s16 accel_x_raw;
    s16 accel_y_raw;
    s16 accel_z_raw;
};
static struct vpm_mock_chip vpm_chip;

static unsigned int vpm_mock_get_period_ms_locked(struct vpm_mock_chip *chip)
{
    u8 odr;

    odr = chip->regs[VPM_REG_ODR];

    if (odr == 0)
        return 0;

    return max(1U, 1000U / odr);
}

static void vpm_mock_write_s16_regs_locked(struct vpm_mock_chip *chip,
                                           u8 reg_l,
                                           s16 value)
{
    chip->regs[reg_l] = value & 0xff;
    chip->regs[reg_l + 1] = (value >> 8) & 0xff;
}


static void vpm_mock_update_sample_locked(struct vpm_mock_chip *chip)
{
    chip->sample_counter++;

    chip->temp_raw = 25000 + (chip->sample_counter % 100);
    chip->accel_x_raw = chip->sample_counter % 64;
    chip->accel_y_raw = -(chip->sample_counter % 32);
    chip->accel_z_raw = 1024;

    vpm_mock_write_s16_regs_locked(chip, VPM_REG_TEMP_L,
                                   chip->temp_raw);
    vpm_mock_write_s16_regs_locked(chip, VPM_REG_ACCEL_X_L,
                                   chip->accel_x_raw);
    vpm_mock_write_s16_regs_locked(chip, VPM_REG_ACCEL_Y_L,
                                   chip->accel_y_raw);
    vpm_mock_write_s16_regs_locked(chip, VPM_REG_ACCEL_Z_L,
                                   chip->accel_z_raw);

    chip->regs[VPM_REG_STATUS] |= VPM_STATUS_DATA_READY;
}

static void vpm_data_workfn(struct work_struct *work)
{
    struct delayed_work *dwork;
    struct vpm_mock_chip *chip;
    unsigned int period_ms;

    dwork = to_delayed_work(work);
    chip = container_of(dwork, struct vpm_mock_chip, data_work);

    mutex_lock(&chip->lock);

    period_ms = vpm_mock_get_period_ms_locked(chip);
    if (period_ms == 0) {
        chip->data_work_enabled = false;
        mutex_unlock(&chip->lock);
        return;
    }

    chip->data_work_enabled = true;
    vpm_mock_update_sample_locked(chip);

    mutex_unlock(&chip->lock);

    schedule_delayed_work(&chip->data_work,
                          msecs_to_jiffies(period_ms));
}

static void vpm_mock_restart_data_work(struct vpm_mock_chip *chip)
{
    unsigned int period_ms;

    cancel_delayed_work_sync(&chip->data_work);

    mutex_lock(&chip->lock);
    period_ms = vpm_mock_get_period_ms_locked(chip);
    chip->data_work_enabled = period_ms != 0;
    mutex_unlock(&chip->lock);

    if (period_ms != 0)
        schedule_delayed_work(&chip->data_work,
                              msecs_to_jiffies(period_ms));
}

static int vpm_mock_read_reg(struct vpm_mock_chip *chip, u8 reg, u8 *val)
{
    if (!chip || !val)
        return -EINVAL;

    if (reg >= VPM_REG_MAX)
        return -EINVAL;

    mutex_lock(&chip->lock);
    *val = chip->regs[reg];
    mutex_unlock(&chip->lock);

    return 0;
}

static ssize_t vpm_debugfs_read_u8_reg(struct file *file,
                                       char __user *user_buf,
                                       size_t count,
                                       loff_t *ppos,
                                       u8 reg)
{
    struct vpm_mock_chip *chip = file->private_data;
    char buf[32];
    u8 val;
    int ret;
    int len;

    ret = vpm_mock_read_reg(chip, reg, &val);
    if (ret)
        return ret;

    len = scnprintf(buf, sizeof(buf), "0x%02x\n", val);

    return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static bool vpm_mock_fault_enabled(struct vpm_mock_chip *chip, u8 fault)
{
    bool enabled;

    mutex_lock(&chip->lock);
    enabled = chip->regs[VPM_REG_FAULT_INJECT] & fault;
    mutex_unlock(&chip->lock);

    return enabled;
}

static void vpm_mock_apply_faults_locked(struct vpm_mock_chip *chip)
{
    u8 fault = chip->regs[VPM_REG_FAULT_INJECT];

    if (fault & VPM_FAULT_INVALID_STATUS)
        chip->regs[VPM_REG_STATUS] |= VPM_STATUS_FAULT;
    else
        chip->regs[VPM_REG_STATUS] &= ~VPM_STATUS_FAULT;
}

static int vpm_mock_write_reg(struct vpm_mock_chip *chip, u8 reg, u8 val)
{
    if (!chip)
        return -EINVAL;

    if (reg >= VPM_REG_MAX)
        return -EINVAL;

    if (reg == VPM_REG_WHOAMI)
        return -EINVAL;

    mutex_lock(&chip->lock);
    chip->regs[reg] = val;
    if (reg == VPM_REG_FAULT_INJECT)
        vpm_mock_apply_faults_locked(chip);
    mutex_unlock(&chip->lock);

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
    chip->regs[VPM_REG_FAULT_INJECT] = 0x00;

    chip->sample_counter = 0;
    chip->temp_raw = 25000;
    chip->accel_x_raw = 0;
    chip->accel_y_raw = 0;
    chip->accel_z_raw = 1024;
    chip->data_work_enabled = false;

    vpm_mock_write_s16_regs_locked(chip, VPM_REG_TEMP_L,
                                   chip->temp_raw);
    vpm_mock_write_s16_regs_locked(chip, VPM_REG_ACCEL_X_L,
                                   chip->accel_x_raw);
    vpm_mock_write_s16_regs_locked(chip, VPM_REG_ACCEL_Y_L,
                                   chip->accel_y_raw);
    vpm_mock_write_s16_regs_locked(chip, VPM_REG_ACCEL_Z_L,
                                   chip->accel_z_raw);

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

static ssize_t vpm_whoami_read(struct file *file,
                               char __user *user_buf,
                               size_t count,
                               loff_t *ppos)
{
    return vpm_debugfs_read_u8_reg(file, user_buf, count, ppos,
                                   VPM_REG_WHOAMI);
}

static ssize_t vpm_revision_read(struct file *file,
                                 char __user *user_buf,
                                 size_t count,
                                 loff_t *ppos)
{
    return vpm_debugfs_read_u8_reg(file, user_buf, count, ppos,
                                   VPM_REG_REVISION);
}

static ssize_t vpm_ctrl_read(struct file *file,
                             char __user *user_buf,
                             size_t count,
                             loff_t *ppos)
{
    return vpm_debugfs_read_u8_reg(file, user_buf, count, ppos,
                                   VPM_REG_CTRL);
}

static ssize_t vpm_status_read(struct file *file,
                               char __user *user_buf,
                               size_t count,
                               loff_t *ppos)
{
    return vpm_debugfs_read_u8_reg(file, user_buf, count, ppos,
                                   VPM_REG_STATUS);
}

static ssize_t vpm_pm_state_read(struct file *file,
                                 char __user *user_buf,
                                 size_t count,
                                 loff_t *ppos)
{
    return vpm_debugfs_read_u8_reg(file, user_buf, count, ppos,
                                   VPM_REG_PM_STATE);
}

static ssize_t vpm_odr_read(struct file *file,
                            char __user *user_buf,
                            size_t count,
                            loff_t *ppos)
{
    struct vpm_mock_chip *chip = file->private_data;
    char buf[32];
    u8 val;
    int ret;
    int len;

    ret = vpm_mock_read_reg(chip, VPM_REG_ODR, &val);
    if (ret)
        return ret;

    len = scnprintf(buf, sizeof(buf), "%u\n", val);

    return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t vpm_odr_write(struct file *file,
                             const char __user *user_buf,
                             size_t count,
                             loff_t *ppos)
{
    struct vpm_mock_chip *chip = file->private_data;
    char buf[32];
    unsigned int val;
    size_t len;
    int ret;

    len = min(count, sizeof(buf) - 1);

    if (copy_from_user(buf, user_buf, len))
        return -EFAULT;

    buf[len] = '\0';

    ret = kstrtouint(buf, 0, &val);
    if (ret)
        return ret;

    if (val > 255)
        return -EINVAL;

    if (vpm_mock_fault_enabled(chip, VPM_FAULT_DEVICE_BUSY)) {
        pr_info("vpm_skeleton: reject ODR update because device is busy\n");
        return -EBUSY;
    }

    ret = vpm_mock_write_reg(chip, VPM_REG_ODR, val);
	if (ret)
		return ret;

	vpm_mock_restart_data_work(chip);

	pr_info("vpm_skeleton: ODR updated to %u Hz\n", val);
	return count;
}

static ssize_t vpm_fault_read(struct file *file,
                              char __user *user_buf,
                              size_t count,
                              loff_t *ppos)
{
    struct vpm_mock_chip *chip = (struct vpm_mock_chip *)file->private_data;
    char buf[32];
    u8 val;
    int ret;
    int len;

    ret = vpm_mock_read_reg(chip,VPM_REG_FAULT_INJECT, &val);
    if (ret)
        return ret;

    len = scnprintf(buf, sizeof(buf), "0x%02x\n", val);

    return simple_read_from_buffer(user_buf, count, ppos, buf, len);

}

static ssize_t vpm_fault_write(struct file *file,
                               const char __user *user_buf,
                               size_t count,
                               loff_t *ppos)
{
    struct vpm_mock_chip *chip = file->private_data;
    char buf[32];
    unsigned int val;
    size_t len;
    int ret;

    len = min(count, sizeof(buf) - 1);

    if (copy_from_user(buf, user_buf, len))
        return -EFAULT;

    buf[len] = '\0';

    ret = kstrtouint(buf, 0, &val);
    if (ret)
        return ret;

    if (val > 0xff)
        return -EINVAL;

    if (val & ~(VPM_FAULT_INVALID_STATUS | VPM_FAULT_DEVICE_BUSY))
        return -EINVAL;

    ret = vpm_mock_write_reg(chip, VPM_REG_FAULT_INJECT, val);
    if (ret)
        return ret;

    pr_info("vpm_skeleton: fault_inject updated to 0x%02x\n", val);

    return count;
}
static ssize_t vpm_sample_read(struct file *file,
                               char __user *user_buf,
                               size_t count,
                               loff_t *ppos)
{
    struct vpm_mock_chip *chip = file->private_data;
    char buf[256];
    int len;

    mutex_lock(&chip->lock);

    len = scnprintf(buf, sizeof(buf),
                    "sample_counter: %u\n"
                    "temp_raw      : %d\n"
                    "accel_x_raw   : %d\n"
                    "accel_y_raw   : %d\n"
                    "accel_z_raw   : %d\n"
                    "status        : 0x%02x\n"
                    "odr_hz        : %u\n"
                    "data_work     : %s\n",
                    chip->sample_counter,
                    chip->temp_raw,
                    chip->accel_x_raw,
                    chip->accel_y_raw,
                    chip->accel_z_raw,
                    chip->regs[VPM_REG_STATUS],
                    chip->regs[VPM_REG_ODR],
                    chip->data_work_enabled ? "enabled" : "disabled");

    mutex_unlock(&chip->lock);

    return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations vpm_sample_fops = {
    .owner = THIS_MODULE,
    .open = simple_open,
    .read = vpm_sample_read,
    .llseek = default_llseek,
};

static const struct file_operations vpm_fault_fops = {
    .owner = THIS_MODULE,
    .open = simple_open,
    .read = vpm_fault_read,
    .write = vpm_fault_write,
    .llseek = default_llseek,
};

static const struct file_operations vpm_whoami_fops = {
    .owner = THIS_MODULE,
    .open = simple_open,
    .read = vpm_whoami_read,
    .llseek = default_llseek,
};

static const struct file_operations vpm_revision_fops = {
    .owner = THIS_MODULE,
    .open = simple_open,
    .read = vpm_revision_read,
    .llseek = default_llseek,
};

static const struct file_operations vpm_ctrl_fops = {
    .owner = THIS_MODULE,
    .open = simple_open,
    .read = vpm_ctrl_read,
    .llseek = default_llseek,
};

static const struct file_operations vpm_status_fops = {
    .owner = THIS_MODULE,
    .open = simple_open,
    .read = vpm_status_read,
    .llseek = default_llseek,
};

static const struct file_operations vpm_pm_state_fops = {
    .owner = THIS_MODULE,
    .open = simple_open,
    .read = vpm_pm_state_read,
    .llseek = default_llseek,
};

static const struct file_operations vpm_odr_fops = {
    .owner = THIS_MODULE,
    .open = simple_open,
    .read = vpm_odr_read,
    .write = vpm_odr_write,
    .llseek = default_llseek,
};

static ssize_t vpm_registers_read(struct file *file,
                                  char __user *user_buf,
                                  size_t count,
                                  loff_t *ppos)
{
    struct vpm_mock_chip *chip = file->private_data;
    char *buf;
    int len = 0;
    ssize_t ret;
    u8 whoami, revision, ctrl, odr, status, pm_state, fault;

    buf = kzalloc(512, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    vpm_mock_read_reg(chip, VPM_REG_WHOAMI, &whoami);
    vpm_mock_read_reg(chip, VPM_REG_REVISION, &revision);
    vpm_mock_read_reg(chip, VPM_REG_CTRL, &ctrl);
    vpm_mock_read_reg(chip, VPM_REG_ODR, &odr);
    vpm_mock_read_reg(chip, VPM_REG_STATUS, &status);
    vpm_mock_read_reg(chip, VPM_REG_PM_STATE, &pm_state);
    vpm_mock_read_reg(chip, VPM_REG_FAULT_INJECT, &fault);

    len += scnprintf(buf + len, 512 - len, "VPM mock register dump\n");
    len += scnprintf(buf + len, 512 - len, "----------------------\n");
    len += scnprintf(buf + len, 512 - len,
                     "0x%02x WHOAMI   : 0x%02x\n",
                     VPM_REG_WHOAMI, whoami);
    len += scnprintf(buf + len, 512 - len,
                     "0x%02x REVISION : 0x%02x\n",
                     VPM_REG_REVISION, revision);
    len += scnprintf(buf + len, 512 - len,
                     "0x%02x CTRL     : 0x%02x\n",
                     VPM_REG_CTRL, ctrl);
    len += scnprintf(buf + len, 512 - len,
                     "0x%02x ODR      : %u Hz\n",
                     VPM_REG_ODR, odr);
    len += scnprintf(buf + len, 512 - len,
                     "0x%02x STATUS   : 0x%02x\n",
                     VPM_REG_STATUS, status);
    len += scnprintf(buf + len, 512 - len,
                     "0x%02x PM_STATE : 0x%02x\n",
                     VPM_REG_PM_STATE, pm_state);
    len += scnprintf(buf + len, 512 - len,
                 "0x%02x FAULT_INJECT : 0x%02x\n",
                 VPM_REG_FAULT_INJECT, fault);
    ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);

    kfree(buf);

    return ret;
}

static const struct file_operations vpm_registers_fops = {
    .owner = THIS_MODULE,
    .open = simple_open,
    .read = vpm_registers_read,
    .llseek = default_llseek,
};

static int vpm_debugfs_init(struct vpm_mock_chip *chip)
{
    chip->debugfs_dir = debugfs_create_dir("vpm_skeleton", NULL);
    if (IS_ERR_OR_NULL(chip->debugfs_dir))
        return -ENOMEM;

    debugfs_create_file("whoami", 0444, chip->debugfs_dir,
                        chip, &vpm_whoami_fops);

    debugfs_create_file("revision", 0444, chip->debugfs_dir,
                        chip, &vpm_revision_fops);

    debugfs_create_file("ctrl", 0444, chip->debugfs_dir,
                        chip, &vpm_ctrl_fops);

    debugfs_create_file("odr_hz", 0644, chip->debugfs_dir,
                        chip, &vpm_odr_fops);

    debugfs_create_file("status", 0444, chip->debugfs_dir,
                        chip, &vpm_status_fops);

    debugfs_create_file("pm_state", 0444, chip->debugfs_dir,
                        chip, &vpm_pm_state_fops);

    debugfs_create_file("registers", 0444, chip->debugfs_dir,
                        chip, &vpm_registers_fops);
    
    debugfs_create_file("fault_inject", 0644, chip->debugfs_dir,
                    	chip, &vpm_fault_fops);

	debugfs_create_file("sample", 0444, chip->debugfs_dir,
                    	chip, &vpm_sample_fops);				
    return 0;
}

static void vpm_debugfs_exit(struct vpm_mock_chip *chip)
{
    debugfs_remove_recursive(chip->debugfs_dir);
    chip->debugfs_dir = NULL;
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

    mutex_init(&vpm_chip.lock);
	INIT_DELAYED_WORK(&vpm_chip.data_work, vpm_data_workfn);

    vpm_mock_init_regs(&vpm_chip);
    vpm_mock_dump_regs(&vpm_chip);

    ret = vpm_mock_validate_chip(&vpm_chip);
    if (ret) {
        pr_err("vpm_skeleton: mock chip validation failed: %d\n", ret);
        return ret;
    }

    ret = vpm_debugfs_init(&vpm_chip);
    if (ret) {
        pr_err("vpm_skeleton: failed to initialize debugfs: %d\n", ret);
        return ret;
    }
	vpm_mock_restart_data_work(&vpm_chip);

    pr_info("vpm_skeleton: debugfs created at /sys/kernel/debug/vpm_skeleton\n");
    pr_info("vpm_skeleton: mock chip initialized successfully\n");

    return 0;
}

static void __exit vpm_skeleton_exit(void)
{
	cancel_delayed_work_sync(&vpm_chip.data_work);
    vpm_debugfs_exit(&vpm_chip);
    pr_info("vpm_skeleton: exit\n");
}

module_init(vpm_skeleton_init);
module_exit(vpm_skeleton_exit);

MODULE_AUTHOR("timatm");
MODULE_DESCRIPTION("VPM Sensor Lab - M1 mock register map");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2");