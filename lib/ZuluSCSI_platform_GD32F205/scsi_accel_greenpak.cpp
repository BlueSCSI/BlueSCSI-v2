#include "scsi_accel_greenpak.h"
#include "AzulSCSI_platform.h"
#include <AzulSCSI_log.h>
#include <assert.h>

#ifndef GREENPAK_PLD_IO1

void scsi_accel_greenpak_send(const uint32_t *buf, uint32_t num_words, volatile int *resetFlag)
{
    assert(false);
}

void scsi_accel_greenpak_recv(uint32_t *buf, uint32_t num_words, volatile int *resetFlag)
{
    assert(false);
}

#else

/*********************************************************/
/* Optimized writes to SCSI bus in GREENPAK_PIO mode     */
/*********************************************************/

extern const uint32_t g_scsi_out_byte_to_bop_pld1hi[256];
extern const uint32_t g_scsi_out_byte_to_bop_pld1lo[256];

// Optimized ASM blocks for the SCSI communication subroutine

// Take 8 bits from d and format them for writing
// d is name of data operand, b is bit offset, x is unique label
#define ASM_LOAD_DATA_PLD1HI(d, b, x) \
"    load_data1_" x "_%=: \n" \
"        ubfx    %[tmp1], %[" d "], #" b ", #8 \n" \
"        ldr     %[tmp1], [%[byte_lookup_pld1hi], %[tmp1], lsl #2] \n"

#define ASM_LOAD_DATA_PLD1LO(d, b, x) \
"    load_data1_" x "_%=: \n" \
"        ubfx    %[tmp1], %[" d "], #" b ", #8 \n" \
"        ldr     %[tmp1], [%[byte_lookup_pld1lo], %[tmp1], lsl #2] \n"

// Write data to SCSI port
#define ASM_SEND_DATA(x) \
"    send_data" x "_%=: \n" \
"        str     %[tmp1], [%[out_port_bop]] \n"

// Wait for ACK to be low or REQ to be high.
// The external logic will set REQ low when PLD_IO1 is toggled and
// back high as soon as ACK goes low. New data can be written to
// GPIO as soon as ACK goes low. If an interrupt happens in the
// middle, we may miss the ACK pulse and should check REQ.
#define ASM_WAIT_DONE(x) \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], wait_done_" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], wait_done_" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], wait_done_" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], wait_done_" x "_%= \n" \
"    wait_req_inactive_" x "_%=: \n" \
"        ldr     %[tmp2], [%[req_pin_bb]] \n" \
"        cbnz    %[tmp2], wait_done_" x "_%= \n" \
"        ldr     %[tmp2], [%[reset_flag]] \n" \
"        cbnz    %[tmp2], wait_done_" x "_%= \n" \
"        b.n     wait_req_inactive_" x "_%= \n" \
"    wait_done_" x "_%=: \n"

