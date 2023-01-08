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

/* eof :-) */
