/* SPDX-License-Identifier: MIT */
/*
 * keystoretool.c
 *
 * Uses the CAAM to encrypt/decrypt a passphrase for
 * use with DM-Crypt/cryptsetup.
 *
 * Command line options are a superset of the keystoretool
 * implementation for Tegra platforms from
 *   https://github.com/madisongh/keystore
 * but the underlying implementation is for the i.MX SoCs,
 * using the CAAM's "black key" feature and key blobbing
 * feature to encrypt a unique per-device passphrase.
 * The encrypted blob can only be decrypted on the device
 * where it was generated.
 *
 * Note that this implementation requires the downstream
 * NXP BSP, as it uses the (rather horrible) CAAM ioctl API
 * that is provided in that kernel.
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
#include <sys/ioctl.h>
#include <imx/linux/caam_keygen.h>
#include "bootinfo.h"

#define stringify(x_) makestring(x_)
#define makestring(x_) #x_

typedef int (*option_routine_t)(int fd, FILE *outf);

#define RED_KEY_SIZE		16
#define RED_KEY_SIZE_MAX	64
/* CCM adds 6-byte nonce and 6-byte IV */
#define CCM_INFO_SIZE		12
#define TAG_HEADER_SIZE		20
#define BLACK_KEY_BUF_SIZE	(RED_KEY_SIZE_MAX+CCM_INFO_SIZE+TAG_HEADER_SIZE)
/* blobbing inserts a key and MAC tag */
#define BLOB_OVERHEAD		48
#define BLOB_BUF_SIZE		(BLACK_KEY_BUF_SIZE+BLOB_OVERHEAD)

#define DMCPP_VARNAME "_dmc_passphrase"

static struct option options[] = {
	{ "dmc-passphrase",	no_argument,		0, 'p' },
	{ "file-passphrase",	no_argument,		0, 'f' },
	{ "bootdone",		no_argument,		0, 'b' },
	{ "output",             required_argument,	0, 'o' },
	{ "help",		no_argument,		0, 'h' },
	{ 0,			0,			0, 0   }
};
static const char *shortopts = ":pfbo:h";

static char *optarghelp[] = {
	"--dmc-passphrase     ",
	"--file-passphrase    ",
	"--bootdone           ",
	"--output             ",
	"--help               ",
};

static char *opthelp[] = {
	"extract the dmcrypt passphrase",
	"extract the file passphrase",
	"set booting complete",
	"file to write the passphrase to instead of stdout",
	"display this help text"
};


/*
 * printhex
 *
 * Print a string of bytes as hexadecimal
 */
static void
printhex (FILE *of, uint8_t *buf, int buflen)
{
	for (; buflen > 0; buf++, buflen--)
		fprintf(of, "%02X", *buf);
	fprintf(of, "\n");
}

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
 * generate_passphrase
 *
 * Generates a random passphrase, encrypted
 *
 * returns length of blob on success, negative value on error.
 */
static int
generate_passphrase (int fd, uint8_t *blob_buf, size_t blob_buf_size, size_t *blob_len)
{
	struct caam_keygen_cmd keygen_params = { 0 };
	uint8_t *black_key = NULL;
	int ret;

	black_key = calloc(1, BLACK_KEY_BUF_SIZE);
	if (black_key == NULL) {
		perror("calloc");
		return -1;
	}
	keygen_params.key_enc_len = 4; // includes null terminator
	keygen_params.key_enc = (uintptr_t) "ecb";
	keygen_params.key_mode_len = 3; // includes null terminator
	keygen_params.key_mode = (uintptr_t) "-s"; // s is for 'size'
	keygen_params.key_value_len = 3;
	keygen_params.key_value = (uintptr_t) stringify(RED_KEY_SIZE); // length we want for the random key
	keygen_params.black_key_len = BLACK_KEY_BUF_SIZE;
	keygen_params.black_key = (uintptr_t) black_key;
	keygen_params.blob_len = blob_buf_size;
	keygen_params.blob = (uintptr_t) blob_buf;

	ret = ioctl(fd, CAAM_KEYGEN_IOCTL_CREATE, &keygen_params);
	free(black_key);
	if (ret != 0)
		return -1;
	*blob_len = keygen_params.blob_len;
	return 0;

} /* generate_passphrase */

/*
 * extract_passphrase
 *
 * Extracts a decrypted passphrase from a blob.
 *
 * returns 0 on success, non-0 on failure.
 */