// Send bytes to SCSI bus using the asynchronous handshake mechanism
// Takes 4 bytes at a time for sending from buf.
void scsi_accel_greenpak_send(const uint32_t *buf, uint32_t num_words, volatile int *resetFlag)
{
    volatile uint32_t *out_port_bop = (volatile uint32_t*)&GPIO_BOP(SCSI_OUT_PORT);
    uint32_t ack_pin_bb = PERIPH_BB_BASE + (((uint32_t)&GPIO_ISTAT(SCSI_ACK_PORT)) - APB1_BUS_BASE) * 32 + SCSI_IN_ACK_IDX * 4;
    uint32_t req_pin_bb = PERIPH_BB_BASE + (((uint32_t)&GPIO_ISTAT(SCSI_OUT_PORT)) - APB1_BUS_BASE) * 32 + SCSI_OUT_REQ_IDX * 4;
    register uint32_t tmp1 = 0;
    register uint32_t tmp2 = 0;
    register uint32_t data = 0;

    // Set REQ pin as input and PLD_IO2 high to enable logic
    GPIO_BC(SCSI_OUT_PORT) = GREENPAK_PLD_IO1;
    gpio_init(SCSI_OUT_PORT, GPIO_MODE_IPU, 0, SCSI_OUT_REQ);
    GPIO_BOP(SCSI_OUT_PORT) = GREENPAK_PLD_IO2;

    asm volatile (
    "   ldr      %[data], [%[buf]], #4 \n" \
        ASM_LOAD_DATA_PLD1HI("data", "0", "first")
        
    "inner_loop_%=: \n" \
        ASM_SEND_DATA("0")
        ASM_LOAD_DATA_PLD1LO("data", "8", "8")
        ASM_WAIT_DONE("0")
        
        ASM_SEND_DATA("8")
        ASM_LOAD_DATA_PLD1HI("data", "16", "16")
        ASM_WAIT_DONE("8")

        ASM_SEND_DATA("16")
        ASM_LOAD_DATA_PLD1LO("data", "24", "24")
        ASM_WAIT_DONE("16")

        ASM_SEND_DATA("24")
    "   ldr      %[data], [%[buf]], #4 \n" \
        ASM_LOAD_DATA_PLD1HI("data", "0", "0")
        ASM_WAIT_DONE("24")

    "   subs     %[num_words], %[num_words], #1 \n" \
    "   bne     inner_loop_%= \n"
    : /* Output */ [tmp1] "+l" (tmp1), [tmp2] "+l" (tmp2), [data] "+r" (data),
                   [buf] "+r" (buf), [num_words] "+r" (num_words)
    : /* Input */ [ack_pin_bb] "r" (ack_pin_bb),
                  [req_pin_bb] "r" (req_pin_bb),
                  [out_port_bop] "r"(out_port_bop),
                  [byte_lookup_pld1hi] "r" (g_scsi_out_byte_to_bop_pld1hi),
                  [byte_lookup_pld1lo] "r" (g_scsi_out_byte_to_bop_pld1lo),
                  [reset_flag] "r" (resetFlag)
    : /* Clobber */ );

    SCSI_RELEASE_DATA_REQ();

    // Disable external logic and set REQ pin as output
    GPIO_BC(SCSI_OUT_PORT) = GREENPAK_PLD_IO2;
    gpio_init(SCSI_OUT_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SCSI_OUT_REQ);
}

/**********************************************/
/* Mapping from data bytes to GPIO BOP values */
/**********************************************/

#define PARITY(n) ((1 ^ (n) ^ ((n)>>1) ^ ((n)>>2) ^ ((n)>>3) ^ ((n)>>4) ^ ((n)>>5) ^ ((n)>>6) ^ ((n)>>7)) & 1)
#define X(n) (\
    ((n & 0x01) ? (SCSI_OUT_DB0 << 16) : SCSI_OUT_DB0) | \
    ((n & 0x02) ? (SCSI_OUT_DB1 << 16) : SCSI_OUT_DB1) | \
    ((n & 0x04) ? (SCSI_OUT_DB2 << 16) : SCSI_OUT_DB2) | \
    ((n & 0x08) ? (SCSI_OUT_DB3 << 16) : SCSI_OUT_DB3) | \
    ((n & 0x10) ? (SCSI_OUT_DB4 << 16) : SCSI_OUT_DB4) | \
    ((n & 0x20) ? (SCSI_OUT_DB5 << 16) : SCSI_OUT_DB5) | \
    ((n & 0x40) ? (SCSI_OUT_DB6 << 16) : SCSI_OUT_DB6) | \
    ((n & 0x80) ? (SCSI_OUT_DB7 << 16) : SCSI_OUT_DB7) | \
    (PARITY(n)  ? (SCSI_OUT_DBP << 16) : SCSI_OUT_DBP) | \
    (GREENPAK_PLD_IO1) \
)
    
