#ifndef opt_h_included
#define opt_h_included
/*
 * otp.h
 *
 * Public definitions for the otp library.
 *
 * Copyright (c) 2022, Matthew Madison.
 */

#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

#undef OTP_FUSEWORD
#define OTP_FUSEWORDS \
	OTP_FUSEWORD(LOCK) \
	OTP_FUSEWORD(TESTER0) \
	OTP_FUSEWORD(TESTER1) \
	OTP_FUSEWORD(TESTER3) \
	OTP_FUSEWORD(TESTER4) \
	OTP_FUSEWORD(TESTER5) \
	OTP_FUSEWORD(BOOT_CFG0) \
	OTP_FUSEWORD(BOOT_CFG1) \
	OTP_FUSEWORD(BOOT_CFG2) \
	OTP_FUSEWORD(BOOT_CFG3) \
	OTP_FUSEWORD(BOOT_CFG4) \
	OTP_FUSEWORD(SRK0) \
	OTP_FUSEWORD(SRK1) \
	OTP_FUSEWORD(SRK2) \
	OTP_FUSEWORD(SRK3) \
	OTP_FUSEWORD(SRK4) \
	OTP_FUSEWORD(SRK5) \
	OTP_FUSEWORD(SRK6) \
	OTP_FUSEWORD(SRK7) \
	OTP_FUSEWORD(SJC_RESP0) \
	OTP_FUSEWORD(SJC_RESP1) \
	OTP_FUSEWORD(USB_ID) \
	OTP_FUSEWORD(FIELD_RETURN) \
	OTP_FUSEWORD(MAC_ADDR0) \
	OTP_FUSEWORD(MAC_ADDR1) \
	OTP_FUSEWORD(MAC_ADDR2) \
	OTP_FUSEWORD(SRK_REVOKE) \
	OTP_FUSEWORD(GP10) \
	OTP_FUSEWORD(GP11) \
	OTP_FUSEWORD(GP20) \
	OTP_FUSEWORD(GP21)

#define OTP_FUSEWORD(x_) OCOTP_##x_,
typedef enum {
	OTP_FUSEWORDS
	OTP___FUSEWORD_COUNT
} otp_fuseword_id_t;

#undef OTP_FUSEWORD
#define OTP_FUSEWORD_COUNT ((int) OTP___FUSEWORD_COUNT)

struct otpctx_s;
typedef struct otpctx_s *otpctx_t;

int otp_context_open(const char *path, bool readonly, otpctx_t *ctxptr);
void otp_context_close(otpctx_t *ctxptr);

#endif /* opt_h_included */
