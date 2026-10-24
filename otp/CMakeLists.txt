# SPDX-License-Identifier: MIT
# Copyright (c) 2022-2026, Matthew Madison

cmake_minimum_required(VERSION 4.0)
enable_language(C)

set(OTP_HEADERS
  otp.h
  otp_lock.h
  otp_macaddr.h
  otp_machineid.h
  otp_srk.h)
add_library(otp SHARED
  otp_core.c
  otp_internal.h
  otp_lock.c
  otp_macaddr.c
  otp_machineid.c
  otp_srk.c
  ${OTP_HEADERS})
set_target_properties(otp PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION 1)
target_link_libraries(otp PUBLIC PkgConfig::SYSTEMD)
install(TARGETS otp LIBRARY)
install(FILES ${OTP_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libotp)
