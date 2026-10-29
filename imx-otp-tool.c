/*
 * imx-otp-tool.c
 *
 * Tool for working with i.MX95 eFuses
 *
 * Copyright (c) 2022-2026, Matthew Madison.
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <hsm/hsm_api.h>
#include "otp.h"
#include "otp_srk.h"
#include "otp_lock.h"
#include "otp_macaddr.h"
#include "otp_machineid.h"

static uint32_t desired_srk_hash[SRK_FUSE_COUNT];
static bool     have_srk_hash = false;
static uint32_t null_hash[SRK_FUSE_COUNT] = { 0 };
static char *progname;
static bool opt_quiet = false;

typedef int (*option_routine_t)(otpctx_t ctx, int argc, char * const argv[]);
static int do_secure(otpctx_t ctx, int argc, char * const argv[]);
static int do_check_secure(otpctx_t ctx, int argc, char * const argv[]);
static int do_mac_address(otpctx_t ctx, int argc, char * const argv[]);
static int do_machine_id(otpctx_t ctx, int argc, char * const argv[]);
static int do_show(otpctx_t ctx, int argc, char * const argv[]);

static struct {
	const char *cmd;
	option_routine_t rtn;
	const char *help;
} commands[] = {
        { "is-secured", do_check_secure, "check fuses are set for secure boot" },
        { "mac-address",do_mac_address,  "show or program MAC address fuses" },
        { "machine-id", do_machine_id,   "show or program machine ID fuses" },
        { "secure",     do_secure,       "program SRK fuses" },
        { "show",       do_show,         "show all fuses" },
};

static struct option options[] = {
	{ "device",		required_argument,	0, 'd' },
	{ "fuse-file",		required_argument,	0, 'f' },
	{ "help",		no_argument,		0, 'h' },
	{ "quiet",		no_argument,		0, 'q' },
	{ "lock",		no_argument,		0, 'l' },
	{ "mac-count",		required_argument,	0, 'm' },
	{ 0,			0,			0, 0   }
};
static const char *shortopts = ":d:f:hqlm:";

static char *optarghelp[] = {
	"--device             ",
	"--fuse-file          ",
	"--help               ",
	"--quiet              ",
	"--lock               ",
	"--mac-count          ",
};

static char *opthelp[] = {
	"path to the OCOTP nvmem device",
	"path to the SRK_1_2_3_4_fuse.bin file",
	"display this help text",
	"omit prompts and information displays",
	"lock after successful fuse write",
	"number of MAC addresses (default 3, for mac-address command only)",
};

static bool lock_after_write = false;
static unsigned long number_of_macs = 3UL;

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
 * show_lifecycle_status
 */
static int
show_lifecycle_status (void)
{
	open_se_lib_serv_args_t ele_open_args = {0};
	se_lib_hdl_t ele_handle;
	op_dev_getinfo_args_t dev_getinfo_args;
	hsm_err_t err;
	int ret = 1;

	ele_open_args.mu_type = HSM1;
	err = se_lib_open_service(&ele_open_args, &ele_handle);
	if (err != HSM_NO_ERROR) {
		printf("ELE service open error, code=0x%x\n", err);
		return 1;
	}
	err = hsm_dev_getinfo(ele_handle, &dev_getinfo_args);
	if (err != HSM_NO_ERROR) {
		printf("Error retreiving ELE device info, code=0x%x\n", err);
	} else {
		if (opt_quiet) {
			printf("%s\n",
				(dev_getinfo_args.lmda_val == HSM_LMDA_OEM_OPEN ? "DISABLED" : "ENABLED"));
		} else {
			printf("Secure boot: %s (0x%x)\n",
				(dev_getinfo_args.lmda_val == HSM_LMDA_OEM_OPEN ? "DISABLED" : "ENABLED"),
				dev_getinfo_args.lmda_val);
		}
		ret = dev_getinfo_args.lmda_val == HSM_LMDA_OEM_OPEN ? 1 : 0;
	}
	se_lib_close_service(ele_handle);
	return ret;

} /* show_lifecycle_status */
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
	struct otp_lock_words_s * locks;
	uint8_t mac_addr[ETH_ALEN], mac_count;
	sd_id128_t machineid;
	unsigned int i;

	if (otp_srk_read(ctx, srk_hash, SRK_FUSE_COUNT) < 0) {
		perror("otp_srk_read");
		return 1;
	}
	if (otp_locks_read(ctx, &locks) < 0) {
		perror("otp_locks_read");
		return 1;
	}
	if (otp_macaddr_read(ctx, mac_addr, &mac_count) < 0) {
		perror("otp_macaddr_read");
		return 1;
	}
	if (otp_machineid_read(ctx, &machineid) < 0) {
		perror("otp_machineid_read");
		return 1;
	}

	printf("%-32.32s ", "SRK hashes:");
	if (memcmp(srk_hash, null_hash, sizeof(srk_hash)) == 0)
		printf("not set\n");
	else if (have_srk_hash) {
		if (memcmp(srk_hash, desired_srk_hash, sizeof(srk_hash)) == 0)
			printf("correctly programmed\n");
		else {
			printf("MISMATCH\n");
			for (i = 0; i < SRK_FUSE_COUNT; i++)
				if (srk_hash[i] != desired_srk_hash[i])
					printf("    SRK_HASH[%d]: actual=0x%08x desired=0x%08x\n",
					       i, srk_hash[i], desired_srk_hash[i]);
		}
	} else
		printf("non-null\n");

	printf("MAC address: %02x:%02x:%02x:%02x:%02x:%02x, count: %u\n",
	       mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5],
	       mac_count);
	printf("Machine ID: %s\n", SD_ID128_TO_STRING(machineid));

	return show_lifecycle_status();

} /* do_show */

