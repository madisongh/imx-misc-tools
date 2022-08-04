#ifndef otp_srk_h_included
#define otp_srk_h_included
/*
 * otp_srk.h
 *
 * Definitions for the SRK fuse settings.
 *
 * Copyright (c) 2022, Matthew Madison.
 */
#include "otp.h"
#include <unistd.h>
#include <stdint.h>

#define SRK_FUSE_COUNT 8

ssize_t otp_srk_read(otpctx_t ctx, uint32_t *srk_hash, size_t sizeinwords);
int otp_srk_write(otpctx_t ctx, uint32_t *srk_hash, size_t sizeinwords);

#endif /* otp_srk_h_included */
