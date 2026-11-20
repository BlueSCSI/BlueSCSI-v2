/* SHA-512, which Ed25519 signatures are built on. The 68000 has no 64-bit
 * registers, so each 64-bit word is a pair of 32-bit halves. Only short
 * messages are hashed with it, so it is written for clarity, not speed. */
#include "kernel.h"

typedef struct {
    u32 hi, lo;
} w64;

static const u32 K512[80][2] = {
    {0x428A2F98UL, 0xD728AE22UL}, {0x71374491UL, 0x23EF65CDUL},
    {0xB5C0FBCFUL, 0xEC4D3B2FUL}, {0xE9B5DBA5UL, 0x8189DBBCUL},
    {0x3956C25BUL, 0xF348B538UL}, {0x59F111F1UL, 0xB605D019UL},
    {0x923F82A4UL, 0xAF194F9BUL}, {0xAB1C5ED5UL, 0xDA6D8118UL},
    {0xD807AA98UL, 0xA3030242UL}, {0x12835B01UL, 0x45706FBEUL},
    {0x243185BEUL, 0x4EE4B28CUL}, {0x550C7DC3UL, 0xD5FFB4E2UL},
    {0x72BE5D74UL, 0xF27B896FUL}, {0x80DEB1FEUL, 0x3B1696B1UL},
    {0x9BDC06A7UL, 0x25C71235UL}, {0xC19BF174UL, 0xCF692694UL},
    {0xE49B69C1UL, 0x9EF14AD2UL}, {0xEFBE4786UL, 0x384F25E3UL},
    {0x0FC19DC6UL, 0x8B8CD5B5UL}, {0x240CA1CCUL, 0x77AC9C65UL},
    {0x2DE92C6FUL, 0x592B0275UL}, {0x4A7484AAUL, 0x6EA6E483UL},
    {0x5CB0A9DCUL, 0xBD41FBD4UL}, {0x76F988DAUL, 0x831153B5UL},
    {0x983E5152UL, 0xEE66DFABUL}, {0xA831C66DUL, 0x2DB43210UL},
    {0xB00327C8UL, 0x98FB213FUL}, {0xBF597FC7UL, 0xBEEF0EE4UL},
    {0xC6E00BF3UL, 0x3DA88FC2UL}, {0xD5A79147UL, 0x930AA725UL},
    {0x06CA6351UL, 0xE003826FUL}, {0x14292967UL, 0x0A0E6E70UL},
    {0x27B70A85UL, 0x46D22FFCUL}, {0x2E1B2138UL, 0x5C26C926UL},
    {0x4D2C6DFCUL, 0x5AC42AEDUL}, {0x53380D13UL, 0x9D95B3DFUL},
    {0x650A7354UL, 0x8BAF63DEUL}, {0x766A0ABBUL, 0x3C77B2A8UL},
    {0x81C2C92EUL, 0x47EDAEE6UL}, {0x92722C85UL, 0x1482353BUL},
    {0xA2BFE8A1UL, 0x4CF10364UL}, {0xA81A664BUL, 0xBC423001UL},
    {0xC24B8B70UL, 0xD0F89791UL}, {0xC76C51A3UL, 0x0654BE30UL},
    {0xD192E819UL, 0xD6EF5218UL}, {0xD6990624UL, 0x5565A910UL},
    {0xF40E3585UL, 0x5771202AUL}, {0x106AA070UL, 0x32BBD1B8UL},
    {0x19A4C116UL, 0xB8D2D0C8UL}, {0x1E376C08UL, 0x5141AB53UL},
    {0x2748774CUL, 0xDF8EEB99UL}, {0x34B0BCB5UL, 0xE19B48A8UL},
    {0x391C0CB3UL, 0xC5C95A63UL}, {0x4ED8AA4AUL, 0xE3418ACBUL},
    {0x5B9CCA4FUL, 0x7763E373UL}, {0x682E6FF3UL, 0xD6B2B8A3UL},
    {0x748F82EEUL, 0x5DEFB2FCUL}, {0x78A5636FUL, 0x43172F60UL},
    {0x84C87814UL, 0xA1F0AB72UL}, {0x8CC70208UL, 0x1A6439ECUL},
    {0x90BEFFFAUL, 0x23631E28UL}, {0xA4506CEBUL, 0xDE82BDE9UL},
    {0xBEF9A3F7UL, 0xB2C67915UL}, {0xC67178F2UL, 0xE372532BUL},
    {0xCA273ECEUL, 0xEA26619CUL}, {0xD186B8C7UL, 0x21C0C207UL},
    {0xEADA7DD6UL, 0xCDE0EB1EUL}, {0xF57D4F7FUL, 0xEE6ED178UL},
    {0x06F067AAUL, 0x72176FBAUL}, {0x0A637DC5UL, 0xA2C898A6UL},
    {0x113F9804UL, 0xBEF90DAEUL}, {0x1B710B35UL, 0x131C471BUL},
    {0x28DB77F5UL, 0x23047D84UL}, {0x32CAAB7BUL, 0x40C72493UL},
    {0x3C9EBE0AUL, 0x15C9BEBCUL}, {0x431D67C4UL, 0x9C100D4CUL},
    {0x4CC5D4BEUL, 0xCB3E42B6UL}, {0x597F299CUL, 0xFC657E2AUL},
    {0x5FCB6FABUL, 0x3AD6FAECUL}, {0x6C44198CUL, 0x4A475817UL},
};

