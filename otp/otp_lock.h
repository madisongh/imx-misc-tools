#ifndef otp_lock_h_included
#define otp_lock_h_included
/*
 * otp_lock.h
 *
 * Definitions for the LOCK fuse settings.
 *
 * Copyright (c) 2022, Matthew Madison.
 */
#include "otp.h"
#include <stdint.h>

#undef OTP_LOCK_2BIT
#define OTP_LOCKS_2BIT \
	OTP_LOCK_2BIT(TESTER) \
	OTP_LOCK_2BIT(BOOT_CFG) \
	OTP_LOCK_2BIT(USB_ID) \
	OTP_LOCK_2BIT(MAC_ADDR) \
	OTP_LOCK_2BIT(GP1) \
	OTP_LOCK_2BIT(GP2) \
	OTP_LOCK_2BIT(GP5)
#undef OTP_LOCK_1BIT
#define OTP_LOCKS_1BIT \
	OTP_LOCK_1BIT(SRK) \
	OTP_LOCK_1BIT(SJC_RESP) \
	OTP_LOCK_1BIT(MANUFACTURE_KEY)

#define OTP_LOCK_2BIT(name_) OTP_LOCK_##name_,
#define OTP_LOCK_1BIT(name_) OTP_LOCK_##name_,
typedef enum {
	OTP_LOCKS_2BIT
	OTP_LOCKS_1BIT
	OTP_LOCK___COUNT
} otp_lock_id_t;
#define OTP_LOCK_COUNT ((unsigned int) OTP_LOCK___COUNT)
#undef OTP_LOCK_2BIT
#undef OTP_LOCK_1BIT

typedef enum {
	OTP_LOCKSTATE_UNLOCKED,     // 1-bit or 2-bit
	OTP_LOCKSTATE_LOCKED,       // 1-bit only
	OTP_LOCKSTATE_O_PROTECT,    // 2-bit only
	OTP_LOCKSTATE_W_PROTECT,    // 2-bit only
	OTP_LOCKSTATE_OW_PROTECT,   // 2-bit only
	OTP_LOCKSTATE___COUNT       // do not use
} otp_lockstate_t;
#define OTP_LOCKSTATE_COUNT ((unsigned int) OTP_LOCKSTATE___COUNT)

int otp_locks_read(otpctx_t ctx, uint32_t *lockword);
int otp_lockstate_get(uint32_t lockword, otp_lock_id_t id, otp_lockstate_t *lockstate);
int otp_lockstate_set(otp_lock_id_t id, otp_lockstate_t newstate, uint32_t *lockword);
int otp_locks_update(otpctx_t ctx, uint32_t lockword);
const char *otp_lock_name(otp_lock_id_t id);

#endif /* otp_lock_h_included */
