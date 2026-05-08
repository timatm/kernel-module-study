#include "vpmctl.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int parse_command(int argc, char *argv[], Node *n)
{
    if (!argv || !n) {
        return 1;
    }

    if (argc < 1 || argc > 2) {
        return 1;
    }

    n->parameter = NULL;

    if (strcmp(argv[0], "help") == 0) {
        if(argc != 1) return 1;
        n->opt = OP_HELP;
    }
    else if (strcmp(argv[0], "info") == 0) {
        if(argc != 1) return 1;
        n->opt = OP_INFO;
    }
    else if (strcmp(argv[0], "read") == 0) {
        if(argc != 2) return 1;
        n->opt = OP_READ;
    }
    else if (strcmp(argv[0], "set-odr") == 0) {
        if(argc != 2) return 1;
        n->opt = OP_SET_ODR;
    }
    else if (strcmp(argv[0], "dump-regs") == 0) {
        if(argc != 1) return 1;
        n->opt = OP_DUMP_REGS;
    }
    else {
        printf("User command operation is not defined\n");
        return 1;
    }

    if (argc == 2) {
        n->parameter = argv[1];
    }

    return 0;
}

int op_help(){
    printf("This is mock VPM controller\n");
    printf("User can use following operation:\n");
    printf("\t ./vpmctl help\n");
    printf("\t ./vpmctl info\n");
    printf("\t ./vpmctl read\n");
    printf("\t ./vpmctl set-odr\n");
    printf("\t ./vpmctl dump-regs\n");
    return 0;
}

static int print_debugfs_file(const char *name)
{
    char buf[256];
    char path[256];

    int n = snprintf(path, sizeof(path), "%s/%s", DEBUG_DIR, name);
    if (n < 0 || (size_t)n >= sizeof(path)) {
        fprintf(stderr, "path too long: %s/%s\n", DEBUG_DIR, name);
        return -1;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "fopen %s failed: %s\n", path, strerror(errno));
        return -1;
    }

    printf("%-10s: ", name);

    while (fgets(buf, sizeof(buf), fp)) {
        printf("%s", buf);
    }

    fclose(fp);
    return 0;
}

int op_info(void)
{
    const char *file[] = {
        "whoami",
        "revision",
        "ctrl",
        "odr_hz",
        "status",
        "pm_state"
    };

    printf("VPM Sensor Debug Interface\n");
    printf("Path: %s\n", DEBUG_DIR);

    int ret = 0;

    for (size_t i = 0; i < sizeof(file) / sizeof(file[0]); i++) {
        if (print_debugfs_file(file[i]) < 0) {
            ret = -1;
        }
    }

    return ret;
}

static int is_valid_read_reg(const char *reg)
{
    const char *valid_regs[] = {
        "whoami",
        "revision",
        "ctrl",
        "odr_hz",
        "status",
        "pm_state",
    };

    for (size_t i = 0; i < sizeof(valid_regs) / sizeof(valid_regs[0]); i++) {
        if (strcmp(reg, valid_regs[i]) == 0)
            return 1;
    }

    return 0;
}

int op_read(const char *reg)
{
    if (!reg) {
        fprintf(stderr, "op_read: register is NULL\n");
        return -EINVAL;
    }

    if (!is_valid_read_reg(reg)) {
        fprintf(stderr, "op_read: unknown register: %s\n", reg);
        fprintf(stderr, "valid registers: whoami revision ctrl odr_hz status pm_state\n");
        return -EINVAL;
    }
    char path[256];
    char buf[256];

    int n = snprintf(path, sizeof(path), "%s/%s", DEBUG_DIR, reg);
    if (n < 0) {
        fprintf(stderr, "op_read: snprintf failed\n");
        return -1;
    }

    if ((size_t)n >= sizeof(path)) {
        fprintf(stderr, "op_read: path too long: %s/%s\n", DEBUG_DIR, reg);
        return -ENAMETOOLONG;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "op_read: fopen %s failed: %s\n",
                path, strerror(errno));
        return -errno;
    }

    if (fgets(buf, sizeof(buf), fp) == NULL) {
        if (ferror(fp)) {
            fprintf(stderr, "op_read: fgets %s failed: %s\n",
                    path, strerror(errno));
            fclose(fp);
            return -errno;
        }

        fprintf(stderr, "op_read: %s is empty\n", path);
        fclose(fp);
        return -1;
    }

    printf("%s: %s", reg, buf);

    fclose(fp);
    return 0;
}

int op_set_odr(const char *odr)
{
    char path[256];
    char *end = NULL;
    unsigned long val;
    int ret;
    FILE *fp;

    if (!odr) {
        fprintf(stderr, "op_set_odr: odr is NULL\n");
        return -EINVAL;
    }

    errno = 0;
    val = strtoul(odr, &end, 0);

    if (errno != 0 || end == odr || *end != '\0') {
        fprintf(stderr, "op_set_odr: invalid ODR value: %s\n", odr);
        return -EINVAL;
    }

    if (val > 255) {
        fprintf(stderr, "op_set_odr: ODR value must be between 0 and 255\n");
        return -EINVAL;
    }

    ret = snprintf(path, sizeof(path), "%s/%s", DEBUG_DIR, "odr_hz");
    if (ret < 0) {
        fprintf(stderr, "op_set_odr: snprintf() failed\n");
        return -1;
    }

    if ((size_t)ret >= sizeof(path)) {
        fprintf(stderr, "op_set_odr: path too long\n");
        return -ENAMETOOLONG;
    }

    fp = fopen(path, "w");
    if (!fp) {
        int err = errno;
        fprintf(stderr, "op_set_odr: fopen %s failed: %s\n",
                path, strerror(err));
        return -err;
    }

    if (fprintf(fp, "%lu\n", val) < 0) {
        int err = errno;
        fprintf(stderr, "op_set_odr: write %s failed: %s\n",
                path, strerror(err));
        fclose(fp);
        return -err;
    }

    fclose(fp);
    return 0;
}

static int print_debugfs_raw_file(const char *name)
{
    char buf[256];
    char path[256];
    int n;
    FILE *fp;

    n = snprintf(path, sizeof(path), "%s/%s", DEBUG_DIR, name);
    if (n < 0 || (size_t)n >= sizeof(path)) {
        fprintf(stderr, "path too long: %s/%s\n", DEBUG_DIR, name);
        return -1;
    }

    fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "fopen %s failed: %s\n", path, strerror(errno));
        return -1;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        printf("%s", buf);
    }

    if (ferror(fp)) {
        fprintf(stderr, "fgets %s failed: %s\n", path, strerror(errno));
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

int op_dump_regs(void)
{
    return print_debugfs_raw_file("registers");
}

int main(int argc,char *argv[]){
    int err = 0;
    Node n;
    err = parse_command(argc-1,&argv[1],&n);
    if(err != 0){
        printf("Parse user command is failed");
        return -1;
    }

    switch (n.opt){
        case OP_HELP:
            err = op_help();
            break;
        case OP_INFO:
            err = op_info();
            break;
        case OP_READ:
            err = op_read(n.parameter);
            break;
        case OP_SET_ODR:
            err = op_set_odr(n.parameter);
            break;
        case OP_DUMP_REGS:
            err = op_dump_regs();
            break;
        default:
            fprintf(stderr, "unknown operation\n");
            err = -1;
            break;
    }
    return err == 0 ? 0 : 1;
}
