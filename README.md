# kernel-module-study-
# Linux ARM IIO Power Validation Lab

## Motivation

This project is a step-by-step embedded Linux driver practice lab.  
The final goal is to implement a Linux sensor driver with mock hardware, validation tests, and power-management analysis.

The project starts from a minimal out-of-tree Linux kernel module and gradually evolves into a complete driver validation environment.

## Milestone 0: Kernel Module Skeleton

The first milestone verifies the basic Linux kernel module workflow:

- Build an out-of-tree kernel module
- Load the module with `insmod`
- Pass module parameters
- Inspect kernel logs with `dmesg`
- Unload the module with `rmmod`

## Build

```bash
cd modules/vpm_skeleton
make