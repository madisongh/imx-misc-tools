#ifndef otp_machineid_h_included
#define otp_machineid_h_included
/*
 * otp_machineid.h
 *
 * Definitions for the MAC address fuse settings.
 *
 * Copyright (c) 2022-2026, Matthew Madison.
 */
#include <systemd/sd-id128.h>
#include "otp.h"

int otp_machineid_read(otpctx_t ctx, sd_id128_t *id);
int otp_machineid_write(otpctx_t ctx, sd_id128_t id);
#endif /* otp_machineid_h_included */
