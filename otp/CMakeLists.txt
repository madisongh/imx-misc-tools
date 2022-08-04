# SPDX-License-Identifier: MIT
# Copyright (c) 2022, Matthew Madison

cmake_minimum_required(VERSION 3.8)
cmake_policy(SET CMP0048 NEW)

set(OTP_HEADERS
  otp.h
  otp_bootcfg.h
  otp_lock.h
  otp_macaddr.h
  otp_srk.h)
add_library(otp SHARED
  otp_bootcfg.c
  otp_core.c
  otp_internal.h
  otp_lock.c
  otp_macaddr.c
  otp_srk.c
  ${OTP_HEADERS})
set_target_properties(otp PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION 1)
install(TARGETS otp LIBRARY)
install(FILES ${OTP_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libotp)
