/*
 * otp_lock.c
 *
 * Fuse lock word routines.
 *
 * Copyright (c) 2022-2026, Matthew Madison.
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

static struct {
	int word_index;
	unsigned int fuse_offset;
} lock_positions[] = {
	[OTP_LOCK_HSW_CFG] = { 0, 6 },
	[OTP_LOCK_LOCK_CFG] = { 0, 12 },
	[OTP_LOCK_BOOT_CFG] = { 1, 6 },
	[OTP_LOCK_BOOT2_CFG] = { 1, 9 },
	[OTP_LOCK_GPR1] = { 1, 12 },
	[OTP_LOCK_COM_DEVICE_ID1] = { 2, 0 },
	[OTP_LOCK_COM_DEVICE_ID0] = { 2, 3 },
	[OTP_LOCK_GPR2] = { 3, 0 },
	[OTP_LOCK_GPR3] = { 3, 3 },
	[OTP_LOCK_GPR4] = { 3, 6 },
};

static const otp_fuseword_id_t otp_lock_ids[4] = {
	OCOTP_LOCK0,
	OCOTP_LOCK1,
	OCOTP_LOCK2,
	OCOTP_LOCK3,
};

#define OTP_LOCK_(name_) #name_ "_LOCK",
static const char *locknames[] = {
	OTP_LOCKS
};
#undef OTP_LOCK

// 4 words for LOCK0...3
struct otp_lock_words_s {
	uint32_t lock_word[4];
};

/*
 * otp_locks_read
 *
 * Read the locks word.
 */
int
otp_locks_read (otpctx_t ctx, struct otp_lock_words_s **lock_words_ptr)
{
	struct otp_lock_words_s *lock_words;
	int i, ret;
	if (ctx == NULL || lock_words_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	lock_words = calloc(1, sizeof(struct otp_lock_words_s));
	if (lock_words == NULL) {
		return -1;
	}
	ret = 0;
	for (i = 0; i < sizeof(otp_lock_ids)/sizeof(otp_lock_ids[0]); i++) {
		ret = otp___fuseword_read(ctx, otp_lock_ids[i], &lock_words->lock_word[i]);
		if (ret < 0)
			break;
	}
	if (ret < 0) {
		free(lock_words);
	} else {
		*lock_words_ptr = lock_words;
	}
	return ret;

} /* otp_locks_read */

/*
 * otp_locks_update
 *
 * Update the locks word.
 */
int
otp_locks_update (otpctx_t ctx, struct otp_lock_words_s * lock_words)
{
	int i, ret = 0;
	if (ctx == NULL || lock_words == NULL) {
		errno = EINVAL;
		return -1;
	}
	for (i = 0; i < sizeof(otp_lock_ids)/sizeof(otp_lock_ids[0]); i++) {
		ret = otp___fuseword_update(ctx, otp_lock_ids[i], lock_words->lock_word[i]);
		if (ret < 0)
			break;
	}

	return ret;

} /* otp_locks_update */

/*
 * otp_locks_free
 *
 * Free locks structure allocated by otp_locks_read
 */
void
otp_locks_free (struct otp_lock_words_s **lock_words)
{
	if (lock_words != NULL) {
		if (*lock_words != NULL) {
			free(*lock_words);
		}
		*lock_words = NULL;
	}

} /* otp_locks_free */

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
otp_lockstate_get (struct otp_lock_words_s * lock_words, otp_lock_id_t id, uint8_t *lockstate)
{
	if (lock_words == NULL || lockstate == NULL || (unsigned int) id >= OTP_LOCK_COUNT) {
		errno = EINVAL;
		return -1;
	}

	*lockstate = (lock_words->lock_word[lock_positions[id].word_index] >> lock_positions[id].fuse_offset) & 7U;
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
otp_lockstate_set (otp_lock_id_t id, uint8_t newstate, struct otp_lock_words_s * lock_words)
{
	uint32_t word;

	if (lock_words == NULL || (unsigned int) id >= OTP_LOCK_COUNT) {
		errno = EINVAL;
		return -1;
	}

	word = lock_words->lock_word[lock_positions[id].word_index];
	word &= ~(7U << lock_positions[id].fuse_offset);
	word |= (newstate & 7U) << lock_positions[id].fuse_offset;
	lock_words->lock_word[lock_positions[id].word_index] = word;
	return 0;

} /* otp_lockstate_set */

/*
 * otp_lockstate_string
 *
 * Form a string representing lock state bits.
 * Returns length of the string (not including null terminator).
 * Buffer must include space for the null terminator.
 */
ssize_t
otp_lockstate_string (uint8_t lockstate, char *output_buffer, size_t output_buffer_size)
{
	char *cp = output_buffer;
	size_t remain = output_buffer_size;
	ssize_t len, total_length = 0;
	if (lockstate == OTP_LOCK_UNLOCKED)
		return snprintf(cp, remain, "%s", "UNLOCKED");
	if (lockstate & OTP_LOCK_WRITE_PROTECT) {
		lockstate &= ~OTP_LOCK_WRITE_PROTECT;
		len = snprintf(cp, remain, "%s%s", "WP", (lockstate ? "," : ""));
		if (len < 0)
			return len;
		total_length += len;
		remain -= len;
		cp += len;
	}
	if (lockstate & OTP_LOCK_OVERRIDE_PROTECT) {
		lockstate &= ~OTP_LOCK_OVERRIDE_PROTECT;
		len = snprintf(cp, remain, "%s%s", "OP", (lockstate ? "," : ""));
		if (len < 0)
			return len;
		remain -= len;
		total_length += len;
		cp += len;
	}
	if (lockstate & OTP_LOCK_READ_PROTECT) {
		len = snprintf(cp, remain, "%s", "RP");
		if (len < 0)
			return len;
		remain -= len;
		total_length += len;
		cp += len;
	}
	if (remain > 0) {
		*cp = '\0';
	} else {
		errno = E2BIG;
		return -1;
	}
	return total_length;

} /* otp_lockstate_string */

void
otp_lock_dump_fuses (struct otp_lock_words_s * lock_words, const char *prefix, FILE *output)
{
	int i;
	for (i = 0; i < sizeof(lock_words->lock_word)/sizeof(lock_words->lock_word[0]); i++) {
		fprintf(output, "%s0x%08x\n", prefix, lock_words->lock_word[i]);
	}
	for (i = 0; i < OTP_LOCK_COUNT; i++) {
		otp_lock_id_t id = (otp_lock_id_t) i;
		uint8_t lockstate;
		char statebuf[256];
		otp_lockstate_get(lock_words, id, &lockstate);
		otp_lockstate_string(lockstate, statebuf, sizeof(statebuf));
		fprintf(output, "%s%s: %s\n", prefix, otp_lock_name(id), statebuf);
	}
}
