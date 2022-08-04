/*
 * otp_srk.c
 *
 * Routines related to the SRK fuse settings.
 *
 * Copyright (c) 2022, Matthew Madison.
 */

#include "otp_internal.h"
#include "otp_srk.h"

static const otp_fuseword_id_t srk_fuse[] = {
	[0] = OCOTP_SRK0,
	[1] = OCOTP_SRK1,
	[2] = OCOTP_SRK2,
	[3] = OCOTP_SRK3,
	[4] = OCOTP_SRK4,
	[5] = OCOTP_SRK5,
	[6] = OCOTP_SRK6,
	[7] = OCOTP_SRK7,
};

/*
 * otp_srk_read
 * Read the SRK fuses. We could take a shortcut
 * here and read them all at once, but for some
 * future platform, the fuses might not be
 * contiguous.
 */
ssize_t
otp_srk_read (otpctx_t ctx, uint32_t *srk_hash, size_t sizeinwords)
{
	ssize_t desired, count;

	desired = (sizeinwords > SRK_FUSE_COUNT ? SRK_FUSE_COUNT : (ssize_t) sizeinwords);
	for (count = 0; count < desired; count += 1)
		if (otp___fuseword_read(ctx, srk_fuse[count], &srk_hash[count]) < 0)
			return -1;
	return count;

} /* otp_srk_read */

/*
 * otp_srk_write
 * Blow the SRK fuses.  Will check to make sure the
 * current fuse is zero or matches the desired value
 * if non-zero.
 */
int
otp_srk_write (otpctx_t ctx, uint32_t *newvals, size_t sizeinwords)
{
	uint32_t curvals[SRK_FUSE_COUNT];
	int i;

	if (sizeinwords != SRK_FUSE_COUNT) {
		errno = EINVAL;
		return -1;
	}

	if (otp_srk_read(ctx, curvals, SRK_FUSE_COUNT) != SRK_FUSE_COUNT)
		return -1;
	/*
	 * Any non-zero values currently set must match the new values
	 * (which we'll skip writing).  Doing anything else is dangerous.
	 */
	for (i = 0; i < SRK_FUSE_COUNT; i++) {
		if (curvals[i] != 0 && curvals[i] != newvals[i]) {
			errno = EINVAL;
			return -1;
		}
	}

	for (i = 0; i < SRK_FUSE_COUNT; i++) {
		if (curvals[i] == newvals[i])
			continue;
		if (otp___fuseword_write(ctx, srk_fuse[i], newvals[i]) < 0)
			return -1;
	}

	return SRK_FUSE_COUNT;

} /* otp_srk_write */