static int
extract_passphrase (int fd, uint8_t *blob, size_t blob_size, FILE *outf)
{
	struct caam_keygen_cmd keygen_params = { 0 };
	uint8_t *black_key = NULL;
	int ret;

	black_key = calloc(1, BLACK_KEY_BUF_SIZE);
	if (black_key == NULL) {
		perror("calloc");
		return -1;
	}
	keygen_params.black_key_len = BLACK_KEY_BUF_SIZE;
	keygen_params.black_key = (uintptr_t) black_key;

	keygen_params.blob_len = blob_size;
	keygen_params.blob = (uintptr_t) blob;

	ret = ioctl(fd, CAAM_KEYGEN_IOCTL_IMPORT, &keygen_params);
	if (ret != 0) {
		free(black_key);
		return -1;
	}

	printhex(outf, black_key + TAG_HEADER_SIZE, (int) keygen_params.black_key_len - TAG_HEADER_SIZE);
	free(black_key);
	return 0;

} /* extract_passphrase */

/*
 * get_passphrase
 *
 * Retrieves the passphrase blob from boot variable storage, or generates a new
 * one (storing it), then extracts the decrypted passphrase to the output file.
 */
static int
get_passphrase (int caamfd, FILE *outf)
{
	static const char hexdigits[] = "0123456789ABCDEF";
	bootinfo_ctx_t *ctx;
	uint8_t *blob_buf = calloc(1, BLOB_BUF_SIZE);
	char *ppblobtext;
	char *blobtext = NULL;
	size_t bloblen = 0;
	int ret = 0;
	unsigned int i, j;

	if (blob_buf == NULL) {
		perror("calloc");
		bootinfo_close(ctx);
		return 1;
	}

	if (bootinfo_open(&ctx, 0) < 0) {
		perror("bootinfo_open");
		free(blob_buf);
		return 1;
	}
	if (bootinfo_bootvar_get(ctx, DMCPP_VARNAME, &ppblobtext) < 0) {
		if (generate_passphrase(caamfd, blob_buf, BLOB_BUF_SIZE, &bloblen) < 0) {
			perror("generate_passphrase");
			free(blob_buf);
			bootinfo_close(ctx);
			return 1;
		}
		blobtext = calloc(1, bloblen*2 + 1);
		if (blobtext == NULL) {
			perror("calloc");
			free(blob_buf);
			bootinfo_close(ctx);
			return 1;
		}
		// Variables can only hold printable characters, so just hex encode
		for (i = 0; i < bloblen; i++) {
			blobtext[i*2]   = hexdigits[(blob_buf[i] >> 4) & 0x0F];
			blobtext[i*2+1] = hexdigits[blob_buf[i] & 0x0F];
		}
		if (bootinfo_bootvar_set(ctx, DMCPP_VARNAME, blobtext) < 0) {
			perror("bootinfo_bootvar_set");
			free(blobtext);
			bootinfo_close(ctx);
			return 1;
		}
	} else {
		size_t textlen = strlen(ppblobtext);
		if (textlen > BLOB_BUF_SIZE*2 || (textlen % 2) != 0) {
			fprintf(stderr, "Error: unrecognized passphrase blob found in variable store\n");
			free(blob_buf);
			bootinfo_close(ctx);
			return 1;
		}
		bloblen = textlen / 2;
		for (i = 0, j = 0; i < textlen; i += 2, j += 1) {
			char *cp1 = strchr(hexdigits, ppblobtext[i]);
			char *cp2 = strchr(hexdigits, ppblobtext[i+1]);

			if (cp1 == NULL || cp2 == NULL) {
				fprintf(stderr, "Error: unrecognized passphrase blob found in variable store\n");
				free(blob_buf);
				bootinfo_close(ctx);
				return 1;
			}
			blob_buf[j] = ((cp1-hexdigits) << 4) | (cp2-hexdigits);
		}
	}

	if (extract_passphrase(caamfd, blob_buf, bloblen, outf) != 0) {
		perror("extract_passphrase");
		ret = 1;
	}
	if (blobtext != NULL)
		free(blobtext);
	return ret;

} /* get_passphrase */

/*
 * main program
 */
int
main (int argc, char * const argv[])
{
	int c, which, ret, caamfd;
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

	caamfd = open("/dev/caam-keygen", O_RDWR);
	if (caamfd < 0) {
		perror("/dev/caam-keygen");
		return 1;
	}

	if (outfile != NULL && dispatch == get_passphrase) {
		int fd = open(outfile, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
		if (fd < 0) {
			perror(outfile);
			close(caamfd);
			return 1;
		}
		outf = fdopen(fd, "w");
		if (outf == NULL) {
			perror(outfile);
			close(fd);
			unlink(outfile);
			close(caamfd);
			return 1;
		}
	}

	ret = dispatch(caamfd, outf);
	close(caamfd);
	if (outf != stdout) {
		if (fclose(outf) == EOF) {
			perror(outfile);
			ret = 1;
		}
	}

	return ret;

} /* main */
