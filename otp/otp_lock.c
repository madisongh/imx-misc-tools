/*
 * otp_lock.c
 *
 * Fuse lock word routines.
 *
 * Copyright (c) 2022, Matthew Madison.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "otp_internal.h"
#include "otp_lock.h"

static unsigned int lock_offsets[] = {
	[OTP_LOCK_TESTER] = 0,
	[OTP_LOCK_BOOT_CFG] = 2,
	[OTP_LOCK_SRK] = 9,
	[OTP_LOCK_SJC_RESP] = 10,
	[OTP_LOCK_USB_ID] = 12,
	[OTP_LOCK_MAC_ADDR] = 14,
	[OTP_LOCK_MANUFACTURE_KEY] = 16,
	[OTP_LOCK_GP1] = 20,
	[OTP_LOCK_GP2] = 22,
	[OTP_LOCK_GP5] = 24,
};

#define OTP_LOCK_1BIT(name_) [OTP_LOCK_##name_] = true,
#define OTP_LOCK_2BIT(name_) [OTP_LOCK_##name_] = false,
static bool lock_is_1bit[] = {
	OTP_LOCKS_2BIT
	OTP_LOCKS_1BIT
};
#undef OTP_LOCK_1BIT
#undef OTP_LOCK_2BIT

#define OTP_LOCK_1BIT(name_) #name_,
#define OTP_LOCK_2BIT(name_) #name_,
static const char *locknames[] = {
	OTP_LOCKS_2BIT
	OTP_LOCKS_1BIT
};
#undef OTP_LOCK_1BIT
#undef OTP_LOCK_2BIT

/*
 * otp_locks_read
 *
 * Read the locks word.
 */
int
otp_locks_read (otpctx_t ctx, uint32_t *lockword)
{
	if (ctx == NULL || lockword == NULL) {
		errno = EINVAL;
		return -1;
	}
	return otp___fuseword_read(ctx, OCOTP_LOCK, lockword);

} /* otp_locks_read */

/*
 * otp_locks_update
 *
 * Update the locks word.
 */
int
otp_locks_update (otpctx_t ctx, uint32_t lockword)
{
	if (ctx == NULL) {
		errno = EINVAL;
		return -1;
	}
	return otp___fuseword_update(ctx, OCOTP_LOCK, lockword);

} /* otp_locks_upate */

/*
 * otp_lock_name
 *
 * Return the label for a particular lock. Returns NULL for
 * an invalid lock ID value.
 */
const char *
otp_lock_name (otp_lock_id_t id)
{
	if ((unsigned int) id >= sizeof(locknames)) {
		errno = EINVAL;
		return NULL;
	}
	return locknames[id];

} /* otp_lock_name */

/*
 * otp_lockstate_get
 *
 * Get the state of a lock from the locks fuse word.
 */
int
otp_lockstate_get (uint32_t lockword, otp_lock_id_t id, otp_lockstate_t *lockstate)
{
	static otp_lockstate_t twobit_states[4] = {
		OTP_LOCKSTATE_UNLOCKED,
		OTP_LOCKSTATE_W_PROTECT,
		OTP_LOCKSTATE_O_PROTECT,
		OTP_LOCKSTATE_OW_PROTECT
	};

	if (lockstate == NULL || (unsigned int) id >= OTP_LOCK_COUNT) {
		errno = EINVAL;
		return -1;
	}

	if (lock_is_1bit[id])
		*lockstate = ((lockword >> lock_offsets[id]) & 1) ? OTP_LOCKSTATE_LOCKED : OTP_LOCKSTATE_UNLOCKED;
	else
		*lockstate = twobit_states[(lockword >> lock_offsets[id]) & 3];
	return 0;

} /* otp_lockstate_get */

/*
 * otp_lockstate_set
 *
 * Update the bits for a single lock. While this function just performs
 * the bit manipulation, please note that real fuses are one-time programmable,
 * so if a lock bit was previously set, it does not make sense to try to
 * clear it.
 */
int
otp_lockstate_set (otp_lock_id_t id, otp_lockstate_t newstate, uint32_t *lockword)
{

	if (lockword == NULL || (unsigned int) id >= OTP_LOCK_COUNT ||
	    (unsigned int) newstate >= OTP_LOCKSTATE_COUNT) {
		errno = EINVAL;
		return -1;
	}
	if (lock_is_1bit[id]) {
		uint32_t mask = 1U << lock_offsets[id];
		if (newstate == OTP_LOCKSTATE_UNLOCKED)
			*lockword &= ~mask;
		else if (newstate == OTP_LOCKSTATE_LOCKED)
			*lockword |= mask;
		else {
			errno = EINVAL;
			return -1;
		}
	} else {
		uint32_t off_mask, on_mask;
		if (newstate == OTP_LOCKSTATE_UNLOCKED) {
			off_mask = (3U << lock_offsets[id]);
			on_mask = 0;
		} else if (newstate == OTP_LOCKSTATE_O_PROTECT) {
			off_mask = (1U << lock_offsets[id]);
			on_mask =  (2U << lock_offsets[id]);
		} else if (newstate == OTP_LOCKSTATE_W_PROTECT) {
			off_mask = (2U << lock_offsets[id]);
			on_mask =  (1U << lock_offsets[id]);
		} else if (newstate == OTP_LOCKSTATE_OW_PROTECT) {
			off_mask = 0;
			on_mask =  (3U << lock_offsets[id]);
		} else {
			errno = EINVAL;
			return -1;
		}
		*lockword &= ~off_mask;
		*lockword |= on_mask;
	}
	return 0;

} /* otp_lockstate_set */