static uint8_t
hexdigit (int c) {
	if (c >= 'a' && c <= 'f')
		return 10 + c - 'a';
	return c - '0';
}

static int
parse_macaddr (uint8_t *a, const char *buf)
{
	const char *cp = buf;
	int count = 0;

	while (*cp != '\0' && count < ETH_ALEN) {
		if (!isxdigit(*cp) || !isxdigit(*(cp+1)))
			break;
		a[count++] = (hexdigit(tolower(*cp)) << 4) | hexdigit(tolower(*(cp+1)));
		cp += 2;
		if (*cp == ':' || *cp == '-')
			cp += 1;
	}
	return (count == ETH_ALEN && *cp == '\0') ? 0 : -1;

} /* parse_macaddr */

/*
 * do_mac_address
 */
static int
do_mac_address (otpctx_t ctx, int argc, char * const argv[])
{
	uint8_t mac_addr[ETH_ALEN], mac_count;
	struct otp_lock_words_s *locks = NULL;
	uint8_t lstate;

	if (argc == 0) {
		if (otp_macaddr_read(ctx, mac_addr, &mac_count) < 0) {
			perror("otp_macaddr_read");
			return -1;
		}
		if (otp_locks_read(ctx, &locks) < 0) {
			perror("otp_locks_read");
			return -1;
		}
		if (otp_lockstate_get(locks, OTP_LOCK_COM_DEVICE_ID1, &lstate) < 0) {
			perror("otp_lockstate_get");
			return -1;
		}
		otp_locks_free(&locks);
		if (opt_quiet) {
			printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
				   mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
		} else {
			printf("MAC address: %02x:%02x:%02x:%02x:%02x:%02x, count: %u (%s)\n",
				   mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5],
				   mac_count, (lstate == OTP_LOCK_UNLOCKED) ? "unlocked" : "locked");
		}
		return 0;
	}

	if (parse_macaddr(mac_addr, argv[0]) < 0) {
		fprintf(stderr, "Invalid MAC address\n");
		return -1;
	}

	if (otp_macaddr_write(ctx, mac_addr, number_of_macs) < 0) {
		if (errno == EALREADY) {
			fprintf(stderr, "ERR: MAC address already programmed\n");
		} else {
			perror("otp_macaddr_write");
		}
		return -1;
	}
	if (lock_after_write) {
		if (otp_locks_read(ctx, &locks) < 0) {
			perror("otp_locks_read");
			return -1;
		}
		if (otp_lockstate_get(locks, OTP_LOCK_COM_DEVICE_ID1, &lstate) < 0) {
			perror("otp_lockstate_get");
			return -1;
		}
		if (lstate == OTP_LOCK_UNLOCKED) {
			if (otp_lockstate_set(OTP_LOCK_COM_DEVICE_ID1, OTP_LOCK_WRITE_LOCKED, locks) < 0) {
				perror("otp_lockstate_set");
				return -1;
			}
			if (otp_locks_update(ctx, locks) < 0) {
				perror("otp_locks_update");
				return -1;
			}
		} else {
			printf("MAC address already locked\n");
		}
		otp_locks_free(&locks);
	}
	return 0;

} /* do_mac_address */