const uint32_t g_scsi_out_byte_to_bop_pld1hi[256] =
{
    X(0x00), X(0x01), X(0x02), X(0x03), X(0x04), X(0x05), X(0x06), X(0x07), X(0x08), X(0x09), X(0x0a), X(0x0b), X(0x0c), X(0x0d), X(0x0e), X(0x0f),
    X(0x10), X(0x11), X(0x12), X(0x13), X(0x14), X(0x15), X(0x16), X(0x17), X(0x18), X(0x19), X(0x1a), X(0x1b), X(0x1c), X(0x1d), X(0x1e), X(0x1f),
    X(0x20), X(0x21), X(0x22), X(0x23), X(0x24), X(0x25), X(0x26), X(0x27), X(0x28), X(0x29), X(0x2a), X(0x2b), X(0x2c), X(0x2d), X(0x2e), X(0x2f),
    X(0x30), X(0x31), X(0x32), X(0x33), X(0x34), X(0x35), X(0x36), X(0x37), X(0x38), X(0x39), X(0x3a), X(0x3b), X(0x3c), X(0x3d), X(0x3e), X(0x3f),
    X(0x40), X(0x41), X(0x42), X(0x43), X(0x44), X(0x45), X(0x46), X(0x47), X(0x48), X(0x49), X(0x4a), X(0x4b), X(0x4c), X(0x4d), X(0x4e), X(0x4f),
    X(0x50), X(0x51), X(0x52), X(0x53), X(0x54), X(0x55), X(0x56), X(0x57), X(0x58), X(0x59), X(0x5a), X(0x5b), X(0x5c), X(0x5d), X(0x5e), X(0x5f),
    X(0x60), X(0x61), X(0x62), X(0x63), X(0x64), X(0x65), X(0x66), X(0x67), X(0x68), X(0x69), X(0x6a), X(0x6b), X(0x6c), X(0x6d), X(0x6e), X(0x6f),
    X(0x70), X(0x71), X(0x72), X(0x73), X(0x74), X(0x75), X(0x76), X(0x77), X(0x78), X(0x79), X(0x7a), X(0x7b), X(0x7c), X(0x7d), X(0x7e), X(0x7f),
    X(0x80), X(0x81), X(0x82), X(0x83), X(0x84), X(0x85), X(0x86), X(0x87), X(0x88), X(0x89), X(0x8a), X(0x8b), X(0x8c), X(0x8d), X(0x8e), X(0x8f),
    X(0x90), X(0x91), X(0x92), X(0x93), X(0x94), X(0x95), X(0x96), X(0x97), X(0x98), X(0x99), X(0x9a), X(0x9b), X(0x9c), X(0x9d), X(0x9e), X(0x9f),
    X(0xa0), X(0xa1), X(0xa2), X(0xa3), X(0xa4), X(0xa5), X(0xa6), X(0xa7), X(0xa8), X(0xa9), X(0xaa), X(0xab), X(0xac), X(0xad), X(0xae), X(0xaf),
    X(0xb0), X(0xb1), X(0xb2), X(0xb3), X(0xb4), X(0xb5), X(0xb6), X(0xb7), X(0xb8), X(0xb9), X(0xba), X(0xbb), X(0xbc), X(0xbd), X(0xbe), X(0xbf),
    X(0xc0), X(0xc1), X(0xc2), X(0xc3), X(0xc4), X(0xc5), X(0xc6), X(0xc7), X(0xc8), X(0xc9), X(0xca), X(0xcb), X(0xcc), X(0xcd), X(0xce), X(0xcf),
    X(0xd0), X(0xd1), X(0xd2), X(0xd3), X(0xd4), X(0xd5), X(0xd6), X(0xd7), X(0xd8), X(0xd9), X(0xda), X(0xdb), X(0xdc), X(0xdd), X(0xde), X(0xdf),
    X(0xe0), X(0xe1), X(0xe2), X(0xe3), X(0xe4), X(0xe5), X(0xe6), X(0xe7), X(0xe8), X(0xe9), X(0xea), X(0xeb), X(0xec), X(0xed), X(0xee), X(0xef),
    X(0xf0), X(0xf1), X(0xf2), X(0xf3), X(0xf4), X(0xf5), X(0xf6), X(0xf7), X(0xf8), X(0xf9), X(0xfa), X(0xfb), X(0xfc), X(0xfd), X(0xfe), X(0xff)
};

#undef X

