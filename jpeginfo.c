/*******************************************************************
 * JPEGinfo
 * Copyright (c) Timo Kokkonen, 1995-2023.
 * All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of JPEGinfo.
 *
 * JPEGinfo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * JPEGinfo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with JPEGinfo. If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#if HAVE_GETOPT_H && HAVE_GETOPT_LONG
#include <getopt.h>
#else
#include "getopt.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <ctype.h>
#include <jpeglib.h>
#include <jerror.h>

#include "sha256/crypto_hash_sha256.h"
#include "md5.h"
#include "jpeginfo.h"


#define VERSION     "1.7.0beta"
#define COPYRIGHT   "Copyright (C) 1996-2023 Timo Kokkonen"

#define BUF_LINES   255

#ifndef HOST_TYPE
#define HOST_TYPE ""
#endif

#ifdef BROKEN_METHODDEF
#undef METHODDEF
#define METHODDEF(x) static x
#endif

#define EXIF_JPEG_MARKER   JPEG_APP0+1
#define EXIF_IDENT_STRING  "Exif\000\000"
#define EXIF_IDENT_STRING_LEN 6

struct my_error_mgr {
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
};
typedef struct my_error_mgr * my_error_ptr;

static struct jpeg_decompress_struct cinfo;
static struct my_error_mgr jerr;

FILE *infile=NULL;
FILE *listfile=NULL;
int global_error_counter = 0;
int global_total_errors = 0;
int verbose_mode = 0;
int quiet_mode = 0;
int delete_mode = 0;
int check_mode = 0;
int com_mode = 0;
int del_mode = 0;
int opt_index = 0;
int list_mode = 0;
int longinfo_mode = 0;
int input_from_file = 0;
int md5_mode = 0;
int sha256_mode = 0;
int stdin_mode = 0;
char *current = NULL;
char last_error[JMSG_LENGTH_MAX + 1];

static struct option long_options[] = {
	{"verbose",0,0,'v'},
	{"delete",0,0,'d'},
	{"mode",1,0,'m'},
	{"file",1,0,'f'},
	{"check",0,0,'c'},
	{"help",0,0,'h'},
	{"quiet",0,0,'q'},
	{"lsstyle",0,0,'l'},
	{"info",0,0,'i'},
	{"md5",0,0,'5'},
	{"sha256",0,0,'2'},
	{"version",0,0,'V'},
	{"comments",0,0,'C'},
	{"stdin",0,&stdin_mode,1},
	{0,0,0,0}
};

/*****************************************************************************/

METHODDEF(void)
	my_error_exit (j_common_ptr cinfo)
{
	my_error_ptr myerr = (my_error_ptr)cinfo->err;
	(*cinfo->err->output_message) (cinfo);
	longjmp(myerr->setjmp_buffer,1);
}


METHODDEF(void)
	my_output_message (j_common_ptr cinfo)
{
	char buffer[JMSG_LENGTH_MAX + 1];

	(*cinfo->err->format_message) (cinfo, buffer);
	buffer[sizeof(buffer)-1]=0;

	if (verbose_mode)
		fprintf(stderr, " %s ",buffer);

	global_error_counter++;
	global_total_errors++;
	strncpy(last_error, buffer, sizeof(last_error));
}


void no_memory(void)
{
	fprintf(stderr,"jpeginfo: not enough memory!\n");
	exit(3);
}


void print_version()
{
	struct jpeg_error_mgr jcerr, *err;


#ifdef  __DATE__
	printf("jpeginfo v%s  %s (%s)\n", VERSION, HOST_TYPE, __DATE__);
#else
	printf("jpeginfo v%s  %s\n", VERSION, HOST_TYPE);
#endif
	printf(COPYRIGHT "\n\n");
	printf("This program comes with ABSOLUTELY NO WARRANTY. This is free software,\n"
		"and you are welcome to redistirbute it under certain conditions.\n"
		"See the GNU General Public License for more details.\n\n");

	if (!(err=jpeg_std_error(&jcerr))) {
		fprintf(stderr, "jpeg_std_error() failed\n");
		exit(1);
	}

	printf("\nlibjpeg version: %s\n%s\n",
		err->jpeg_message_table[JMSG_VERSION],
		err->jpeg_message_table[JMSG_COPYRIGHT]);
}


void print_usage(void)
{
	if (quiet_mode)
		exit(0);

	fprintf(stderr,"jpeginfo v" VERSION " " COPYRIGHT "\n");
	fprintf(stderr,
		"Usage: jpeginfo [options] <filenames>\n\n"
		"  -2, --sha256    calculate SHA-256 checksum for each file\n"
		"  -5, --md5       calculate MD5 checksum for each file\n"
		"  -c, --check     check files also for errors\n"
		"  -C, --comments  display comments (from COM markers)\n"
		"  -d, --delete    delete files that have errors\n"
		"  -f<filename>,  --file<filename>\n"
		"                  read the filenames to process from given file\n"
		"                  (for standard input use '-' as a filename)\n"
		"  -h, --help      display this help and exit\n"
		"  -i, --info      display even more information about pictures\n"
		"  -l, --lsstyle   use alternate listing format (ls -l style)\n"
		"  -m<mode>, --mode=<mode>\n"
		"                  defines which jpegs to remove (when using"
		" the -d option).\n"
		"                  Mode can be one of the following:\n"
		"                    erronly     only files with serious errrors\n"
		"                    all         files ontaining warnings or errors (default)\n"
		"  -q, --quiet     quiet mode, output just jpeg infos\n"
		"  -v, --verbose   enable verbose mode (positively chatty)\n"
		"  -V, --version	  print program version and exit\n"
		"\n"
		"   -, --stdin         read input from standard input (instead of a file)\n"
		"\n\n");

	exit(0);
}


