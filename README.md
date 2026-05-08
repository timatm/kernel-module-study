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

## Milestone 1: Mock Register Map

M1 adds a fake hardware register map to the kernel module.  
The goal is to model a simple embedded sensor chip before implementing a real bus driver.

The mock chip includes:

- WHOAMI register
- silicon revision register
- control register
- output data rate register
- status register
- power state register

This allows the module to validate a hardware-like identity value and initialize fake chip state during module load.

## Milestone 2: DebugFS Register Interface

M2 exposes the mock VPM sensor registers to user space through debugfs.

This is intentionally implemented as a debug and validation interface rather than a stable userspace ABI. The goal is to make the fake hardware state observable and controllable before adding a formal driver subsystem such as IIO.

## Milestone 3: VPM controller

M3 implements the VPM controller user CLI, `vpmctl`, to control and inspect the VPM sensor driver from user space.

Supported commands:

- `vpmctl info` shows a human-readable device summary
- `vpmctl read <register>` reads a single debugfs register
- `vpmctl set-odr <hz>` updates the output data rate
- `vpmctl dump-regs` dumps all virtual register values
- `vpmctl help` shows usage information

### DebugFS layout

```text
/sys/kernel/debug/vpm_skeleton/
├── whoami
├── revision
├── ctrl
├── odr_hz
├── status
├── pm_state
└── registers
```

## Build

```bash
cd modules/vpm_skeleton
make