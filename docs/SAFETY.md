# Safety model

This project intentionally has a much narrower scope than generic USB driver tools.

## Allowed target

A write operation may only be considered when all of the following are true:

- operating system is Windows
- the device is currently present
- USB VID is `0x0694`
- USB PID is `0x0008`
- exactly one matching device is connected
- device name/description identifies `LEGO Technic Large Hub in DFU Mode`
- if Windows exposes a manufacturer value, it identifies LEGO
- libwdi independently sees exactly one matching `0694:0008` USB device
- the requested driver is WinUSB
- the user explicitly types `INSTALL` immediately before the change

## Mandatory refusal cases

The program must make no driver changes when:

- no matching DFU device is present
- more than one `0694:0008` device is present
- the device identity is ambiguous
- the device description does not identify the expected Large Hub DFU device
- a non-empty manufacturer value does not identify LEGO
- libwdi enumeration disagrees with the SetupAPI target count
- a device other than the target VID/PID is selected or discovered
- the requested driver would be anything other than WinUSB
- the user does not provide the explicit confirmation
- the operation would affect a runtime serial/COM device

## Idempotent behavior

If the verified target already reports `Service=WinUSB`, normal execution exits successfully without preparing or installing a driver package.

This means repeated normal execution on an already-configured PC must not replace or reinstall the driver unnecessarily.

`--prepare-only` is an explicit diagnostic exception: it may generate temporary files for validation even when WinUSB is already assigned, but it must not change the device service or Windows certificate stores.

## Non-destructive validation modes

### `--self-test`

This mode does not enumerate USB devices. It runs synthetic policy checks for:

- expected metadata acceptance
- wrong PID rejection
- wrong device-name rejection
- wrong manufacturer rejection when the manufacturer is present
- driverless metadata handling
- WinUSB state recognition
- presence of WinUSB support in the linked libwdi binary

It must not call `wdi_prepare_driver()` or `wdi_install_driver()`.

### `--prepare-only`

This mode requires the same SetupAPI metadata checks as normal execution and then independently requires libwdi to find exactly one `0694:0008` device.

It calls `wdi_prepare_driver()` only with:

- `driver_type = WDI_WINUSB`
- `disable_cat = TRUE`
- `disable_signing = TRUE`
- `use_wcid_driver = FALSE`
- `external_inf = FALSE`

The generated temporary INF must contain `VID_0694&PID_0008` and a WinUSB reference. The temporary directory is deleted after validation.

This mode must never call `wdi_install_driver()`. With signing disabled, it must not request self-signed certificate installation. It therefore validates the libwdi extraction/tokenization path without intentionally changing device binding, the driver store, or certificate trust.

## Installation path

For a verified target that is not already using WinUSB:

1. show the exact target identity and current service
2. require the user to type `INSTALL`
3. use pinned libwdi v1.5.1 with `WDI_WINUSB` only
4. allow Windows/libwdi to request UAC or driver confirmation when required
5. never expose libwdi's generic device chooser to the user
6. re-enumerate the device after installation
7. report success only after SetupAPI reports `Service=WinUSB`

If libwdi reports success but the final SetupAPI verification fails, the program reports an incomplete/failed setup and instructs the user to reconnect the DFU Hub and rerun the tool.

## Separation from SPIKE-RT runtime serial

DFU driver setup and runtime debugging are separate concerns.

- DFU WebUSB target: `0694:0008`
- SPIKE-RT runtime serial: handled as a normal Windows COM port by `spike-rt-web-toolkit`

This installer must never replace the runtime serial driver with WinUSB.

## Dependency boundary

The installation implementation uses upstream `pbatard/libwdi` v1.5.1 pinned to commit `9b23b82a2dd1cbffc16d46c212f92c6bf8c0c602`.

libwdi is built as a separate DLL. The application performs its own device validation before invoking libwdi and does not expose arbitrary VID/PID or driver-type selection.

## Modes

- normal execution: detect, validate, and if needed offer the guarded WinUSB installation
- `--detect-only`: perform validation/reporting only; never install
- `--prepare-only`: non-destructively exercise libwdi target enumeration and unsigned INF generation; never install
- `--self-test`: run synthetic safety-policy checks without USB enumeration
- `--no-pause`: do not wait for Enter before process exit; this changes only console behavior, not installation confirmation
