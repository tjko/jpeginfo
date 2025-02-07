/*******************************************************************
 * JPEGinfo
 * Copyright (c) Timo Kokkonen, 1995-2025.
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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <ctype.h>
#include <jpeglib.h>
#include <jerror.h>

#include "md5/md5.h"
#include "sha1/sha1.h"
#include "sha256/crypto_hash_sha256.h"
#include "sha512/crypto_hash_sha512.h"
#include "jpegmarker.h"
#include "jpeginfo.h"


#define VERSION     "1.7.2beta"
#define COPYRIGHT   "Copyright (C) 1996-2025 Timo Kokkonen"

#define BUF_LINES   512

#ifndef HOST_TYPE
#define HOST_TYPE ""
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

enum hash_modes {
	HASH_NONE = 0,
	HASH_MD5,
	HASH_SHA1,
	HASH_SHA256,
	HASH_SHA512,
};

FILE *infile=NULL;
FILE *listfile=NULL;
int global_error_counter = 0;
int global_total_errors = 0;
int verbose_mode = 0;
int quiet_mode = 0;
bool delete_mode = false;
bool check_mode = false;
bool com_mode = false;
bool del_mode = false;
int opt_index = 0;
bool list_mode = false;
bool longinfo_mode = false;
bool input_from_file = false;
enum hash_modes hash_mode = HASH_NONE;
int stdin_mode = 0;
bool csv_mode = false;
bool json_mode = false;
bool header_mode = false;
int files_stdin_mode = 0;
char *current = NULL;
char last_error[JMSG_LENGTH_MAX + 1];
char escape_char = 0;
char escape_val = 0;


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
	{"sha1",0,0,'1'},
	{"sha256",0,0,'2'},
	{"sha512",0,(int*)&hash_mode,HASH_SHA512},
	{"version",0,0,'V'},
	{"comments",0,0,'C'},
	{"csv",0,0,'s'},
	{"json",0,0,'j'},
	{"header",0,0,'H'},
	{"stdin",0,&stdin_mode,1},
	{"files-from",1,0,'f'},
	{"files-stdin",0,&files_stdin_mode,1},
	{0,0,0,0}
};

/*****************************************************************************/


static void my_error_exit (j_common_ptr cinfo)
{
	my_error_ptr myerr = (my_error_ptr)cinfo->err;
	(*cinfo->err->output_message) (cinfo);
	longjmp(myerr->setjmp_buffer,1);
}


static void my_output_message (j_common_ptr cinfo)
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
		"  -1, --sha1      Calculate SHA-1 checksum for each file.\n"
		"  -2, --sha256    Calculate SHA-256 checksum for each file.\n"
		"      --sha512    Calculate SHA-512 checksum for each file.\n"
		"  -5, --md5       Calculate MD5 checksum for each file.\n"
		"  -c, --check     Check files also for errors.\n"
		"  -C, --comments  Display comments (from COM markers)\n"
		"  -d, --delete    Delete files that have errors\n"
		"  -f <filename>,  --files-from=<filename>\n"
		"                  Read the filenames to process from given file\n"
		"   --files-stdin  Read the filenames to process from standard input\n"
		"  -h, --help      Display this help and exit\n"
		"  -H, --header    Display column name header in output\n"
		"  -i, --info      Display even more information about pictures\n"
		"  -j, --json      JSON output style.\n"
		"  -l, --lsstyle   Use alternate listing format (ls -l style)\n"
		"  -m <mode>, --mode=<mode>\n"
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


void print_hash_header()
{
	switch (hash_mode) {
	case HASH_MD5:
		printf("MD5                              ");
		break;
	case HASH_SHA1:
		printf("SHA-1                                    ");
		break;
	case HASH_SHA256:
		printf("SHA-256                                                          ");
		break;
	case HASH_SHA512:
		printf("SHA-512                                                          "
			"                                                                ");
		break;

	default:
		break;
	}
}


