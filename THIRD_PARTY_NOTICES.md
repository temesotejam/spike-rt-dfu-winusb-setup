# Third-party notices

## libwdi

This project uses **libwdi**, the Windows Driver Installer library for USB devices, to prepare and install the WinUSB driver package.

- Upstream: `pbatard/libwdi`
- Version: `v1.5.1`
- Pinned commit: `9b23b82a2dd1cbffc16d46c212f92c6bf8c0c602`
- License: GNU Lesser General Public License v3.0 or later (LGPL-3.0-or-later)

The GitHub Actions build checks out the pinned upstream source and builds `libwdi.dll` without modifying libwdi source files. The distributed artifact includes the upstream `COPYING-LGPL` text alongside the DLL.

The setup executable is intentionally device-specific. It performs its own fail-closed device validation before calling libwdi and does not expose libwdi's generic USB-device selection functionality to the user.
