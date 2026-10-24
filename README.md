# imx-misc-tools
This repository contains miscellaneous boot-related tools for i.MX95 platforms.

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
the imx-ocotp-ele driver's nvmem interface. Note that the driver in NXP's
downstream kernel (as of 6.18.20-2.0.0) does not map the entire set of
eFuses for its read-side interface, and it ignores its mapping
for the write-side interface, so some fuses may read as zero incorrectly,
but still be programmable.

**PLEASE NOTE** that incorrect manipulation of the eFuses can render
your device unbootable.  **USE AT YOUR OWN RISK.**

# Builds
This package uses CMake for building.

## Dependencies
This package depends on systemd, libz, and libkeyutils.

# License
Distributed under license. See the [LICENSE](LICENSE) file for details.
