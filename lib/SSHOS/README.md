# SSH crypto for SSH OS

[SSH OS](https://github.com/naruse/SSH-OS) is a small operating system for
the Macintosh SE whose job is to be an SSH terminal; it reaches the network
through the BlueSCSI's DaynaPORT emulation. An SSH log-in needs four big
calculations (X25519 twice, an Ed25519 signature and a verification). The
SE's 8 MHz 68000 takes 12 to 20 seconds over each, about a minute in all,
and afterwards manages about 7 KB a second of ChaCha20-Poly1305. The
BlueSCSI's processor does each in a few hundredths of a second.

This directory lets a guest hand that work to the BlueSCSI: one vendor
SCSI command, `0x1D`, on the network device, next to the WiFi commands
(`0x1C`). A firmware without it rejects the command, and the guest then
does the work itself; a guest that never sends it sees no difference.

## The command

Six bytes: `1D`, request, `00`, length high, length low, `00`.

| Request | The guest writes | then reads back with request `10` |
|---|---|---|
| `00` hello | nothing; reads 8 bytes: `SSHOS`, version, 0, 0 | |
| `01` X25519 | scalar (32), point (32) | 1, shared point (32) |
| `02` public key | seed (32) | 1, Ed25519 public key (32) |
| `03` sign | seed (32), public key (32), message | 1, signature (64) |
| `04` verify | signature (64), public key (32), message | 1, then 1 = genuine |
| `05` open a packet | its number (4), the key (64), the packet as it arrived | 1, then 1 = genuine, then its body in clear |
| `06` seal a packet | its number (4), the key (64), the packet in clear | 1, the packet encrypted, its check sum (16) |

The first byte read back is 1 if the request was done and 0 if it was
refused. An answer is handed out once. The packets are those of
`chacha20-poly1305@openssh.com`. Nothing is remembered between requests:
the key travels with each packet.

## What it costs the firmware

- No RAM of its own. The work is done in `scsiDev.data`, where the request
  arrived; the tables the signature arithmetic needs are put in the far
  half of that buffer; the code is linked into flash (see the line in
  `rp2040-template.ld`).
- Some stack, a few kilobytes at the deepest.
- Nothing runs unless command `0x1D` arrives.

The keys cross the SCSI cable in the clear. Both ends are in the same
machine.

## The files

`x25519.c` (with ChaCha20), `ed25519.c`, `sha512.c`, `poly1305.c` and
`fe25519.h` are SSH OS's own arithmetic, unchanged, written for that
project by the same author and offered here under this project's licence.
`sshos_crypto.c` is the request handler. `kernel.h`, `sshos_types.h` and
the `w_*.c` files only let those sources compile here as they are.

They are checked in the SSH OS repository: against the published test
vectors when built for an ordinary computer, and by an emulated SE logging
in to a real SSH server with every calculation and every packet going
through these requests.
