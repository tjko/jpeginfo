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
	struct stat buf;

	if (!fp)
		return 0;

	if (fstat(fileno(fp), &buf))
		return 0;

	return (S_ISDIR(buf.st_mode) ? 1 : 0);
}


long long filesize(FILE *fp)
{
	struct stat buf;

	if (!fp)
		return -1;

	if (fstat(fileno(fp), &buf))
		return -1;

	return buf.st_size;
}


void delete_file(char *name, int verbose_mode, int quiet_mode)
{
	if (!name)
		return;

	if (verbose_mode && !quiet_mode)
		fprintf(stderr, "deleting: %s\n", name);
	if (unlink(name) && !quiet_mode)
		fprintf(stderr, "error unlinking file: %s\n", name);
}


char *fgetstr(char *s, int n, FILE *stream)
{
	char *p;

	if (!stream || !s || n < 1)
		return NULL;

	if (!fgets(s,n,stream))
		return NULL;

	p=&s[strlen(s)-1];
	while ((p >= s) && ((*p == 10) || (*p == 13)))
		*p--=0;

	return s;
}


char *digest2str(unsigned char *digest, char *s, unsigned int len)
{
	int i;
	char *r;

	if (!digest || !s)
		return NULL;

	r = s;
	for (i = 0; i < len; i++) {
		snprintf(r, 3, "%02x", digest[i]);
		r += 2;
	}

	return s;
}


long long read_file(FILE *fp, size_t start_size, unsigned char **bufptr)
{
	unsigned char *buf;
	size_t buf_size, bytes_read;
	size_t buf_used = 0;

	if (!fp || !bufptr)
		return -1;


	/* allocate initial buffer for reading the file */
	if (*bufptr)
		free(*bufptr);
	buf_size = (start_size > 8192 ? start_size : 8192);
	*bufptr = malloc(buf_size);
	if (! *bufptr)
		return -2;


	/* read file into the buffer */
	do {
		buf = *bufptr + buf_used;
		bytes_read = fread(buf, 1, buf_size - buf_used, fp);
		buf_used += bytes_read;
		if (buf_used >= buf_size) {
			buf_size *= 2;
			*bufptr = realloc(*bufptr, buf_size);
			if (! *bufptr)
				return -3;
		}
	} while (bytes_read > 0);

	return buf_used;
}


char *strncopy(char *dst, const char *src, size_t len)
{
	if (!dst || !src || len < 1)
		return NULL;

	strncpy(dst, src, len - 1);
	dst[len - 1] = 0;

	return dst;
}


char *strnconcat(char *dst, const char *src, size_t size)
{
	int len, free;

	if (!dst || !src || size < 1)
		return NULL;

	/* Check if dst string is already "full" ... */
	len = strnlen(dst, size);
	if ((free = size - len) <= 1)
		return dst;

	return strncat(dst + len, src, free - 1);
}


char *str_add_list(char *dst, size_t size, const char *src, const char *delim)
{
	if (!dst || !src || !delim || size < 1)
		return NULL;


	if (strnlen(dst, size) > 0)
		strnconcat(dst, delim, size);

	return strnconcat(dst, src, size);
}

/* eof :-) */
