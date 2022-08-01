/* SPDX-License-Identifier: MIT */
/*
 * keystoretool.c
 *
 * Sets up a passphrase in the kernel keyring, using a
 * secure/trusted master key backing an encrypted key
 * for the passphrase, for use with dm-crypt.
 *
 * Command line options are a superset of the keystoretool
 * implementation for Tegra platforms from
 *   https://github.com/madisongh/keystore
 * but the underlying implementation is for the i.MX SoCs,
 * using (a patched NXP downstream 5.4 kernel) the
 * CONFIG_SECURE_KEYS and CONFIG_ENCRYPTED_KEYS kernel
 * config options, which use CAAM key blobbing to wrap
 * the keys when they are exported to userland.
 *
 * Copyright (c) 2022, Matthew Madison
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <keyutils.h>
#include "bootinfo.h"

typedef int (*option_routine_t)(void);

#define SSKEY_VARNAME "_ss_key"
#define SSKEY_NAME    "sskey"
#define DMCPP_VARNAME "_dmc_passphrase"
#define DMCPP_NAME    "dmcryptpp"

static struct option options[] = {
	{ "dmc-passphrase",	no_argument,		0, 'p' },
	{ "file-passphrase",	no_argument,		0, 'f' },
	{ "bootdone",		no_argument,		0, 'b' },
	{ "generate",           no_argument,            0, 'g' },
	{ "output",             required_argument,	0, 'o' },
	{ "help",		no_argument,		0, 'h' },
	{ 0,			0,			0, 0   }
};
static const char *shortopts = ":pfbgo:h";

static char *optarghelp[] = {
	"--dmc-passphrase     ",
	"--file-passphrase    ",
	"--bootdone           ",
	"--generate           ",
	"--output             ",
	"--help               ",
};

static char *opthelp[] = {
	"extract the dmcrypt passphrase",
	"extract the file passphrase",
	"set booting complete",
	"force generation of new passphrase",
	"file to write the passphrase to instead of stdout",
	"display this help text"
};

static bool force_generate = false;


/*
 * print_usage
 */
static void
print_usage (void)
{
	int i;
	printf("\nUsage:\n");
	printf("\tkeystoretool <option>\n\n");
	printf("Options (use only one per invocation):\n");
	for (i = 0; i < sizeof(options)/sizeof(options[0]) && options[i].name != 0; i++) {
		printf(" %s\t%c%c\t%s\n",
		       optarghelp[i],
		       (options[i].val == 0 ? ' ' : '-'),
		       (options[i].val == 0 ? ' ' : options[i].val),
		       opthelp[i]);
	}

} /* print_usage */

/*
 * setup_passphrase
 *
 * Installs (or creates) the secure storage key and passphrase
 * into the kernel keyring for the current UID.
 *
 * returns 0 on success, negative number on error.
 */
static int
setup_passphrase (bool generate, char **sskeyptr, char **dmcppptr)
{
	key_serial_t ssk, dmcpp;
	void *keybuf;
	char payload[1024];
	static const char loadcmd[] = "load ";

	ssk = find_key_by_type_and_desc("secure", SSKEY_NAME, KEY_SPEC_USER_KEYRING);
	if (ssk < 0) {
		if (!generate && *sskeyptr != NULL) {
			if (strlen(*sskeyptr) + sizeof(loadcmd) >= sizeof(payload)) {
				errno = EINVAL;
				return -1;
			}
			snprintf(payload, sizeof(payload)-1, "%s%s", loadcmd, *sskeyptr);
			payload[sizeof(payload)-1] = '\0';
		} else
			strcpy(payload, "new 32");
		/*
		 * Default permissions on a key in the user keyring only grants read/write/etc. permission
		 * to the possessor of a key, so we have to create it in the session keyring, change
		 * the permissions and link the key to the user keyring so it can be used by other
		 * processes/
		 */
		ssk = add_key("secure", SSKEY_NAME, payload, strlen(payload), KEY_SPEC_SESSION_KEYRING);
		if (ssk < 0)
			return -1;
		if (keyctl_setperm(ssk, KEY_POS_ALL|KEY_USR_ALL|KEY_GRP_VIEW|KEY_GRP_SEARCH|KEY_OTH_VIEW|KEY_OTH_SEARCH) < 0)
			return -1;
		if (keyctl_link(ssk, KEY_SPEC_USER_KEYRING) < 0)
			return -1;
	} else {
		/*
		 * We have to link the secure key into the session keyring to
		 * read it or to use the encrypted key (which is encrypted with it)
		 */
		keyctl_link(ssk, KEY_SPEC_SESSION_KEYRING);
	}
	if (keyctl_read_alloc(ssk, &keybuf) < 0)
		return -1;
	if (generate || *sskeyptr == NULL)
		*sskeyptr = keybuf;
	else if (strcmp(*sskeyptr, keybuf) != 0) {
		errno = EIO;
		fputs("Error: secure storage key mismatch with keyring\n", stderr);
		free(keybuf);
		return -1;
	} else
		free(keybuf);

	dmcpp = find_key_by_type_and_desc("encrypted", DMCPP_NAME, KEY_SPEC_USER_KEYRING);
	if (dmcpp < 0) {
		if (!generate && *dmcppptr != NULL) {
		        if (strlen(*dmcppptr) + sizeof(loadcmd) >= sizeof(payload)) {
				errno = EINVAL;
				return -1;
			}
			snprintf(payload, sizeof(payload)-1, "%s%s", loadcmd, *dmcppptr);
			payload[sizeof(payload)-1] = '\0';
		} else
			strcpy(payload, "new default secure:" SSKEY_NAME " 32");
		dmcpp = add_key("encrypted", DMCPP_NAME, payload, strlen(payload), KEY_SPEC_SESSION_KEYRING);
		if (dmcpp < 0)
			return -1;
		if (keyctl_setperm(dmcpp, KEY_POS_ALL|KEY_USR_ALL|KEY_GRP_VIEW|KEY_GRP_SEARCH|KEY_OTH_VIEW|KEY_OTH_SEARCH) < 0)
			return -1;
		if (keyctl_link(dmcpp, KEY_SPEC_USER_KEYRING) < 0)
			return -1;
	}
	if (keyctl_read_alloc(dmcpp, &keybuf) < 0)
		return -1;
	if (generate || *dmcppptr == NULL)
		*dmcppptr = keybuf;
	else if (strcmp(*dmcppptr, keybuf) != 0) {
		errno = EIO;
		fputs("Error: passphrase mismatch with keyring\n", stderr);
		free(keybuf);
		return -1;
	} else
		free(keybuf);
	return 0;

} /* setup_passphrase */

