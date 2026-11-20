/* Arithmetic modulo 2^255 - 19, as sixteen 16-bit limbs, least significant
 * first (see x25519.c for why). A value may be anywhere below 2^256 until
 * fe_tobytes writes it out. Results may overlap the inputs. */
#ifndef SSHOS_FE25519_H
#define SSHOS_FE25519_H

#define LIMBS 16

void fe_add(u16 *r, const u16 *a, const u16 *b);
void fe_sub(u16 *r, const u16 *a, const u16 *b);
void fe_mul(u16 *r, const u16 *a, const u16 *b);
void fe_sqr(u16 *r, const u16 *a);
void fe_invert(u16 *out, const u16 *z);
void fe_frombytes(u16 *r, const u8 *in32);      /* all 256 bits, as they are */
void fe_tobytes(u8 *out32, const u16 *a);       /* fully reduced */

extern void (*crypto_idle)(void);

#endif
