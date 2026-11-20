/* Ed25519 signatures (RFC 8032): checking the signature a server makes with
 * its host key, and signing with our own key to log in.
 *
 * Points on the curve -x^2 + y^2 = 1 + d x^2 y^2 are kept as (X, Y, Z, T)
 * with x = X/Z, y = Y/Z and T = XY/Z, which lets two points be added
 * without a division. One formula covers every case, including adding a
 * point to itself or to the neutral point (0, 1).
 */
#include "kernel.h"
#include "fe25519.h"

/* The curve constant d = -121665/121666, twice that, a square root of -1,
 * the base point, and the order L of the base point. */
static const u16 ed_d[LIMBS] = {
    0x78A3, 0x1359, 0x4DCA, 0x75EB, 0xD8AB, 0x4141, 0x0A4D, 0x0070,
    0xE898, 0x7779, 0x4079, 0x8CC7, 0xFE73, 0x2B6F, 0x6CEE, 0x5203,
};
static const u16 ed_2d[LIMBS] = {
    0xF159, 0x26B2, 0x9B94, 0xEBD6, 0xB156, 0x8283, 0x149A, 0x00E0,
    0xD130, 0xEEF3, 0x80F2, 0x198E, 0xFCE7, 0x56DF, 0xD9DC, 0x2406,
};
static const u16 ed_sqrtm1[LIMBS] = {
    0xA0B0, 0x4A0E, 0x1B27, 0xC4EE, 0xE478, 0xAD2F, 0x1806, 0x2F43,
    0xD7A7, 0x3DFB, 0x0099, 0x2B4D, 0xDF0B, 0x4FC1, 0x2480, 0x2B83,
};
static const u16 ed_bx[LIMBS] = {
    0xD51A, 0x8F25, 0x2D60, 0xC956, 0xA7B2, 0x9525, 0xC760, 0x692C,
    0xDC5C, 0xFDD6, 0xE231, 0xC0A4, 0x53FE, 0xCD6E, 0x36D3, 0x2169,
};
static const u16 ed_by[LIMBS] = {
    0x6658, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666,
    0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666,
};
static const u16 sc_L[LIMBS] = {
    0xD3ED, 0x5CF5, 0x631A, 0x5812, 0x9CD6, 0xA2F7, 0xF9DE, 0x14DE,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1000,
};

struct ge {
    u16 x[LIMBS], y[LIMBS], z[LIMBS], t[LIMBS];
};

static void fe_copy(u16 *r, const u16 *a)
{
    int i;

    for (i = 0; i < LIMBS; i++)
        r[i] = a[i];
}

static void fe_small(u16 *r, u16 value)
{
    int i;

    r[0] = value;
    for (i = 1; i < LIMBS; i++)
        r[i] = 0;
}

static void fe_neg(u16 *r, const u16 *a)
{
    u16 zero[LIMBS];

    fe_small(zero, 0);
    fe_sub(r, zero, a);
}

static int fe_iszero(const u16 *a)
{
    u8 b[32], any = 0;
    int i;

    fe_tobytes(b, a);
    for (i = 0; i < 32; i++)
        any |= b[i];
    return any == 0;
}

static int fe_isodd(const u16 *a)
{
    u8 b[32];

    fe_tobytes(b, a);
    return b[0] & 1;
}

/* out = z^(2^252 - 3), used to take square roots. */
static void fe_pow22523(u16 *out, const u16 *z)
{
    u16 t0[LIMBS], t1[LIMBS], t2[LIMBS];
    int i;

    fe_sqr(t0, z);                                          /* 2 */
    fe_sqr(t1, t0);
    fe_sqr(t1, t1);                                         /* 8 */
    fe_mul(t1, z, t1);                                      /* 9 */
    fe_mul(t0, t0, t1);                                     /* 11 */
    fe_sqr(t0, t0);                                         /* 22 */
    fe_mul(t0, t1, t0);                                     /* 2^5 - 1 */
    fe_sqr(t1, t0);
    for (i = 1; i < 5; i++)
        fe_sqr(t1, t1);
    fe_mul(t0, t1, t0);                                     /* 2^10 - 1 */
    fe_sqr(t1, t0);
    for (i = 1; i < 10; i++)
        fe_sqr(t1, t1);
    fe_mul(t1, t1, t0);                                     /* 2^20 - 1 */
    fe_sqr(t2, t1);
    for (i = 1; i < 20; i++)
        fe_sqr(t2, t2);
    fe_mul(t1, t2, t1);                                     /* 2^40 - 1 */
    for (i = 0; i < 10; i++)
        fe_sqr(t1, t1);
    fe_mul(t0, t1, t0);                                     /* 2^50 - 1 */
    fe_sqr(t1, t0);
    for (i = 1; i < 50; i++)
        fe_sqr(t1, t1);
    fe_mul(t1, t1, t0);                                     /* 2^100 - 1 */
    fe_sqr(t2, t1);
    for (i = 1; i < 100; i++)
        fe_sqr(t2, t2);
    fe_mul(t1, t2, t1);                                     /* 2^200 - 1 */
    for (i = 0; i < 50; i++)
        fe_sqr(t1, t1);
    fe_mul(t0, t1, t0);                                     /* 2^250 - 1 */
    fe_sqr(t0, t0);
    fe_sqr(t0, t0);                                         /* 2^252 - 4 */
    fe_mul(out, t0, z);                                     /* 2^252 - 3 */
}

