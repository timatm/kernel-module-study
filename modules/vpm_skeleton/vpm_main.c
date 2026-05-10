#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include "vpm_core.h"
#include "vpm_mock_bus.h"

static unsigned int mock_revision = 1;
static unsigned int default_odr_hz = 10;
static bool low_power_default = false;

module_param(mock_revision, uint, 0644);
MODULE_PARM_DESC(mock_revision, "Mock silicon revision");

module_param(default_odr_hz, uint, 0644);
MODULE_PARM_DESC(default_odr_hz, "Default output data rate in Hz");

module_param(low_power_default, bool, 0644);
MODULE_PARM_DESC(low_power_default, "Start device in low-power mode");

static struct vpm_mock_chip vpm_chip;
static struct vpm_mock_hw vpm_hw;

static int __init vpm_skeleton_init(void)
{
    int ret;

    pr_info("vpm_skeleton: init\n");

    vpm_mock_bus_init_hw(&vpm_hw,
                         mock_revision,
                         default_odr_hz,
                         low_power_default);

    ret = vpm_core_init(&vpm_chip,
                        &vpm_mock_bus_ops,
                        &vpm_hw);
    if (ret) {
        pr_err("vpm_skeleton: core init failed: %d\n", ret);
        return ret;
    }

    ret = vpm_debugfs_init(&vpm_chip);
    if (ret) {
        pr_err("vpm_skeleton: failed to initialize debugfs: %d\n", ret);
        vpm_core_exit(&vpm_chip);
        return ret;
    }

    vpm_restart_data_work(&vpm_chip);

    pr_info("vpm_skeleton: initialized successfully\n");
    return 0;
}

static void __exit vpm_skeleton_exit(void)
{
    vpm_debugfs_exit(&vpm_chip);
    vpm_core_exit(&vpm_chip);

    pr_info("vpm_skeleton: exit\n");
}

module_init(vpm_skeleton_init);
module_exit(vpm_skeleton_exit);

MODULE_AUTHOR("timatm");
MODULE_DESCRIPTION("VPM Sensor Lab - mock register map");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2");