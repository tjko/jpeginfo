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
#include "sha512/crypto_hash_sha512.h"
#include "md5.h"
#include "jpegmarker.h"
#include "jpeginfo.h"


#define VERSION     "1.7.0beta"
#define COPYRIGHT   "Copyright (C) 1996-2023 Timo Kokkonen"

#define BUF_LINES   512

#ifndef HOST_TYPE
#define HOST_TYPE ""
#endif

#ifdef BROKEN_METHODDEF
#undef METHODDEF
#define METHODDEF(x) static x
#endif

struct my_error_mgr {
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
};
typedef struct my_error_mgr * my_error_ptr;

static struct jpeg_decompress_struct cinfo;
static struct my_error_mgr jerr;

struct jpeg_info {
	int width;
	int height;
	int color_depth;
	int progressive;
	int check;
	size_t size;
	char *filename;
	char *type;;
	char *info;
	char *comments;
	char *digest;
	char *error;
};

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
int sha512_mode = 0;
int stdin_mode = 0;
int csv_mode = 0;
int json_mode = 0;
int header_mode = 0;
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
	{"sha512",0,&sha512_mode,1},
	{"version",0,0,'V'},
	{"comments",0,0,'C'},
	{"csv",0,0,'s'},
	{"json",0,0,'j'},
	{"header",0,0,'H'},
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

	(*cinfo->err->format_message)(cinfo, buffer);
	buffer[sizeof(buffer) - 1] = 0;

	if (verbose_mode > 1)
		fprintf(stderr, "libjpeg: %s\n",buffer);

	global_error_counter++;
	global_total_errors++;
	strncopy(last_error, buffer, sizeof(last_error));
}


void no_memory(void)
{
	fprintf(stderr,"jpeginfo: not enough memory!\n");
	exit(3);
}


void print_version()
{
#ifdef  __DATE__
	printf("jpeginfo v%s  %s (%s)\n", VERSION, HOST_TYPE, __DATE__);
#else
	printf("jpeginfo v%s  %s\n", VERSION, HOST_TYPE);
#endif
	printf(COPYRIGHT "\n\n");
	printf("This program comes with ABSOLUTELY NO WARRANTY. This is free software,\n"
		"and you are welcome to redistribute it under certain conditions.\n"
		"See the GNU General Public License for more details.\n\n");

	printf("\nlibjpeg version: %s\n%s\n",
		jerr.pub.jpeg_message_table[JMSG_VERSION],
		jerr.pub.jpeg_message_table[JMSG_COPYRIGHT]);
}


void print_usage(void)
{
	if (quiet_mode)
		exit(0);

	fprintf(stderr,"jpeginfo v" VERSION " " COPYRIGHT "\n");
	fprintf(stderr,
		"Usage: jpeginfo [options] <filenames>\n\n"
		"  -2, --sha256    Calculate SHA-256 checksum for each file.\n"
		"      --sha512    Calculate SHA-512 checksum for each file.\n"
		"  -5, --md5       Calculate MD5 checksum for each file.\n"
		"  -c, --check     Check files also for errors.\n"
		"  -C, --comments  Display comments (from COM markers)\n"
		"  -d, --delete    Delete files that have errors\n"
		"  -f<filename>,  --file<filename>\n"
		"                  Read the filenames to process from given file\n"
		"                  (for standard input use '-' as a filename)\n"
		"  -h, --help      Display this help and exit\n"
		"  -H, --header    Display column name header in output\n"
		"  -i, --info      Display even more information about pictures\n"
		"  -j, --json      JSON output style.\n"
		"  -l, --lsstyle   Use alternate listing format (ls -l style)\n"
		"  -m<mode>, --mode=<mode>\n"
		"                  Defines which jpegs to remove (when using"
		" the -d option).\n"
		"                  Mode can be one of the following:\n"
		"                    erronly     only files with serious errors\n"
		"                    all         files containing warnings or errors (default)\n"
		"  -q, --quiet     Quiet mode, output just jpeg infos\n"
		"  -s, --csv       Comma separated (CSV) output style.\n"
		"  -v, --verbose   Enable verbose mode (positively chatty)\n"
		"  -V, --version	  Print program version and exit\n"
		"\n"
		"   -, --stdin     Read input from standard input (instead of a file)\n"
		"\n\n");

	exit(0);
}


