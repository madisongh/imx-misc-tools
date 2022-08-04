#ifndef otp_bootcfg_h_included
#define otp_bootcfg_h_included
/*
 * otp_bootcfg.h
 *
 * Definitions for the BOOT_CFGx fuse settings.
 *
 * Copyright (c) 2022, Matthew Madison.
 */
#include "otp.h"
#include <stdint.h>
#include <unistd.h>

#undef OTP_BOOTCFG0
#define OTP_BOOT_CFG0_GENERIC_FUSES \
	OTP_BOOTCFG0(DIR_BT_DIS) \
	OTP_BOOTCFG0(BT_FUSE_SEL) \
	OTP_BOOTCFG0(SJC_DISABLE) \
	OTP_BOOTCFG0(SEC_CONFIG)
#undef OTP_BOOTCFG1
#define OTP_BOOT_CFG1_GENERIC_FUSES \
	OTP_BOOTCFG1(WDOG_ENABLE) \
	OTP_BOOTCFG1(TZASC_ENABLE) \
	OTP_BOOTCFG1(WDOG_TIMEOUT)
#define OTP_BOOTCFG0(name_) OTP_BOOT_CFG_##name_,
#define OTP_BOOTCFG1(name_) OTP_BOOT_CFG_##name_,
typedef enum {
	OTP_BOOT_CFG0_GENERIC_FUSES
	OTP_BOOT_CFG1_GENERIC_FUSES
	OTP_BOOT_CFG___COUNT
} otp_boot_cfg_id_t;
#undef OTP_BOOTCFG0
#undef OTP_BOOTCFG1

#define OTP_BOOT_CFG_COUNT ((unsigned int) OTP_BOOT_CFG___COUNT)
#define OTP_BOOTCFG_WORD_COUNT 5

ssize_t otp_bootcfg_read(otpctx_t ctx, uint32_t *fusewords, size_t sizeinwords);
int otp_bootcfg_bool_get(uint32_t *fusewords, size_t sizeinwords,
			 otp_boot_cfg_id_t id, bool *value);
int otp_bootcfg_wdog_get(uint32_t *fusewords, size_t sizeinwords,
			 bool *enabled, unsigned int *timeout_in_seconds);
int otp_bootcfg_bool_set(uint32_t *fusewords, size_t sizeinwords,
			 otp_boot_cfg_id_t id, bool newvalue);
int otp_bootcfg_wdog_set(uint32_t *fusewords, size_t sizeinwords,
			 bool enable, unsigned int timeout_in_seconds);
#endif /* otp_bootcfg_h_included */
