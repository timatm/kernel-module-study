#ifndef __VPM_CORE_H__
#define __VPM_CORE_H__

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>

#include "vpm_regs.h"
#include "vpm_mock_bus.h"
struct vpm_sample_cache {
    u32 sample_counter;
    s16 temp_raw;
    s16 accel_x_raw;
    s16 accel_y_raw;
    s16 accel_z_raw;
};

struct vpm_bus_ops {
    int (*read_u8)(void *ctx, u8 reg, u8 *val);
    int (*write_u8)(void *ctx, u8 reg, u8 val);
    int (*bulk_read)(void *ctx, u8 reg, u8 *buf, size_t len);
    int (*bulk_write)(void *ctx, u8 reg, const u8 *buf, size_t len);
};

struct vpm_mock_chip {
    struct vpm_sample_cache sample;

    const struct vpm_bus_ops *bus;
    void *bus_ctx;

    struct mutex lock;
    struct dentry *debugfs_dir;

    struct delayed_work data_work;
    bool data_work_enabled;
};

int vpm_core_init(struct vpm_mock_chip *chip,
                  const struct vpm_bus_ops *bus,
                  void *bus_ctx);

void vpm_core_exit(struct vpm_mock_chip *chip);

int vpm_read_u8(struct vpm_mock_chip *chip, u8 reg, u8 *val);
int vpm_write_u8(struct vpm_mock_chip *chip, u8 reg, u8 val);

bool vpm_fault_enabled(struct vpm_mock_chip *chip, u8 fault);

void vpm_restart_data_work(struct vpm_mock_chip *chip);

int vpm_debugfs_init(struct vpm_mock_chip *chip);
void vpm_debugfs_exit(struct vpm_mock_chip *chip);

#endif