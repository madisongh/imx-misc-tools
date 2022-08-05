# SPDX-License-Identifier: MIT
# Copyright (c) 2022, Matthew Madison

cmake_minimum_required(VERSION 3.8)
cmake_policy(SET CMP0048 NEW)

project(imx_boot_tool LANGUAGES C VERSION 0.3.0)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)

set(LKDIRGROUP "root" CACHE STRING "Group owner for lock directory")

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZLIB REQUIRED IMPORTED_TARGET zlib)
pkg_check_modules(KEYUTILS REQUIRED IMPORTED_TARGET libkeyutils)
pkg_get_variable(TMPFILESDIR systemd tmpfilesdir)

configure_file(config-files/imx-bootinfo.conf.in imx-bootinfo.conf @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/imx-bootinfo.conf DESTINATION ${TMPFILESDIR})

add_subdirectory(otp)

add_executable(imx-bootinfo imx-bootinfo.c bootinfo.c bootinfo.h util.c util.h posix-crc32.c posix-crc32.h)
target_compile_definitions(imx-bootinfo PUBLIC VERSION="${PROJECT_VERSION}")
target_link_libraries(imx-bootinfo PUBLIC PkgConfig::ZLIB)

add_executable(keystoretool keystoretool.c bootinfo.c bootinfo.h util.c util.h posix-crc32.c posix-crc32.h)
target_link_libraries(keystoretool PUBLIC PkgConfig::KEYUTILS PkgConfig::ZLIB)

add_executable(imx-otp-tool imx-otp-tool.c)
target_include_directories(imx-otp-tool PRIVATE otp)
target_link_libraries(imx-otp-tool otp)

install(TARGETS imx-bootinfo keystoretool imx-otp-tool RUNTIME)