/* ---------------------------------------------------------------- points -- */

static void ge_neutral(struct ge *p)
{
    fe_small(p->x, 0);
    fe_small(p->y, 1);
    fe_small(p->z, 1);
    fe_small(p->t, 0);
}

static void ge_base(struct ge *p)
{
    fe_copy(p->x, ed_bx);
    fe_copy(p->y, ed_by);
    fe_small(p->z, 1);
    fe_mul(p->t, ed_bx, ed_by);
}

/* r = p + q. r may be p or q. */
static void ge_add(struct ge *r, const struct ge *p, const struct ge *q)
{
    u16 a[LIMBS], b[LIMBS], c[LIMBS], d[LIMBS], e[LIMBS], f[LIMBS], g[LIMBS], h[LIMBS];

    fe_sub(a, p->y, p->x);
    fe_sub(e, q->y, q->x);
    fe_mul(a, a, e);
    fe_add(b, p->y, p->x);
    fe_add(e, q->y, q->x);
    fe_mul(b, b, e);
    fe_mul(c, p->t, q->t);
    fe_mul(c, c, ed_2d);
    fe_mul(d, p->z, q->z);
    fe_add(d, d, d);
    fe_sub(e, b, a);
    fe_sub(f, d, c);
    fe_add(g, d, c);
    fe_add(h, b, a);
    fe_mul(r->x, e, f);
    fe_mul(r->y, g, h);
    fe_mul(r->t, e, h);
    fe_mul(r->z, f, g);
}

/* r = p + p, a little cheaper than ge_add. All four results come out
 * negated, which is the same point. */
static void ge_double(struct ge *r, const struct ge *p)
{
    u16 a[LIMBS], b[LIMBS], c[LIMBS], e[LIMBS], f[LIMBS], g[LIMBS], h[LIMBS];

    fe_sqr(a, p->x);
    fe_sqr(b, p->y);
    fe_sqr(c, p->z);
    fe_add(c, c, c);
    fe_add(e, p->x, p->y);
    fe_sqr(e, e);
    fe_sub(e, e, a);
    fe_sub(e, e, b);
    fe_sub(g, b, a);
    fe_sub(f, c, g);
    fe_add(h, a, b);
    fe_mul(r->x, e, f);
    fe_mul(r->y, g, h);
    fe_mul(r->t, e, h);
    fe_mul(r->z, f, g);
}

/* The 32-byte form: y, with the lowest bit of x in the top bit. */
static void ge_tobytes(u8 *out, const struct ge *p)
{
    u16 zi[LIMBS], x[LIMBS], y[LIMBS];

    fe_invert(zi, p->z);
    fe_mul(x, p->x, zi);
    fe_mul(y, p->y, zi);
    fe_tobytes(out, y);
    out[31] ^= (u8)(fe_isodd(x) << 7);
}

