# imx-boot-tools
This repository contains boot-related tools for i.MX8 platforms.

Derived from [tegra-boot-tools](https://github.com/OE4T/tegra-boot-tools).

## imx-bootinfo
The `imx-bootinfo` tool provides storage, similar to U-Boot environment
variables, for information that should persist across reboots. The variables
are stored (with redundancy) outside of any Linux filesystem.

# Builds
This package uses CMake for building.

## Dependencies
This package depends on systemd and libz.

# License
Distributed under license. See the [LICENSE](LICENSE) file for details.
