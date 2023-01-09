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

#define JFIF_JPEG_MARKER  JPEG_APP0
#define JFIF_IDENT_STRING "JFIF\0"
#define JFIF_IDENT_STRING_SIZE 5

#define JFXX_JPEG_MARKER  JPEG_APP0
#define JFXX_IDENT_STRING "JFXX\0"
#define JFXX_IDENT_STRING_SIZE 5

#define EXIF_JPEG_MARKER   JPEG_APP0 + 1
#define EXIF_IDENT_STRING  "Exif\000\000"
#define EXIF_IDENT_STRING_SIZE 6

#define IPTC_JPEG_MARKER   JPEG_APP0 + 13

#define ICC_JPEG_MARKER   JPEG_APP0 + 2
#define ICC_IDENT_STRING  "ICC_PROFILE\0"
#define ICC_IDENT_STRING_SIZE 12

#define XMP_JPEG_MARKER   JPEG_APP0 + 1
#define XMP_IDENT_STRING  "http://ns.adobe.com/xap/1.0/\000"
#define XMP_IDENT_STRING_SIZE 29

#define ADOBE_JPEG_MARKER JPEG_APP0 + 14
#define ADOBE_IDENT_STRING "Adobe"
#define ADOBE_IDENT_STRING_SIZE 5


#define PROGRAMNAME "jpeginfo"

#define MIN(a,b) (a < b ? a : b)


/* misc.c */

int  is_dir(FILE *fp);
long long filesize(FILE *fp);
void delete_file(char *name, int verbose_mode, int quiet_mode);
char *fgetstr(char *s,int n,FILE *stream);
char *digest2str(unsigned char *digest, char *s, unsigned int len);
long long read_file(FILE *fp, size_t start_size, unsigned char **bufptr);
char *strncopy(char *dst, const char *src, size_t len);
char *strnconcat(char *dst, const char *src, size_t size);
char *str_add_list(char *dst, size_t size, const char *src, const char *delim);



/* eof :-) */
