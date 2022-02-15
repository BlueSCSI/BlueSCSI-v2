#include "scsi_accel_asm.h"
#include "AzulSCSI_platform.h"

// Optimized ASM blocks for the SCSI communication subroutine

// Take 8 bits from d and format them for writing
// d is name of data operand, b is bit offset, x is unique label
#define ASM_LOAD_DATA(d, b, x) \
"    load_data1_" x "_%=: \n" \
"        ubfx    %[tmp1], %[" d "], #" b ", #8 \n" \
"        ldr     %[tmp1], [%[byte_lookup], %[tmp1], lsl #2] \n"

// Write data to SCSI port and set REQ high
#define ASM_SEND_DATA(x) \
"    send_data" x "_%=: \n" \
"        str     %[tmp1], [%[out_port_bop]] \n"

// Wait for ACK to be high, set REQ low, wait ACK low
#define ASM_HANDSHAKE(x) \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        str     %[tmp2], [%[req_pin_bb]] \n" \
"        cbnz    %[tmp2], req_is_low_now" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        str     %[tmp2], [%[req_pin_bb]] \n" \
"        cbnz    %[tmp2], req_is_low_now" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        str     %[tmp2], [%[req_pin_bb]] \n" \
"        cbnz    %[tmp2], req_is_low_now" x "_%= \n" \
"    wait_ack_inactive" x "_%=: \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        str     %[tmp2], [%[req_pin_bb]] \n" \
"        cbnz    %[tmp2], req_is_low_now" x "_%= \n" \
"        ldr     %[tmp2], [%[reset_flag]] \n" \
"        cbnz    %[tmp2], req_is_low_now" x "_%= \n" \
"        b.n     wait_ack_inactive" x "_%= \n" \
"    req_is_low_now" x "_%=: \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], over_ack_active" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], over_ack_active" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], over_ack_active" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], over_ack_active" x "_%= \n" \
"    wait_ack_active" x "_%=: \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], over_ack_active" x "_%= \n" \
"        ldr     %[tmp2], [%[reset_flag]] \n" \
"        cbnz    %[tmp2], over_ack_active" x "_%= \n" \
"        b.n     wait_ack_active" x "_%= \n" \
"    over_ack_active" x "_%=: \n" \

// Send bytes to SCSI bus using the asynchronous handshake mechanism
// Takes 4 bytes at a time for sending from buf.
// Returns the next buffer pointer.
void scsi_accel_asm_send(const uint32_t *buf, uint32_t num_words, volatile int *resetFlag)
{
    volatile uint32_t *out_port_bop = (volatile uint32_t*)&GPIO_BOP(SCSI_OUT_PORT);
    const uint32_t *byte_lookup = g_scsi_out_byte_to_bop;
    uint32_t ack_pin_bb = PERIPH_BB_BASE + (((uint32_t)&GPIO_ISTAT(SCSI_ACK_PORT)) - APB1_BUS_BASE) * 32 + 12 * 4;
    uint32_t req_pin_bb = PERIPH_BB_BASE + (((uint32_t)out_port_bop) - APB1_BUS_BASE) * 32 + (9 + 16) * 4;
    register uint32_t tmp1 = 0;
    register uint32_t tmp2 = 0;
    register uint32_t data = 0;

    asm volatile (
    "   ldr      %[data], [%[buf]], #4 \n" \
        ASM_LOAD_DATA("data", "0", "first")

    "inner_loop_%=: \n" \
        ASM_SEND_DATA("0")
        ASM_LOAD_DATA("data", "8", "8")
        ASM_HANDSHAKE("0")
        
        ASM_SEND_DATA("8")
        ASM_LOAD_DATA("data", "16", "16")
        ASM_HANDSHAKE("8")

        ASM_SEND_DATA("16")
        ASM_LOAD_DATA("data", "24", "24")
        ASM_HANDSHAKE("16")

        ASM_SEND_DATA("24")
    "   ldr      %[data], [%[buf]], #4 \n" \
        ASM_LOAD_DATA("data", "0", "0")
        ASM_HANDSHAKE("24")

    "   subs     %[num_words], %[num_words], #1 \n" \
    "   bne     inner_loop_%= \n"
    : /* Output */ [tmp1] "+l" (tmp1), [tmp2] "+l" (tmp2), [data] "+r" (data),
                   [buf] "+r" (buf), [num_words] "+r" (num_words)
    : /* Input */ [ack_pin_bb] "r" (ack_pin_bb),
                  [req_pin_bb] "r" (req_pin_bb),
                  [out_port_bop] "r"(out_port_bop),
                  [byte_lookup] "r" (byte_lookup),
                  [reset_flag] "r" (resetFlag)
    : /* Clobber */ );

    SCSI_RELEASE_DATA_REQ();
}