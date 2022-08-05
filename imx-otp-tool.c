/*
 * imx-otp-tool.c
 *
 * Tool for working with i.MX OTP fuses
 *
 * Copyright (c) 2022, Matthew Madison.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "otp.h"
#include "otp_bootcfg.h"
#include "otp_srk.h"
#include "otp_lock.h"

static uint32_t desired_srk_hash[8];
static bool     have_srk_hash = false;
static uint32_t null_hash[8] = { 0 };
static char *progname;
static bool opt_quiet = false;

typedef int (*option_routine_t)(otpctx_t ctx, int argc, char * const argv[]);
static int do_check_secure(otpctx_t ctx, int argc, char * const argv[]);
static int do_secure(otpctx_t ctx, int argc, char * const argv[]);
static int do_show(otpctx_t ctx, int argc, char * const argv[]);

static struct {
	const char *cmd;
	option_routine_t rtn;
	const char *help;
} commands[] = {
        { "is-secured", do_check_secure, "check fuses are set for secure boot" },
        { "secure",     do_secure,       "program fuses for secure boot" },
        { "show",       do_show,         "show fuses" },
};

static struct option options[] = {
	{ "device",		required_argument,	0, 'd' },
	{ "fuse-file",		required_argument,	0, 'f' },
	{ "help",		no_argument,		0, 'h' },
	{ "quiet",		no_argument,		0, 'q' },
	{ 0,			0,			0, 0   }
};
static const char *shortopts = ":d:f:chq";

static char *optarghelp[] = {
	"--device             ",
	"--fuse-file          ",
	"--help               ",
	"--quiet              ",
};

static char *opthelp[] = {
	"path to the OCOTP nvmem device",
	"path to the SRK_1_2_3_4_fuse.bin file",
	"display this help text",
	"omit prompts and information displays",
};


static void
print_usage (void)
{
	unsigned int i;
	printf("\nUsage:\n");
	printf("\t%s [<option>] <command> [fuse] [arg...]\n\n", progname);
	printf("Commands:\n");
	for (i = 0; i < sizeof(commands)/sizeof(commands[0]); i++)
		printf(" %-20.20s %s\n", commands[i].cmd, commands[i].help);
	printf("Options:\n");
	for (i = 0; i < sizeof(options)/sizeof(options[0]) && options[i].name != 0; i++) {
		printf(" %-20.20s %c%c        %s\n",
		       optarghelp[i],
		       (options[i].val == 0 ? ' ' : '-'),
		       (options[i].val == 0 ? ' ' : options[i].val),
		       opthelp[i]);
	}

} /* print_usage */


/*
 * do_show
 *
 * Show fuses.
 *
 */
static int
do_show (otpctx_t ctx, int argc, char * const argv[])
{
	uint32_t srk_hash[SRK_FUSE_COUNT];
	uint32_t bootcfg[OTP_BOOTCFG_WORD_COUNT];
	uint32_t locks;
	otp_lockstate_t lstate;
	bool val, wd_enabled;
	unsigned int i, wd_timeout;

	static struct {
		otp_boot_cfg_id_t id;
		const char *label;
	} bootcfg_fuses[] = {
		{ OTP_BOOT_CFG_SJC_DISABLE, "JTAG disabled:" },
		{ OTP_BOOT_CFG_SEC_CONFIG,  "Secure config closed:" },
		{ OTP_BOOT_CFG_DIR_BT_DIS,  "NXP reserved modes disabled:" },
		{ OTP_BOOT_CFG_BT_FUSE_SEL, "Boot from fuses enabled:" },
		{ OTP_BOOT_CFG_WDOG_ENABLE, "Watchdog enabled:" },
		{ OTP_BOOT_CFG_TZASC_ENABLE,"TZASC enabled:" },
	};
	static const char *lstate_label[OTP_LOCKSTATE_COUNT] = {
		[OTP_LOCKSTATE_UNLOCKED] = "unlocked",
		[OTP_LOCKSTATE_LOCKED] = "locked",
		[OTP_LOCKSTATE_O_PROTECT] = "override-protected",
		[OTP_LOCKSTATE_W_PROTECT] = "write-protected",
		[OTP_LOCKSTATE_OW_PROTECT] = "locked",
	};

	if (otp_srk_read(ctx, srk_hash, SRK_FUSE_COUNT) < 0) {
		perror("otp_srk_read");
		return 1;
	}
	if (otp_bootcfg_read(ctx, bootcfg, OTP_BOOTCFG_WORD_COUNT) < 0) {
		perror("otp_bootcfg_read");
		return 1;
	}
	if (otp_bootcfg_wdog_get(bootcfg, OTP_BOOTCFG_WORD_COUNT, &wd_enabled, &wd_timeout) < 0) {
		perror("otp_bootcfg_wdog_get");
		return 1;
	}
	if (otp_locks_read(ctx, &locks) < 0) {
		perror("otp_locks_read");
		return 1;
	}

	if (otp_lockstate_get(locks, OTP_LOCK_SRK, &lstate) < 0) {
		perror("otp_lockstate_get");
		return 1;
	}

	printf("%-32.32s ", "SRK hashes:");
	if (memcmp(srk_hash, null_hash, sizeof(srk_hash)) == 0)
		printf("not set, %s\n", lstate_label[lstate]);
	else if (have_srk_hash) {
		if (memcmp(srk_hash, desired_srk_hash, sizeof(srk_hash)) == 0)
			printf("correctly programmed, %s\n", lstate_label[lstate]);
		else {
			printf("MISMATCH, %s\n", lstate_label[lstate]);
			for (i = 0; i < SRK_FUSE_COUNT; i++)
				if (srk_hash[i] != desired_srk_hash[i])
					printf("    SRK_HASH[%d]: actual=0x%08x desired=0x%08x\n",
					       i, srk_hash[i], desired_srk_hash[i]);
		}
	} else
		printf("non-null, %s\n", lstate_label[lstate]);

	for (i = 0; i < sizeof(bootcfg_fuses)/sizeof(bootcfg_fuses[0]); i++) {
		if (otp_bootcfg_bool_get(bootcfg, OTP_BOOTCFG_WORD_COUNT,
					 bootcfg_fuses[i].id, &val) < 0) {
			perror("otp_bootcfg_bool_get");
			return 1;
		}
		printf("%-32.32s %s\n", bootcfg_fuses[i].label, (val ? "YES" : "NO"));
		if (i == OTP_BOOT_CFG_WDOG_ENABLE)
			printf("%-32.32s %u sec\n", "Watchdog timeout:", wd_timeout);
	}

	if (otp_lockstate_get(locks, OTP_LOCK_BOOT_CFG, &lstate) < 0) {
		perror("otp_lockstate_get");
		return 1;
	}
	printf("%-32.32s %s\n", "Boot configuration fuses", lstate_label[lstate]);

	return 0;

} /* do_show */


