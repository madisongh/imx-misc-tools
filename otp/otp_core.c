/*
 * otp_core.c
 *
 * Core routines for reading/writing OTP fuses via the
 * nvmem interface exported by the imx-ocotp driver.
 *
 * Copyright (c) 2022, Matthew Madison.
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "otp.h"
#include "otp_internal.h"

#define DEFAULT_PATH "/sys/bus/nvmem/devices/imx-ocotp0/nvmem"

struct otpctx_s {
	int fd;
};

static off_t fuseword_offsets[] = {
	[OCOTP_LOCK]		= 0x0,
	[OCOTP_TESTER0]		= 0x4,
	[OCOTP_TESTER1]		= 0x8,
	[OCOTP_TESTER3]		= 0x10,
	[OCOTP_TESTER4]		= 0x14,
	[OCOTP_TESTER5]		= 0x18,
	[OCOTP_BOOT_CFG0]	= 0x1c,
	[OCOTP_BOOT_CFG1]	= 0x20,
	[OCOTP_BOOT_CFG2]	= 0x24,
	[OCOTP_BOOT_CFG3]	= 0x28,
	[OCOTP_BOOT_CFG4]	= 0x2c,
	[OCOTP_SRK0]		= 0x60,
	[OCOTP_SRK1]		= 0x64,
	[OCOTP_SRK2]		= 0x68,
	[OCOTP_SRK3]		= 0x6c,
	[OCOTP_SRK4]		= 0x70,
	[OCOTP_SRK5]		= 0x74,
	[OCOTP_SRK6]		= 0x78,
	[OCOTP_SRK7]		= 0x7c,
	[OCOTP_SJC_RESP0]	= 0x80,
	[OCOTP_SJC_RESP1]	= 0x84,
	[OCOTP_USB_ID]		= 0x88,
	[OCOTP_FIELD_RETURN]	= 0x8c,
	[OCOTP_MAC_ADDR0]	= 0x90,
	[OCOTP_MAC_ADDR1]	= 0x94,
	[OCOTP_MAC_ADDR2]	= 0x98,
	[OCOTP_SRK_REVOKE]	= 0x9c,
	[OCOTP_GP10]		= 0xe0,
	[OCOTP_GP11]		= 0xe4,
	[OCOTP_GP20]		= 0xe8,
	[OCOTP_GP21]		= 0xec,
};

/*
 * is_compatible
 *
 * Sanity checks the SoC ID to make sure it's an i.MX8M Mini,
 * since the fuse definitions here are only for that SoC.
 */
static bool
is_compatible (void)
{
	int fd;
	char socid[32];
	ssize_t n;

	fd = open("/sys/devices/soc0/soc_id", O_RDONLY);
	if (fd < 0)
		return false;
	n = read(fd, socid, sizeof(socid));
	close(fd);
	if (n <= 0 || n >= (ssize_t) sizeof(socid))
		return false;
	// Change terminating \n to \0
	socid[n-1] = '\0';
	return strcmp(socid, "i.MX8MM") == 0;

} /* is_compatible */

/*
 * otp_context_open
 *
 * Set up a context for working with the fuses.
 */
int
otp_context_open (const char *path, bool readonly, otpctx_t *ctxptr)
{
	otpctx_t ctx;

	if (ctxptr == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (!is_compatible()) {
		errno = EFAULT;
		return -1;
	}

	ctx = calloc(1, sizeof(struct otpctx_s));
	if (ctx == NULL)
		return -1;
	ctx->fd = open((path == NULL ? DEFAULT_PATH : path),
		       (readonly ? O_RDONLY : O_RDWR));
	if (ctx->fd < 0) {
		free(ctx);
		return -1;
	}
	*ctxptr = ctx;
	return 0;

} /* otp_context_open */

/*
 * otp_context_close
 *
 * Clean up.
 */
void
otp_context_close (otpctx_t *ctxptr)
{
	if (ctxptr == NULL || *ctxptr == NULL)
		return;
	close((*ctxptr)->fd);
	free(*ctxptr);
	*ctxptr = NULL;

} /* otp_context_close */

// --- Internal functions below this point ---

/*
 * otp___fuseword_offset
 *
 * Returns an offset into the nvmem for a fuse word.
 */
off_t INTERNAL
otp___fuseword_offset (otp_fuseword_id_t id)
{
	if (id >= OTP___FUSEWORD_COUNT)
		return (off_t) (-1);
	return fuseword_offsets[id];

} /* otp___fuseword_offset */

/*
 * otp___fuseword_read
 *
 * Reads a fuse word.
 */
int INTERNAL
otp___fuseword_read (otpctx_t ctx, otp_fuseword_id_t id, uint32_t *val)
{
	if (ctx == NULL || val == NULL || ctx->fd < 0 || id >= OTP___FUSEWORD_COUNT) {
		errno = EINVAL;
		return -1;
	}
	if (lseek(ctx->fd, fuseword_offsets[id], SEEK_SET) < 0)
		return -1;
	if (read(ctx->fd, val, sizeof(uint32_t)) != sizeof(uint32_t))
		return -1;
	return 0;

} /* otp___fuseword_read */

/*
 * otp___fuseword_write
 *
 * Writes a fuse word if the current value does not match
 * the desired value.
 */
int INTERNAL
otp___fuseword_write (otpctx_t ctx, otp_fuseword_id_t id, uint32_t newval)
{

	if (ctx == NULL || ctx->fd < 0 || id >= OTP___FUSEWORD_COUNT) {
		errno = EINVAL;
		return -1;
	}
	if (lseek(ctx->fd, fuseword_offsets[id], SEEK_SET) < 0)
		return -1;
	if (write(ctx->fd, &newval, sizeof(uint32_t)) != sizeof(uint32_t))
		return -1;

	return 0;

} /* otp___fuseword_write */

/*
 * otp___fuseword_update
 *
 * Writes a fuse word if the current value does not match
 * the desired value.
 */
int INTERNAL
otp___fuseword_update (otpctx_t ctx, otp_fuseword_id_t id, uint32_t newval)
{
	uint32_t curval;

	if (ctx == NULL || ctx->fd < 0 || id >= OTP___FUSEWORD_COUNT) {
		errno = EINVAL;
		return -1;
	}
	if (lseek(ctx->fd, fuseword_offsets[id], SEEK_SET) < 0)
		return -1;
	if (read(ctx->fd, &curval, sizeof(uint32_t)) != sizeof(uint32_t))
		return -1;
	if (curval == newval)
		return 0;
	if (lseek(ctx->fd, fuseword_offsets[id], SEEK_SET) < 0)
		return -1;
	if (write(ctx->fd, &newval, sizeof(uint32_t)) != sizeof(uint32_t))
		return -1;

	return 0;

} /* otp___fuseword_update */
