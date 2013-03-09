#ifndef MD5_H
#define MD5_H

#if SIZEOF_LONG == 4
typedef unsigned long uint32;
#else
#if SIZEOF_INT == 4
typedef unsigned int uint32;
#else
#warn Maybe you just need to reconfigure.
#error You get to rewrite this code to not require 32-bit integer type!
#endif
#endif

struct MD5Context {
    uint32 buf[4];
    uint32 bits[2];
    unsigned char in[64];
};

/* This is needed to make RSAREF happy on some MS-DOS compilers. */
typedef struct MD5Context MD5_CTX;

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf, unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);
void MD5Transform(uint32 buf[4], uint32 const in[16]);

#endif /* MD5_H */
