/* X25519 key agreement (RFC 7748) and the ChaCha20 cipher (RFC 8439), for
 * the SSH client (ssh.c). The arithmetic modulo 2^255 - 19 is shared with
 * ed25519.c through fe25519.h.
 *
 * The 68000 multiplies 16 x 16 bits into 32 and has nothing wider, so a
 * number modulo 2^255 - 19 is kept as sixteen 16-bit limbs, least
 * significant first. A value may be anywhere below 2^256; it is only brought
 * below the modulus when it is written out. 2^256 is 38 modulo 2^255 - 19,
 * which is what every "fold" below uses.
 */
#include "kernel.h"
#include "fe25519.h"

/* Called now and then during the long calculations here and in ed25519.c,
 * if set: ssh.c uses it to keep the network answered and show progress. */
void (*crypto_idle)(void);

/* Set (by that call, say) to make the calculation give up at once. What it
 * returns is then worthless: the caller must look at this before using it. */
volatile u8 crypto_stop;

/* Add c * 2^256 (as c * 38) into r. */
static void fe_fold(u16 *r, u32 c)
{
    int i;

    while (c) {                     /* a second turn only after a wrap, and then r is tiny */
        c *= 38;
        for (i = 0; i < LIMBS; i++) {
            c += r[i];
            r[i] = (u16)c;
            c >>= 16;
        }
    }
}

void fe_add(u16 *r, const u16 *a, const u16 *b)
{
    u32 c = 0;
    int i;

    for (i = 0; i < LIMBS; i++) {
        c += (u32)a[i] + b[i];
        r[i] = (u16)c;
        c >>= 16;
    }
    fe_fold(r, c);
}

void fe_sub(u16 *r, const u16 *a, const u16 *b)
{
    u32 t, borrow = 0;
    int i;

    for (i = 0; i < LIMBS; i++) {
        t = (u32)a[i] - b[i] - borrow;
        r[i] = (u16)t;
        borrow = (t >> 16) & 1;
    }
    while (borrow) {                /* we hold a - b + 2^256: take the 38 back off */
        borrow = 38;
        for (i = 0; i < LIMBS; i++) {
            t = (u32)r[i] - borrow;
            r[i] = (u16)t;
            borrow = (t >> 16) & 1;
        }
    }
}

/* Bring a 32-limb product back to 16 limbs. */
static void fe_reduce(u16 *r, const u16 *t)
{
    u32 c = 0;
    int i;

    for (i = 0; i < LIMBS; i++) {
        c += t[i] + (u32)t[i + LIMBS] * (u16)38;
        r[i] = (u16)c;
        c >>= 16;
    }
    fe_fold(r, c);
}

/* Column by column: each column's 16 x 16 products are summed in acc, with
 * the overflows out of 32 bits counted in hi. */
void fe_mul(u16 *r, const u16 *a, const u16 *b)
{
    u16 t[2 * LIMBS];
    u32 acc = 0, hi = 0, p;
    int i, k, first, last;

    for (k = 0; k < 2 * LIMBS - 1; k++) {
        first = k < LIMBS ? 0 : k - (LIMBS - 1);
        last = k < LIMBS ? k : LIMBS - 1;
        for (i = first; i <= last; i++) {
            p = (u32)a[i] * b[k - i];
            acc += p;
            if (acc < p)
                hi++;
        }
        t[k] = (u16)acc;
        acc = (acc >> 16) | (hi << 16);
        hi = 0;
    }
    t[2 * LIMBS - 1] = (u16)acc;
    fe_reduce(r, t);
}

/* Squaring needs each cross product only once, doubled. */
void fe_sqr(u16 *r, const u16 *a)
{
    u16 t[2 * LIMBS];
    u32 acc, hi, p, carry = 0;
    int i, k, first;

    for (k = 0; k < 2 * LIMBS - 1; k++) {
        first = k < LIMBS ? 0 : k - (LIMBS - 1);
        acc = 0;
        hi = 0;
        for (i = first; 2 * i < k; i++) {
            p = (u32)a[i] * a[k - i];
            acc += p;
            if (acc < p)
                hi++;
        }
        hi = (hi << 1) | (acc >> 31);
        acc <<= 1;
        if (!(k & 1)) {
            p = (u32)a[k / 2] * a[k / 2];
            acc += p;
            if (acc < p)
                hi++;
        }
        acc += carry;
        if (acc < carry)
            hi++;
        t[k] = (u16)acc;
        carry = (acc >> 16) | (hi << 16);
    }
    t[2 * LIMBS - 1] = (u16)carry;
    fe_reduce(r, t);
}

