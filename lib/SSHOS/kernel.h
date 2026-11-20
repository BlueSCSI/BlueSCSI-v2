/* What SSH OS's arithmetic expects of its kernel.h, and no more: the
 * sources here are SSH OS's own files, unchanged, and include it by this
 * name. */
#ifndef SSHOS_KERNEL_H
#define SSHOS_KERNEL_H
#include <string.h>
#include "sshos_types.h"

void x25519(u8 *out32, const u8 *scalar32, const u8 *point32);
void chacha20(const u8 *key32, const u8 *nonce12, u32 counter, u8 *data, u32 len);
/* If set, called now and then during the long calculations. */
extern void (*crypto_idle)(void);
extern volatile u8 crypto_stop;         /* makes those calculations give up at once */

struct sha512 {
    u32 h[8][2];                /* each word as high half, low half */
    u32 total;
    u8  buf[128];
    u8  used;
};
void sha512_init(struct sha512 *s);
void sha512_update(struct sha512 *s, const void *data, u32 len);
void sha512_finish(struct sha512 *s, u8 *out64);

void ed25519_public(u8 *pub32, const u8 *seed32);
void ed25519_sign(u8 *sig64, const u8 *msg, u32 len, const u8 *seed32, const u8 *pub32);
int  ed25519_verify(const u8 *sig64, const u8 *msg, u32 len, const u8 *pub32);    /* 1 = genuine */

void poly1305(u8 *tag16, const u8 *key32, const u8 *msg, u32 len);
#endif
