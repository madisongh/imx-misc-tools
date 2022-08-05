/*
 * otp_bootcfg.c
 *
 * Routines related to the BOOT_CFGx fuse settings.
 *
 * Note that only the subset of these fuses, related
 * to enabling secure boot, are handled.
 *
 * Copyright (c) 2022, Matthew Madison.
 */

#include "otp_internal.h"
#include "otp_bootcfg.h"

static const otp_fuseword_id_t bootcfg_fuses[OTP_BOOTCFG_WORD_COUNT] = {
	OCOTP_BOOT_CFG0,
	OCOTP_BOOT_CFG1,
	OCOTP_BOOT_CFG2,
	OCOTP_BOOT_CFG3,
	OCOTP_BOOT_CFG4,
};

// Index into above array to identify the fuse word
#define OTP_BOOTCFG0(name_) [OTP_BOOT_CFG_##name_] = 0,
#define OTP_BOOTCFG1(name_) [OTP_BOOT_CFG_##name_] = 1,
static const int bootcfg_fuseword_index[] = {
	OTP_BOOT_CFG0_GENERIC_FUSES
	OTP_BOOT_CFG1_GENERIC_FUSES
};
#undef OTP_BOOTCFG0
#undef OTP_BOOTCFG1

// Offset of the field within the fuse word
static const unsigned int bootcfg_offset[] = {
	// BOOT_CFG0
	[OTP_BOOT_CFG_SJC_DISABLE]  = 21,
	[OTP_BOOT_CFG_SEC_CONFIG]   = 25,
	[OTP_BOOT_CFG_DIR_BT_DIS]   = 27,
	[OTP_BOOT_CFG_BT_FUSE_SEL]  = 28,
	// BOOT_CFG1
	[OTP_BOOT_CFG_WDOG_ENABLE]  = 10,
	[OTP_BOOT_CFG_TZASC_ENABLE] = 11,
	[OTP_BOOT_CFG_WDOG_TIMEOUT] = 16,
};

static const unsigned int timeouts[] = {
	[0] = 64,
	[1] = 32,
	[2] = 16,
	[3] =  8,
	[4] =  4,
};

/*
 * otp_bootcfg_read
 *
 * Reads in the BOOT_CFGx fuse words for processing.
 */
