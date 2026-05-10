#ifndef __VPMCTL_H__
#define __VPMCTL_H__

#include <string.h>

#define DEBUG_DIR ("/sys/kernel/debug/vpm_skeleton")
#define VPM_PATH_FAULT_INJECT DEBUG_DIR "/fault_inject"

#define VPM_FAULT_INVALID_STATUS_VALUE  0x01
#define VPM_FAULT_DEVICE_BUSY_VALUE     0x02

typedef enum{
    OP_HELP,
    OP_INFO,
    OP_READ,
    OP_SET_ODR,
    OP_DUMP_REGS,
    OP_FAULT,
    OP_SAMPLE,
} Operation;



typedef struct {
    Operation opt;
    char *subcmd;
    char *parameter;
} Node;

int parse_command(int argc,char *argv[],Node *);

#endif