#define X(n) (\
    ((n & 0x01) ? (SCSI_OUT_DB0 << 16) : SCSI_OUT_DB0) | \
    ((n & 0x02) ? (SCSI_OUT_DB1 << 16) : SCSI_OUT_DB1) | \
    ((n & 0x04) ? (SCSI_OUT_DB2 << 16) : SCSI_OUT_DB2) | \
    ((n & 0x08) ? (SCSI_OUT_DB3 << 16) : SCSI_OUT_DB3) | \
    ((n & 0x10) ? (SCSI_OUT_DB4 << 16) : SCSI_OUT_DB4) | \
    ((n & 0x20) ? (SCSI_OUT_DB5 << 16) : SCSI_OUT_DB5) | \
    ((n & 0x40) ? (SCSI_OUT_DB6 << 16) : SCSI_OUT_DB6) | \
    ((n & 0x80) ? (SCSI_OUT_DB7 << 16) : SCSI_OUT_DB7) | \
    (PARITY(n)  ? (SCSI_OUT_DBP << 16) : SCSI_OUT_DBP) | \
    (GREENPAK_PLD_IO1 << 16) \
)
    
const uint32_t g_scsi_out_byte_to_bop_pld1lo[256] =
{
    X(0x00), X(0x01), X(0x02), X(0x03), X(0x04), X(0x05), X(0x06), X(0x07), X(0x08), X(0x09), X(0x0a), X(0x0b), X(0x0c), X(0x0d), X(0x0e), X(0x0f),
    X(0x10), X(0x11), X(0x12), X(0x13), X(0x14), X(0x15), X(0x16), X(0x17), X(0x18), X(0x19), X(0x1a), X(0x1b), X(0x1c), X(0x1d), X(0x1e), X(0x1f),
    X(0x20), X(0x21), X(0x22), X(0x23), X(0x24), X(0x25), X(0x26), X(0x27), X(0x28), X(0x29), X(0x2a), X(0x2b), X(0x2c), X(0x2d), X(0x2e), X(0x2f),
    X(0x30), X(0x31), X(0x32), X(0x33), X(0x34), X(0x35), X(0x36), X(0x37), X(0x38), X(0x39), X(0x3a), X(0x3b), X(0x3c), X(0x3d), X(0x3e), X(0x3f),
    X(0x40), X(0x41), X(0x42), X(0x43), X(0x44), X(0x45), X(0x46), X(0x47), X(0x48), X(0x49), X(0x4a), X(0x4b), X(0x4c), X(0x4d), X(0x4e), X(0x4f),
    X(0x50), X(0x51), X(0x52), X(0x53), X(0x54), X(0x55), X(0x56), X(0x57), X(0x58), X(0x59), X(0x5a), X(0x5b), X(0x5c), X(0x5d), X(0x5e), X(0x5f),
    X(0x60), X(0x61), X(0x62), X(0x63), X(0x64), X(0x65), X(0x66), X(0x67), X(0x68), X(0x69), X(0x6a), X(0x6b), X(0x6c), X(0x6d), X(0x6e), X(0x6f),
    X(0x70), X(0x71), X(0x72), X(0x73), X(0x74), X(0x75), X(0x76), X(0x77), X(0x78), X(0x79), X(0x7a), X(0x7b), X(0x7c), X(0x7d), X(0x7e), X(0x7f),
    X(0x80), X(0x81), X(0x82), X(0x83), X(0x84), X(0x85), X(0x86), X(0x87), X(0x88), X(0x89), X(0x8a), X(0x8b), X(0x8c), X(0x8d), X(0x8e), X(0x8f),
    X(0x90), X(0x91), X(0x92), X(0x93), X(0x94), X(0x95), X(0x96), X(0x97), X(0x98), X(0x99), X(0x9a), X(0x9b), X(0x9c), X(0x9d), X(0x9e), X(0x9f),
    X(0xa0), X(0xa1), X(0xa2), X(0xa3), X(0xa4), X(0xa5), X(0xa6), X(0xa7), X(0xa8), X(0xa9), X(0xaa), X(0xab), X(0xac), X(0xad), X(0xae), X(0xaf),
    X(0xb0), X(0xb1), X(0xb2), X(0xb3), X(0xb4), X(0xb5), X(0xb6), X(0xb7), X(0xb8), X(0xb9), X(0xba), X(0xbb), X(0xbc), X(0xbd), X(0xbe), X(0xbf),
    X(0xc0), X(0xc1), X(0xc2), X(0xc3), X(0xc4), X(0xc5), X(0xc6), X(0xc7), X(0xc8), X(0xc9), X(0xca), X(0xcb), X(0xcc), X(0xcd), X(0xce), X(0xcf),
    X(0xd0), X(0xd1), X(0xd2), X(0xd3), X(0xd4), X(0xd5), X(0xd6), X(0xd7), X(0xd8), X(0xd9), X(0xda), X(0xdb), X(0xdc), X(0xdd), X(0xde), X(0xdf),
    X(0xe0), X(0xe1), X(0xe2), X(0xe3), X(0xe4), X(0xe5), X(0xe6), X(0xe7), X(0xe8), X(0xe9), X(0xea), X(0xeb), X(0xec), X(0xed), X(0xee), X(0xef),
    X(0xf0), X(0xf1), X(0xf2), X(0xf3), X(0xf4), X(0xf5), X(0xf6), X(0xf7), X(0xf8), X(0xf9), X(0xfa), X(0xfb), X(0xfc), X(0xfd), X(0xfe), X(0xff)
};

