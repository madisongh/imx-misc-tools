/*
 * otp_core.c
 *
 * Core routines for reading/writing OTP fuses via the
 * nvmem interface exported by the imx-ocotp driver.
 *
 * Copyright (c) 2022-2026, Matthew Madison.
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "otp.h"
#include "otp_internal.h"

#define DEFAULT_PATH "/sys/bus/nvmem/devices/ELE-OCOTP0/nvmem"

struct otpctx_s {
	int fd;
};

/*
 * Offsets listed in the Fuse Map chapter of the
 * i.MX95 reference manual are listed as "fuse word
 * index." A "word" is 4 bytes, so the index numbers
 * in the manual should be multiplied by 4 to get the
 * offset into the nvmem pseudo-file.
 */
#define FUSE_WORD_INDEX(n_) ((n_) << 2U)
static off_t fuseword_offsets[] = {
	[OCOTP_LOCK0]		= FUSE_WORD_INDEX(0),
	[OCOTP_LOCK1]		= FUSE_WORD_INDEX(7),
	[OCOTP_LOCK2]		= FUSE_WORD_INDEX(10),
	[OCOTP_LOCK3]		= FUSE_WORD_INDEX(11),
	[OCOTP_BOOT_CFG0]	= FUSE_WORD_INDEX(24),
	[OCOTP_BOOT_CFG1]	= FUSE_WORD_INDEX(25),
	[OCOTP_BOOT_CFG2]	= FUSE_WORD_INDEX(26),
	[OCOTP_BOOT_CFG3]	= FUSE_WORD_INDEX(27),
	[OCOTP_BOOT_CFG8]	= FUSE_WORD_INDEX(32),
	[OCOTP_BOOT_CFG11]	= FUSE_WORD_INDEX(35),
	[OCOTP_GPR0_CFG0]   = FUSE_WORD_INDEX(36),
	[OCOTP_GPR0_CFG1]   = FUSE_WORD_INDEX(37),
	[OCOTP_SRK0]		= FUSE_WORD_INDEX(128),
	[OCOTP_SRK1]		= FUSE_WORD_INDEX(129),
	[OCOTP_SRK2]		= FUSE_WORD_INDEX(130),
	[OCOTP_SRK3]		= FUSE_WORD_INDEX(131),
	[OCOTP_SRK4]		= FUSE_WORD_INDEX(132),
	[OCOTP_SRK5]		= FUSE_WORD_INDEX(133),
	[OCOTP_SRK6]		= FUSE_WORD_INDEX(134),
	[OCOTP_SRK7]		= FUSE_WORD_INDEX(135),
	[OCOTP_SRK8]		= FUSE_WORD_INDEX(136),
	[OCOTP_SRK9]		= FUSE_WORD_INDEX(137),
	[OCOTP_SRKA]		= FUSE_WORD_INDEX(138),
	[OCOTP_SRKB]		= FUSE_WORD_INDEX(139),
	[OCOTP_SRKC]		= FUSE_WORD_INDEX(140),
	[OCOTP_SRKD]		= FUSE_WORD_INDEX(141),
	[OCOTP_SRKE]		= FUSE_WORD_INDEX(142),
	[OCOTP_SRKF]		= FUSE_WORD_INDEX(143),
	[OCOTP_GPR1_CFG0]   = FUSE_WORD_INDEX(180),
	[OCOTP_GPR1_CFG1]   = FUSE_WORD_INDEX(181),
	[OCOTP_GPR1_CFG2]   = FUSE_WORD_INDEX(182),
	[OCOTP_GPR1_CFG3]   = FUSE_WORD_INDEX(183),
	[OCOTP_COM_DEVICE_ID_CFG0] = FUSE_WORD_INDEX(322),
	[OCOTP_COM_DEVICE_ID_CFG1] = FUSE_WORD_INDEX(323),
	[OCOTP_MAC_ADDR0]	= FUSE_WORD_INDEX(325), // bits 47...16
	[OCOTP_MAC_ADDR1]	= FUSE_WORD_INDEX(326), // bits 15...0 and 8-bit count
	[OCOTP_GPR4_CFG0]	= FUSE_WORD_INDEX(376),
	[OCOTP_GPR4_CFG1]	= FUSE_WORD_INDEX(377),
	[OCOTP_GPR4_CFG2]	= FUSE_WORD_INDEX(378),
	[OCOTP_GPR4_CFG3]	= FUSE_WORD_INDEX(379),
	[OCOTP_GPR4_CFG4]	= FUSE_WORD_INDEX(380),
	[OCOTP_GPR4_CFG5]	= FUSE_WORD_INDEX(381),
	[OCOTP_GPR4_CFG6]	= FUSE_WORD_INDEX(382),
	[OCOTP_GPR4_CFG7]	= FUSE_WORD_INDEX(383),
	[OCOTP_GPR4_CFG8]	= FUSE_WORD_INDEX(384),
	[OCOTP_GPR4_CFG9]	= FUSE_WORD_INDEX(385),
	[OCOTP_GPR4_CFG10]	= FUSE_WORD_INDEX(386),
	[OCOTP_GPR4_CFG11]	= FUSE_WORD_INDEX(387),
	[OCOTP_GPR4_CFG12]	= FUSE_WORD_INDEX(388),
	[OCOTP_GPR4_CFG13]	= FUSE_WORD_INDEX(389),
	[OCOTP_GPR4_CFG14]	= FUSE_WORD_INDEX(390),
	[OCOTP_GPR4_CFG15]	= FUSE_WORD_INDEX(391),
};

/*
 * is_compatible
 *
 * Sanity checks the SoC ID to make sure it's an i.MX95,
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
	return strcmp(socid, "i.MX959") == 0;

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
