/*
 * otp_macaddr.c
 *
 * Routines related to the MAC address fuse settings.
 * Copyright (c) 2022, Matthew Madison.
 */

#include <string.h>
#include "otp_internal.h"
#include "otp_macaddr.h"

/*
 * otp_macaddr_read
 * Read the MAC address fuses.
 */
int
otp_macaddr_read (otpctx_t ctx, uint8_t macaddr[6])
{
	uint32_t mac0, mac1;

	if (ctx == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (otp___fuseword_read(ctx, OCOTP_MAC_ADDR0, &mac0) < 0 ||
	    otp___fuseword_read(ctx, OCOTP_MAC_ADDR1, &mac1) < 0)
		return -1;

	macaddr[0] = (mac1 >> 8) & 0xFF;
	macaddr[1] = mac1 & 0xFF;
	macaddr[2] = (mac0 >> 24) & 0xFF;
	macaddr[3] = (mac0 >> 16) & 0xFF;
	macaddr[4] = (mac0 >> 8) & 0xFF;
	macaddr[5] = mac0 & 0xFF;

	return 0;

} /* otp_macaddr_read */

/*
 * otp_macaddr_write
 * Blow the MAC0/1 fuses.  Will check to make sure the
 * current fuses are zero.
 */
int
otp_macaddr_write (otpctx_t ctx, uint8_t newaddr[6])
{
	uint8_t curaddr[6];
	uint32_t mac0, mac1;
	int ret;

	static const uint8_t zeros[6] = { 0 };

	if (otp_macaddr_read(ctx, curaddr) < 0)
		return -1;
	if (memcmp(curaddr, zeros, sizeof(curaddr)) != 0) {
		errno = EALREADY;
		return -1;
	}
	if (memcmp(curaddr, newaddr, sizeof(curaddr)) == 0)
		return 0;

	mac1 = (newaddr[0] << 8) | newaddr[1];
	mac0 = (newaddr[2] << 24) | (newaddr[3] << 16) |
		(newaddr[4] << 8) | newaddr[5];

 	ret = otp___fuseword_write(ctx, OCOTP_MAC_ADDR0, mac0);
	if (ret == 0)
		ret = otp___fuseword_write(ctx, OCOTP_MAC_ADDR1, mac1);

	return ret;

} /* otp_macaddr_write */
