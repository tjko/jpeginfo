/* misc.c - misc routines for jpeginfo
 *
 * Copyright (c) 1997-2023 Timo Kokkonen
 * All Rights Reserved.
 *
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

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "jpeginfo.h"


int is_dir(FILE *fp)
{
	if (!fp)
		return 0;

	struct stat buf;
	if (fstat(fileno(fp), &buf))
		return 0;

	return (S_ISDIR(buf.st_mode) ? 1 : 0);
}


long long filesize(FILE *fp)
{
	if (!fp)
		return -1;

	struct stat buf;
	if (fstat(fileno(fp), &buf))
		return -1;

	return buf.st_size;
}


void delete_file(const char *name, int verbose_mode, int quiet_mode)
{
	if (!name)
		return;

	if (verbose_mode && !quiet_mode)
		fprintf(stderr, "deleting: %s\n", name);
	if (unlink(name) && !quiet_mode)
		fprintf(stderr, "error unlinking file: %s\n", name);
}


char *fgetstr(char *s, size_t size, FILE *stream)
{
	if (!s || size < 1 || !stream)
		return NULL;

	if (!fgets(s, size, stream))
		return NULL;

	char *p = s + strnlen(s, size) - 1;
	while ((p >= s) && ((*p == 10) || (*p == 13)))
		*p--=0;

	return s;
}


char *digest2str(unsigned char *digest, char *s, unsigned int len)
{
	char *output = s;

	if (!digest || !s)
		return NULL;

	*output = 0;
	for (int i = 0; i < len; i++) {
		snprintf(output, 3, "%02x", digest[i]);
		output += 2;
	}

	return s;
}


const size_t MIN_READ_BUFFER_SIZE=512;

long long read_file(FILE *fp, size_t buf_size, unsigned char **bufptr)
{
	if (!fp || !bufptr)
		return -1;

	/* Allocate initial buffer for reading the file */
	if (buf_size < MIN_READ_BUFFER_SIZE)
		buf_size = MIN_READ_BUFFER_SIZE;
	if ((*bufptr = realloc(*bufptr, buf_size)) == NULL)
		return -2;

	size_t buf_used = 0;
	size_t bytes_read;

	/* Read file into the buffer */
	do {
		bytes_read = fread(*bufptr + buf_used, 1, buf_size - buf_used, fp);
		buf_used += bytes_read;
		if (buf_used >= buf_size) {
			/* Expand buffer if needed */
			buf_size *= 2;
			*bufptr = realloc(*bufptr, buf_size);
			if (! *bufptr)
				return -3;
		}
	} while (bytes_read > 0);

	return buf_used;
}


char *strncopy(char *dst, const char *src, size_t size)
{
	if (!dst || !src || size < 1)
		return dst;

	if (size > 1)
		strncpy(dst, src, size - 1);
	dst[size - 1] = 0;

	return dst;
}


char *strncatenate(char *dst, const char *src, size_t size)
{

	if (!dst || !src || size < 1)
		return dst;

	/* Check if dst string is already "full" ... */
	const int used = strnlen(dst, size);
	const int free = size - used;
	if (free <= 1)
		return dst;

	return strncat(dst + used, src, free - 1);
}


char *str_add_list(char *dst, size_t size, const char *src, const char *delim)
{
	if (!dst || !src || !delim || size < 1)
		return dst;

	if (strnlen(dst, size) > 0)
		strncatenate(dst, delim, size);

	return strncatenate(dst, src, size);
}

/* eof :-) */
