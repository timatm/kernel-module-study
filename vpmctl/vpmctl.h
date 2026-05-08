#ifndef __VPMCTL_H__
#define __VPMCTL_H__

#include <string.h>

#define DEBUG_DIR ("/sys/kernel/debug/vpm_skeleton")
typedef enum{
    OP_HELP,
    OP_INFO,
    OP_READ,
    OP_SET_ODR,
    OP_DUMP_REGS
} Operation;



typedef struct 
{
    Operation opt;
    char *parameter;
}Node;

int parse_command(int argc,char *argv[],Node *);

#endif