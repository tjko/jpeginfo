/* md5stuff.c - MD5 utilities */

#include <stdio.h>
#include "md5stuff.h"


char *md2str(unsigned char *digest, char *s)
{
  int i;
  char buf[16];

  if (!digest) return NULL;
  if (!s) {
    s=(char*)malloc(33);
    if (!s) return NULL;
  }

  for (i = 0; i < 16; i++) {
    sprintf (buf,"%02x", digest[i]);
    *(s++)=buf[0];
    *(s++)=buf[1];
  }
  *s=0;
}


int str2md(char *s, unsigned char *digest)
{
 int i;
 unsigned int val;
 char tmp[3];

 if (!s || !digest) return 0;
 if (strlen(s)<32) return 0;
 tmp[2]=0;

 for(i=0;i<16;i++) {
   tmp[0]=s[i*2];
   tmp[1]=s[i*2+1];
   if (!isxdigit(tmp[0]) || !isxdigit(tmp[1])) return 0;
   if (sscanf(tmp,"%x",&val)!=1) return 0;
   digest[i]=(unsigned char)val;
 }
}