/*
 * get_passphrase
 *
 * Retrieves the secure storage key and dm-crypt passphrase hex blobs from
 * boot variable storage (if present) and installs them in the user keyring,
 * or generates new ones if '-g' is used, or if one of the keys is missing.
 */
static int
get_passphrase (void)
{
	bootinfo_ctx_t *ctx;
	char *sskeytext = NULL, *ppblobtext = NULL;
	bool generate = force_generate;

	if (bootinfo_open(&ctx, 0) < 0) {
		perror("bootinfo_open");
		return 1;
	}
	if (!generate)
		generate = (bootinfo_bootvar_get(ctx, SSKEY_VARNAME, &sskeytext) < 0 ||
			    bootinfo_bootvar_get(ctx, DMCPP_VARNAME, &ppblobtext) < 0);
	if (generate) {
		int ret;
		if (setup_passphrase(true, &sskeytext, &ppblobtext) < 0) {
			perror("generate_passphrase");
			bootinfo_close(ctx);
			return 1;
		}
		ret = bootinfo_bootvar_set(ctx, DMCPP_VARNAME, ppblobtext);
		if (ret == 0)
			ret = bootinfo_bootvar_set(ctx, SSKEY_VARNAME, sskeytext);
		// These were allocated by libkeyutils, must be freed
		free(ppblobtext);
		free(sskeytext);
		if (ret != 0) {
			perror("bootinfo_bootvar_set");
			bootinfo_close(ctx);
			return 1;
		}
	} else if (setup_passphrase(false, &sskeytext, &ppblobtext) < 0) {
		perror("setup_passphrase");
		bootinfo_close(ctx);
		return 1;
	}
	bootinfo_close(ctx);
	return 0;

} /* get_passphrase */

/*
 * main program
 */
int
main (int argc, char * const argv[])
{
	int c, which, ret;
	option_routine_t dispatch = NULL;
	char *outfile = NULL;
	FILE *outf = stdout;

	if (argc < 2) {
		print_usage();
		return 1;
	}

	while ((c = getopt_long_only(argc, argv, shortopts, options, &which)) != -1) {

		switch (c) {

			case 'h':
				print_usage();
				return 0;
			case 'p':
				dispatch = get_passphrase;
				break;
			case 'f':
				fprintf(stderr, "file passphrase not supported on this platform\n");
				return 1;
				break;
			case 'b':
				/* No-op on this platform */
				return 0;
				break;
			case 'g':
				force_generate = true;
				break;
			case 'o':
				outfile = strdup(optarg);
				break;
			default:
				fprintf(stderr, "Error: unrecognized option\n");
				print_usage();
				return 1;
		}
	}

	if (optind < argc) {
		fprintf(stderr, "Error: unrecognized extra arguments\n");
		print_usage();
		return 1;
	}

	if (dispatch == NULL) {
		fprintf(stderr, "No operation specified\n");
		print_usage();
		return 1;
	}

	if (outfile != NULL && dispatch == get_passphrase) {
		int fd = open(outfile, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
		if (fd < 0) {
			perror(outfile);
			return 1;
		}
		outf = fdopen(fd, "w");
		if (outf == NULL) {
			perror(outfile);
			close(fd);
			unlink(outfile);
			return 1;
		}
	}

	ret = dispatch();
	if (outf != stdout) {
		if (fclose(outf) == EOF) {
			perror(outfile);
			ret = 1;
		}
	}

	return ret;

} /* main */
