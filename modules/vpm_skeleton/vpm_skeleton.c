#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

static unsigned int mock_revision = 1;
static unsigned int default_odr_hz = 10;
static bool low_power_default = false;

module_param(mock_revision, uint, 0644);
MODULE_PARM_DESC(mock_revision, "Mock silicon revision");

module_param(default_odr_hz, uint, 0644);
MODULE_PARM_DESC(default_odr_hz, "Default output data rate in Hz");

module_param(low_power_default, bool, 0644);
MODULE_PARM_DESC(low_power_default, "Start device in low-power mode");

static int __init  vpm_skeleton_init(void){
    pr_info("vpm_skeleton: init\n");
    pr_info("vpm_skeleton: mock_revision=%u\n", mock_revision);
    pr_info("vpm_skeleton: default_odr_hz=%u\n", default_odr_hz);
    pr_info("vpm_skeleton: low_power_default=%s\n",
            low_power_default ? "true" : "false");
    return 0;
}

static void __exit vpm_skeleton_exit(void)
{
    pr_info("vpm_skeleton: exit\n");
}


module_init(vpm_skeleton_init);
module_exit(vpm_skeleton_exit);


MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("VPM Sensor Lab - Linux kernel module skeleton");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