/*
 * do_machine_id
 */
static int
do_machine_id(otpctx_t ctx, int argc, char * const argv[])
{
	sd_id128_t id;
	struct otp_lock_words_s *locks = NULL;
	uint8_t lstate;

	if (argc == 0) {
		if (otp_machineid_read(ctx, &id) < 0) {
			perror("otp_machineid_read");
			return -1;
		}
		if (otp_locks_read(ctx, &locks) < 0) {
			perror("otp_locks_read");
			return -1;
		}
		if (otp_lockstate_get(locks, OTP_LOCK_GPR1, &lstate) < 0) {
			perror("otp_lockstate_get");
			return -1;
		}
		otp_locks_free(&locks);
		if (opt_quiet) {
			printf("%s\n", SD_ID128_TO_STRING(id));
		} else {
			printf("Machine ID: %s (%s)\n",
				   SD_ID128_TO_STRING(id),
				   (lstate == OTP_LOCK_UNLOCKED) ? "unlocked" : "locked");
		}
		return 0;
	}

	if (sd_id128_from_string(argv[0], &id) != 0) {
		perror("sd_id128_from_string");
		return -1;
	}

	if (otp_machineid_write(ctx, id) < 0) {
		if (errno == EALREADY) {
			fprintf(stderr, "ERR: Machine ID already programmed\n");
		} else {
			perror("otp_machineid_write");
		}
		return -1;
	}
	if (lock_after_write) {
		if (otp_locks_read(ctx, &locks) < 0) {
			perror("otp_locks_read");
			return -1;
		}
		if (otp_lockstate_get(locks, OTP_LOCK_GPR1, &lstate) < 0) {
			perror("otp_lockstate_get");
			return -1;
		}
		if (lstate == OTP_LOCK_UNLOCKED) {
			if (otp_lockstate_set(OTP_LOCK_GPR1, OTP_LOCK_WRITE_LOCKED, locks) < 0) {
				perror("otp_lockstate_set");
				return -1;
			}
			if (otp_locks_update(ctx, locks) < 0) {
				perror("otp_locks_update");
				return -1;
			}
		} else {
			printf("Machine ID already locked\n");
		}
		otp_locks_free(&locks);
	}
	return 0;

} /* do_machine_id*/

/*
 * do_secure
 */
static int
do_secure (otpctx_t ctx, int argc, char * const argv[])
{
	uint32_t srk_hash[SRK_FUSE_COUNT];
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
		if (!opt_quiet) {
			printf("Programmed SRK fuses.\n");
		}
	}

	return 0;

} /* do_secure */

/*
 * do_check_secure
 */
static int
do_check_secure (otpctx_t ctx, int argc, char * const argv[])
{

	if (have_srk_hash) {
		uint32_t srk_hash[SRK_FUSE_COUNT];
		if (otp_srk_read(ctx, srk_hash, SRK_FUSE_COUNT) < 0) {
			perror("otp_srk_read");
			return 1;
		}
		if (memcmp(srk_hash, desired_srk_hash, sizeof(srk_hash)) == 0) {
			if (!opt_quiet) {
				printf("SRK hashes correctly programmed\n");
			}
		} else {
			fprintf(stderr, "ERR: SRK hash mismatch\n");
			return 1;
		}
	} else if (!opt_quiet) {
		printf("No fuse file specified, skipping hash check\n");
	}

	return show_lifecycle_status();

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
	char *fuse_file = getenv("IMX_OTP_FUSE_FILE");

	progname = basename(argv0_copy);

#ifdef DEFAULT_FUSE_FILE
	if (fuse_file == NULL && access(DEFAULT_FUSE_FILE, R_OK) == 0) {
		fuse_file = DEFAULT_FUSE_FILE;
	}
#endif

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
		case 'l':
			lock_after_write = true;
			break;
		case 'm':
			number_of_macs = strtoul(optarg, NULL, 0);
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