/* Multiply by 121665 = 0x1DB41, the curve constant (A - 2) / 4. */
static void fe_mul_a24(u16 *r, const u16 *a)
{
    u32 c = 0, prev = 0, ai;
    int i;

    for (i = 0; i < LIMBS; i++) {
        ai = a[i];
        c += (u32)a[i] * (u16)0xDB41 + prev;    /* prev: the 0x10000 part of the constant */
        r[i] = (u16)c;
        c >>= 16;
        prev = ai;
    }
    fe_fold(r, c + prev);
}

/* Swap a and b when mask is 0xFFFF, leave them when it is 0, in the same time. */
static void fe_cswap(u16 *a, u16 *b, u16 mask)
{
    u16 t;
    int i;

    for (i = 0; i < LIMBS; i++) {
        t = mask & (a[i] ^ b[i]);
        a[i] ^= t;
        b[i] ^= t;
    }
}

/* out = z^(p - 2), the inverse of z: 254 squarings and 11 multiplications. */
void fe_invert(u16 *out, const u16 *z)
{
    u16 t0[LIMBS], t1[LIMBS], t2[LIMBS], t3[LIMBS];
    int i;

    fe_sqr(t0, z);                                          /* 2 */
    fe_sqr(t1, t0);
    fe_sqr(t1, t1);                                         /* 8 */
    fe_mul(t1, z, t1);                                      /* 9 */
    fe_mul(t0, t0, t1);                                     /* 11 */
    fe_sqr(t2, t0);                                         /* 22 */
    fe_mul(t1, t1, t2);                                     /* 2^5 - 1 */
    fe_sqr(t2, t1);
    for (i = 1; i < 5; i++)
        fe_sqr(t2, t2);
    fe_mul(t1, t2, t1);                                     /* 2^10 - 1 */
    fe_sqr(t2, t1);
    for (i = 1; i < 10; i++)
        fe_sqr(t2, t2);
    fe_mul(t2, t2, t1);                                     /* 2^20 - 1 */
    fe_sqr(t3, t2);
    for (i = 1; i < 20; i++)
        fe_sqr(t3, t3);
    fe_mul(t2, t3, t2);                                     /* 2^40 - 1 */
    for (i = 0; i < 10; i++)
        fe_sqr(t2, t2);
    fe_mul(t1, t2, t1);                                     /* 2^50 - 1 */
    fe_sqr(t2, t1);
    for (i = 1; i < 50; i++)
        fe_sqr(t2, t2);
    fe_mul(t2, t2, t1);                                     /* 2^100 - 1 */
    fe_sqr(t3, t2);
    for (i = 1; i < 100; i++)
        fe_sqr(t3, t3);
    fe_mul(t2, t3, t2);                                     /* 2^200 - 1 */
    for (i = 0; i < 50; i++)
        fe_sqr(t2, t2);
    fe_mul(t1, t2, t1);                                     /* 2^250 - 1 */
    for (i = 0; i < 5; i++)
        fe_sqr(t1, t1);                                     /* 2^255 - 32 */
    fe_mul(out, t1, t0);                                    /* 2^255 - 21 */
}

/* Write a out as 32 bytes, least significant first, fully reduced. */
void fe_tobytes(u8 *out, const u16 *a)
{
    u16 r[LIMBS], t[LIMBS], mask;
    u32 c;
    int i;

    for (i = 0; i < LIMBS; i++)
        r[i] = a[i];
    c = (u32)(r[LIMBS - 1] >> 15) * 19;     /* 2^255 is 19: afterwards r < 2^255 + 19 < 2p */
    r[LIMBS - 1] &= 0x7FFF;
    for (i = 0; i < LIMBS; i++) {
        c += r[i];
        r[i] = (u16)c;
        c >>= 16;
    }
    c = 19;                                 /* r >= p exactly when r + 19 reaches 2^255 */
    for (i = 0; i < LIMBS; i++) {
        c += r[i];
        t[i] = (u16)c;
        c >>= 16;
    }
    mask = (u16)(0 - (t[LIMBS - 1] >> 15));
    t[LIMBS - 1] &= 0x7FFF;
    for (i = 0; i < LIMBS; i++) {
        r[i] ^= mask & (r[i] ^ t[i]);
        out[2 * i] = (u8)r[i];
        out[2 * i + 1] = (u8)(r[i] >> 8);
    }
}

void fe_frombytes(u16 *r, const u8 *in)
{
    int i;

    for (i = 0; i < LIMBS; i++)
        r[i] = (u16)(in[2 * i] | ((u16)in[2 * i + 1] << 8));
}

