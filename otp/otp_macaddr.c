/*
 * otp_macaddr.c
 *
 * Routines related to the MAC address fuse settings.
 * Copyright (c) 2022-2026, Matthew Madison.
 */

#include <string.h>
#include "otp_internal.h"
#include "otp_macaddr.h"

/*
 * otp_macaddr_read
 * Read the MAC address fuses.
 */
int
otp_macaddr_read (otpctx_t ctx, uint8_t macaddr[ETH_ALEN], uint8_t *mac_count_ptr)
{
	uint32_t mac0, mac1;

	if (ctx == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (otp___fuseword_read(ctx, OCOTP_MAC_ADDR0, &mac0) < 0 ||
	    otp___fuseword_read(ctx, OCOTP_MAC_ADDR1, &mac1) < 0)
		return -1;

	/*
	 * See imx_get_mac_from_fuse() in U-Boot
	 * source file arch/arm/mach-imx/imx9/scmi/soc.c
	 */
	macaddr[5] = (mac1 >> 8) & 0xFF;
	macaddr[4] = mac1 & 0xFF;
	macaddr[3] = (mac0 >> 24) & 0xFF;
	macaddr[2] = (mac0 >> 16) & 0xFF;
	macaddr[1] = (mac0 >> 8) & 0xFF;
	macaddr[0] = mac0 & 0xFF;
	// There is an unused byte between the top
	// of the MAC address and the count
	if (mac_count_ptr != NULL)
		*mac_count_ptr = (mac1 >> 24) & 0xFF;

	return 0;

} /* otp_macaddr_read */

/*
 * otp_macaddr_write
 * Blow the MAC0/1 fuses.  Will check to make sure the
 * current fuses are zero.
 */
int
otp_macaddr_write (otpctx_t ctx, uint8_t newaddr[ETH_ALEN], uint8_t new_count)
{
	uint8_t curaddr[6], oldcount;
	uint32_t mac0, mac1;
	int ret;

	static const uint8_t zeros[6] = { 0 };

	if (otp_macaddr_read(ctx, curaddr, &oldcount) < 0)
		return -1;
	if (memcmp(curaddr, zeros, sizeof(curaddr)) != 0 || oldcount != 0) {
		errno = EALREADY;
		return -1;
	}
	if (memcmp(curaddr, newaddr, sizeof(curaddr)) == 0 && oldcount == new_count)
		return 0;

	mac1 = (new_count << 24) | (newaddr[5] << 8) | newaddr[4];
	mac0 = (newaddr[3] << 24) | (newaddr[2] << 16) |
		(newaddr[1] << 8) | newaddr[0];

 	ret = otp___fuseword_write(ctx, OCOTP_MAC_ADDR0, mac0);
	if (ret == 0)
		ret = otp___fuseword_write(ctx, OCOTP_MAC_ADDR1, mac1);

	return ret;

} /* otp_macaddr_write */
