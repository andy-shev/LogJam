/* See md5.c for explanation and copyright information.  */

#ifndef __LIVEJOURNAL_MD5_H__
#define __LIVEJOURNAL_MD5_H__

/* Unlike previous versions of this code, uint32 need not be exactly
   32 bits, merely 32 bits or more.  Choosing a data type which is 32
   bits instead of 64 is not important; speed is considerably more
   important.  ANSI guarantees that "unsigned long" will be big enough,
   and always using it seems to have few disadvantages.  */
typedef unsigned long lj_uint32;

struct lj_md5_context {
	lj_uint32 buf[4];
	lj_uint32 bits[2];
	unsigned char in[64];
};

void lj_md5_init      (struct lj_md5_context *context);
void lj_md5_update    (struct lj_md5_context *context,
                       unsigned char const *buf, unsigned len);
void lj_md5_final     (unsigned char digest[16],
                       struct lj_md5_context *context);
void lj_md5_transform (lj_uint32 buf[4], const unsigned char in[64]);

/* simplified interface. */
void lj_md5_hash(const char *src, char *dest);

#endif /* __LIVEJOURNAL_MD5_H__ */
