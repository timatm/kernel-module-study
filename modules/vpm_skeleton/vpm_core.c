#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>

#include "vpm_regs.h"
#include "vpm_core.h"

static int vpm_mock_apply_faults_locked(struct vpm_mock_chip *chip);
static int vpm_read_u8_locked(struct vpm_mock_chip *chip, u8 reg, u8 *val)
{
    if (!chip || !val)
        return -EINVAL;

    if (reg >= VPM_REG_MAX)
        return -EINVAL;

    return chip->bus->read_u8(chip->bus_ctx, reg, val);
}
int vpm_read_u8(struct vpm_mock_chip *chip, u8 reg, u8 *val)
{
    int ret;
	if (!chip || !val)
        return -EINVAL;
	
    mutex_lock(&chip->lock);
	ret = vpm_read_u8_locked(chip,reg,val);
    mutex_unlock(&chip->lock);

    return ret;
}

static int vpm_write_u8_locked(struct vpm_mock_chip *chip, u8 reg, u8 val)
{
    if (!chip)
        return -EINVAL;

    if (!chip->bus || !chip->bus->read_u8)
        return -ENODEV;
    if (reg >= VPM_REG_MAX)
        return -EINVAL;

    if (reg == VPM_REG_WHOAMI)
        return -EINVAL;

    return chip->bus->write_u8(chip->bus_ctx, reg, val);
}

int vpm_write_u8(struct vpm_mock_chip *chip, u8 reg, u8 val)
{
    int ret;
    if (!chip)
        return -EINVAL;

    if (!chip->bus || !chip->bus->write_u8)
        return -ENODEV;

    if (reg >= VPM_REG_MAX)
        return -EINVAL;

    if (reg == VPM_REG_WHOAMI)
        return -EINVAL;
	
    mutex_lock(&chip->lock);
    ret = vpm_write_u8_locked(chip,reg,val);
	if(ret) goto out;
    if (reg == VPM_REG_FAULT_INJECT)
        ret = vpm_mock_apply_faults_locked(chip);
out:
    mutex_unlock(&chip->lock);
    return ret;
}

static int vpm_bulk_read_locked(struct vpm_mock_chip *chip,
                                u8 reg, u8 *buf, size_t len)
{
    if (!chip || !buf)
        return -EINVAL;

    if (!chip->bus || !chip->bus->bulk_read)
        return -ENODEV;
    
    if (reg >= VPM_REG_MAX || len > VPM_REG_MAX - reg)
        return -EINVAL;

    return chip->bus->bulk_read(chip->bus_ctx, reg, buf, len);
}

static int vpm_bulk_write_locked(struct vpm_mock_chip *chip,
                                 u8 reg, const u8 *buf, size_t len)
{
    if (!chip || !buf)
        return -EINVAL;
    
    if (!chip->bus || !chip->bus->bulk_write)
        return -ENODEV;

    if (reg >= VPM_REG_MAX || len > VPM_REG_MAX - reg)
        return -EINVAL;

    return chip->bus->bulk_write(chip->bus_ctx, reg, buf, len);
}

static int vpm_update_bits_locked(struct vpm_mock_chip *chip,
                                  u8 reg, u8 mask, u8 val)
{
    u8 tmp;
    int ret;

    ret = vpm_read_u8_locked(chip, reg, &tmp);
    if (ret)
        return ret;

    tmp &= ~mask;
    tmp |= val & mask;

    return vpm_write_u8_locked(chip, reg, tmp);
}

static int vpm_write_s16_le_locked(struct vpm_mock_chip *chip,
                                   u8 reg_l, s16 value)
{
    u8 buf[2];

    buf[0] = value & 0xff;
    buf[1] = (value >> 8) & 0xff;

    return vpm_bulk_write_locked(chip, reg_l, buf, sizeof(buf));
}

static int vpm_read_s16_le_locked(struct vpm_mock_chip *chip,
                                  u8 reg_l, s16 *value)
{
    u8 buf[2];
    int ret;

    if (!value)
        return -EINVAL;

    ret = vpm_bulk_read_locked(chip, reg_l, buf, sizeof(buf));
    if (ret)
        return ret;

    *value = (s16)(((u16)buf[1] << 8) | buf[0]);
    return 0;
}


static unsigned int vpm_mock_get_period_ms_locked(struct vpm_mock_chip *chip)
{
    u8 odr;
	int ret;
    ret = vpm_read_u8_locked(chip, VPM_REG_ODR, &odr);
    if (ret)
        return 0;
    if (odr == 0)
        return 0;

    return max(1U, 1000U / odr);
}


static void vpm_mock_update_sample_locked(struct vpm_mock_chip *chip)
{
   chip->sample.sample_counter++;

   chip->sample.temp_raw = 25000 + (chip->sample.sample_counter % 100);
   chip->sample.accel_x_raw =chip->sample.sample_counter % 64;
   chip->sample.accel_y_raw = -(chip->sample.sample_counter % 32);
   chip->sample.accel_z_raw = 1024;

    vpm_write_s16_le_locked(chip, VPM_REG_TEMP_L,chip->sample.temp_raw);
	vpm_write_s16_le_locked(chip, VPM_REG_ACCEL_X_L,chip->sample.accel_x_raw);
	vpm_write_s16_le_locked(chip, VPM_REG_ACCEL_Y_L,chip->sample.accel_y_raw);
	vpm_write_s16_le_locked(chip, VPM_REG_ACCEL_Z_L,chip->sample.accel_z_raw);

	vpm_update_bits_locked(chip,
						VPM_REG_STATUS,
						VPM_STATUS_DATA_READY,
						VPM_STATUS_DATA_READY);
}

