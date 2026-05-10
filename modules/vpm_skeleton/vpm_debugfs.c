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
#include "vpm_core.h"
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

    ret = vpm_read_u8(chip, reg, &val);
    if (ret)
        return ret;

    len = scnprintf(buf, sizeof(buf), "0x%02x\n", val);

    return simple_read_from_buffer(user_buf, count, ppos, buf, len);
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

    ret = vpm_read_u8(chip, VPM_REG_ODR, &val);
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

    if (vpm_fault_enabled(chip, VPM_FAULT_DEVICE_BUSY)) {
        pr_info("vpm_skeleton: reject ODR update because device is busy\n");
        return -EBUSY;
    }

    ret = vpm_write_u8(chip, VPM_REG_ODR, val);
	if (ret)
		return ret;

	vpm_restart_data_work(chip);

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

    ret = vpm_read_u8(chip,VPM_REG_FAULT_INJECT, &val);
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

    if (val & ~(VPM_FAULT_INVALID_STATUS | 
				VPM_FAULT_DEVICE_BUSY|
				VPM_FAULT_STALE_DATA))
        return -EINVAL;

    ret = vpm_write_u8(chip, VPM_REG_FAULT_INJECT, val);
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
    u8 status;
    u8 odr;
    u8 fault;
    u32 sample_counter;
    s16 temp_raw;
    s16 accel_x_raw;
    s16 accel_y_raw;
    s16 accel_z_raw;
    bool data_work_enabled;

    if (vpm_read_u8(chip, VPM_REG_STATUS, &status))
        status = 0;

    if (vpm_read_u8(chip, VPM_REG_ODR, &odr))
        odr = 0;

    if (vpm_read_u8(chip, VPM_REG_FAULT_INJECT, &fault))
        fault = 0;

    mutex_lock(&chip->lock);
    sample_counter = chip->sample.sample_counter;
    temp_raw = chip->sample.temp_raw;
    accel_x_raw = chip->sample.accel_x_raw;
    accel_y_raw = chip->sample.accel_y_raw;
    accel_z_raw = chip->sample.accel_z_raw;
    data_work_enabled = chip->data_work_enabled;
    mutex_unlock(&chip->lock);

    len = scnprintf(buf, sizeof(buf),
                    "sample_counter: %u\n"
                    "temp_raw      : %d\n"
                    "accel_x_raw   : %d\n"
                    "accel_y_raw   : %d\n"
                    "accel_z_raw   : %d\n"
                    "status        : 0x%02x\n"
                    "odr_hz        : %u\n"
                    "fault_inject  : 0x%02x\n"
                    "data_work     : %s\n",
                    sample_counter,
                    temp_raw,
                    accel_x_raw,
                    accel_y_raw,
                    accel_z_raw,
                    status,
                    odr,
                    fault,
                    data_work_enabled ? "enabled" : "disabled");

    return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}


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

    vpm_read_u8(chip, VPM_REG_WHOAMI, &whoami);
    vpm_read_u8(chip, VPM_REG_REVISION, &revision);
    vpm_read_u8(chip, VPM_REG_CTRL, &ctrl);
    vpm_read_u8(chip, VPM_REG_ODR, &odr);
    vpm_read_u8(chip, VPM_REG_STATUS, &status);
    vpm_read_u8(chip, VPM_REG_PM_STATE, &pm_state);
    vpm_read_u8(chip, VPM_REG_FAULT_INJECT, &fault);

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


static const struct file_operations vpm_registers_fops = {
    .owner = THIS_MODULE,
    .open = simple_open,
    .read = vpm_registers_read,
    .llseek = default_llseek,
};



int vpm_debugfs_init(struct vpm_mock_chip *chip)
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

void vpm_debugfs_exit(struct vpm_mock_chip *chip)
{
    debugfs_remove_recursive(chip->debugfs_dir);
    chip->debugfs_dir = NULL;
}