/*
 * do_secure
 */
static int
do_secure (otpctx_t ctx, int argc, char * const argv[])
{
	uint32_t srk_hash[SRK_FUSE_COUNT];
	uint32_t bootcfg[OTP_BOOTCFG_WORD_COUNT];
	uint32_t locks;
	otp_lockstate_t lstate;
	bool val;
	unsigned int i;

	if (!have_srk_hash) {
		fprintf(stderr, "ERR: securing device requires fuse file\n");
		return 1;
	}
	if (otp_srk_read(ctx, srk_hash, SRK_FUSE_COUNT) < 0) {
		perror("otp_srk_read");
		return 1;
	}
	if (memcmp(srk_hash, desired_srk_hash, sizeof(srk_hash)) == 0) {
		if (!opt_quiet)
			printf("SRK fuses already programmed correctly.\n");
	} else {
		// Check for a misprogrammed hash. If the fuses were partially programmed
		// with the desired SRK hashes (some still zero), that's OK.
		for (i = 0; i < SRK_FUSE_COUNT && (srk_hash[i] == 0 || srk_hash[i] == desired_srk_hash[i]); i++);
		if (i < SRK_FUSE_COUNT) {
			fprintf(stderr, "ERR: SRK fuses already programmed with different hashes\n");
			return 1;
		}
		if (!opt_quiet)
			printf("Programming SRK fuses.\n");
		if (otp_srk_write(ctx, desired_srk_hash, SRK_FUSE_COUNT) < 0) {
			perror("otp_srk_write");
			return 1;
		}
		if (!opt_quiet)
			printf("Programmed SRK fuses.\n");
	}
	if (otp_locks_read(ctx, &locks) < 0) {
		perror("otp_locks_read");
		return 1;
	}
	if (otp_lockstate_get(locks, OTP_LOCK_SRK, &lstate) < 0) {
		perror("otp_lockstate_get");
		return 1;
	}
	if (lstate == OTP_LOCKSTATE_LOCKED) {
		if (!opt_quiet)
			printf("SRK fuses already locked.\n");
	} else if (lstate == OTP_LOCKSTATE_UNLOCKED) {
		if (!opt_quiet)
			printf("Locking SRK fuses.\n");
		if (otp_lockstate_set(OTP_LOCK_SRK, OTP_LOCKSTATE_LOCKED, &locks) < 0) {
			perror("otp_lockstate_set");
			return 1;
		}
		if (otp_locks_update(ctx, locks) < 0) {
			perror("otp_locks_update");
			return 1;
		}
		if (!opt_quiet)
			printf("Locked SRK fuses.\n");
	} else {
		fprintf(stderr, "ERR: unknown SRK lockstate: %u\n", lstate);
		return 1;
	}

	if (otp_bootcfg_read(ctx, bootcfg, OTP_BOOTCFG_WORD_COUNT) < 0) {
		perror("otp_bootcfg_read");
		return 1;
	}
	if (otp_bootcfg_bool_get(bootcfg, OTP_BOOTCFG_WORD_COUNT,
				 OTP_BOOT_CFG_SEC_CONFIG, &val) < 0) {
		perror("otp_bootcfg_bool_get");
		return 1;
	}
	if (val) {
		if (!opt_quiet)
			printf("SEC_CONFIG fuse already programmed.\n");
	} else {
		if (!opt_quiet)
			printf("Programming SEC_CONFIG fuse.\n");
		if (otp_bootcfg_bool_set(bootcfg, OTP_BOOTCFG_WORD_COUNT,
					 OTP_BOOT_CFG_SEC_CONFIG, true) < 0) {
			perror("otp_bootcfg_bool_set");
			return 1;
		}
		if (otp_bootcfg_update(ctx, bootcfg, OTP_BOOTCFG_WORD_COUNT) < 0) {
			perror("otp_bootcfg_update");
			return 1;
		}
		if (!opt_quiet)
			printf("SEC_CONFIG fuse programmed.\n");
	}

	return 0;

} /* do_secure */