#undef X

/*********************************************************/
/* Optimized reads from SCSI bus in GREENPAK_PIO mode    */
/*********************************************************/

// Wait for ACK to go high and back low.
// This indicates that there is a byte ready to be read
// If interrupt occurs in middle, we may miss ACK going high.
// In that case, verify that REQ is already low.
#define ASM_WAIT_ACK(x) \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbnz    %[tmp2], ack_is_high_" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbnz    %[tmp2], ack_is_high_" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbnz    %[tmp2], ack_is_high_" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbnz    %[tmp2], ack_is_high_" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbnz    %[tmp2], ack_is_high_" x "_%= \n" \
"    wait_req_low_" x "_%=: \n" \
"        cpsid   i \n" \
"        ldr     %[tmp2], [%[req_pin_bb]] \n" \
"        cbz     %[tmp2], ack_is_high_" x "_%= \n" \
"        ldr     %[tmp2], [%[reset_flag]] \n" \
"        cbnz    %[tmp2], ack_is_high_" x "_%= \n" \
"        cpsie   i \n" \
"        b.n     wait_req_low_" x "_%= \n" \
"    ack_is_high_" x "_%=: \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], ack_is_low_" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], ack_is_low_" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], ack_is_low_" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], ack_is_low_" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], ack_is_low_" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], ack_is_low_" x "_%= \n" \
"    wait_ack_low_" x "_%=: \n" \
"        cpsid   i \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], ack_is_low_" x "_%= \n" \
"        ldr     %[tmp2], [%[reset_flag]] \n" \
"        cbnz    %[tmp2], ack_is_low_" x "_%= \n" \
"        cpsie   i \n" \
"        b.n     wait_ack_low_" x "_%= \n" \
"    ack_is_low_" x "_%=: \n"

// Prepare for reception of data by loading the next PLD_IO1 value
// and disabling interrupts.
#define ASM_PREP_RECV(l) \
"        mov    %[tmp1], %[" l "] \n" \
"        cpsid  i \n"

// Read GPIO bus, take the data byte and toggle PLD_IO1.
// Note that the PLD_IO1 write is done first to reduce latency, but
// the istat value that is read by next instruction is still the old
// one due to IO port delays. Interrupts must be disabled for this
// sequence to work correctly.
//
// d is the name of register where data is to be stored
// b is the bit offset to store the byte at
// x is unique label
#define ASM_RECV_DATA(d, b, x) \
"    read_data_" x "_%=: \n" \
"        str    %[tmp1], [%[out_port_bop]] \n" \
"        ldr    %[tmp1], [%[in_port_istat]] \n" \
"        ubfx   %[tmp1], %[tmp1], %[data_in_shift], #8 \n" \
"        bfi    %[" d "], %[tmp1], #" b ", #8 \n" \
"        cpsie  i \n"