static const u32 H512[8][2] = {
    {0x6A09E667UL, 0xF3BCC908UL}, {0xBB67AE85UL, 0x84CAA73BUL},
    {0x3C6EF372UL, 0xFE94F82BUL}, {0xA54FF53AUL, 0x5F1D36F1UL},
    {0x510E527FUL, 0xADE682D1UL}, {0x9B05688CUL, 0x2B3E6C1FUL},
    {0x1F83D9ABUL, 0xFB41BD6BUL}, {0x5BE0CD19UL, 0x137E2179UL},
};

static w64 add(w64 a, w64 b)
{
    w64 r;

    r.lo = a.lo + b.lo;
    r.hi = a.hi + b.hi + (r.lo < a.lo);
    return r;
}

/* Rotate right by n; n is never 0 or 32 here. */
static w64 rotr(w64 a, int n)
{
    w64 r;
    u32 t;

    if (n > 32) {
        t = a.hi;
        a.hi = a.lo;
        a.lo = t;
        n -= 32;
    }
    r.hi = (a.hi >> n) | (a.lo << (32 - n));
    r.lo = (a.lo >> n) | (a.hi << (32 - n));
    return r;
}

static w64 shr(w64 a, int n)
{
    w64 r;

    r.hi = a.hi >> n;
    r.lo = (a.lo >> n) | (a.hi << (32 - n));
    return r;
}

static w64 xor3(w64 a, w64 b, w64 c)
{
    w64 r;

    r.hi = a.hi ^ b.hi ^ c.hi;
    r.lo = a.lo ^ b.lo ^ c.lo;
    return r;
}

static u32 be32(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

static void block(struct sha512 *s, const u8 *p)
{
    w64 w[80], v[8], t1, t2, x;
    int i, j;

    for (i = 0; i < 16; i++) {
        w[i].hi = be32(p + 8 * i);
        w[i].lo = be32(p + 8 * i + 4);
    }
    for (i = 16; i < 80; i++) {
        t1 = xor3(rotr(w[i - 2], 19), rotr(w[i - 2], 61), shr(w[i - 2], 6));
        t2 = xor3(rotr(w[i - 15], 1), rotr(w[i - 15], 8), shr(w[i - 15], 7));
        w[i] = add(add(t1, w[i - 7]), add(t2, w[i - 16]));
    }
    for (i = 0; i < 8; i++) {
        v[i].hi = s->h[i][0];
        v[i].lo = s->h[i][1];
    }
    for (i = 0; i < 80; i++) {
        /* v[0..7] are a..h */
        x.hi = (v[4].hi & v[5].hi) ^ (~v[4].hi & v[6].hi);
        x.lo = (v[4].lo & v[5].lo) ^ (~v[4].lo & v[6].lo);
        t1 = add(v[7], xor3(rotr(v[4], 14), rotr(v[4], 18), rotr(v[4], 41)));
        t1 = add(t1, x);
        x.hi = K512[i][0];
        x.lo = K512[i][1];
        t1 = add(add(t1, x), w[i]);
        x.hi = (v[0].hi & v[1].hi) ^ (v[0].hi & v[2].hi) ^ (v[1].hi & v[2].hi);
        x.lo = (v[0].lo & v[1].lo) ^ (v[0].lo & v[2].lo) ^ (v[1].lo & v[2].lo);
        t2 = add(xor3(rotr(v[0], 28), rotr(v[0], 34), rotr(v[0], 39)), x);
        for (j = 7; j > 0; j--)
            v[j] = v[j - 1];
        v[4] = add(v[4], t1);
        v[0] = add(t1, t2);
    }
    for (i = 0; i < 8; i++) {
        x.hi = s->h[i][0];
        x.lo = s->h[i][1];
        x = add(x, v[i]);
        s->h[i][0] = x.hi;
        s->h[i][1] = x.lo;
    }
}

void sha512_init(struct sha512 *s)
{
    int i;

    for (i = 0; i < 8; i++) {
        s->h[i][0] = H512[i][0];
        s->h[i][1] = H512[i][1];
    }
    s->total = 0;
    s->used = 0;
}

void sha512_update(struct sha512 *s, const void *data, u32 len)
{
    const u8 *p = data;

    s->total += len;
    while (len) {
        if (s->used == 0 && len >= 128) {
            block(s, p);
            p += 128;
            len -= 128;
            continue;
        }
        s->buf[s->used++] = *p++;
        len--;
        if (s->used == 128) {
            block(s, s->buf);
            s->used = 0;
        }
    }
}

void sha512_finish(struct sha512 *s, u8 *out64)
{
    u32 total = s->total;
    u8 pad[17];
    int i;

    pad[0] = 0x80;
    sha512_update(s, pad, 1);
    pad[0] = 0;
    while (s->used != 112)
        sha512_update(s, pad, 1);
    /* the length in bits, as 128 bits; messages here are far below 512 MB */
    memset(pad, 0, 8);
    sha512_update(s, pad, 8);
    pad[0] = pad[1] = pad[2] = 0;
    pad[3] = (u8)(total >> 29);
    pad[4] = (u8)(total >> 21);
    pad[5] = (u8)(total >> 13);
    pad[6] = (u8)(total >> 5);
    pad[7] = (u8)(total << 3);
    sha512_update(s, pad, 8);
    for (i = 0; i < 8; i++) {
        out64[8 * i] = (u8)(s->h[i][0] >> 24);
        out64[8 * i + 1] = (u8)(s->h[i][0] >> 16);
        out64[8 * i + 2] = (u8)(s->h[i][0] >> 8);
        out64[8 * i + 3] = (u8)s->h[i][0];
        out64[8 * i + 4] = (u8)(s->h[i][1] >> 24);
        out64[8 * i + 5] = (u8)(s->h[i][1] >> 16);
        out64[8 * i + 6] = (u8)(s->h[i][1] >> 8);
        out64[8 * i + 7] = (u8)s->h[i][1];
    }
}
