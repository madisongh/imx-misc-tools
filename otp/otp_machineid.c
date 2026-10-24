/*
 * otp_machineid.c
 *
 * Routines related to the MAC address fuse settings.
 * Copyright (c) 2026, Matthew Madison.
 */

#include <string.h>
#include "otp_internal.h"
#include "otp_machineid.h"

static const otp_fuseword_id_t id_fuses[4] = {
	OCOTP_GPR1_CFG0,
	OCOTP_GPR1_CFG1,
	OCOTP_GPR1_CFG2,
	OCOTP_GPR1_CFG3,
};

/*
 * otp_machineid_read
 * Read the GPR1 fuses we use for storing the machine ID.
 */
int
otp_machineid_read (otpctx_t ctx, sd_id128_t *id)
{
	int i;

	if (ctx == NULL || id == NULL) {
		errno = EINVAL;
		return -1;
	}
	for (i = 0; i < 4; i++) {
		uint32_t *id_words = (uint32_t *) id;
		if (otp___fuseword_read(ctx, id_fuses[i], &id_words[i]) < 0)
			return -1;
	}

	return 0;

} /* otp_machineid_read */

/*
 * otp_machineid_write
 * Blow the GPR1 fuses.  Will check to make sure the
 * current fuses are zero.
 */
int
otp_machineid_write (otpctx_t ctx, sd_id128_t id)
{
	sd_id128_t oldid;
	int ret, i;

	if (sd_id128_is_null(id)) {
		errno = EINVAL;
		return -1;
	}
	if (otp_machineid_read(ctx, &oldid) < 0)
		return -1;
	if (!sd_id128_is_null(oldid)) {
		errno = EALREADY;
		return -1;
	}
	if (sd_id128_equal(oldid, id))
		return 0;

	for (i = 0, ret = 0; i < 4 && ret == 0; i++) {
		uint32_t *id_words = (uint32_t *) &id;
		ret = otp___fuseword_write(ctx, id_fuses[i], id_words[i]);
	}

	return ret;

} /* otp_machineid_write */
