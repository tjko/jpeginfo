/* jpeginfo.h
 *
 * Copyright (c) Timo Kokkonen, 1997.
 *
 * $Id$
 */

#include <stdio.h>

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

#define PROGRAMNAME "jpeginfo"


#define MIN(a,b) (a < b ? a : b)


/* misc.c */

int  is_dir(FILE *fp);
long long filesize(FILE *fp);
void delete_file(const char *name, int verbose_mode, int quiet_mode);
char *fgetstr(char *s, size_t size, FILE *stream);
char *digest2str(unsigned char *digest, char *s, unsigned int len);
long long read_file(FILE *fp, size_t start_size, unsigned char **bufptr);
char *strncopy(char *dst, const char *src, size_t size);
char *strncatenate(char *dst, const char *src, size_t size);
char *str_add_list(char *dst, size_t size, const char *src, const char *delim);
char *escape_str(const char *src, char escape_char, char escape);