static void vpm_data_workfn(struct work_struct *work)
{
    struct delayed_work *dwork;
    struct vpm_mock_chip *chip;
    unsigned int period_ms;
	int ret;
    u8 fault;
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
    

	ret = vpm_read_u8_locked(chip, VPM_REG_FAULT_INJECT, &fault);
	if (!ret && !(fault & VPM_FAULT_STALE_DATA))
    	vpm_mock_update_sample_locked(chip);

    mutex_unlock(&chip->lock);

    schedule_delayed_work(&chip->data_work,
                          msecs_to_jiffies(period_ms));
}

void vpm_restart_data_work(struct vpm_mock_chip *chip)
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




bool vpm_fault_enabled(struct vpm_mock_chip *chip, u8 fault)
{
    bool enabled = false;
    int ret;
    u8 val;

    if (!chip)
        return false;

    mutex_lock(&chip->lock);

    ret = vpm_read_u8_locked(chip, VPM_REG_FAULT_INJECT, &val);
    if (!ret)
        enabled = !!(val & fault);

    mutex_unlock(&chip->lock);

    return enabled;
}

static int vpm_mock_apply_faults_locked(struct vpm_mock_chip *chip)
{
    u8 fault;
    int ret;

    ret = vpm_read_u8_locked(chip, VPM_REG_FAULT_INJECT, &fault);
    if (ret)
        return ret;

    if (fault & VPM_FAULT_INVALID_STATUS)
        return vpm_update_bits_locked(chip,
                                      VPM_REG_STATUS,
                                      VPM_STATUS_FAULT,
                                      VPM_STATUS_FAULT);

    return vpm_update_bits_locked(chip,
                                  VPM_REG_STATUS,
                                  VPM_STATUS_FAULT,
                                  0);
}


void vpm_core_exit(struct vpm_mock_chip *chip)
{
    if (!chip)
        return;

    cancel_delayed_work_sync(&chip->data_work);
}

static int vpm_init_sample_cache_locked(struct vpm_mock_chip *chip)
{
    int ret;

    chip->sample.sample_counter = 0;
    chip->sample.temp_raw = 25000;
    chip->sample.accel_x_raw = 0;
    chip->sample.accel_y_raw = 0;
    chip->sample.accel_z_raw = 1024;
    chip->data_work_enabled = false;

    ret = vpm_write_s16_le_locked(chip, VPM_REG_TEMP_L,chip->sample.temp_raw);
    if (ret)
        return ret;

    ret = vpm_write_s16_le_locked(chip, VPM_REG_ACCEL_X_L,chip->sample.accel_x_raw);
    if (ret)
        return ret;

    ret = vpm_write_s16_le_locked(chip, VPM_REG_ACCEL_Y_L,chip->sample.accel_y_raw);
    if (ret)
        return ret;

    return vpm_write_s16_le_locked(chip, VPM_REG_ACCEL_Z_L,chip->sample.accel_z_raw);
}

static int vpm_validate_chip(struct vpm_mock_chip *chip)
{
    int ret;
    u8 val;

    ret = vpm_read_u8(chip, VPM_REG_WHOAMI, &val);
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

    vpm_read_u8(chip, VPM_REG_WHOAMI, &whoami);
    vpm_read_u8(chip, VPM_REG_REVISION, &revision);
    vpm_read_u8(chip, VPM_REG_CTRL, &ctrl);
    vpm_read_u8(chip, VPM_REG_ODR, &odr);
    vpm_read_u8(chip, VPM_REG_STATUS, &status);
    vpm_read_u8(chip, VPM_REG_PM_STATE, &pm_state);

    pr_info("vpm_skeleton: WHOAMI=0x%02x\n", whoami);
    pr_info("vpm_skeleton: REVISION=%u\n", revision);
    pr_info("vpm_skeleton: CTRL=0x%02x\n", ctrl);
    pr_info("vpm_skeleton: ODR=%u Hz\n", odr);
    pr_info("vpm_skeleton: STATUS=0x%02x\n", status);
    pr_info("vpm_skeleton: PM_STATE=%u\n", pm_state);
}

int vpm_core_init(struct vpm_mock_chip *chip,
                  const struct vpm_bus_ops *bus,
                  void *bus_ctx)
{
    int ret;

    if (!chip || !bus || !bus_ctx)
        return -EINVAL;

    chip->bus = bus;
    chip->bus_ctx = bus_ctx;

    mutex_init(&chip->lock);
    INIT_DELAYED_WORK(&chip->data_work, vpm_data_workfn);

    mutex_lock(&chip->lock);
    ret = vpm_init_sample_cache_locked(chip);
    mutex_unlock(&chip->lock);
    if (ret)
        return ret;

    vpm_mock_dump_regs(chip);

    ret = vpm_validate_chip(chip);
    if (ret)
        return ret;

    return 0;
}