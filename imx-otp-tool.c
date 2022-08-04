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

typedef int (*option_routine_t)(otpctx_t ctx, int argc, char * const argv[]);
static int do_program(otpctx_t ctx, int argc, char * const argv[]);
static int do_show(otpctx_t ctx, int argc, char * const argv[]);
static int do_lock(otpctx_t ctx, int argc, char * const argv[]);

static struct {
	const char *cmd;
	option_routine_t rtn;
	const char *help;
} commands[] = {
        { "program",    do_program,    "program fuses" },
        { "show",       do_show,       "show fuses" },
        { "lock",       do_lock,       "lock fuses" },
};

static struct option options[] = {
	{ "device",		required_argument,	0, 'd' },
	{ "fuse-file",		required_argument,	0, 'f' },
	{ "help",		no_argument,		0, 'h' },
	{ 0,			0,			0, 0   }
};
static const char *shortopts = ":d:f:ch";

static char *optarghelp[] = {
	"--device             ",
	"--fuse-file          ",
	"--help               ",
};

static char *opthelp[] = {
	"path to the OCOTP nvmem device",
	"path to the SRK_1_2_3_4_fuse.bin file",
	"display this help text",
};


static void
print_usage (void)
{
	unsigned int i;
	printf("\nUsage:\n");
	printf("\t%s [<option>] <command> [fuse] [arg...]\n\n", progname);
	printf("Commands:\n");
	for (i = 0; i < sizeof(commands)/sizeof(commands[0]); i++)
		printf(" %s\t\t%s\n", commands[i].cmd, commands[i].help);
	printf("Options:\n");
	for (i = 0; i < sizeof(options)/sizeof(options[0]) && options[i].name != 0; i++) {
		printf(" %s\t%c%c\t%s\n",
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
	bool val;
	unsigned int i;

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
	
	if (otp_srk_read(ctx, srk_hash, SRK_FUSE_COUNT) < 0) {
		perror("otp_srk_read");
		return 1;
	}
	if (otp_bootcfg_read(ctx, bootcfg, OTP_BOOTCFG_WORD_COUNT) < 0) {
		perror("otp_bootcfg_read");
		return 1;
	}

	for (i = 0; i < SRK_FUSE_COUNT; i++)
		printf("SRK_HASH[%d]: %08x\n", i, srk_hash[i]);
	putchar('\n');
	if (memcmp(srk_hash, null_hash, sizeof(srk_hash)) == 0)
		printf("No SRK hashes programmed.\n");
	else if (have_srk_hash) {
		if (memcmp(srk_hash, desired_srk_hash, sizeof(srk_hash)) == 0)
			printf("SRK fuses match desired programming.\n");
		else
			printf("SRK fuses DO NOT MATCH desired programming.\n");
	}

	for (i = 0; i < sizeof(bootcfg_fuses)/sizeof(bootcfg_fuses[0]); i++) {
		if (otp_bootcfg_bool_get(bootcfg, OTP_BOOTCFG_WORD_COUNT,
					 bootcfg_fuses[i].id, &val) < 0) {
			perror("otp_bootcfg_bool_get");
			return 1;
		}
		printf("%-32.32s %s\n", bootcfg_fuses[i].label, (val ? "YES" : "NO"));
	}

	return 0;

} /* do_show */


/*
 * do_program
 */
static int
do_program (otpctx_t ctx, int argc, char * const argv[])
{
	printf("TBD\n");
	return 0;

} /* do_program */

/*
 * do_lock
 */
static int
do_lock (otpctx_t ctx, int argc, char * const argv[])
{
	printf("TBD\n");
	return 0;

} /* do_lock */

/*
 * main program
 */
int
main (int argc, char * const argv[])
{
	int c, which, ret;
	unsigned int cmd;
	char *argv0_copy = strdup(argv[0]);
	otpctx_t ctx;
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