ssize_t
otp_bootcfg_read (otpctx_t ctx, uint32_t *fusewords, size_t sizeinwords)
{
	ssize_t count;

	if (ctx == NULL || fusewords == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (sizeinwords > OTP_BOOTCFG_WORD_COUNT)
		sizeinwords = OTP_BOOTCFG_WORD_COUNT;

	for (count = 0; count < (ssize_t) sizeinwords; count += 1)
		if (otp___fuseword_read(ctx, bootcfg_fuses[count], &fusewords[count]) < 0)
			return -1;
	return count;

} /* otp_bootcfg_read */

/*
 * otp_bootcfg_update
 *
 * Updates the BOOT_CFGx fuse words. The size of the fuse word
 * vector must exactly match the size in the fuse box.
 */
int
otp_bootcfg_update (otpctx_t ctx, uint32_t *newvals, size_t sizeinwords)
{
	ssize_t count;
	uint32_t curvals[OTP_BOOTCFG_WORD_COUNT];

	if (ctx == NULL || newvals == NULL || sizeinwords != OTP_BOOTCFG_WORD_COUNT) {
		errno = EINVAL;
		return -1;
	}

	for (count = 0; count < OTP_BOOTCFG_WORD_COUNT; count += 1)
		if (otp___fuseword_read(ctx, bootcfg_fuses[count], &curvals[count]) < 0)
			return -1;
	for (count = 0; count < OTP_BOOTCFG_WORD_COUNT; count += 1) {
		if (curvals[count] == newvals[count])
			continue;
		if (otp___fuseword_write(ctx, bootcfg_fuses[count], newvals[count]) < 0)
			return -1;
	}
	return 0;

} /* otp_bootcfg_update */

/*
 * otp_bootcfg_bool_get
 *
 * Extracts the setting of a boolean (1-bit) fuse in the
 * BOOT_CFGx fuses.
 */
int
otp_bootcfg_bool_get (uint32_t *fusewords, size_t sizeinwords,
		      otp_boot_cfg_id_t id, bool *value)
{
	// XXX - magic 2 here because we're only working with
	//       BOOT_CFG0 and 1 - XXX
	if (fusewords == NULL || value == NULL ||
	    sizeinwords < 2 || id >= OTP_BOOT_CFG_COUNT) {
		errno = EINVAL;
		return -1;
	}

	*value = (fusewords[bootcfg_fuseword_index[id]] & (1U << bootcfg_offset[id])) != 0;
	return 0;

} /* otp_bootcfg_bool_get */


/*
 * otp_bootcfg_wdog_get
 *
 * Extracts the watchdog settings from the
 * BOOT_CFGx fuses.
 */
int
otp_bootcfg_wdog_get (uint32_t *fusewords, size_t sizeinwords,
		      bool *enabled, unsigned int *timeout_in_seconds)
{
	unsigned int tmo;
	if (timeout_in_seconds == NULL) {
		errno = EINVAL;
		return -1;
	}
	// The bool_get function validates the other args for us
	if (otp_bootcfg_bool_get(fusewords, sizeinwords, OTP_BOOT_CFG_WDOG_ENABLE, enabled) < 0)
		return -1;

	tmo = fusewords[bootcfg_fuseword_index[OTP_BOOT_CFG_WDOG_ENABLE]] & (3U << bootcfg_offset[OTP_BOOT_CFG_WDOG_ENABLE]);
	if (tmo >= sizeof(timeouts))
		*timeout_in_seconds = 0;
	*timeout_in_seconds = timeouts[tmo];

	return 0;

} /* otp_bootcfg_wdog_get */

/*
 * otp_bootcfg_bool_set
 *
 * Sets/clears a BOOT_CFGx fuse bit in the fusewords array.
 */
int
otp_bootcfg_bool_set (uint32_t *fusewords, size_t sizeinwords,
		      otp_boot_cfg_id_t id, bool value)
{
	// XXX - magic 2 here because we're only working with
	//       BOOT_CFG0 and 1 - XXX
	if (fusewords == NULL || sizeinwords < 2 || id >= OTP_BOOT_CFG_COUNT) {
		errno = EINVAL;
		return -1;
	}

	if (value)
		fusewords[bootcfg_fuseword_index[id]] |= 1U << bootcfg_offset[id];
	else
		fusewords[bootcfg_fuseword_index[id]] &= ~(1U << bootcfg_offset[id]);

	return 0;

} /* otp_bootcfg_bool_set */

/*
 * otp_bootcfg_wdog_set
 *
 * Sets the watchdog setting in the BOOT_CFGx fuses.
 * (Only needed if you need to program a different timeout, but you
 *  can pass 0 for timeout_in_seconds to leave it unchanged.)
 */
int
otp_bootcfg_wdog_set (uint32_t *fusewords, size_t sizeinwords,
		      bool enabled, unsigned int timeout_in_seconds)
{
	unsigned int tmo;
	if (timeout_in_seconds != 0) {
		for (tmo = 0; tmo < sizeof(timeouts) && timeouts[tmo] != timeout_in_seconds; tmo += 1);
		if (tmo >= sizeof(timeouts)) {
			errno = EINVAL;
			return -1;
		}
	}
	// The bool_set function validates the other args for us
	if (otp_bootcfg_bool_set(fusewords, sizeinwords, OTP_BOOT_CFG_WDOG_ENABLE, enabled) < 0)
		return -1;

	if (timeout_in_seconds != 0) {
		fusewords[bootcfg_fuseword_index[OTP_BOOT_CFG_WDOG_ENABLE]] &= ~(3U << bootcfg_offset[OTP_BOOT_CFG_WDOG_ENABLE]);
		fusewords[bootcfg_fuseword_index[OTP_BOOT_CFG_WDOG_ENABLE]] |=  (tmo << bootcfg_offset[OTP_BOOT_CFG_WDOG_ENABLE]);
	}

	return 0;

} /* otp_bootcfg_wdog_set */
