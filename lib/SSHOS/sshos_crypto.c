/* The slow sums of SSH, done by the BlueSCSI for the Macintosh.
 *
 * The SE's 68000 needs 12 to 20 seconds for each of the four big
 * calculations of an SSH log-in; the BlueSCSI's processor needs a few
 * thousandths of one. This is the part of the BlueSCSI firmware that does
 * them when SSH OS asks, over the SCSI cable they already share: a command
 * of our own on the network adapter, next to the firmware's WiFi commands.
 *
 *   SCSI command 0x1D, six bytes: 1D, request, 0, length high, length low, 0
 *
 *   request 00  HELLO    read 8 bytes: "SSHOS", version, 0, 0. A firmware
 *                        without this answers nothing, and SSH OS then
 *                        does the sums itself.
 *   request 01  X25519   write scalar (32), point (32)
 *   request 02  PUBLIC   write seed (32): the Ed25519 public key for it
 *   request 03  SIGN     write seed (32), public key (32), message
 *   request 04  VERIFY   write signature (64), public key (32), message
 *   request 05  OPEN     write packet number (4), key (64), then an SSH
 *                        packet as it arrived: length (4, still encrypted),
 *                        body, check sum (16)
 *   request 06  SEAL     write packet number (4), key (64), then a packet
 *                        to send: length (4), body
 *   request 10  RESULT   read 1 byte, 1 = done or 0 = refused, then the
 *                        answer of the request before: 32, 32, 64 bytes;
 *                        for VERIFY one byte, 1 = genuine; for OPEN one
 *                        byte, 1 = genuine, then the body in clear; for
 *                        SEAL the packet encrypted, with its check sum
 *
 * OPEN and SEAL are the packets of a session once logged in, in the form
 * chacha20-poly1305@openssh.com gives them: the key is the 64 bytes SSH
 * derives for that direction (the second half hides the length, the first
 * the body and makes the check sum's key), the number is the packet's
 * place in the connection. Nothing is remembered between requests.
 *
 * The arithmetic is SSH OS's own (kernel/x25519.c, ed25519.c, sha512.c),
 * the same files the SE runs and the tests check; nothing here depends on
 * the BlueSCSI. In the firmware, network.c hands command 0x1D to
 * sshos_crypto_job(), which works in the buffer the request arrived in and
 * leaves the answer there: the firmware has some 9 KB of memory free, and
 * none of it to spare for a buffer of our own. For the same reason the
 * tables of the signature sums (4 KB) go into memory the firmware lends,
 * sshos_scratch, and the code itself is linked into flash, not into RAM
 * where the firmware keeps what must be quick.
 *
 * The same file is built into SSH OS itself with -DCOPRO_FAKE, where it
 * stands in for the BlueSCSI so that the hand-off can be tested in the
 * emulator (make test-copro).
 */
#include "sshos_crypto.h"

/* 4 KB of memory, at an even address, for the signature sums' tables; set
 * before a request of that kind. Only looked at where the arithmetic is
 * built with SSHOS_SCRATCH. */
unsigned char *sshos_scratch;

static void move(unsigned char *to, const unsigned char *from, unsigned int n)
{
    while (n--)                 /* to lies below from wherever they overlap */
        *to++ = *from++;
}

/* One packet either way. buf: number (4), key (64), packet. Returns the
 * length of the answer, left at the start of buf. */
static unsigned int packet(int seal, unsigned char *buf, unsigned int len, unsigned int room)
{
    unsigned char *key = buf + 4, *p = buf + SSHOS_PACKET_HEAD;
    unsigned char nonce[12], polykey[32], tag[16], diff = 0;
    unsigned int n, i;

    if (len < SSHOS_PACKET_HEAD + 4 + (seal ? 0 : 16)) {
        buf[0] = 0;
        return 1;
    }
    n = len - SSHOS_PACKET_HEAD - (seal ? 0 : 16);      /* the length and the body */
    for (i = 0; i < 8; i++)
        nonce[i] = 0;
    for (i = 0; i < 4; i++)
        nonce[8 + i] = buf[i];
    for (i = 0; i < 32; i++)
        polykey[i] = 0;
    chacha20(key, nonce, 0, polykey, 32);
    if (seal) {
        if (1 + n + 16 > room) {
            buf[0] = 0;
            return 1;
        }
        chacha20(key + 32, nonce, 0, p, 4);
        chacha20(key, nonce, 1, p + 4, n - 4);
        poly1305(tag, polykey, p, n);
        move(buf + 1, p, n);            /* over the key, which has done its work */
        move(buf + 1 + n, tag, 16);
        n = 1 + n + 16;
    } else {
        poly1305(tag, polykey, p, n);
        for (i = 0; i < 16; i++)
            diff |= (unsigned char)(tag[i] ^ p[n + i]);
        if (!diff) {
            chacha20(key, nonce, 1, p + 4, n - 4);
            move(buf + 2, p + 4, n - 4);
            n = 2 + n - 4;
        } else {
            for (i = 0; i < len; i++)   /* a forgery: none of it goes back */
                buf[i] = 0;
            n = 2;
        }
        buf[1] = (unsigned char)(diff == 0);
    }
    for (i = 0; i < 32; i++)
        polykey[i] = 0;
    buf[0] = 1;
    return n;
}

/* Do one request. buf holds what the Macintosh wrote (len bytes) and has
 * room bytes in all; the answer is left in it, and its length returned:
 * first a byte, 1 = done or 0 = refused, then what the list above says. */
unsigned int sshos_crypto_job(unsigned char request, unsigned char *buf, unsigned int len,
                              unsigned int room)
{
    unsigned char out[64];
    unsigned int n = 0, i;

    if (len > room || room < 1 + 64) {
        if (room)
            buf[0] = 0;
        return room ? 1 : 0;
    }
    switch (request) {
    case SSHOS_X25519:
        if (len != 64)
            break;
        x25519(out, buf, buf + 32);
        n = 32;
        break;
    case SSHOS_PUBLIC:
        if (len != 32)
            break;
        ed25519_public(out, buf);
        n = 32;
        break;
    case SSHOS_SIGN:
        if (len < 64)
            break;
        ed25519_sign(out, buf + 64, len - 64, buf, buf + 32);
        n = 64;
        break;
    case SSHOS_VERIFY:
        if (len < 96)
            break;
        out[0] = (unsigned char)(ed25519_verify(buf, buf + 96, len - 96, buf + 64) == 1);
        n = 1;
        break;
    case SSHOS_OPEN:
        return packet(0, buf, len, room);
    case SSHOS_SEAL:
        return packet(1, buf, len, room);
    }
    for (i = 0; i < len; i++)           /* what was asked may have held a secret */
        buf[i] = 0;
    buf[0] = (unsigned char)(n != 0);
    for (i = 0; i < n; i++) {
        buf[1 + i] = out[i];
        out[i] = 0;
    }
    return 1 + n;
}

/* The eight bytes of HELLO. */
unsigned int sshos_crypto_hello(unsigned char *out)
{
    static const unsigned char hello[8] = {'S', 'S', 'H', 'O', 'S', SSHOS_CRYPTO_VERSION, 0, 0};
    int i;

    for (i = 0; i < 8; i++)
        out[i] = hello[i];
    return 8;
}