// Read bytes from SCSI bus using asynchronous handshake mechanism
// Takes 4 bytes at a time.
void scsi_accel_greenpak_recv(uint32_t *buf, uint32_t num_words, volatile int *resetFlag)
{
    volatile uint32_t *out_port_bop = (volatile uint32_t*)&GPIO_BOP(SCSI_OUT_PORT);
    volatile uint32_t *in_port_istat = (volatile uint32_t*)&GPIO_ISTAT(SCSI_IN_PORT);
    uint32_t ack_pin_bb = PERIPH_BB_BASE + (((uint32_t)&GPIO_ISTAT(SCSI_ACK_PORT)) - APB1_BUS_BASE) * 32 + SCSI_IN_ACK_IDX * 4;
    uint32_t req_pin_bb = PERIPH_BB_BASE + (((uint32_t)&GPIO_ISTAT(SCSI_OUT_PORT)) - APB1_BUS_BASE) * 32 + SCSI_OUT_REQ_IDX * 4;
    register uint32_t tmp1 = 0;
    register uint32_t tmp2 = 0;
    register uint32_t data = 0;

    // Last word requires special handling so that hardware doesn't issue new REQ pulse.
    assert(num_words >= 2);
    num_words -= 1;

    // Set PLD_IO3 high to enable read from SCSI bus
    GPIO_BOP(SCSI_OUT_PORT) = GREENPAK_PLD_IO3;

    // Make sure that the previous access has fully completed.
    // E.g. Macintosh can hold ACK low for long time after last byte of block.
    while (SCSI_IN(ACK) && !*resetFlag);

    // Set REQ pin as input and PLD_IO2 high to enable logic
    GPIO_BC(SCSI_OUT_PORT) = GREENPAK_PLD_IO1;
    gpio_init(SCSI_OUT_PORT, GPIO_MODE_IPU, 0, SCSI_OUT_REQ);
    GPIO_BOP(SCSI_OUT_PORT) = GREENPAK_PLD_IO2;

    asm volatile (
    "inner_loop_%=: \n"
        ASM_PREP_RECV("pld1_hi")
        ASM_WAIT_ACK("0")
        ASM_RECV_DATA("data", "0", "0")
        
        ASM_PREP_RECV("pld1_lo")
        ASM_WAIT_ACK("8")
        ASM_RECV_DATA("data", "8", "8")
        
        ASM_PREP_RECV("pld1_hi")
        ASM_WAIT_ACK("16")
        ASM_RECV_DATA("data", "16", "16")
        
        ASM_PREP_RECV("pld1_lo")
        ASM_WAIT_ACK("24")
        ASM_RECV_DATA("data", "24", "24")

    "   mvn      %[data], %[data] \n"
    "   str      %[data], [%[buf]], #4 \n"
    "   subs     %[num_words], %[num_words], #1 \n"
    "   bne     inner_loop_%= \n"

    // Process last word separately to avoid issuing extra REQ pulse at end.
    "recv_last_word_%=: \n"
        ASM_PREP_RECV("pld1_hi")
        ASM_WAIT_ACK("0b")
        ASM_RECV_DATA("data", "0", "0b")
        
        ASM_PREP_RECV("pld1_lo")
        ASM_WAIT_ACK("8b")
        ASM_RECV_DATA("data", "8", "8b")
        
        ASM_PREP_RECV("pld1_hi")
        ASM_WAIT_ACK("16b")
        ASM_RECV_DATA("data", "16", "16b")
        
        ASM_PREP_RECV("pld1_hi")
        ASM_WAIT_ACK("24b")
        ASM_RECV_DATA("data", "24", "24b")

    "   mvn      %[data], %[data] \n"
    "   str      %[data], [%[buf]], #4 \n"

    : /* Output */ [tmp1] "+l" (tmp1), [tmp2] "+l" (tmp2), [data] "+r" (data),
                   [buf] "+r" (buf), [num_words] "+r" (num_words)
    : /* Input */ [ack_pin_bb] "r" (ack_pin_bb),
                  [req_pin_bb] "r" (req_pin_bb),
                  [out_port_bop] "r"(out_port_bop),
                  [in_port_istat] "r" (in_port_istat),
                  [reset_flag] "r" (resetFlag),
                  [data_in_shift] "I" (SCSI_IN_SHIFT),
                  [pld1_lo] "I" (SCSI_OUT_PLD1 << 16),
                  [pld1_hi] "I" (SCSI_OUT_PLD1)
    : /* Clobber */ );

    SCSI_RELEASE_DATA_REQ();

    // Disable external logic and set REQ pin as output
    GPIO_BC(SCSI_OUT_PORT) = GREENPAK_PLD_IO2;
    gpio_init(SCSI_OUT_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SCSI_OUT_REQ);
    GPIO_BC(SCSI_OUT_PORT) = GREENPAK_PLD_IO3;
}

#endif