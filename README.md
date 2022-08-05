# imx-boot-tools
This repository contains boot-related tools for i.MX8 platforms.

Derived from [tegra-boot-tools](https://github.com/OE4T/tegra-boot-tools).

## imx-bootinfo
The `imx-bootinfo` tool provides storage, similar to U-Boot environment
variables, for information that should persist across reboots. The variables
are stored (with redundancy) outside of any Linux filesystem.

## keystoretool
The `keystoretool` tool leverages secure key and encrypted key support
in the Linux kernel on i.MX SoCs to create a key for use with dm-crypt to
encrypt filesystem partitions, persistently storing the blobbed/wrapped
keys using  `imx-bootinfo`'s variable storage. The CLI is derived from
a similar tool for use with the  [keystore](https://github.com/madisongh/keystore)
TA on NVIDIA Jetson platforms, but the underlying implementation leverages
CAAM features of the i.MX.

## imx-otp-tool
The `imx-otp-tool` tool provides access to the eFuses exported through
the imx-ocotp driver's nvmem interface, mainly for automating secure boot
setup.  **PLEASE NOTE** that incorrect manipulation of the eFuses can render
your device unbootable.  **USE AT YOUR OWN RISK.**

# Builds
This package uses CMake for building.

## Dependencies
This package depends on systemd, libz, and libkeyutils.

# License
Distributed under license. See the [LICENSE](LICENSE) file for details.