void parse_args(int argc, char **argv)
{
	int i, c;

	if (argc < 2) {
		if (quiet_mode < 2) fprintf(stderr, "jpeginfo: file arguments missing\n"
					"Try 'jpeginfo --help' for more information.\n");
		exit(1);
	}

	while(1) {
		opt_index=0;
		if ( (c=getopt_long(argc,argv, "livVdcChqm:f:52",
						long_options, &opt_index))  == -1)
			break;
		switch (c) {
		case 'm':
			if (!strcasecmp(optarg, "all")) del_mode=0;
			else if (!strcasecmp(optarg, "erronly")) del_mode=1;
			else if (!quiet_mode)
				fprintf(stderr, "Unknown parameter for -m, --mode.\n");
			break;
		case 'f':
			if (!strcmp(optarg, "-")) listfile=stdin;
			else if ((listfile=fopen(optarg, "r")) == NULL) {
				fprintf(stderr, "Cannot open file '%s'.\n", optarg);
				exit(2);
			}
			input_from_file=1;
			break;
		case 'v':
			verbose_mode=1;
			break;
		case 'V':
			print_version();
			exit(0);
		case 'd':
			delete_mode=1;
			break;
		case 'c':
			check_mode=1;
			break;
		case 'h':
			print_usage();
			break;
		case 'q':
			quiet_mode++;
			break;
		case 'l':
			list_mode=1;
			break;
		case 'i':
			longinfo_mode=1;
			break;
		case '5':
			md5_mode=1;
			break;
		case '2':
			sha256_mode=1;
			break;
		case 'C':
			com_mode=1;
			break;
		case '?':
			break;

		}
	}

	/* check for '-' option indicating input is from stdin... */
	i=1;
	while (argv[i]) {
		if (argv[i][0]=='-' && argv[i][1]==0)
			stdin_mode=1;
		i++;
	}

	if (delete_mode && verbose_mode && !quiet_mode)
		fprintf(stderr, "jpeginfo: delete mode enabled (%s)\n",
			(!del_mode ? "normal" : "errors only"));
}


