/* See sshos_crypto.c. */
#ifndef SSHOS_CRYPTO_H
#define SSHOS_CRYPTO_H

#define SSHOS_CRYPTO_CMD        0x1D    /* the SCSI command */
#define SSHOS_HELLO             0x00    /* its requests, in the second byte */
#define SSHOS_X25519            0x01
#define SSHOS_PUBLIC            0x02
#define SSHOS_SIGN              0x03
#define SSHOS_VERIFY            0x04
#define SSHOS_OPEN              0x05    /* version 2 on */
#define SSHOS_SEAL              0x06
#define SSHOS_RESULT            0x10
#define SSHOS_CRYPTO_VERSION    2
#define SSHOS_CRYPTO_MAX        4608    /* the most one request may write, or get back */
#define SSHOS_PACKET_HEAD       68      /* before a packet: its number (4), the key (64) */

unsigned int sshos_crypto_job(unsigned char request, unsigned char *buf, unsigned int len,
                              unsigned int room);
unsigned int sshos_crypto_hello(unsigned char *out);
extern unsigned char *sshos_scratch;    /* 4 KB lent for the signature sums' tables */
#define SSHOS_SCRATCH_SIZE      4096

/* SSH OS's own arithmetic (kernel/x25519.c, ed25519.c) */
#ifndef SSHOS_KERNEL
void x25519(unsigned char *out32, const unsigned char *scalar32, const unsigned char *point32);
void ed25519_public(unsigned char *pub32, const unsigned char *seed32);
void ed25519_sign(unsigned char *sig64, const unsigned char *msg, unsigned int len,
                  const unsigned char *seed32, const unsigned char *pub32);
int  ed25519_verify(const unsigned char *sig64, const unsigned char *msg, unsigned int len,
                    const unsigned char *pub32);
void chacha20(const unsigned char *key32, const unsigned char *nonce12, unsigned int counter,
              unsigned char *data, unsigned int len);
void poly1305(unsigned char *tag16, const unsigned char *key32, const unsigned char *msg,
              unsigned int len);
#endif

#endif
