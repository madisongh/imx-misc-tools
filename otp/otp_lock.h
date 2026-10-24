#ifndef otp_lock_h_included
#define otp_lock_h_included
/*
 * otp_lock.h
 *
 * Definitions for the LOCK fuse settings.
 *
 * Copyright (c) 2022-2026, Matthew Madison.
 */
#include "otp.h"
#include <stdint.h>

#define OTP_LOCKS \
	OTP_LOCK_(HSW_CFG) \
	OTP_LOCK_(LOCK_CFG) \
	OTP_LOCK_(BOOT_CFG) \
	OTP_LOCK_(BOOT2_CFG) \
	OTP_LOCK_(GPR1) \
	OTP_LOCK_(COM_DEVICE_ID1) \
	OTP_LOCK_(COM_DEVICE_ID0) \
	OTP_LOCK_(GPR2) \
	OTP_LOCK_(GPR3) \
	OTP_LOCK_(GPR4)

#define OTP_LOCK_(name_) OTP_LOCK_##name_,
typedef enum {
	OTP_LOCKS
	OTP_LOCK___COUNT
} otp_lock_id_t;
#undef OTP_LOCK_
#define OTP_LOCK_COUNT ((unsigned int) OTP_LOCK___COUNT)

#define OTP_LOCK_UNLOCKED			0
#define OTP_LOCK_WRITE_PROTECT		(1<<0)
#define OTP_LOCK_OVERRIDE_PROTECT	(1<<1)
#define OTP_LOCK_READ_PROTECT		(1<<2)

#define OTP_LOCK_WRITE_LOCKED (OTP_LOCK_WRITE_PROTECT|OTP_LOCK_OVERRIDE_PROTECT)
#define OTP_LOCK_READ_LOCKED (OTP_LOCK_WRITE_LOCKED|OTP_LOCK_READ_PROTECT)

struct otp_lock_words_s;
int otp_locks_read(otpctx_t ctx, struct otp_lock_words_s **lock_words_ptr);
int otp_lockstate_get(struct otp_lock_words_s * lock_words, otp_lock_id_t id, uint8_t *lockstate);
int otp_lockstate_set(otp_lock_id_t id, uint8_t lockstate, struct otp_lock_words_s * lock_words);
int otp_locks_update(otpctx_t ctx, struct otp_lock_words_s * lock_words);
void otp_locks_free(struct otp_lock_words_s **lock_words);
const char *otp_lock_name(otp_lock_id_t id);
ssize_t otp_lockstate_string(uint8_t lockstate, char *output_buffer, size_t output_buffer_size);
void otp_lock_dump_fuses(struct otp_lock_words_s * lock_words, const char *prefix, FILE *fp);

#endif /* otp_lock_h_included */