/* The point for a 32-byte form; 0 if there is none. */
static int ge_frombytes(struct ge *p, const u8 *in)
{
    u16 u[LIMBS], v[LIMBS], v3[LIMBS], x[LIMBS], t[LIMBS], one[LIMBS];

    fe_small(one, 1);
    fe_frombytes(p->y, in);
    p->y[LIMBS - 1] &= 0x7FFF;
    fe_sqr(u, p->y);
    fe_mul(v, u, ed_d);
    fe_sub(u, u, one);                  /* u = y^2 - 1 */
    fe_add(v, v, one);                  /* v = d y^2 + 1;  x^2 = u / v */

    fe_sqr(v3, v);
    fe_mul(v3, v3, v);                  /* v^3 */
    fe_sqr(x, v3);
    fe_mul(x, x, v);
    fe_mul(x, x, u);                    /* u v^7 */
    fe_pow22523(x, x);
    fe_mul(x, x, v3);
    fe_mul(x, x, u);                    /* u v^3 (u v^7)^((p - 5) / 8) */

    fe_sqr(t, x);
    fe_mul(t, t, v);                    /* v x^2 must be u or -u */
    fe_sub(v3, t, u);
    if (!fe_iszero(v3)) {
        fe_add(v3, t, u);
        if (!fe_iszero(v3))
            return 0;
        fe_mul(x, x, ed_sqrtm1);
    }
    if (fe_isodd(x) != (in[31] >> 7)) {
        if (fe_iszero(x))
            return 0;
        fe_neg(x, x);
    }
    fe_copy(p->x, x);
    fe_small(p->z, 1);
    fe_mul(p->t, p->x, p->y);
    return 1;
}

/* The multiples 0..15 of the two points being multiplied: 4 KB. Built into
 * the BlueSCSI's firmware (firmware/), which has no memory to spare, they
 * live in a buffer the firmware lends instead. */
#ifdef SSHOS_SCRATCH
extern unsigned char *sshos_scratch;
#define table_a ((struct ge *)(void *)sshos_scratch)
#define table_b (table_a + 16)
#else
static struct ge table_a[16], table_b[16];
#endif

static void make_table(struct ge *table, const struct ge *p)
{
    int i;

    ge_neutral(&table[0]);
    table[1] = *p;
    for (i = 2; i < 16; i++)
        ge_add(&table[i], &table[i - 1], p);
}

/* r = table[index], reading every entry so that the time taken says
 * nothing about index. */
static void pick(struct ge *r, const struct ge *table, u8 index)
{
    u16 *out = (u16 *)r, mask;
    const u16 *in;
    int i, j;

    for (i = 0; i < 4 * LIMBS; i++)
        out[i] = 0;
    for (j = 0; j < 16; j++) {
        mask = (u16)(0 - (((u16)((j ^ index) - 1)) >> 15));
        in = (const u16 *)&table[j];
        for (i = 0; i < 4 * LIMBS; i++)
            out[i] |= in[i] & mask;
    }
}

/* r = a * A + b * B, the scalars a and b being 32 bytes, least significant
 * first. b and B may be NULL for just a * A. Four bits of each scalar are
 * handled per turn: four doublings, then one addition from each table. */
static void ge_multiply(struct ge *r, const u8 *a, const struct ge *A, const u8 *b,
                        const struct ge *B)
{
    struct ge t;
    int i, j;

    make_table(table_a, A);
    if (B)
        make_table(table_b, B);
    ge_neutral(r);
    for (i = 63; i >= 0; i--) {
        for (j = 0; j < 4; j++)
            ge_double(r, r);
        pick(&t, table_a, (a[i >> 1] >> ((i & 1) * 4)) & 15);
        ge_add(r, r, &t);
        if (B) {
            pick(&t, table_b, (b[i >> 1] >> ((i & 1) * 4)) & 15);
            ge_add(r, r, &t);
        }
        if (crypto_idle)
            crypto_idle();
        if (crypto_stop)
            break;
    }
    memset(table_a, 0, 16 * sizeof(struct ge));     /* they may describe a secret */
}

/* --------------------------------------------- numbers modulo the order L -- */

/* out = x modulo L; x is n bytes, least significant first. One bit at a
 * time: double, add the bit, and take L off again if that reached it. */
static void sc_reduce(u8 *out, const u8 *x, int n)
{
    u16 r[LIMBS], t[LIMBS], mask;
    u32 c, d, borrow;
    int i, j;

    fe_small(r, 0);
    for (i = n * 8 - 1; i >= 0; i--) {
        c = (x[i >> 3] >> (i & 7)) & 1;
        for (j = 0; j < LIMBS; j++) {
            c |= (u32)r[j] << 1;
            r[j] = (u16)c;
            c >>= 16;
        }
        borrow = 0;
        for (j = 0; j < LIMBS; j++) {
            d = (u32)r[j] - sc_L[j] - borrow;
            t[j] = (u16)d;
            borrow = (d >> 16) & 1;
        }
        mask = (u16)(borrow - 1);           /* all ones when r had reached L */
        for (j = 0; j < LIMBS; j++)
            r[j] ^= mask & (r[j] ^ t[j]);
    }
    for (j = 0; j < LIMBS; j++) {
        out[2 * j] = (u8)r[j];
        out[2 * j + 1] = (u8)(r[j] >> 8);
    }
}

