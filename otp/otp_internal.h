#ifndef opt_internal_h_included
#define opt_internal_h_included
/*
 * otp_internal.h
 *
 * Internal definitions for the otp library.
 *
 * Copyright (c) 2022, Matthew Madison.
 */

#include "otp.h"

#define INTERNAL __attribute__((visibility("hidden")))
off_t INTERNAL otp___fuseword_offset(otp_fuseword_id_t id);
int INTERNAL otp___fuseword_read(otpctx_t ctx, otp_fuseword_id_t id, uint32_t *val);
int INTERNAL otp___fuseword_write(otpctx_t ctx, otp_fuseword_id_t id, uint32_t newval);
int INTERNAL otp___fuseword_update(otpctx_t ctx, otp_fuseword_id_t id, uint32_t newval);

#endif /* opt_internal_h_included */
