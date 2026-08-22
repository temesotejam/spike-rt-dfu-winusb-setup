# Safety model

This project intentionally has a much narrower scope than generic USB driver tools.

## Allowed target

A write operation may only be considered when all of the following are true:

- operating system is Windows
- the device is currently present
- USB VID is `0x0694`
- USB PID is `0x0008`
- exactly one matching device is connected
- device metadata is consistent with the expected LEGO SPIKE/Technic Large Hub DFU device
- the requested driver is WinUSB

## Mandatory refusal cases

The program must make no driver changes when:

- no matching DFU device is present
- more than one `0694:0008` device is present
- the device identity is ambiguous
- expected metadata cannot be verified in an installation-capable build
- a device other than the target VID/PID is selected or discovered
- the operation would affect a runtime serial/COM device

## Separation from SPIKE-RT runtime serial

DFU driver setup and runtime debugging are separate concerns.

- DFU WebUSB target: `0694:0008`
- SPIKE-RT runtime serial: handled as a normal Windows COM port by `spike-rt-web-toolkit`

This installer must never replace the runtime serial driver with WinUSB.

## Development stages

`v0.1` is read-only. It enumerates the target and reports current device/driver information.

An installation-capable version must preserve the same fail-closed detection path and add an explicit confirmation immediately before changing the driver.