/* out = scalar * point; all three are 32 bytes. The base point is {9, 0...}. */
void x25519(u8 *out, const u8 *scalar, const u8 *point)
{
    u16 x1[LIMBS], x2[LIMBS], z2[LIMBS], x3[LIMBS], z3[LIMBS];
    u16 a[LIMBS], aa[LIMBS], b[LIMBS], bb[LIMBS], e[LIMBS], c[LIMBS], d[LIMBS];
    u16 da[LIMBS], cb[LIMBS];
    u8 k[32];
    u16 bit, swap = 0;
    int i, t;

    for (i = 0; i < 32; i++)
        k[i] = scalar[i];
    k[0] &= 248;
    k[31] &= 127;
    k[31] |= 64;

    fe_frombytes(x1, point);
    for (i = 0; i < LIMBS; i++)
        x2[i] = z2[i] = z3[i] = 0;
    x1[LIMBS - 1] &= 0x7FFF;
    for (i = 0; i < LIMBS; i++)
        x3[i] = x1[i];
    x2[0] = 1;
    z3[0] = 1;

    for (t = 254; t >= 0; t--) {
        bit = (u16)(0 - ((k[t >> 3] >> (t & 7)) & 1));
        swap ^= bit;
        fe_cswap(x2, x3, swap);
        fe_cswap(z2, z3, swap);
        swap = bit;

        fe_add(a, x2, z2);
        fe_sqr(aa, a);
        fe_sub(b, x2, z2);
        fe_sqr(bb, b);
        fe_sub(e, aa, bb);
        fe_add(c, x3, z3);
        fe_sub(d, x3, z3);
        fe_mul(da, d, a);
        fe_mul(cb, c, b);
        fe_add(x3, da, cb);
        fe_sqr(x3, x3);
        fe_sub(z3, da, cb);
        fe_sqr(z3, z3);
        fe_mul(z3, z3, x1);
        fe_mul(x2, aa, bb);
        fe_mul_a24(z2, e);
        fe_add(z2, z2, aa);
        fe_mul(z2, z2, e);
        if (crypto_idle && (t & 7) == 0)
            crypto_idle();
        if (crypto_stop)
            break;
    }
    fe_cswap(x2, x3, swap);
    fe_cswap(z2, z3, swap);

    fe_invert(z2, z2);
    fe_mul(x2, x2, z2);
    fe_tobytes(out, x2);
}

/* ------------------------------------------------------------ ChaCha20 -- */

#define ROL(x, n) ((u32)(((x) << (n)) | ((x) >> (32 - (n)))))
#define QUARTER(a, b, c, d) \
    do { \
        a += b; d ^= a; d = ROL(d, 16); \
        c += d; b ^= c; b = ROL(b, 12); \
        a += b; d ^= a; d = ROL(d, 8); \
        c += d; b ^= c; b = ROL(b, 7); \
    } while (0)

static u32 load32(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

/* XOR len bytes of data with the key stream that starts at block `counter`.
 * key is 32 bytes, nonce 12. */
void chacha20(const u8 *key, const u8 *nonce, u32 counter, u8 *data, u32 len)
{
    u32 s[16], x[16], w;
    u32 i, n;

    s[0] = 0x61707865UL;
    s[1] = 0x3320646eUL;
    s[2] = 0x79622d32UL;
    s[3] = 0x6b206574UL;
    for (i = 0; i < 8; i++)
        s[4 + i] = load32(key + 4 * i);
    s[12] = counter;
    for (i = 0; i < 3; i++)
        s[13 + i] = load32(nonce + 4 * i);

    while (len > 0) {
        for (i = 0; i < 16; i++)
            x[i] = s[i];
        for (i = 0; i < 10; i++) {
            QUARTER(x[0], x[4], x[8], x[12]);
            QUARTER(x[1], x[5], x[9], x[13]);
            QUARTER(x[2], x[6], x[10], x[14]);
            QUARTER(x[3], x[7], x[11], x[15]);
            QUARTER(x[0], x[5], x[10], x[15]);
            QUARTER(x[1], x[6], x[11], x[12]);
            QUARTER(x[2], x[7], x[8], x[13]);
            QUARTER(x[3], x[4], x[9], x[14]);
        }
        n = len < 64 ? len : 64;
        w = 0;
        for (i = 0; i < n; i++) {
            if ((i & 3) == 0)
                w = x[i >> 2] + s[i >> 2];
            data[i] ^= (u8)w;
            w >>= 8;
        }
        data += n;
        len -= n;
        s[12]++;
    }
}
