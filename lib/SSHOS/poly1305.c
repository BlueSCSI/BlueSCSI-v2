/* Poly1305 (RFC 8439): the check sum that goes with ChaCha20 in SSH's
 * chacha20-poly1305 packets. Numbers modulo 2^130 - 5 are kept as 16-bit
 * limbs, least significant first, like everything else for this CPU. */
#include "kernel.h"

#define HL 9            /* limbs of the running value h: 144 bits, of which 132 are used */
#define RL 8            /* limbs of the key part r: 128 bits */

#ifdef __m68k__
void poly_mul(u16 *d, const u16 *h, const u16 *r);
#endif

/* h = h * r modulo 2^130 - 5, left a little above the modulus (below 2^131). */
static void mul_r(u16 *h, const u16 *r)
{
    u16 d[HL + RL];
    u32 c;
    int i;

#ifdef __m68k__
    poly_mul(d, h, r);          /* kernel/polymul.S: the same product, twice as quick */
#else
    u32 acc = 0, hi = 0, p;
    int k, first, last;

    for (k = 0; k < HL + RL - 1; k++) {
        first = k < RL ? 0 : k - (RL - 1);
        last = k < HL ? k : HL - 1;
        for (i = first; i <= last; i++) {
            p = (u32)h[i] * r[k - i];
            acc += p;
            if (acc < p)
                hi++;
        }
        d[k] = (u16)acc;
        acc = (acc >> 16) | (hi << 16);
        hi = 0;
    }
    d[HL + RL - 1] = (u16)acc;
#endif

    /* 2^130 is 5: add five times the part above bit 130 to the part below. */
    c = 0;
    for (i = 0; i < RL; i++) {
        /* two limbs as one long, shifted down by two: cheap on this CPU */
        u32 t = (u16)(((((u32)d[9 + i]) << 16) | d[8 + i]) >> 2);

        c += d[i] + t + (t << 2);
        h[i] = (u16)c;
        c >>= 16;
    }
    h[8] = (u16)(c + (d[8] & 3));
}

void poly1305(u8 *tag16, const u8 *key32, const u8 *msg, u32 len)
{
    u16 h[HL], r[RL], g[HL], mask;
    u8 last[16];
    u32 c;
    int i;

    for (i = 0; i < RL; i++)
        r[i] = (u16)(key32[2 * i] | ((u16)key32[2 * i + 1] << 8));
    r[1] &= 0x0FFF;             /* the "clamp": some bits of r must be zero */
    r[2] &= 0xFFFC;
    r[3] &= 0x0FFF;
    r[4] &= 0xFFFC;
    r[5] &= 0x0FFF;
    r[6] &= 0xFFFC;
    r[7] &= 0x0FFF;
    for (i = 0; i < HL; i++)
        h[i] = 0;

    while (len) {
        const u8 *b = msg;
        u16 high = 1;           /* the 1 added above each full 16-byte block */

        if (len < 16) {
            for (i = 0; i < 16; i++)
                last[i] = (u8)(i < (int)len ? msg[i] : i == (int)len ? 1 : 0);
            b = last;
            high = 0;
            len = 0;
        } else {
            msg += 16;
            len -= 16;
        }
        c = 0;
        for (i = 0; i < 8; i++) {
            c += (u32)h[i] + (u16)(b[2 * i] | ((u16)b[2 * i + 1] << 8));
            h[i] = (u16)c;
            c >>= 16;
        }
        h[8] = (u16)(h[8] + c + high);
        mul_r(h, r);
    }

    /* mul_r left h below 2^130 + 2^129. If h + 5 reaches 2^130, h was at or
     * above the modulus 2^130 - 5, and h + 5 without its bits from 130 up
     * is h minus the modulus. Those top bits never reach the tag, which is
     * 128 bits, so they are simply left in place. */
    c = 5;
    for (i = 0; i < HL; i++) {
        c += h[i];
        g[i] = (u16)c;
        c >>= 16;
    }
    mask = (u16)(0 - (g[8] >> 2));
    for (i = 0; i < HL; i++)
        h[i] ^= mask & (h[i] ^ g[i]);

    /* tag = (h + s) modulo 2^128, s being the second half of the key */
    c = 0;
    for (i = 0; i < 8; i++) {
        c += (u32)h[i] + (u16)(key32[16 + 2 * i] | ((u16)key32[17 + 2 * i] << 8));
        tag16[2 * i] = (u8)c;
        tag16[2 * i + 1] = (u8)(c >> 8);
        c >>= 16;
    }
}
