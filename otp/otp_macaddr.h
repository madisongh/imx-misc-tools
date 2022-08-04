#ifndef otp_macaddr_h_included
#define otp_macaddr_h_included
/*
 * otp_macaddr.h
 *
 * Definitions for the MAC address fuse settings.
 *
 * Copyright (c) 2022, Matthew Madison.
 */
#include <stdint.h>

int otp_macaddr_read(otpctx_t ctx, uint8_t macaddr[6]);
int otp_macaddr_write(otpctx_t ctx, uint8_t macaddr[6]);
#endif /* otp_macaddr_h_included */