/*****************************************************************************/
int main(int argc, char **argv)
{
	MD5_CTX *MD5 = malloc(sizeof(MD5_CTX));
	JSAMPARRAY buf = malloc(sizeof(JSAMPROW)*BUF_LINES);
	jpeg_saved_marker_ptr exif_marker, cmarker;
	volatile int i;
	int j;
	unsigned char ch;
	char namebuf[1024];
	unsigned char *inbuf = NULL;
	long long fs;
	char digest_text[65];
	size_t inbuffersize;
	volatile int phase;

	if (!buf || !MD5) no_memory();

	for(i = 0; i < BUF_LINES; i++) {
		buf[i] = NULL;
	}

	cinfo.err = jpeg_std_error(&jerr.pub);
	jpeg_create_decompress(&cinfo);
	jerr.pub.error_exit=my_error_exit;
	jerr.pub.output_message=my_output_message;

	/* parse command line parameters */
	parse_args(argc, argv);
	i=(optind > 0 ? optind : 1);

	/* process input file(s) */
	do {
		phase = 0;
		current = argv[i];

		/* open input file */

		if (stdin_mode) {
			if (verbose_mode)
				fprintf(stderr, "Reading file: <STDIN>\n");
			infile = stdin;
			inbuffersize = 256 * 1024;
			current = "-";
		} else {
			if (input_from_file) {
				if (!fgetstr(namebuf, sizeof(namebuf), listfile))
					break;
				current=namebuf;
			}
			if (current[0]==0) continue;

			if (verbose_mode)
				fprintf(stderr, "Reading file: %s\n", current);
			if ((infile=fopen(current,"rb"))==NULL) {
				if (!quiet_mode) fprintf(stderr, "jpeginfo: can't open '%s'\n", current);
				continue;
			}
			if (is_dir(infile)) {
				fclose(infile);
				if (verbose_mode) printf("directory: %s  skipped\n", current);
				continue;
			}
			inbuffersize = filesize(infile);
		}

		/* read input file to buffer */
		fs = read_file(infile, (inbuffersize > 0 ? inbuffersize : 65536), &inbuf);
		if (fs < 0) {
			no_memory();
			continue;
		}
		fclose(infile);

		if (setjmp(jerr.setjmp_buffer)) {
			jpeg_abort_decompress(&cinfo);
			for(j = 0; j < BUF_LINES; j++) {
				if (buf[j]) {
					free(buf[j]);
					buf[j] = NULL;
				}
			}
			if (quiet_mode < 2) {
				if (phase < 1)
					printf("%s", current);
				else if (phase < 2) {
					printf("   0 x 0    n/a   n/a   N %7d", fs);
					if (md5_mode | sha256_mode)
						printf(" %s", digest_text);
					if (list_mode)
						printf(" %s ", current);
				}
				printf(" [ERROR] %s\n", last_error);
			}
			if (delete_mode) delete_file(current, verbose_mode, quiet_mode);
			continue;
		}


		if (md5_mode || sha256_mode) {
			/* calculate hash (message-digest) of the input file */
			unsigned char digest[32];

			digest_text[0] = 0;
			if (md5_mode) {
				MD5Init(MD5);
				MD5Update(MD5, inbuf, fs);
				MD5Final(digest, MD5);
				digest2str(digest, digest_text, 16);
			} else {
				crypto_hash_sha256(digest, inbuf, fs);
				digest2str(digest, digest_text, 32);
			}
		}

		if (!list_mode && quiet_mode < 2) printf("%s ", current);
		phase = 1;

		global_error_counter=0;
		if (com_mode) jpeg_save_markers(&cinfo, JPEG_COM, 0xffff);
		jpeg_save_markers(&cinfo, EXIF_JPEG_MARKER, 0xffff);
		jpeg_mem_src(&cinfo, inbuf, fs);
		jpeg_read_header(&cinfo, TRUE);

		/* check for Exif marker */
		exif_marker=NULL;
		cmarker=cinfo.marker_list;
		while (cmarker) {
			if (cmarker->marker == EXIF_JPEG_MARKER && cmarker->data_length >= EXIF_IDENT_STRING_LEN) {
				if (!memcmp(cmarker->data, EXIF_IDENT_STRING, EXIF_IDENT_STRING_LEN))
					exif_marker=cmarker;
			}
			cmarker=cmarker->next;
		}

		if (quiet_mode < 2) {
			printf("%4d x %-4d %2dbit ", (int)cinfo.image_width,
				(int)cinfo.image_height, (int)cinfo.num_components*8);

			if (exif_marker) printf("Exif  ");
			else if (cinfo.saw_JFIF_marker) printf("JFIF  ");
			else if (cinfo.saw_Adobe_marker) printf("Adobe ");
			else printf("n/a   ");

			if (longinfo_mode) {
				printf("%s %s", (cinfo.progressive_mode?"Progressive":"Normal"),
					(cinfo.arith_code?"Arithmetic":"Huffman") );

				if (cinfo.density_unit==1||cinfo.density_unit==2)
					printf(",%ddp%c", MIN(cinfo.X_density,cinfo.Y_density),
						(cinfo.density_unit==1?'i':'c') );

				if (cinfo.CCIR601_sampling) printf(",CCIR601");
				printf(" %7lld ",fs);

			} else printf("%c %7lld ",(cinfo.progressive_mode ? 'P' : 'N'), fs);

			if (md5_mode || sha256_mode) printf("%s ", digest_text);
			if (list_mode) printf("%s ", current);

			if (com_mode) {
				cmarker=cinfo.marker_list;
				while (cmarker) {
					if (cmarker->marker == JPEG_COM) {
						printf("\"");
						for (j=0; j < cmarker->data_length; j++) {
							ch = cmarker->data[j];
							if (ch < 32 || iscntrl(ch)) continue;
							printf("%c", cmarker->data[j]);
						}
						printf("\" ");
					}
					cmarker=cmarker->next;
				}
			}
		}
		phase = 2;

		if (check_mode) {
			cinfo.out_color_space = JCS_GRAYSCALE; /* to speed up the process... */
			cinfo.scale_denom = 8;
			cinfo.scale_num = 1;
			jpeg_start_decompress(&cinfo);

			for (j=0;j<BUF_LINES;j++) {
				buf[j]=malloc(sizeof(JSAMPLE) *
					cinfo.output_width *
					cinfo.out_color_components);
				if (!buf[j]) no_memory();
			}

			while (cinfo.output_scanline < cinfo.output_height) {
				jpeg_read_scanlines(&cinfo, buf, BUF_LINES);
			}

			jpeg_finish_decompress(&cinfo);
			for(j = 0; j < BUF_LINES; j++) {
				free(buf[j]);
				buf[j] = NULL;
			}

			if (!global_error_counter) {
				if (quiet_mode < 2) printf(" [OK]\n");
			}
			else {
				if (quiet_mode < 2) printf(" [WARNING]\n");
				if (delete_mode && !del_mode)
					delete_file(current, verbose_mode, quiet_mode);
			}
		}
		else { /* !check_mode */
			if (quiet_mode < 2) printf("\n");
			jpeg_abort_decompress(&cinfo);
		}


	} while ((!stdin_mode && ++i<argc) || input_from_file);

	if (inbuf)
		free(inbuf);
	jpeg_destroy_decompress(&cinfo);
	free(buf);
	free(MD5);

	return (global_total_errors > 0 ? 1 : 0); /* return 1 if any errors found file(s)
						     we checked */
}

/* :-) */