void parse_args(int argc, char **argv)
{
	int i, c;

	while(1) {
		opt_index=0;
		if ( (c=getopt_long(argc,argv, "livVdcChqm:f:52sH",
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
			if (verbose_mode)
				fprintf(stderr, "Reading input filenames from: '%s'\n", optarg);
			if (!strcmp(optarg, "-")) listfile=stdin;
			else if ((listfile=fopen(optarg, "r")) == NULL) {
				fprintf(stderr, "Cannot open file '%s'.\n", optarg);
				exit(2);
			}
			input_from_file=1;
			break;
		case 'v':
			verbose_mode++;
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
		case 's':
			csv_mode=1;
			break;
		case 'j':
			json_mode=1;
			break;
		case 'H':
			header_mode=1;
			break;
		case '?':
			exit(1);

		}
	}

	/* check for '-' option indicating input is from stdin... */
	i = optind;
	while (argv[i]) {
		if (argv[i][0]=='-' && argv[i][1]==0)
			stdin_mode=1;
		i++;
	}

	if (delete_mode && verbose_mode && !quiet_mode)
		fprintf(stderr, "jpeginfo: delete mode enabled (%s)\n",
			(!del_mode ? "normal" : "errors only"));

	if (argc <= optind && !input_from_file) {
		if (quiet_mode < 2) fprintf(stderr, "jpeginfo: file arguments missing\n"
					"Try 'jpeginfo --help' for more information.\n");
		exit(1);
	}

}


void clear_jpeg_info(struct jpeg_info *info)
{
	if (!info)
		return;

	memset(info, 0, sizeof(struct jpeg_info));
}


void free_jpeg_info(struct jpeg_info *info)
{
	if (!info)
		return;

	if (info->filename)
		free(info->filename);
	if (info->type)
		free(info->type);
	if (info->info)
		free(info->info);
	if (info->comments)
		free(info->comments);
	if (info->digest)
		free(info->digest);
	if (info->error)
		free(info->error);

	clear_jpeg_info(info);
}


void parse_jpeg_info(struct jpeg_decompress_struct *cinfo, struct jpeg_info *info)
{
	jpeg_saved_marker_ptr cmarker;
	char marker_str[256], info_str[256];
	char comment_str[1024], tmp[64];
	int special;
	int marker_in_count = 0;
	int comment_count = 0;
	int unknown_count = 0;
	unsigned long marker_in_size = 0;
	char *seen;
	size_t marker_types = jpeg_special_marker_types_count();

	if (!cinfo || !info)
		return;

	if ((seen = malloc(marker_types)) == NULL)
		no_memory();
	memset(seen, 0, marker_types);

	info->width = (int)cinfo->image_width;
	info->height = (int)cinfo->image_height;
	info->color_depth = (int)cinfo->num_components * 8;
	info->progressive = (cinfo->progressive_mode ? 1 : 0);

	strncopy(info_str, (cinfo->arith_code ? "Arithmetic" : "Huffman"), sizeof(info_str));
	comment_str[0]=0;
	marker_str[0]=0;

	/* Check for special (Exif/IPTC/ICC/XMP/etc...) markers */
	cmarker=cinfo->marker_list;
	while (cmarker) {
		marker_in_count++;
		marker_in_size+=cmarker->data_length;
		special = jpeg_special_marker(cmarker);

		if (verbose_mode)
			fprintf(stderr, "Found marker %s (0x%X): type=%s, original_length=%u, data_length=%u\n",
				jpeg_marker_name(cmarker->marker), cmarker->marker,
				(special >= 0 ? jpeg_special_marker_types[special].name : "Unknown"),
				cmarker->original_length, cmarker->data_length);

		if (special >= 0) {
			if (!seen[special])
				str_add_list(marker_str, sizeof(marker_str),
					jpeg_special_marker_types[special].name, ",");
			seen[special]++;
		}
		else if (cmarker->marker == JPEG_COM) {
			if (cmarker->data_length > 0) {
				int o = 0;
				for (int i = 0; i < cmarker->data_length; i++) {
					char ch = cmarker->data[i];
					if (!isprint(ch)) {
						ch = '.';
					}
					if (o < sizeof(tmp) - 1) {
						*(tmp + o++) = ch;
					}
				}
				*(tmp + o) = 0;

				str_add_list(comment_str, sizeof(comment_str), tmp, ",");
				comment_count++;
			}
		}
		else {
			unknown_count++;
		}

		cmarker=cmarker->next;
	}

	if (verbose_mode)
		fprintf(stderr, "Found total of %d markers (total size %lu bytes), and %d unknown markers\n",
			marker_in_count, marker_in_size, unknown_count);

	if (comment_count > 0)
		str_add_list(marker_str, sizeof(marker_str), "COM", ",");

	if (unknown_count > 0)
		str_add_list(marker_str, sizeof(marker_str), "UNKNOWN", ",");

	if (cinfo->density_unit == 1 || cinfo->density_unit == 2) {
		snprintf(tmp, sizeof(tmp), "%ddp%c", MIN(cinfo->X_density, cinfo->Y_density),
			(cinfo->density_unit == 1 ? 'i' : 'c') );
		str_add_list(info_str, sizeof(marker_str), tmp, ",");
	}

	if (cinfo->CCIR601_sampling) {
		str_add_list(info_str, sizeof(marker_str), "CCIR601", ",");
	}

	info->type = strdup(marker_str);
	info->info = strdup(info_str);
	info->comments = strdup(comment_str);

	free(seen);
}


const char *check_status_str(int check)
{
	switch (check) {
	case 1:
		return "OK";
	case 2:
		return "WARNING";
	case 3:
		return "ERROR";
	}

	return "";
}


void print_jpeg_info(struct jpeg_info *info)
{
	const char *type, *einfo, *com, *error, *digest;
	char p;
	static int header_printed = 0;
	static long line =  0;

	if (!info)
		return;

	if (quiet_mode > 1)
		return;

	if ((header_mode || json_mode) && !header_printed) {
		if (csv_mode) {
			printf("filename,size,hash,width,height,color_depth,markers,progressive_normal,extra_info,comments,status,status_detail\n");
		}
		else if (json_mode) {
			printf("[\n");
		}
		else if (list_mode) {
			printf("  W  x  H   Color P Markers                  ");
			if (longinfo_mode)
				printf("ExtraInfo            ");
			printf("   Size ");
			if (md5_mode)
				printf("MD5                              ");
			if (sha256_mode)
				printf("SHA-256                                                          ");
			if (sha512_mode)
				printf("SHA-512                                                          "
				        "                                                                ");
			if (com_mode)
				printf("Comments                         ");
			printf("Filename                         ");
			if (check_mode)
				printf("Status  Details");
			printf("\n");
		}
		else {
			printf("Filename                           W  x  H   Color P Markers                  ");
			if (longinfo_mode)
				printf("ExtraInfo            ");
			printf("   Size ");
			if (md5_mode)
				printf("MD5                              ");
			if (sha256_mode)
				printf("SHA-256                                                          ");
			if (sha512_mode)
				printf("SHA-512                                                          "
					"                                                                ");
			if (com_mode)
				printf("Comments                         ");
			if (check_mode)
				printf("Status  Details");
			printf("\n");
		}
		header_printed = 1;
	}

	type = (info->type ? info->type : "");
	einfo = (info->info ? info->info : "");
	com = (info->comments ? info->comments : "");
	if (!com_mode && !csv_mode)
		com = "";
	error = (info->error ? info->error : "");
	digest = (info->digest ? info->digest : "");

	p = (info->progressive ? 'P' : 'N');

	line++;

	if (csv_mode) {
		printf("\"%s\",%lu,\"%s\",%d,%d,\"%dbit\",\"%s\",\"%c\",\"%s\",\"%s\",\"%s\",\"%s\"\n",
			info->filename,
			info->size,
			digest,
			info->width,
			info->height,
			info->color_depth,
			type,
			p,
			einfo,
			com,
			check_status_str(info->check),
			error
			);
	}
	else if (json_mode) {
		if (line > 1)
			printf(",\n");
		printf(" { \"filename\":\"%s\", \"size\":%lu, \"hash\":\"%s\", \"width\":%d, \"height\":%d,"
			" \"color_depth\":\"%dbit\", \"type\":\"%s\", \"mode\":\"%s\", \"info\":\"%s\","
			" \"comments\":\"%s\", \"status\":\"%s\", \"status_detail\":\"%s\" }",
			info->filename,
			info->size,
			digest,
			info->width,
			info->height,
			info->color_depth,
			type,
			(p == 'P' ? "Progressive" : "Normal"),
			einfo,
			com,
			check_status_str(info->check),
			error
			);
	}
	else if (list_mode) {
		printf("%4d x %4d %2dbit %c %-24s ",
			info->width,
			info->height,
			info->color_depth,
			p,
			type);
		if (longinfo_mode)
			printf("%-20s ", einfo);
		printf("%7lu ",
			info->size);
		if (info->digest)
			printf("%s ", digest);
		if (com_mode)
			printf("%-32s ", com);
		printf("%-32s %-7s%s%s\n",
			info->filename,
			check_status_str(info->check),
			(info->error ? " " : ""),
			error
			);
	}
	else {
		printf("%-32s %4d x %4d %2dbit %c %-24s ",
			info->filename,
			info->width,
			info->height,
			info->color_depth,
			p,
			type);
		if (longinfo_mode)
			printf("%-20s ", einfo);
		printf("%7lu ",
			info->size);
		if (info->digest)
			printf("%s ", digest);
		if (com_mode)
			printf("%-32s ", com);
		printf("%-7s%s%s\n",
			check_status_str(info->check),
			(info->error ? " " : ""),
			error
			);
	}
}


void clear_line_buffer(JSAMPARRAY buf)
{
	for (int i = 0; i < BUF_LINES; i++) {
		if (buf[i])
			free(buf[i]);
		buf[i] = NULL;
	}
}


/*****************************************************************************/
int main(int argc, char **argv)
{
	char namebuf[MAXPATHLEN + 1];
	JSAMPROW line_buffer[BUF_LINES];
	JSAMPARRAY buf = line_buffer;
	struct jpeg_info info;
	volatile int i;
	int j;
	unsigned char *inbuf = NULL;
	long long file_size;
	size_t inbuffer_size;


	/* Initialize memory structures... */
	for(i = 0; i < BUF_LINES; i++) {
		buf[i] = NULL;
	}
	clear_jpeg_info(&info);
	cinfo.err = jpeg_std_error(&jerr.pub);
	jpeg_create_decompress(&cinfo);
	jerr.pub.error_exit=my_error_exit;
	jerr.pub.output_message=my_output_message;

	/* Parse command line parameters */
	parse_args(argc, argv);
	i=(optind > 0 ? optind : 1);

	/* Loop to process input file(s) */
	do {
		free_jpeg_info(&info);

		/* Open input file */
		if (stdin_mode) {
			if (verbose_mode)
				fprintf(stderr, "Reading file: <STDIN>\n");
			infile = stdin;
			inbuffer_size = 256 * 1024;
			current = "-";
		} else {
			if (input_from_file) {
				if (!fgetstr(namebuf, sizeof(namebuf), listfile))
					break;
				current=namebuf;
			} else {
				if ((current = argv[i]) == NULL)
					break;
			}
			if (*current == 0)
				continue;

			if (verbose_mode)
				fprintf(stderr, "Reading file: %s\n", current);
			if ((infile=fopen(current,"rb"))==NULL) {
				if (!quiet_mode) fprintf(stderr, "jpeginfo: can't open '%s'\n", current);
				continue;
			}
			if (is_dir(infile)) {
				fclose(infile);
				if (verbose_mode) fprintf(stderr, "Skipping directory: %s\n", current);
				continue;
			}
			inbuffer_size = filesize(infile);
		}
		info.filename = strdup(current);

		/* Read input file into a memory buffer */
		if ((file_size = read_file(infile, inbuffer_size, &inbuf)) < 0)
			no_memory();
		fclose(infile);
		info.size = file_size;
		last_error[0] = 0;

		/* Error handler for (libjpeg) errors in decoding */
		if (setjmp(jerr.setjmp_buffer)) {
			info.check = 3;
			info.error = strdup(last_error);
			if (verbose_mode)
				fprintf(stderr, "Error decoding JPEG image: %s\n", last_error);
			jpeg_abort_decompress(&cinfo);
			clear_line_buffer(buf);
			if (quiet_mode < 2)
				print_jpeg_info(&info);
			if (stdin_mode)
				break;
			if (delete_mode)
				delete_file(current, verbose_mode, quiet_mode);
			continue;
		}

		/* Calculate hash (message-digest) of the input file */
		if (md5_mode || sha256_mode || sha512_mode) {
			MD5_CTX md5;
			unsigned char digest[64];
			char digest_text[128 + 1];

			if (md5_mode) {
				MD5Init(&md5);
				MD5Update(&md5, inbuf, file_size);
				MD5Final(digest, &md5);
				digest2str(digest, digest_text, 16);
			} else if (sha256_mode) {
				crypto_hash_sha256(digest, inbuf, file_size);
				digest2str(digest, digest_text, 32);
			} else {
				crypto_hash_sha512(digest, inbuf, file_size);
				digest2str(digest, digest_text, 64);
			}
			info.digest = strdup(digest_text);
		}

		/* Read JPEG file header */
		global_error_counter=0;
		jpeg_save_markers(&cinfo, JPEG_COM, 0xffff);
		for (j = 0; j < 16; j++) {
			jpeg_save_markers(&cinfo, JPEG_APP0 + j, 0xffff);
		}
		jpeg_mem_src(&cinfo, inbuf, file_size);
		jpeg_read_header(&cinfo, TRUE);
		parse_jpeg_info(&cinfo, &info);


		/* Decode JPEG to check for errors in the file */
		if (check_mode) {
			cinfo.out_color_space = JCS_GRAYSCALE; /* to speed up the process... */
			cinfo.scale_denom = 8;
			cinfo.scale_num = 1;
			jpeg_start_decompress(&cinfo);

			for (j = 0; j < BUF_LINES; j++) {
				buf[j] = malloc(sizeof(JSAMPLE) * cinfo.output_width *
						cinfo.out_color_components);
				if (!buf[j])
					no_memory();
			}
			while (cinfo.output_scanline < cinfo.output_height) {
				jpeg_read_scanlines(&cinfo, buf, BUF_LINES);
			}
			clear_line_buffer(buf);

			jpeg_finish_decompress(&cinfo);
			if (verbose_mode && global_error_counter > 0)
				fprintf(stderr, "Warnings decoding JPEG image: %s\n", last_error);
			info.check = (global_error_counter == 0 ? 1 : 2);
			info.error = strdup(last_error);
			print_jpeg_info(&info);
			if (delete_mode && !del_mode && info.check > 1)
					delete_file(current, verbose_mode, quiet_mode);
		}
		else {
			/* When not checking integrity, just print out info we have. */
			jpeg_abort_decompress(&cinfo);
			print_jpeg_info(&info);
		}

	} while ((!stdin_mode && ++i<argc) || input_from_file);

	if (json_mode)
		printf("\n]\n");

	/* Free up allocated memory to keep MemorySanitizier happy :-) */
	jpeg_destroy_decompress(&cinfo);
	free_jpeg_info(&info);
	if (inbuf)
		free(inbuf);

	 /* Return 1 if any errors found in files checked */
	return (global_total_errors > 0 ? 1 : 0);
}

/* :-) */