void parse_args(int argc, char **argv)
{
	while(1) {
		opt_index=0;
		const int c = getopt_long(argc,argv, "livVdcChqm:f:521sHj",
					  long_options, &opt_index);
		if (c == -1)
			break;
		switch (c) {
		case 'm':
			if (!strcasecmp(optarg, "all")) del_mode=false;
			else if (!strcasecmp(optarg, "erronly")) del_mode=true;
			else if (!quiet_mode)
				fprintf(stderr, "Unknown parameter for -m, --mode.\n");
			break;
		case 'f':
			if (verbose_mode)
				fprintf(stderr, "Reading input filenames from: '%s'\n", optarg);
			if (!strcmp(optarg, "-"))
				listfile = stdin;
			else if ((listfile = fopen(optarg, "r")) == NULL) {
				fprintf(stderr, "Cannot open file '%s'.\n", optarg);
				exit(2);
			}
			input_from_file = true;
			break;
		case 'v':
			verbose_mode++;
			break;
		case 'V':
			print_version();
			exit(0);
		case 'd':
			delete_mode = true;
			break;
		case 'c':
			check_mode = true;
			break;
		case 'h':
			print_usage();
			break;
		case 'q':
			quiet_mode++;
			break;
		case 'l':
			list_mode = true;
			break;
		case 'i':
			longinfo_mode = true;
			break;
		case '5':
			hash_mode = HASH_MD5;
			break;
		case '2':
			hash_mode = HASH_SHA256;
			break;
		case '1':
			hash_mode = HASH_SHA1;
			break;
		case 'C':
			com_mode = true;
			break;
		case 's':
			csv_mode = true;
			break;
		case 'j':
			json_mode = true;
			break;
		case 'H':
			header_mode = true;
			break;
		case '?':
			exit(1);

		}
	}

	/* check for '-' option indicating input is from stdin... */
	int i = optind;
	while (argv[i]) {
		if (argv[i][0]=='-' && argv[i][1]==0)
			stdin_mode=1;
		i++;
	}

	if (files_stdin_mode) {
		listfile = stdin;
		input_from_file = true;
	}

	if (delete_mode && verbose_mode && !quiet_mode)
		fprintf(stderr, "jpeginfo: delete mode enabled (%s)\n",
			(!del_mode ? "normal" : "errors only"));

	if (argc <= optind && !input_from_file) {
		if (quiet_mode < 2) fprintf(stderr, "jpeginfo: file arguments missing\n"
					"Try 'jpeginfo --help' for more information.\n");
		exit(1);
	}

	if (csv_mode) {
		escape_char = '"';
		escape_val = '"';

	}
	else if (json_mode) {
		escape_char = '"';
		escape_val = '\\';
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
	if (!cinfo || !info)
		return;

	const size_t marker_types = jpeg_special_marker_types_count();

	char *seen = malloc(marker_types);
	if (seen == NULL)
		no_memory();
	memset(seen, 0, marker_types);

	info->width = (int)cinfo->image_width;
	info->height = (int)cinfo->image_height;
	info->color_depth = (int)cinfo->num_components * 8;
	info->progressive = (cinfo->progressive_mode ? 1 : 0);

	char info_str[256];
	strncopy(info_str, (cinfo->arith_code ? "Arithmetic" : "Huffman"), sizeof(info_str));
	char comment_str[1024];
	comment_str[0]=0;
	char marker_str[256];
	marker_str[0]=0;

	/* Check for special (Exif/IPTC/ICC/XMP/etc...) markers */
	jpeg_saved_marker_ptr cmarker=cinfo->marker_list;

	int marker_in_count = 0;
	int comment_count = 0;
	int unknown_count = 0;
	unsigned long marker_in_size = 0;

	while (cmarker) {
		marker_in_count++;
		marker_in_size+=cmarker->data_length;
		const int special = jpeg_special_marker(cmarker);

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
				char tmp[64];
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
		char tmp[9];
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
	if (!info)
		return;

	if (quiet_mode > 1)
		return;

	static int header_printed = 0;
	static long line =  0;

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
			print_hash_header();
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
			print_hash_header();
			if (com_mode)
				printf("Comments                         ");
			if (check_mode)
				printf("Status  Details");
			printf("\n");
		}
		header_printed = 1;
	}

	char *filename = escape_str(info->filename, escape_char, escape_val);
	char *com = (info->comments ? info->comments : "");
	if (!com_mode && !csv_mode && !json_mode)
		com = "";
	com = escape_str(com, escape_char, escape_val);
	const char *type = (info->type ? info->type : "");
	const char *einfo = (info->info ? info->info : "");
	const char *error = (info->error ? info->error : "");
	const char *digest = (info->digest ? info->digest : "");

	const char p = (info->progressive ? 'P' : 'N');

	line++;

	if (csv_mode) {
		printf("\"%s\",%lu,\"%s\",%d,%d,\"%dbit\",\"%s\",\"%c\",\"%s\",\"%s\",\"%s\",\"%s\"\n",
			filename,
			(long unsigned int)info->size,
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
			filename,
			(long unsigned int)info->size,
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
			(long unsigned int)info->size);
		if (info->digest)
			printf("%s ", digest);
		if (com_mode)
			printf("%-32s ", com);
		printf("%-32s %-7s%s%s\n",
			filename,
			check_status_str(info->check),
			(info->error ? " " : ""),
			error
			);
	}
	else {
		printf("%-32s %4d x %4d %2dbit %c %-24s ",
			filename,
			info->width,
			info->height,
			info->color_depth,
			p,
			type);
		if (longinfo_mode)
			printf("%-20s ", einfo);
		printf("%7lu ",
			(long unsigned int)info->size);
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


	if (filename)
		free(filename);
	if (com)
		free(com);
}


void clear_line_buffer(JSAMPARRAY buf)
{
	for (int i = 0; i < BUF_LINES; i++) {
		if (buf[i])
			free(buf[i]);
		buf[i] = NULL;
	}
}


char* calculate_hash(const unsigned char *buf, size_t buf_len)
{
	unsigned char digest[64];
	char digest_text[128 + 1];
	MD5_CTX md5;
	SHA1Context sha1;

	switch(hash_mode) {
	case HASH_MD5:
		MD5Init(&md5);
		MD5Update(&md5, buf, buf_len);
		MD5Final(digest, &md5);
		digest2str(digest, digest_text, 16);
		break;
	case HASH_SHA1:
		SHA1Reset(&sha1);
		SHA1Input(&sha1, buf, buf_len);
		SHA1Result(&sha1, digest);
		digest2str(digest, digest_text, 20);
		break;
	case HASH_SHA256:
		crypto_hash_sha256(digest, buf, buf_len);
		digest2str(digest, digest_text, 32);
		break;
	case HASH_SHA512:
		crypto_hash_sha512(digest, buf, buf_len);
		digest2str(digest, digest_text, 64);
		break;
	default:
		digest_text[0] = 0;
	}

	return strdup(digest_text);
}


/*****************************************************************************/
int main(int argc, char **argv)
{
	char namebuf[MAXPATHLEN + 1];
	JSAMPROW line_buffer[BUF_LINES];
	JSAMPARRAY buf = line_buffer;
	unsigned char *inbuf = NULL;
	long long file_size;
	size_t inbuffer_size;

	/* Initialize memory structures... */
	for(int i = 0; i < BUF_LINES; i++) {
		buf[i] = NULL;
	}
	struct jpeg_info info;
	clear_jpeg_info(&info);
	cinfo.err = jpeg_std_error(&jerr.pub);
	jpeg_create_decompress(&cinfo);
	jerr.pub.error_exit=my_error_exit;
	jerr.pub.output_message=my_output_message;

	/* Parse command line parameters */
	parse_args(argc, argv);
	int i=(optind > 0 ? optind : 1);

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
		if (hash_mode != HASH_NONE) {
			info.digest = calculate_hash(inbuf, file_size);
		}

		/* Read JPEG file header */
		global_error_counter=0;
		jpeg_save_markers(&cinfo, JPEG_COM, 0xffff);
		for (int j = 0; j < 16; j++) {
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

			for (int j = 0; j < BUF_LINES; j++) {
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

/* eof :-) */
