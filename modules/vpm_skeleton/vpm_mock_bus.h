#ifndef __VPM_MOCK_BUS_H__
#define __VPM_MOCK_BUS_H__

#include <linux/types.h>

#include "vpm_core.h"
#include "vpm_regs.h"

struct vpm_mock_hw {
    u8 regs[VPM_REG_MAX];
};

extern const struct vpm_bus_ops vpm_mock_bus_ops;

void vpm_mock_bus_init_hw(struct vpm_mock_hw *hw,
                          unsigned int mock_revision,
                          unsigned int default_odr_hz,
                          bool low_power_default);

#endif