/*
 * do_check_secure
 */
static int
do_check_secure (otpctx_t ctx, int argc, char * const argv[])
{
	uint32_t bootcfg[OTP_BOOTCFG_WORD_COUNT];
	bool enabled;

	if (otp_bootcfg_read(ctx, bootcfg, OTP_BOOTCFG_WORD_COUNT) < 0) {
		perror("otp_bootcfg_read");
		return 1;
	}
	if (otp_bootcfg_bool_get(bootcfg, OTP_BOOTCFG_WORD_COUNT,
				 OTP_BOOT_CFG_SEC_CONFIG, &enabled) < 0) {
		if (!opt_quiet)
			perror("otp_bootcfg_bool_get");
		return errno;
	}
	if (!opt_quiet)
		printf("Secure boot: %s\n", (enabled ? "ENABLED" : "DISABLED"));

	return (enabled ? 0 : 1);

} /* do_check_secure */

/*
 * main program
 */
int
main (int argc, char * const argv[])
{
	int c, which, ret;
	unsigned int cmd;
	char *argv0_copy = strdup(argv[0]);
	otpctx_t ctx = NULL;
	option_routine_t dispatch = NULL;
	char *nvmem_path = NULL;
	char *fuse_file = NULL;

	progname = basename(argv0_copy);

	for (;;) {
		c = getopt_long_only(argc, argv, shortopts, options, &which);
		if (c == -1)
			break;

		switch (c) {

		case 'h':
			print_usage();
			ret = 0;
			goto depart;
		case 'd':
			nvmem_path = optarg;
			break;
		case 'f':
			fuse_file = optarg;
			break;
		case 'q':
			opt_quiet = true;
			break;
		default:
			fprintf(stderr, "Error: unrecognized option\n");
			ret = 1;
			goto depart;
		}
	}

	argc -= optind;
	argv += optind;

	if (fuse_file != NULL) {
		int fd = open(fuse_file, O_RDONLY);
		ssize_t n;
		if (fd < 0) {
			perror(fuse_file);
			ret = 1;
			goto depart;
		}
		n = read(fd, desired_srk_hash, sizeof(desired_srk_hash));
		close(fd);
		if (n != (ssize_t) sizeof(desired_srk_hash)) {
			if (n < 0)
				perror(fuse_file);
			else
				fprintf(stderr, "%s: file too short\n", fuse_file);
			ret = 1;
			goto depart;
		}
		if (memcmp(desired_srk_hash, null_hash, sizeof(desired_srk_hash)) == 0)
			fprintf(stderr, "%s: ignoring, all SRK fuses are null\n", fuse_file);
		else
			have_srk_hash = true;
	}

	if (argc < 1) {
		print_usage();
		ret = 1;
		goto depart;
	}

	for (cmd = 0; cmd < sizeof(commands)/sizeof(commands[0]); cmd++) {
		if (strcmp(argv[0], commands[cmd].cmd) == 0) {
			dispatch = commands[cmd].rtn;
			break;
		}
	}

	if (dispatch == NULL) {
		fprintf(stderr, "Unrecognized command: %s\n", argv[0]);
		ret = 1;
		goto depart;
	}

	if (otp_context_open(nvmem_path, false, &ctx) < 0) {
		perror("otp_context_open");
		ret = 1;
		goto depart;
	}
	ret = dispatch(ctx, argc-1, &argv[1]);

  depart:
	otp_context_close(&ctx);
	free(argv0_copy);
	return ret;

} /* main */
