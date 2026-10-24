#ifndef otp_macaddr_h_included
#define otp_macaddr_h_included
/*
 * otp_macaddr.h
 *
 * Definitions for the MAC address fuse settings.
 *
 * Copyright (c) 2022-2026, Matthew Madison.
 */
#include <stdint.h>
#include <net/ethernet.h>

int otp_macaddr_read(otpctx_t ctx, uint8_t macaddr[ETH_ALEN], uint8_t *count);
int otp_macaddr_write(otpctx_t ctx, uint8_t macaddr[ETH_ALEN], uint8_t count);
#endif /* otp_macaddr_h_included */