/* out = (a * b + c) modulo L; all are 32 bytes. */
static void sc_muladd(u8 *out, const u8 *a, const u8 *b, const u8 *c)
{
    u16 la[LIMBS], lb[LIMBS];
    u8 wide[64];
    u32 acc = 0, hi = 0, p;
    int i, k, first, last;

    fe_frombytes(la, a);
    fe_frombytes(lb, b);
    for (k = 0; k < 2 * LIMBS; k++) {
        first = k < LIMBS ? 0 : k - (LIMBS - 1);
        last = k < LIMBS ? k : LIMBS - 1;
        for (i = first; i <= last; i++) {
            p = (u32)la[i] * lb[k - i];
            acc += p;
            if (acc < p)
                hi++;
        }
        if (k < LIMBS) {                    /* add c in as we go */
            p = (u32)c[2 * k] | ((u32)c[2 * k + 1] << 8);
            acc += p;
            if (acc < p)
                hi++;
        }
        wide[2 * k] = (u8)acc;
        wide[2 * k + 1] = (u8)(acc >> 8);
        acc = (acc >> 16) | (hi << 16);
        hi = 0;
    }
    sc_reduce(out, wide, 64);
    memset(la, 0, sizeof la);
    memset(wide, 0, sizeof wide);
}

/* ------------------------------------------------------------ signatures -- */

/* The secret number and the "prefix" that a 32-byte seed stands for. */
static void expand(u8 *h64, const u8 *seed)
{
    struct sha512 s;

    sha512_init(&s);
    sha512_update(&s, seed, 32);
    sha512_finish(&s, h64);
    h64[0] &= 248;
    h64[31] &= 127;
    h64[31] |= 64;
}

void ed25519_public(u8 *pub32, const u8 *seed32)
{
    struct ge base, p;
    u8 h[64];

    expand(h, seed32);
    ge_base(&base);
    ge_multiply(&p, h, &base, NULL, NULL);
    ge_tobytes(pub32, &p);
    memset(h, 0, sizeof h);
}

void ed25519_sign(u8 *sig64, const u8 *msg, u32 len, const u8 *seed32, const u8 *pub32)
{
    struct sha512 s;
    struct ge base, p;
    u8 h[64], r[64], k[64];

    expand(h, seed32);
    sha512_init(&s);
    sha512_update(&s, h + 32, 32);
    sha512_update(&s, msg, len);
    sha512_finish(&s, r);
    sc_reduce(r, r, 64);                    /* the one-time number r */

    ge_base(&base);
    ge_multiply(&p, r, &base, NULL, NULL);
    ge_tobytes(sig64, &p);                  /* R = r * base */

    sha512_init(&s);
    sha512_update(&s, sig64, 32);
    sha512_update(&s, pub32, 32);
    sha512_update(&s, msg, len);
    sha512_finish(&s, k);
    sc_reduce(k, k, 64);
    sc_muladd(sig64 + 32, k, h, r);         /* S = k * secret + r */

    memset(h, 0, sizeof h);
    memset(r, 0, sizeof r);
}

/* 1 if sig64 is pub32's signature of msg. */
int ed25519_verify(const u8 *sig64, const u8 *msg, u32 len, const u8 *pub32)
{
    static const u8 order[32] = {
        0xED, 0xD3, 0xF5, 0x5C, 0x1A, 0x63, 0x12, 0x58, 0xD6, 0x9C, 0xF7, 0xA2, 0xDE, 0xF9,
        0xDE, 0x14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10,
    };
    struct sha512 s;
    struct ge base, a, p;
    u8 k[64], check[32], diff = 0;
    int i;

    /* S must be below L, or one signature could be written in several ways. */
    for (i = 31; i >= 0; i--) {
        if (sig64[32 + i] < order[i])
            break;
        if (sig64[32 + i] > order[i] || i == 0)
            return 0;
    }
    if (!ge_frombytes(&a, pub32))
        return 0;
    fe_neg(a.x, a.x);
    fe_neg(a.t, a.t);

    sha512_init(&s);
    sha512_update(&s, sig64, 32);
    sha512_update(&s, pub32, 32);
    sha512_update(&s, msg, len);
    sha512_finish(&s, k);
    sc_reduce(k, k, 64);

    /* S * base - k * A must be the R in the signature. */
    ge_base(&base);
    ge_multiply(&p, sig64 + 32, &base, k, &a);
    ge_tobytes(check, &p);
    for (i = 0; i < 32; i++)
        diff |= check[i] ^ sig64[i];
    return diff == 0;
}
