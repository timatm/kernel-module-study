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

## Milestone 4: Mock Fault Injection

M4 adds mock-only fault injection support.
The purpose is to validate error-handling paths before real hardware is available.

Current fault modes include:
- `invalid_status`  Simulates a hardware-reported fault state
- `device_busy` Simulates a device rejecting configuration writes

`VPM_REG_FAULT_INJECT` is a mock-only validation register. It is not intended to represent a production hardware register. It exists to inject deterministic error conditions into the virtual hardware model.

## Milestone 5: Fake Sensor Data Generator

M5 adds a fake sensor data generator inside the kernel module.
The module uses kernel delayed work to periodically update mock sensor samples, including:

- sample counter
- temperature raw data
- accelerometer X/Y/Z raw data

The configured ODR controls how often samples are updated.

## Milestone 6: Stale Data Validation

M6 adds a stale_data fault mode.

This simulates a realistic sensor issue where register reads still succeed, but sensor samples stop updating. This helps validate that the data path is producing fresh data, not merely returning readable values.

When stale_data is active, the delayed work remains enabled, but sample values stop changing. After clearing the fault, sample generation resumes.
