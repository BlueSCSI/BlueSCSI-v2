/*!
    \file    sdcard.c
    \brief   SD card driver

    \version 2015-07-15, V1.0.0, firmware for GD32F20x
    \version 2017-06-05, V2.0.0, firmware for GD32F20x
    \version 2018-10-31, V2.1.0, firmware for GD32F20x
    \version 2020-09-30, V2.2.0, firmware for GD32F20x
    \version 2021-07-30, V2.3.0, firmware for GD32F20x
*/

/*
    Copyright (c) 2021, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#include "gd32_sdio_sdcard.h"
#include "gd32f20x_sdio.h"
#include <stddef.h>

/* card status of R1 definitions */
#define SD_R1_OUT_OF_RANGE                  BIT(31)                   /* command's argument was out of the allowed range */
#define SD_R1_ADDRESS_ERROR                 BIT(30)                   /* misaligned address which did not match the block length */
#define SD_R1_BLOCK_LEN_ERROR               BIT(29)                   /* transferred block length is not allowed */
#define SD_R1_ERASE_SEQ_ERROR               BIT(28)                   /* an error in the sequence of erase commands occurred */
#define SD_R1_ERASE_PARAM                   BIT(27)                   /* an invalid selection of write-blocks for erase occurred */
#define SD_R1_WP_VIOLATION                  BIT(26)                   /* the host attempts to write to a protected block or to the temporary or permanent write protected card */
#define SD_R1_CARD_IS_LOCKED                BIT(25)                   /* the card is locked by the host */
#define SD_R1_LOCK_UNLOCK_FAILED            BIT(24)                   /* a sequence or password error has been detected in lock/unlock card command */
#define SD_R1_COM_CRC_ERROR                 BIT(23)                   /* CRC check of the previous command failed */
#define SD_R1_ILLEGAL_COMMAND               BIT(22)                   /* command not legal for the card state */
#define SD_R1_CARD_ECC_FAILED               BIT(21)                   /* card internal ECC was applied but failed to correct the data */
#define SD_R1_CC_ERROR                      BIT(20)                   /* internal card controller error */
#define SD_R1_GENERAL_UNKNOWN_ERROR         BIT(19)                   /* a general or an unknown error occurred during the operation */
#define SD_R1_CSD_OVERWRITE                 BIT(16)                   /* read only section of the CSD does not match or attempt to reverse the copy or permanent WP bits */
#define SD_R1_WP_ERASE_SKIP                 BIT(15)                   /* partial address space was erased */
#define SD_R1_CARD_ECC_DISABLED             BIT(14)                   /* command has been executed without using the internal ECC */
#define SD_R1_ERASE_RESET                   BIT(13)                   /* an erase sequence was cleared before executing */
#define SD_R1_READY_FOR_DATA                BIT(8)                    /* correspond to buffer empty signaling on the bus */
#define SD_R1_APP_CMD                       BIT(5)                    /* card will expect ACMD */
#define SD_R1_AKE_SEQ_ERROR                 BIT(3)                    /* error in the sequence of the authentication process */
#define SD_R1_ERROR_BITS                    (uint32_t)0xFDF9E008      /* all the R1 error bits */

/* card status of R6 definitions */
#define SD_R6_COM_CRC_ERROR                 BIT(15)                   /* CRC check of the previous command failed */
#define SD_R6_ILLEGAL_COMMAND               BIT(14)                   /* command not legal for the card state */
#define SD_R6_GENERAL_UNKNOWN_ERROR         BIT(13)                   /* a general or an unknown error occurred during the operation */

/* card state */
#define SD_CARDSTATE_IDLE                   ((uint8_t)0x00)           /* card is in idle state */
#define SD_CARDSTATE_READY                  ((uint8_t)0x01)           /* card is in ready state */
#define SD_CARDSTATE_IDENTIFICAT            ((uint8_t)0x02)           /* card is in identificat state */
#define SD_CARDSTATE_STANDBY                ((uint8_t)0x03)           /* card is in standby state */
#define SD_CARDSTATE_TRANSFER               ((uint8_t)0x04)           /* card is in transfer state */
#define SD_CARDSTATE_DATA                   ((uint8_t)0x05)           /* card is in data sending state */
#define SD_CARDSTATE_RECEIVING              ((uint8_t)0x06)           /* card is in receiving state */
#define SD_CARDSTATE_PROGRAMMING            ((uint8_t)0x07)           /* card is in programming state */
#define SD_CARDSTATE_DISCONNECT             ((uint8_t)0x08)           /* card is in disconnect state */
#define SD_CARDSTATE_LOCKED                 ((uint32_t)0x02000000)    /* card is in locked state */

#define SD_CHECK_PATTERN                    ((uint32_t)0x000001AA)    /* check pattern for CMD8 */
#define SD_VOLTAGE_WINDOW                   ((uint32_t)0x80100000)    /* host 3.3V request in ACMD41 */

/* parameters for ACMD41(voltage validation) */
#define SD_HIGH_CAPACITY                    ((uint32_t)0x40000000)    /* high capacity SD memory card */
#define SD_STD_CAPACITY                     ((uint32_t)0x00000000)    /* standard capacity SD memory card */

/* SD bus width, check SCR register */
#define SD_BUS_WIDTH_4BIT                   ((uint32_t)0x00040000)    /* 4-bit width bus mode */
#define SD_BUS_WIDTH_1BIT                   ((uint32_t)0x00010000)    /* 1-bit width bus mode */

/* masks for SCR register */
#define SD_MASK_0_7BITS                     ((uint32_t)0x000000FF)    /* mask [7:0] bits */
#define SD_MASK_8_15BITS                    ((uint32_t)0x0000FF00)    /* mask [15:8] bits */
#define SD_MASK_16_23BITS                   ((uint32_t)0x00FF0000)    /* mask [23:16] bits */
#define SD_MASK_24_31BITS                   ((uint32_t)0xFF000000)    /* mask [31:24] bits */

#define SDIO_FIFO_ADDR                      ((uint32_t)SDIO + 0x80U)  /* address of SDIO_FIFO */
#define SD_FIFOHALF_WORDS                   ((uint32_t)0x00000008)    /* words of FIFO half full/empty */
#define SD_FIFOHALF_BYTES                   ((uint32_t)0x00000020)    /* bytes of FIFO half full/empty */

#define SD_DATATIMEOUT                      ((uint32_t)0xFFFFFFFF)    /* DSM data timeout */
#define SD_MAX_VOLT_VALIDATION              ((uint32_t)0x0000FFFF)    /* the maximum times of voltage validation */
#define SD_MAX_DATA_LENGTH                  ((uint32_t)0x01FFFFFF)    /* the maximum length of data */
#define SD_ALLZERO                          ((uint32_t)0x00000000)    /* all zero */
#define SD_RCA_SHIFT                        ((uint8_t)0x10)           /* RCA shift bits */
#define SD_CLK_DIV_INIT                     ((uint16_t)0x0076)        /* SD clock division in initilization phase */
#define SD_CLK_DIV_TRANS                    ((uint16_t)0x0002)        /* SD clock division in transmission phase */

#define SDIO_MASK_INTC_FLAGS                ((uint32_t)0x00C007FF)    /* mask flags of SDIO_INTC */

uint32_t sd_scr[2] = {0, 0};                                          /* content of SCR register */

static sdio_card_type_enum cardtype = SDIO_STD_CAPACITY_SD_CARD_V1_1; /* SD card type */
static uint32_t sd_csd[4] = {0, 0, 0, 0};                             /* content of CSD register */
static uint32_t sd_cid[4] = {0, 0, 0, 0};                             /* content of CID register */
static uint16_t sd_rca = 0;                                           /* RCA of SD card */
static uint32_t transmode = SD_POLLING_MODE;
static uint32_t totalnumber_bytes = 0, stopcondition = 0;
static __IO sd_error_enum transerror = SD_OK;
static __IO uint32_t transend = 0, number_bytes = 0;

/* check if the command sent error occurs */
static sd_error_enum cmdsent_error_check(void);
/* check if error occurs for R1 response */
static sd_error_enum r1_error_check(uint8_t cmdindex);
/* check if error type for R1 response */
static sd_error_enum r1_error_type_check(uint32_t resp);
/* check if error occurs for R2 response */
static sd_error_enum r2_error_check(void);
/* check if error occurs for R3 response */
static sd_error_enum r3_error_check(void);
/* check if error occurs for R6 response */
static sd_error_enum r6_error_check(uint8_t cmdindex, uint16_t *prca);
/* check if error occurs for R7 response */
static sd_error_enum r7_error_check(void);

/* get the state which the card is in */
static sd_error_enum sd_card_state_get(uint8_t *pcardstate);
/* configure the bus width mode */
static sd_error_enum sd_bus_width_config(uint32_t buswidth);
/* get the SCR of corresponding card */
static sd_error_enum sd_scr_get(uint16_t rca, uint32_t *pscr);
/* get the data block size */
static uint32_t sd_datablocksize_get(uint16_t bytesnumber);

/* configure the DMA for SDIO transfer request */
static void dma_transfer_config(uint32_t *srcbuf, uint32_t bufsize);
/* configure the DMA for SDIO receive request */
static void dma_receive_config(uint32_t *dstbuf, uint32_t bufsize);

unsigned long millis(void);

/*!
    \brief      initialize the SD card and make it in standby state
    \param[in]  none
    \param[out] none
    \retval     sd_error_enum
*/
sd_error_enum sd_init(void)
{
    sd_error_enum status = SD_OK;
    /* configure the RCU and GPIO, deinitialize the SDIO */
    sdio_deinit();

    /* configure the clock and work voltage */
    status = sd_power_on();
    if(SD_OK != status) {
        return status;
    }

    /* initialize the card and get CID and CSD of the card */
    status = sd_card_init();
    if(SD_OK != status) {
        return status;
    }

    /* configure the SDIO peripheral */
    sdio_clock_config(SDIO_SDIOCLKEDGE_RISING, SDIO_CLOCKBYPASS_DISABLE, SDIO_CLOCKPWRSAVE_DISABLE, SD_CLK_DIV_TRANS);
    sdio_bus_mode_set(SDIO_BUSMODE_1BIT);
    sdio_hardware_clock_disable();

    return status;
}

/*!
    \brief      initialize the card and get CID and CSD of the card
    \param[in]  none
    \param[out] none
    \retval     sd_error_enum
*/
sd_error_enum sd_card_init(void)
{
    sd_error_enum status = SD_OK;
    uint16_t temp_rca = 0x01;

    if(SDIO_POWER_OFF == sdio_power_state_get()) {
        status = SD_OPERATION_IMPROPER;
        return status;
    }

    /* the card is not I/O only card */
    if(SDIO_SECURE_DIGITAL_IO_CARD != cardtype) {
        /* send CMD2(SD_CMD_ALL_SEND_CID) to get the CID numbers */
        sdio_csm_disable();
        sdio_command_response_config(SD_CMD_ALL_SEND_CID, (uint32_t)0x0, SDIO_RESPONSETYPE_LONG);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();
        /* check if some error occurs */
        status = r2_error_check();
        if(SD_OK != status) {
            return status;
        }

        /* store the CID numbers */
        sd_cid[0] = sdio_response_get(SDIO_RESPONSE0);
        sd_cid[1] = sdio_response_get(SDIO_RESPONSE1);
        sd_cid[2] = sdio_response_get(SDIO_RESPONSE2);
        sd_cid[3] = sdio_response_get(SDIO_RESPONSE3);
    }

    /* the card is SD memory card or the I/O card has the memory portion */
    if((SDIO_STD_CAPACITY_SD_CARD_V1_1 == cardtype) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == cardtype) ||
            (SDIO_HIGH_CAPACITY_SD_CARD == cardtype) || (SDIO_SECURE_DIGITAL_IO_COMBO_CARD == cardtype)) {
        /* send CMD3(SEND_RELATIVE_ADDR) to ask the card to publish a new relative address (RCA) */
        sdio_csm_disable();
        sdio_command_response_config(SD_CMD_SEND_RELATIVE_ADDR, (uint32_t)0x0, SDIO_RESPONSETYPE_SHORT);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();
        /* check if some error occurs */
        status = r6_error_check(SD_CMD_SEND_RELATIVE_ADDR, &temp_rca);
        if(SD_OK != status) {
            return status;
        }
    }

    if(SDIO_SECURE_DIGITAL_IO_CARD != cardtype) {
        /* the card is not I/O only card */
        sd_rca = temp_rca;

        /* send CMD9(SEND_CSD) to get the addressed card's card-specific data (CSD) */
        sdio_csm_disable();
        sdio_command_response_config(SD_CMD_SEND_CSD, (uint32_t)(temp_rca << SD_RCA_SHIFT), SDIO_RESPONSETYPE_LONG);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();
        /* check if some error occurs */
        status = r2_error_check();
        if(SD_OK != status) {
            return status;
        }

        /* store the card-specific data (CSD) */
        sd_csd[0] = sdio_response_get(SDIO_RESPONSE0);
        sd_csd[1] = sdio_response_get(SDIO_RESPONSE1);
        sd_csd[2] = sdio_response_get(SDIO_RESPONSE2);
        sd_csd[3] = sdio_response_get(SDIO_RESPONSE3);
    }
    return status;
}

/*!
    \brief      configure the clock and the work voltage, and get the card type
    \param[in]  none
    \param[out] none
    \retval     sd_error_enum
*/
sd_error_enum sd_power_on(void)
{
    sd_error_enum status = SD_OK;
    uint32_t sdcardtype = SD_STD_CAPACITY, response = 0, count = 0;
    uint8_t busyflag = 0;

    /* configure the SDIO peripheral */
    sdio_clock_config(SDIO_SDIOCLKEDGE_RISING, SDIO_CLOCKBYPASS_DISABLE, SDIO_CLOCKPWRSAVE_DISABLE, SD_CLK_DIV_INIT);
    sdio_bus_mode_set(SDIO_BUSMODE_1BIT);
    sdio_hardware_clock_disable();
    sdio_power_state_set(SDIO_POWER_ON);
    /* enable SDIO_CLK clock output */
    sdio_clock_enable();

    /* send CMD0(GO_IDLE_STATE) to reset the card */
    sdio_csm_disable();
    sdio_command_response_config(SD_CMD_GO_IDLE_STATE, (uint32_t)0x0, SDIO_RESPONSETYPE_NO);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    /* enable the CSM */
    sdio_csm_enable();

    /* check if command sent error occurs */
    status = cmdsent_error_check();
    if(SD_OK != status) {
        return status;
    }

    /* send CMD8(SEND_IF_COND) to get SD memory card interface condition */
    sdio_csm_disable();
    sdio_command_response_config(SD_CMD_SEND_IF_COND, SD_CHECK_PATTERN, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();

    if(SD_OK == r7_error_check()) {
        /* SD Card 2.0 */
        cardtype = SDIO_STD_CAPACITY_SD_CARD_V2_0;
        sdcardtype = SD_HIGH_CAPACITY;
    }

    while((!busyflag) && (count < SD_MAX_VOLT_VALIDATION)) {
        /* send CMD55(APP_CMD) to indicate next command is application specific command */
        sdio_csm_disable();
        sdio_command_response_config(SD_CMD_APP_CMD, (uint32_t)0x0, SDIO_RESPONSETYPE_SHORT);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();

        
        /* check if some error occurs */
        /* ignoring return value, SD_ILLEGAL_COMMAND, for v1.x spec SD cards */
        status = r1_error_check(SD_CMD_APP_CMD);
        if(SD_OK != status && SD_ILLEGAL_COMMAND != status) {
            return status;
        }
        
        /* send ACMD41(SD_SEND_OP_COND) to get host capacity support information (HCS) and OCR content */
        sdio_csm_disable();
        sdio_command_response_config(SD_APPCMD_SD_SEND_OP_COND, (SD_VOLTAGE_WINDOW | sdcardtype), SDIO_RESPONSETYPE_SHORT);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();
        /* check if some error occurs */

        status = r3_error_check();
        if(SD_OK != status) {
            return status;
        }
        /* get the response and check card power up status bit(busy) */
        response = sdio_response_get(SDIO_RESPONSE0);
        busyflag = (uint8_t)((response >> 31) & (uint32_t)0x01);
        ++count;
    }
    if(count >= SD_MAX_VOLT_VALIDATION) {
        status = SD_VOLTRANGE_INVALID;
        return status;
    }
    if(response &= SD_HIGH_CAPACITY) {
        /* SDHC card */
        cardtype = SDIO_HIGH_CAPACITY_SD_CARD;
    }

    return status;
}

/*!
    \brief      close the power of SDIO
    \param[in]  none
    \param[out] none
    \retval     sd_error_enum
*/
sd_error_enum sd_power_off(void)
{
    sd_error_enum status = SD_OK;
    sdio_power_state_set(SDIO_POWER_OFF);
    return status;
}

/*!
    \brief      configure the bus mode
    \param[in]  busmode: the bus mode
      \arg        SDIO_BUSMODE_1BIT: 1-bit SDIO card bus mode
      \arg        SDIO_BUSMODE_4BIT: 4-bit SDIO card bus mode
      \arg        SDIO_BUSMODE_8BIT: 8-bit SDIO card bus mode (MMC only)
    \param[out] none
    \retval     sd_error_enum
*/
sd_error_enum sd_bus_mode_config(uint32_t busmode)
{
    sd_error_enum status = SD_OK;
    if(SDIO_MULTIMEDIA_CARD == cardtype) {
        /* MMC card doesn't support this function */
        status = SD_FUNCTION_UNSUPPORTED;
        return status;
    } else if((SDIO_STD_CAPACITY_SD_CARD_V1_1 == cardtype) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == cardtype) ||
              (SDIO_HIGH_CAPACITY_SD_CARD == cardtype)) {
        if(SDIO_BUSMODE_8BIT == busmode) {
            /* 8 bit bus mode doesn't support */
            status = SD_FUNCTION_UNSUPPORTED;
            return status;
        } else if(SDIO_BUSMODE_4BIT == busmode) {
            /* configure SD bus width and the SDIO */
            status = sd_bus_width_config(SD_BUS_WIDTH_4BIT);
            if(SD_OK == status) {
                sdio_clock_config(SDIO_SDIOCLKEDGE_RISING, SDIO_CLOCKBYPASS_DISABLE,
                                  SDIO_CLOCKPWRSAVE_DISABLE, SD_CLK_DIV_TRANS);
                sdio_bus_mode_set(busmode);
                sdio_hardware_clock_disable();
            }
        } else if(SDIO_BUSMODE_1BIT == busmode) {
            /* configure SD bus width and the SDIO */
            status = sd_bus_width_config(SD_BUS_WIDTH_1BIT);
            if(SD_OK == status) {
                sdio_clock_config(SDIO_SDIOCLKEDGE_RISING, SDIO_CLOCKBYPASS_DISABLE,
                                  SDIO_CLOCKPWRSAVE_DISABLE, SD_CLK_DIV_TRANS);
                sdio_bus_mode_set(busmode);
                sdio_hardware_clock_disable();
            }
        } else {
            status = SD_PARAMETER_INVALID;
        }
    }
    return status;
}

/*!
    \brief      configure the mode of transmission
    \param[in]  txmode: transfer mode
      \arg        SD_DMA_MODE: DMA mode
      \arg        SD_POLLING_MODE: polling mode
    \param[out] none
    \retval     sd_error_enum
*/
sd_error_enum sd_transfer_mode_config(uint32_t txmode)
{
    sd_error_enum status = SD_OK;
    /* set the transfer mode */
    if((SD_DMA_MODE == txmode) || (SD_POLLING_MODE == txmode)) {
        transmode = txmode;
    } else {
        status = SD_PARAMETER_INVALID;
    }
    return status;
}

/*!
    \brief      read a block data into a buffer from the specified address of a card
    \param[out] preadbuffer: a pointer that store a block read data
    \param[in]  readaddr: the read data address
    \param[in]  blocksize: the data block size
    \retval     sd_error_enum
*/
sd_error_enum sd_block_read(uint32_t *preadbuffer, uint64_t readaddr, uint16_t blocksize, sdio_callback_t callback)
{
    /* initialize the variables */
    sd_error_enum status = SD_OK;
    uint32_t count = 0, align = 0, datablksize = SDIO_DATABLOCKSIZE_1BYTE, *ptempbuff = preadbuffer;

    if(NULL == preadbuffer) {
        status = SD_PARAMETER_INVALID;
        return status;
    }

    transerror = SD_OK;
    transend = 0;
    totalnumber_bytes = 0;
    /* clear all DSM configuration */
    sdio_data_config(0, 0, SDIO_DATABLOCKSIZE_1BYTE);
    sdio_data_transfer_config(SDIO_TRANSMODE_BLOCK, SDIO_TRANSDIRECTION_TOCARD);
    sdio_dsm_disable();
    sdio_dma_disable();

    /* check whether the card is locked */
    if(sdio_response_get(SDIO_RESPONSE0) & SD_CARDSTATE_LOCKED) {
        status = SD_LOCK_UNLOCK_FAILED;
        return status;
    }

    /* blocksize is fixed in 512B for SDHC card */
    if(SDIO_HIGH_CAPACITY_SD_CARD == cardtype) {
        blocksize = 512;
        readaddr /= 512;
    }

    align = blocksize & (blocksize - 1);
    if((blocksize > 0) && (blocksize <= 2048) && (0 == align)) {
        datablksize = sd_datablocksize_get(blocksize);
        /* send CMD16(SET_BLOCKLEN) to set the block length */
        sdio_csm_disable();
        sdio_command_response_config(SD_CMD_SET_BLOCKLEN, (uint32_t)blocksize, SDIO_RESPONSETYPE_SHORT);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();

        /* check if some error occurs */
        status = r1_error_check(SD_CMD_SET_BLOCKLEN);
        if(SD_OK != status) {
            return status;
        }
    } else {
        status = SD_PARAMETER_INVALID;
        return status;
    }

    stopcondition = 0;
    totalnumber_bytes = blocksize;

    /* configure SDIO data transmission */
    sdio_data_config(SD_DATATIMEOUT, totalnumber_bytes, datablksize);
    sdio_data_transfer_config(SDIO_TRANSMODE_BLOCK, SDIO_TRANSDIRECTION_TOSDIO);
    sdio_dsm_enable();

    /* send CMD17(READ_SINGLE_BLOCK) to read a block */
    sdio_csm_disable();
    sdio_command_response_config(SD_CMD_READ_SINGLE_BLOCK, (uint32_t)readaddr, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    /* check if some error occurs */
    status = r1_error_check(SD_CMD_READ_SINGLE_BLOCK);
    if(SD_OK != status) {
        return status;
    }

    if(SD_POLLING_MODE == transmode) {
        /* polling mode */
        while(!sdio_flag_get(SDIO_FLAG_DTCRCERR | SDIO_FLAG_DTTMOUT | SDIO_FLAG_RXORE | SDIO_FLAG_DTBLKEND | SDIO_FLAG_STBITE)) {
            if(RESET != sdio_flag_get(SDIO_FLAG_RFH)) {
                /* at least 8 words can be read in the FIFO */
                for(count = 0; count < SD_FIFOHALF_WORDS; count++) {
                    *(ptempbuff + count) = sdio_data_read();
                }
                ptempbuff += SD_FIFOHALF_WORDS;
            }
        }

        /* whether some error occurs and return it */
        if(RESET != sdio_flag_get(SDIO_FLAG_DTCRCERR)) {
            status = SD_DATA_CRC_ERROR;
            sdio_flag_clear(SDIO_FLAG_DTCRCERR);
            return status;
        } else if(RESET != sdio_flag_get(SDIO_FLAG_DTTMOUT)) {
            status = SD_DATA_TIMEOUT;
            sdio_flag_clear(SDIO_FLAG_DTTMOUT);
            return status;
        } else if(RESET != sdio_flag_get(SDIO_FLAG_RXORE)) {
            status = SD_RX_OVERRUN_ERROR;
            sdio_flag_clear(SDIO_FLAG_RXORE);
            return status;
        } else if(RESET != sdio_flag_get(SDIO_FLAG_STBITE)) {
            status = SD_START_BIT_ERROR;
            sdio_flag_clear(SDIO_FLAG_STBITE);
            return status;
        }
        while(RESET != sdio_flag_get(SDIO_FLAG_RXDTVAL)) {
            *ptempbuff = sdio_data_read();
            ++ptempbuff;
        }
        /* clear the SDIO_INTC flags */
        sdio_flag_clear(SDIO_MASK_INTC_FLAGS);
    } else if(SD_DMA_MODE == transmode) {
        /* DMA mode */
        /* enable the SDIO corresponding interrupts and DMA function */
        sdio_interrupt_enable(SDIO_INT_CCRCERR | SDIO_INT_DTTMOUT | SDIO_INT_RXORE | SDIO_INT_DTEND | SDIO_INT_STBITE);
        sdio_dma_enable();
        dma_receive_config(preadbuffer, blocksize);
        uint32_t start = millis();
        while((RESET == dma_flag_get(DMA1, DMA_CH3, DMA_FLAG_FTF))) {
            if((uint32_t)(millis() - start) > 1000) {
                return SD_ERROR;
            }
            if (callback)
            {
                uint32_t complete = (blocksize - DMA_CHCNT(DMA1, DMA_CH3) * 4);
                callback(complete);
            }
        }
    } else {
        status = SD_PARAMETER_INVALID;
    }
    return status;
}

/*!
    \brief      read multiple blocks data into a buffer from the specified address of a card
    \param[out] preadbuffer: a pointer that store multiple blocks read data
    \param[in]  readaddr: the read data address
    \param[in]  blocksize: the data block size
    \param[in]  blocksnumber: number of blocks that will be read
    \retval     sd_error_enum
*/
sd_error_enum sd_multiblocks_read(uint32_t *preadbuffer, uint64_t readaddr, uint16_t blocksize, uint32_t blocksnumber, sdio_callback_t callback)
{
    /* initialize the variables */
    sd_error_enum status = SD_OK;
    uint32_t count = 0, align = 0, datablksize = SDIO_DATABLOCKSIZE_1BYTE, *ptempbuff = preadbuffer;
    
    if(NULL == preadbuffer) {
        status = SD_PARAMETER_INVALID;
        return status;
    }

    transerror = SD_OK;
    transend = 0;
    totalnumber_bytes = 0;
    /* clear all DSM configuration */
    sdio_data_config(0, 0, SDIO_DATABLOCKSIZE_1BYTE);
    sdio_data_transfer_config(SDIO_TRANSMODE_BLOCK, SDIO_TRANSDIRECTION_TOCARD);
    sdio_dsm_disable();
    sdio_dma_disable();

    /* check whether the card is locked */
    if(sdio_response_get(SDIO_RESPONSE0) & SD_CARDSTATE_LOCKED) {
        status = SD_LOCK_UNLOCK_FAILED;
        return status;
    }

    /* blocksize is fixed in 512B for SDHC card */
    if(SDIO_HIGH_CAPACITY_SD_CARD == cardtype) {
        blocksize = 512;
        readaddr /= 512;
    }

    align = blocksize & (blocksize - 1);
    if((blocksize > 0) && (blocksize <= 2048) && (0 == align)) {
        datablksize = sd_datablocksize_get(blocksize);
        /* send CMD16(SET_BLOCKLEN) to set the block length */
        sdio_csm_disable();
        sdio_command_response_config(SD_CMD_SET_BLOCKLEN, (uint32_t)blocksize, SDIO_RESPONSETYPE_SHORT);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();

        /* check if some error occurs */
        status = r1_error_check(SD_CMD_SET_BLOCKLEN);
        if(SD_OK != status) {
            return status;
        }
    } else {
        status = SD_PARAMETER_INVALID;
        return status;
    }

    if(blocksnumber >= 1) {
        if(blocksnumber * blocksize > SD_MAX_DATA_LENGTH) {
            /* exceeds the maximum length */
            status = SD_PARAMETER_INVALID;
            return status;
        }

        stopcondition = 1;
        totalnumber_bytes = blocksnumber * blocksize;

        /* configure the SDIO data transmission */
        sdio_data_config(SD_DATATIMEOUT, totalnumber_bytes, datablksize);
        sdio_data_transfer_config(SDIO_TRANSMODE_BLOCK, SDIO_TRANSDIRECTION_TOSDIO);
        sdio_dsm_enable();

        /* send CMD18(READ_MULTIPLE_BLOCK) to read multiple blocks */
        sdio_csm_disable();
        sdio_command_response_config(SD_CMD_READ_MULTIPLE_BLOCK, readaddr, SDIO_RESPONSETYPE_SHORT);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();
        /* check if some error occurs */
        status = r1_error_check(SD_CMD_READ_MULTIPLE_BLOCK);
        if(SD_OK != status) {
            return status;
        }

        if(SD_POLLING_MODE == transmode) {
            /* polling mode */
            while(!sdio_flag_get(SDIO_FLAG_DTCRCERR | SDIO_FLAG_DTTMOUT | SDIO_FLAG_RXORE | SDIO_FLAG_DTEND | SDIO_FLAG_STBITE)) {
                if(RESET != sdio_flag_get(SDIO_FLAG_RFH)) {
                    /* at least 8 words can be read in the FIFO */
                    for(count = 0; count < SD_FIFOHALF_WORDS; count++) {
                        *(ptempbuff + count) = sdio_data_read();
                    }
                    ptempbuff += SD_FIFOHALF_WORDS;
                }
            }

            /* whether some error occurs and return it */
            if(RESET != sdio_flag_get(SDIO_FLAG_DTCRCERR)) {
                status = SD_DATA_CRC_ERROR;
                sdio_flag_clear(SDIO_FLAG_DTCRCERR);
                return status;
            } else if(RESET != sdio_flag_get(SDIO_FLAG_DTTMOUT)) {
                status = SD_DATA_TIMEOUT;
                sdio_flag_clear(SDIO_FLAG_DTTMOUT);
                return status;
            } else if(RESET != sdio_flag_get(SDIO_FLAG_RXORE)) {
                status = SD_RX_OVERRUN_ERROR;
                sdio_flag_clear(SDIO_FLAG_RXORE);
                return status;
            } else if(RESET != sdio_flag_get(SDIO_FLAG_STBITE)) {
                status = SD_START_BIT_ERROR;
                sdio_flag_clear(SDIO_FLAG_STBITE);
                return status;
            }
            while(RESET != sdio_flag_get(SDIO_FLAG_RXDTVAL)) {
                *ptempbuff = sdio_data_read();
                ++ptempbuff;
            }

            if(RESET != sdio_flag_get(SDIO_FLAG_DTEND)) {
                if((SDIO_STD_CAPACITY_SD_CARD_V1_1 == cardtype) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == cardtype) ||
                        (SDIO_HIGH_CAPACITY_SD_CARD == cardtype)) {
                    /* send CMD12(STOP_TRANSMISSION) to stop transmission */
                    sdio_csm_disable();
                    sdio_command_response_config(SD_CMD_STOP_TRANSMISSION, (uint32_t)0x0, SDIO_RESPONSETYPE_SHORT);
                    sdio_wait_type_set(SDIO_WAITTYPE_NO);
                    sdio_csm_enable();
                    /* check if some error occurs */
                    status = r1_error_check(SD_CMD_STOP_TRANSMISSION);
                    if(SD_OK != status) {
                        return status;
                    }
                }
            }
            sdio_flag_clear(SDIO_MASK_INTC_FLAGS);
        } else if(SD_DMA_MODE == transmode) {
            /* DMA mode */
            /* enable the SDIO corresponding interrupts and DMA function */
            sdio_interrupt_enable(SDIO_INT_DTCRCERR | SDIO_INT_DTTMOUT | SDIO_INT_RXORE | SDIO_INT_DTEND | SDIO_INT_STBITE);
            sdio_dma_enable();
            dma_receive_config(preadbuffer, totalnumber_bytes);

            uint32_t start = millis();
            while((RESET == dma_flag_get(DMA1, DMA_CH3, DMA_FLAG_FTF))) {
                if((uint32_t)(millis() - start) > 1000) {
                    return SD_ERROR;
                }
                if (callback)
                {
                    uint32_t complete = (totalnumber_bytes - DMA_CHCNT(DMA1, DMA_CH3) * 4);
                    callback(complete);
                }
            }
            while((0 == transend) && (SD_OK == transerror)) {
                if (callback)
                {
                    callback(totalnumber_bytes);
                }
            }
            if(SD_OK != transerror) {
                return transerror;
            }
        } else {
            status = SD_PARAMETER_INVALID;
        }
    }
    return status;
}

/*!
    \brief      write a block data to the specified address of a card
    \param[in]  pwritebuffer: a pointer that store a block data to be transferred
    \param[in]  writeaddr: the read data address
    \param[in]  blocksize: the data block size
    \param[out] none
    \retval     sd_error_enum
*/
sd_error_enum sd_block_write(uint32_t *pwritebuffer, uint64_t writeaddr, uint16_t blocksize, sdio_callback_t callback)
{
    /* initialize the variables */
    sd_error_enum status = SD_OK;
    uint8_t cardstate = 0;
    uint32_t count = 0, align = 0, datablksize = SDIO_DATABLOCKSIZE_1BYTE, *ptempbuff = pwritebuffer;
    uint32_t transbytes = 0, restwords = 0, response = 0;
    
    if(NULL == pwritebuffer) {
        status = SD_PARAMETER_INVALID;
        return status;
    }

    transerror = SD_OK;
    transend = 0;
    totalnumber_bytes = 0;
    /* clear all DSM configuration */
    sdio_data_config(0, 0, SDIO_DATABLOCKSIZE_1BYTE);
    sdio_data_transfer_config(SDIO_TRANSMODE_BLOCK, SDIO_TRANSDIRECTION_TOCARD);
    sdio_dsm_disable();
    sdio_dma_disable();

    /* check whether the card is locked */
    if(sdio_response_get(SDIO_RESPONSE0) & SD_CARDSTATE_LOCKED) {
        status = SD_LOCK_UNLOCK_FAILED;
        return status;
    }

    /* blocksize is fixed in 512B for SDHC card */
    if(SDIO_HIGH_CAPACITY_SD_CARD == cardtype) {
        blocksize = 512;
        writeaddr /= 512;
    }

    align = blocksize & (blocksize - 1);
    if((blocksize > 0) && (blocksize <= 2048) && (0 == align)) {
        datablksize = sd_datablocksize_get(blocksize);
        /* send CMD16(SET_BLOCKLEN) to set the block length */
        sdio_csm_disable();
        sdio_command_response_config(SD_CMD_SET_BLOCKLEN, (uint32_t)blocksize, SDIO_RESPONSETYPE_SHORT);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();

        /* check if some error occurs */
        status = r1_error_check(SD_CMD_SET_BLOCKLEN);
        if(SD_OK != status) {
            return status;
        }
    } else {
        status = SD_PARAMETER_INVALID;
        return status;
    }

    /* send CMD13(SEND_STATUS), addressed card sends its status registers */
    sdio_csm_disable();
    sdio_command_response_config(SD_CMD_SEND_STATUS, (uint32_t)sd_rca << SD_RCA_SHIFT, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    /* check if some error occurs */
    status = r1_error_check(SD_CMD_SEND_STATUS);
    if(SD_OK != status) {
        return status;
    }

    response = sdio_response_get(SDIO_RESPONSE0);
    
    uint32_t start = millis();
    while((0 == (response & SD_R1_READY_FOR_DATA))) {
        /* continue to send CMD13 to polling the state of card until buffer empty or timeout */
        if((uint32_t)(millis() - start) > 1000) {
            return SD_ERROR;
        }

        if (callback)
        {
            callback(0);
        }

        /* send CMD13(SEND_STATUS), addressed card sends its status registers */
        sdio_csm_disable();
        sdio_command_response_config(SD_CMD_SEND_STATUS, (uint32_t)sd_rca << SD_RCA_SHIFT, SDIO_RESPONSETYPE_SHORT);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();
        /* check if some error occurs */
        status = r1_error_check(SD_CMD_SEND_STATUS);
        if(SD_OK != status) {
            return status;
        }
        response = sdio_response_get(SDIO_RESPONSE0);
    }

    /* send CMD24(WRITE_BLOCK) to write a block */
    sdio_csm_disable();
    sdio_command_response_config(SD_CMD_WRITE_BLOCK, writeaddr, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    /* check if some error occurs */
    status = r1_error_check(SD_CMD_WRITE_BLOCK);
    if(SD_OK != status) {
        return status;
    }

    stopcondition = 0;
    totalnumber_bytes = blocksize;

    /* configure the SDIO data transmission */
    sdio_data_config(SD_DATATIMEOUT, totalnumber_bytes, datablksize);
    sdio_data_transfer_config(SDIO_TRANSMODE_BLOCK, SDIO_TRANSDIRECTION_TOCARD);
    sdio_dsm_enable();

    if(SD_POLLING_MODE == transmode) {
        /* polling mode */
        while(!sdio_flag_get(SDIO_FLAG_DTCRCERR | SDIO_FLAG_DTTMOUT | SDIO_FLAG_TXURE | SDIO_FLAG_DTBLKEND | SDIO_FLAG_STBITE)) {
            if(RESET != sdio_flag_get(SDIO_FLAG_TFH)) {
                /* at least 8 words can be written into the FIFO */
                if((totalnumber_bytes - transbytes) < SD_FIFOHALF_BYTES) {
                    restwords = (totalnumber_bytes - transbytes) / 4 + (((totalnumber_bytes - transbytes) % 4 == 0) ? 0 : 1);
                    for(count = 0; count < restwords; count++) {
                        sdio_data_write(*ptempbuff);
                        ++ptempbuff;
                        transbytes += 4;
                    }
                } else {
                    for(count = 0; count < SD_FIFOHALF_WORDS; count++) {
                        sdio_data_write(*(ptempbuff + count));
                    }
                    /* 8 words(32 bytes) has been transferred */
                    ptempbuff += SD_FIFOHALF_WORDS;
                    transbytes += SD_FIFOHALF_BYTES;
                }
            }
        }

        /* whether some error occurs and return it */
        if(RESET != sdio_flag_get(SDIO_FLAG_DTCRCERR)) {
            status = SD_DATA_CRC_ERROR;
            sdio_flag_clear(SDIO_FLAG_DTCRCERR);
            return status;
        } else if(RESET != sdio_flag_get(SDIO_FLAG_DTTMOUT)) {
            status = SD_DATA_TIMEOUT;
            sdio_flag_clear(SDIO_FLAG_DTTMOUT);
            return status;
        } else if(RESET != sdio_flag_get(SDIO_FLAG_TXURE)) {
            status = SD_TX_UNDERRUN_ERROR;
            sdio_flag_clear(SDIO_FLAG_TXURE);
            return status;
        } else if(RESET != sdio_flag_get(SDIO_FLAG_STBITE)) {
            status = SD_START_BIT_ERROR;
            sdio_flag_clear(SDIO_FLAG_STBITE);
            return status;
        }
    } else if(SD_DMA_MODE == transmode) {
        /* DMA mode */
        /* enable the SDIO corresponding interrupts and DMA */
        sdio_interrupt_enable(SDIO_INT_DTCRCERR | SDIO_INT_DTTMOUT | SDIO_INT_TXURE | SDIO_INT_DTEND | SDIO_INT_STBITE);
        dma_transfer_config(pwritebuffer, blocksize);
        sdio_dma_enable();

        uint32_t start = millis();
        while((RESET == dma_flag_get(DMA1, DMA_CH3, DMA_FLAG_FTF))) {
            if((uint32_t)(millis() - start) > 1000) {
                return SD_ERROR;
            }
            if (callback)
            {
                uint32_t complete = (blocksize - DMA_CHCNT(DMA1, DMA_CH3) * 4);
                callback(complete);
            }
        }
        while((0 == transend) && (SD_OK == transerror)) {
            if (callback)
            {
                callback(blocksize);
            }
        }

        if(SD_OK != transerror) {
            return transerror;
        }
    } else {
        status = SD_PARAMETER_INVALID;
        return status;
    }

    /* clear the SDIO_INTC flags */
    sdio_flag_clear(SDIO_MASK_INTC_FLAGS);
    /* get the card state and wait the card is out of programming and receiving state */
    status = sd_card_state_get(&cardstate);
    while((SD_OK == status) && ((SD_CARDSTATE_PROGRAMMING == cardstate) || (SD_CARDSTATE_RECEIVING == cardstate))) {
        if (callback)
        {
            callback(blocksize);
        }
        status = sd_card_state_get(&cardstate);
    }
    return status;
}

/*!
    \brief      write multiple blocks data to the specified address of a card
    \param[in]  pwritebuffer: a pointer that store multiple blocks data to be transferred
    \param[in]  writeaddr: the read data address
    \param[in]  blocksize: the data block size
    \param[in]  blocksnumber: number of blocks that will be written
    \param[out] none
    \retval     sd_error_enum
*/
sd_error_enum sd_multiblocks_write(uint32_t *pwritebuffer, uint64_t writeaddr, uint16_t blocksize, uint32_t blocksnumber, sdio_callback_t callback)
{
    /* initialize the variables */
    sd_error_enum status = SD_OK;
    uint8_t cardstate = 0;
    uint32_t count = 0, align = 0, datablksize = SDIO_DATABLOCKSIZE_1BYTE, *ptempbuff = pwritebuffer;
    uint32_t transbytes = 0, restwords = 0;
    
    if(NULL == pwritebuffer) {
        status = SD_PARAMETER_INVALID;
        return status;
    }

    transerror = SD_OK;
    transend = 0;
    totalnumber_bytes = 0;
    /* clear all DSM configuration */
    sdio_data_config(0, 0, SDIO_DATABLOCKSIZE_1BYTE);
    sdio_data_transfer_config(SDIO_TRANSMODE_BLOCK, SDIO_TRANSDIRECTION_TOCARD);
    sdio_dsm_disable();
    sdio_dma_disable();

    /* check whether the card is locked */
    if(sdio_response_get(SDIO_RESPONSE0) & SD_CARDSTATE_LOCKED) {
        status = SD_LOCK_UNLOCK_FAILED;
        return status;
    }

    /* blocksize is fixed in 512B for SDHC card */
    if(SDIO_HIGH_CAPACITY_SD_CARD == cardtype) {
        blocksize = 512;
        writeaddr /= 512;
    }

    align = blocksize & (blocksize - 1);
    if((blocksize > 0) && (blocksize <= 2048) && (0 == align)) {
        datablksize = sd_datablocksize_get(blocksize);
        /* send CMD16(SET_BLOCKLEN) to set the block length */
        sdio_csm_disable();
        sdio_command_response_config(SD_CMD_SET_BLOCKLEN, (uint32_t)blocksize, SDIO_RESPONSETYPE_SHORT);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();

        /* check if some error occurs */
        status = r1_error_check(SD_CMD_SET_BLOCKLEN);
        if(SD_OK != status) {
            return status;
        }
    } else {
        status = SD_PARAMETER_INVALID;
        return status;
    }

    /* send CMD13(SEND_STATUS), addressed card sends its status registers */
    sdio_csm_disable();
    sdio_command_response_config(SD_CMD_SEND_STATUS, (uint32_t)sd_rca << SD_RCA_SHIFT, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    /* check if some error occurs */
    status = r1_error_check(SD_CMD_SEND_STATUS);
    if(SD_OK != status) {
        return status;
    }

    if(blocksnumber >= 1) {
        if(blocksnumber * blocksize > SD_MAX_DATA_LENGTH) {
            status = SD_PARAMETER_INVALID;
            return status;
        }

        if((SDIO_STD_CAPACITY_SD_CARD_V1_1 == cardtype) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == cardtype) ||
                (SDIO_HIGH_CAPACITY_SD_CARD == cardtype)) {
            /* send CMD55(APP_CMD) to indicate next command is application specific command */
            sdio_csm_disable();
            sdio_command_response_config(SD_CMD_APP_CMD, (uint32_t)sd_rca << SD_RCA_SHIFT, SDIO_RESPONSETYPE_SHORT);
            sdio_wait_type_set(SDIO_WAITTYPE_NO);
            sdio_csm_enable();
            /* check if some error occurs */
            status = r1_error_check(SD_CMD_APP_CMD);
            if(SD_OK != status) {
                return status;
            }

            /* send ACMD23(SET_WR_BLK_ERASE_COUNT) to set the number of write blocks to be preerased before writing */
            sdio_csm_disable();
            sdio_command_response_config(SD_APPCMD_SET_WR_BLK_ERASE_COUNT, blocksnumber, SDIO_RESPONSETYPE_SHORT);
            sdio_wait_type_set(SDIO_WAITTYPE_NO);
            sdio_csm_enable();
            /* check if some error occurs */
            status = r1_error_check(SD_APPCMD_SET_WR_BLK_ERASE_COUNT);
            if(SD_OK != status) {
                return status;
            }
        }
        /* send CMD25(WRITE_MULTIPLE_BLOCK) to continuously write blocks of data */
        sdio_csm_disable();
        sdio_command_response_config(SD_CMD_WRITE_MULTIPLE_BLOCK, writeaddr, SDIO_RESPONSETYPE_SHORT);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();
        /* check if some error occurs */
        status = r1_error_check(SD_CMD_WRITE_MULTIPLE_BLOCK);
        if(SD_OK != status) {
            return status;
        }

        stopcondition = 1;
        totalnumber_bytes = blocksnumber * blocksize;

        /* configure the SDIO data transmission */
        sdio_data_config(SD_DATATIMEOUT, totalnumber_bytes, datablksize);
        sdio_data_transfer_config(SDIO_TRANSMODE_BLOCK, SDIO_TRANSDIRECTION_TOCARD);
        sdio_dsm_enable();

        if(SD_POLLING_MODE == transmode) {
            /* polling mode */
            while(!sdio_flag_get(SDIO_FLAG_DTCRCERR | SDIO_FLAG_DTTMOUT | SDIO_FLAG_TXURE | SDIO_FLAG_DTEND | SDIO_FLAG_STBITE)) {
                if(RESET != sdio_flag_get(SDIO_FLAG_TFH)) {
                    /* at least 8 words can be written into the FIFO */
                    if(!((totalnumber_bytes - transbytes) < SD_FIFOHALF_BYTES)) {
                        for(count = 0; count < SD_FIFOHALF_WORDS; count++) {
                            sdio_data_write(*(ptempbuff + count));
                        }
                        /* 8 words(32 bytes) has been transferred */
                        ptempbuff += SD_FIFOHALF_WORDS;
                        transbytes += SD_FIFOHALF_BYTES;
                    } else {
                        restwords = (totalnumber_bytes - transbytes) / 4 + (((totalnumber_bytes - transbytes) % 4 == 0) ? 0 : 1);
                        for(count = 0; count < restwords; count++) {
                            sdio_data_write(*ptempbuff);
                            ++ptempbuff;
                            transbytes += 4;
                        }
                    }
                }
            }

            /* whether some error occurs and return it */
            if(RESET != sdio_flag_get(SDIO_FLAG_DTCRCERR)) {
                status = SD_DATA_CRC_ERROR;
                sdio_flag_clear(SDIO_FLAG_DTCRCERR);
                return status;
            } else if(RESET != sdio_flag_get(SDIO_FLAG_DTTMOUT)) {
                status = SD_DATA_TIMEOUT;
                sdio_flag_clear(SDIO_FLAG_DTTMOUT);
                return status;
            } else if(RESET != sdio_flag_get(SDIO_FLAG_TXURE)) {
                status = SD_TX_UNDERRUN_ERROR;
                sdio_flag_clear(SDIO_FLAG_TXURE);
                return status;
            } else if(RESET != sdio_flag_get(SDIO_FLAG_STBITE)) {
                status = SD_START_BIT_ERROR;
                sdio_flag_clear(SDIO_FLAG_STBITE);
                return status;
            }

            if(RESET != sdio_flag_get(SDIO_FLAG_DTEND)) {
                if((SDIO_STD_CAPACITY_SD_CARD_V1_1 == cardtype) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == cardtype) ||
                        (SDIO_HIGH_CAPACITY_SD_CARD == cardtype)) {
                    /* send CMD12(STOP_TRANSMISSION) to stop transmission */
                    sdio_csm_disable();
                    sdio_command_response_config(SD_CMD_STOP_TRANSMISSION, (uint32_t)0x0, SDIO_RESPONSETYPE_SHORT);
                    sdio_wait_type_set(SDIO_WAITTYPE_NO);
                    sdio_csm_enable();
                    /* check if some error occurs */
                    status = r1_error_check(SD_CMD_STOP_TRANSMISSION);
                    if(SD_OK != status) {
                        return status;
                    }
                }
            }
            sdio_flag_clear(SDIO_MASK_INTC_FLAGS);
        } else if(SD_DMA_MODE == transmode) {
            /* DMA mode */
            /* enable SDIO corresponding interrupts and DMA */
            sdio_interrupt_enable(SDIO_INT_DTCRCERR | SDIO_INT_DTTMOUT | SDIO_INT_TXURE | SDIO_INT_DTEND | SDIO_INT_STBITE);
            sdio_dma_enable();
            dma_transfer_config(pwritebuffer, totalnumber_bytes);

            uint32_t start = millis();
            while((RESET == dma_flag_get(DMA1, DMA_CH3, DMA_FLAG_FTF))) {
                if((uint32_t)(millis() - start) > 1000) {
                    return SD_ERROR;
                }
                if (callback)
                {
                    uint32_t complete = (totalnumber_bytes - DMA_CHCNT(DMA1, DMA_CH3) * 4);
                    callback(complete);
                }
            }
            while((0 == transend) && (SD_OK == transerror)) {
                if (callback)
                {
                    callback(totalnumber_bytes);
                }
            }
            if(SD_OK != transerror) {
                return transerror;
            }
        } else {
            status = SD_PARAMETER_INVALID;
            return status;
        }
    }

    /* clear the SDIO_INTC flags */
    sdio_flag_clear(SDIO_MASK_INTC_FLAGS);
    /* get the card state and wait the card is out of programming and receiving state */
    status = sd_card_state_get(&cardstate);
    while((SD_OK == status) && ((SD_CARDSTATE_PROGRAMMING == cardstate) || (SD_CARDSTATE_RECEIVING == cardstate))) {
        if (callback)
        {
            callback(totalnumber_bytes);
        }
        status = sd_card_state_get(&cardstate);
    }
    return status;
}

/*!
    \brief      erase a continuous area of a card
    \param[in]  startaddr: the start address
    \param[in]  endaddr: the end address
    \param[out] none
    \retval     sd_error_enum
*/
sd_error_enum sd_erase(uint64_t startaddr, uint64_t endaddr)
{
    /* initialize the variables */
    sd_error_enum status = SD_OK;
    uint32_t count = 0, clkdiv = 0;
    __IO uint32_t delay = 0;
    uint8_t cardstate = 0, tempbyte = 0;
    uint16_t tempccc = 0;

    /* get the card command classes from CSD */
    tempbyte = (uint8_t)((sd_csd[1] & SD_MASK_24_31BITS) >> 24);
    tempccc = (uint16_t)((uint16_t)tempbyte << 4);
    tempbyte = (uint8_t)((sd_csd[1] & SD_MASK_16_23BITS) >> 16);
    tempccc |= (uint16_t)((uint16_t)(tempbyte & 0xF0) >> 4);
    if(0 == (tempccc & SD_CCC_ERASE)) {
        /* don't support the erase command */
        status = SD_FUNCTION_UNSUPPORTED;
        return status;
    }
    clkdiv = (SDIO_CLKCTL & SDIO_CLKCTL_DIV);
    clkdiv += ((SDIO_CLKCTL & SDIO_CLKCTL_DIV8) >> 31) * 256;
    clkdiv += 2;
    delay = 120000 / clkdiv;

    /* check whether the card is locked */
    if(sdio_response_get(SDIO_RESPONSE0) & SD_CARDSTATE_LOCKED) {
        status = SD_LOCK_UNLOCK_FAILED;
        return(status);
    }

    /* blocksize is fixed in 512B for SDHC card */
    if(SDIO_HIGH_CAPACITY_SD_CARD == cardtype) {
        startaddr /= 512;
        endaddr /= 512;
    }

    if((SDIO_STD_CAPACITY_SD_CARD_V1_1 == cardtype) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == cardtype) ||
            (SDIO_HIGH_CAPACITY_SD_CARD == cardtype)) {
        /* send CMD32(ERASE_WR_BLK_START) to set the address of the first write block to be erased */
        sdio_csm_disable();
        sdio_command_response_config(SD_CMD_ERASE_WR_BLK_START, startaddr, SDIO_RESPONSETYPE_SHORT);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();
        /* check if some error occurs */
        status = r1_error_check(SD_CMD_ERASE_WR_BLK_START);
        if(SD_OK != status) {
            return status;
        }

        /* send CMD33(ERASE_WR_BLK_END) to set the address of the last write block of the continuous range to be erased */
        sdio_csm_disable();
        sdio_command_response_config(SD_CMD_ERASE_WR_BLK_END, endaddr, SDIO_RESPONSETYPE_SHORT);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();
        /* check if some error occurs */
        status = r1_error_check(SD_CMD_ERASE_WR_BLK_END);
        if(SD_OK != status) {
            return status;
        }
    }

    /* send CMD38(ERASE) to set the address of the first write block to be erased */
    sdio_csm_disable();
    sdio_command_response_config(SD_CMD_ERASE, (uint32_t)0x0, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    /* check if some error occurs */
    status = r1_error_check(SD_CMD_ERASE);
    if(SD_OK != status) {
        return status;
    }
    /* loop until the counter is reach to the calculated time */
    for(count = 0; count < delay; count++) {
    }
    /* get the card state and wait the card is out of programming and receiving state */
    status = sd_card_state_get(&cardstate);
    while((SD_OK == status) && ((SD_CARDSTATE_PROGRAMMING == cardstate) || (SD_CARDSTATE_RECEIVING == cardstate))) {
        status = sd_card_state_get(&cardstate);
    }
    return status;
}

/*!
    \brief      process all the interrupts which the corresponding flags are set
    \param[in]  none
    \param[out] none
    \retval     sd_error_enum
*/
sd_error_enum sd_interrupts_process(void)
{
    transerror = SD_OK;
    if(RESET != sdio_interrupt_flag_get(SDIO_INT_FLAG_DTEND)) {
        /* send CMD12 to stop data transfer in multiple blocks operation */
        if(1 == stopcondition) {
            transerror = sd_transfer_stop();
        } else {
            transerror = SD_OK;
        }
        sdio_interrupt_flag_clear(SDIO_INT_DTEND);
        /* disable all the interrupts */
        sdio_interrupt_disable(SDIO_INT_DTCRCERR | SDIO_INT_DTTMOUT | SDIO_INT_DTEND | SDIO_INT_STBITE |
                               SDIO_INT_TFH | SDIO_INT_RFH | SDIO_INT_TXURE | SDIO_INT_RXORE);
        transend = 1;
        number_bytes = 0;
        return transerror;
    }

    if(RESET != sdio_interrupt_flag_get(SDIO_INT_FLAG_DTCRCERR)) {
        sdio_interrupt_flag_clear(SDIO_INT_DTCRCERR);
        /* disable all the interrupts */
        sdio_interrupt_disable(SDIO_INT_DTCRCERR | SDIO_INT_DTTMOUT | SDIO_INT_DTEND | SDIO_INT_STBITE |
                               SDIO_INT_TFH | SDIO_INT_RFH | SDIO_INT_TXURE | SDIO_INT_RXORE);
        number_bytes = 0;
        transerror = SD_DATA_CRC_ERROR;
        return transerror;
    }

    if(RESET != sdio_interrupt_flag_get(SDIO_INT_FLAG_DTTMOUT)) {
        sdio_interrupt_flag_clear(SDIO_INT_DTTMOUT);
        /* disable all the interrupts */
        sdio_interrupt_disable(SDIO_INT_DTCRCERR | SDIO_INT_DTTMOUT | SDIO_INT_DTEND | SDIO_INT_STBITE |
                               SDIO_INT_TFH | SDIO_INT_RFH | SDIO_INT_TXURE | SDIO_INT_RXORE);
        number_bytes = 0;
        transerror = SD_DATA_TIMEOUT;
        return transerror;
    }

    if(RESET != sdio_interrupt_flag_get(SDIO_INT_FLAG_STBITE)) {
        sdio_interrupt_flag_clear(SDIO_INT_STBITE);
        /* disable all the interrupts */
        sdio_interrupt_disable(SDIO_INT_DTCRCERR | SDIO_INT_DTTMOUT | SDIO_INT_DTEND | SDIO_INT_STBITE |
                               SDIO_INT_TFH | SDIO_INT_RFH | SDIO_INT_TXURE | SDIO_INT_RXORE);
        number_bytes = 0;
        transerror = SD_START_BIT_ERROR;
        return transerror;
    }

    if(RESET != sdio_interrupt_flag_get(SDIO_INT_FLAG_TXURE)) {
        sdio_interrupt_flag_clear(SDIO_INT_TXURE);
        /* disable all the interrupts */
        sdio_interrupt_disable(SDIO_INT_DTCRCERR | SDIO_INT_DTTMOUT | SDIO_INT_DTEND | SDIO_INT_STBITE |
                               SDIO_INT_TFH | SDIO_INT_RFH | SDIO_INT_TXURE | SDIO_INT_RXORE);
        number_bytes = 0;
        transerror = SD_TX_UNDERRUN_ERROR;
        return transerror;
    }

    if(RESET != sdio_interrupt_flag_get(SDIO_INT_FLAG_RXORE)) {
        sdio_interrupt_flag_clear(SDIO_INT_RXORE);
        /* disable all the interrupts */
        sdio_interrupt_disable(SDIO_INT_DTCRCERR | SDIO_INT_DTTMOUT | SDIO_INT_DTEND | SDIO_INT_STBITE |
                               SDIO_INT_TFH | SDIO_INT_RFH | SDIO_INT_TXURE | SDIO_INT_RXORE);
        number_bytes = 0;
        transerror = SD_RX_OVERRUN_ERROR;
        return transerror;
    }
    return transerror;
}

/*!
    \brief      select or deselect a card
    \param[in]  cardrca: the RCA of a card
    \param[out] none
    \retval     sd_error_enum
*/
sd_error_enum sd_card_select_deselect(uint16_t cardrca)
{
    sd_error_enum status = SD_OK;
    /* send CMD7(SELECT/DESELECT_CARD) to select or deselect the card */
    sdio_csm_disable();
    sdio_command_response_config(SD_CMD_SELECT_DESELECT_CARD, (uint32_t)(cardrca << SD_RCA_SHIFT), SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();

    status = r1_error_check(SD_CMD_SELECT_DESELECT_CARD);
    return status;
}

/*!
    \brief      get the card status
    \param[in]  none
    \param[out] pcardstatus: a pointer that store card status
    \retval     sd_error_enum
*/
sd_error_enum sd_cardstatus_get(uint32_t *pcardstatus)
{
    sd_error_enum status = SD_OK;
    if(NULL == pcardstatus) {
        status = SD_PARAMETER_INVALID;
        return status;
    }

    /* send CMD13(SEND_STATUS), addressed card sends its status register */
    sdio_csm_disable();
    sdio_command_response_config(SD_CMD_SEND_STATUS, (uint32_t)sd_rca << SD_RCA_SHIFT, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    /* check if some error occurs */
    status = r1_error_check(SD_CMD_SEND_STATUS);
    if(SD_OK != status) {
        return status;
    }

    *pcardstatus = sdio_response_get(SDIO_RESPONSE0);
    return status;
}

/*!
    \brief      get the SD card status
    \param[in]  none
    \param[out] psdstatus: a pointer that store SD card status
    \retval     sd_error_enum
*/
sd_error_enum sd_sdstatus_get(uint32_t *psdstatus)
{
    sd_error_enum status = SD_OK;
    uint32_t count = 0;

    /* check whether the card is locked */
    if(sdio_response_get(SDIO_RESPONSE0) & SD_CARDSTATE_LOCKED) {
        status = SD_LOCK_UNLOCK_FAILED;
        return(status);
    }

    /* send CMD16(SET_BLOCKLEN) to set the block length */
    sdio_csm_disable();
    sdio_command_response_config(SD_CMD_SET_BLOCKLEN, (uint32_t)64, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    /* check if some error occurs */
    status = r1_error_check(SD_CMD_SET_BLOCKLEN);
    if(SD_OK != status) {
        return status;
    }

    /* send CMD55(APP_CMD) to indicate next command is application specific command */
    sdio_csm_disable();
    sdio_command_response_config(SD_CMD_APP_CMD, (uint32_t)sd_rca << SD_RCA_SHIFT, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    /* check if some error occurs */
    status = r1_error_check(SD_CMD_APP_CMD);
    if(SD_OK != status) {
        return status;
    }

    /* configure the SDIO data transmission */
    sdio_data_config(SD_DATATIMEOUT, (uint32_t)64, SDIO_DATABLOCKSIZE_64BYTES);
    sdio_data_transfer_config(SDIO_TRANSMODE_BLOCK, SDIO_TRANSDIRECTION_TOSDIO);
    sdio_dsm_enable();

    /* send ACMD13(SD_STATUS) to get the SD status */
    sdio_csm_disable();
    sdio_command_response_config(SD_APPCMD_SD_STATUS, (uint32_t)0x0, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    /* check if some error occurs */
    status = r1_error_check(SD_APPCMD_SD_STATUS);
    if(SD_OK != status) {
        return status;
    }

    while(!sdio_flag_get(SDIO_FLAG_DTCRCERR | SDIO_FLAG_DTTMOUT | SDIO_FLAG_RXORE | SDIO_FLAG_DTBLKEND | SDIO_FLAG_STBITE)) {
        if(RESET != sdio_flag_get(SDIO_FLAG_RFH)) {
            for(count = 0; count < SD_FIFOHALF_WORDS; count++) {
                *(psdstatus + count) = sdio_data_read();
            }
            psdstatus += SD_FIFOHALF_WORDS;
        }
    }

    /* whether some error occurs and return it */
    if(RESET != sdio_flag_get(SDIO_FLAG_DTCRCERR)) {
        status = SD_DATA_CRC_ERROR;
        sdio_flag_clear(SDIO_FLAG_DTCRCERR);
        return status;
    } else if(RESET != sdio_flag_get(SDIO_FLAG_DTTMOUT)) {
        status = SD_DATA_TIMEOUT;
        sdio_flag_clear(SDIO_FLAG_DTTMOUT);
        return status;
    } else if(RESET != sdio_flag_get(SDIO_FLAG_RXORE)) {
        status = SD_RX_OVERRUN_ERROR;
        sdio_flag_clear(SDIO_FLAG_RXORE);
        return status;
    } else if(RESET != sdio_flag_get(SDIO_FLAG_STBITE)) {
        status = SD_START_BIT_ERROR;
        sdio_flag_clear(SDIO_FLAG_STBITE);
        return status;
    }
    while(RESET != sdio_flag_get(SDIO_FLAG_RXDTVAL)) {
        *psdstatus = sdio_data_read();
        ++psdstatus;
    }

    /* clear the SDIO_INTC flags */
    sdio_flag_clear(SDIO_MASK_INTC_FLAGS);
    psdstatus -= 16;
    for(count = 0; count < 16; count++) {
        psdstatus[count] = ((psdstatus[count] & SD_MASK_0_7BITS) << 24) | ((psdstatus[count] & SD_MASK_8_15BITS) << 8) |
                           ((psdstatus[count] & SD_MASK_16_23BITS) >> 8) | ((psdstatus[count] & SD_MASK_24_31BITS) >> 24);
    }
    return status;
}

/*!
    \brief      stop an ongoing data transfer
    \param[in]  none
    \param[out] none
    \retval     sd_error_enum
*/
sd_error_enum sd_transfer_stop(void)
{
    sd_error_enum status = SD_OK;
    /* send CMD12(STOP_TRANSMISSION) to stop transmission */
    sdio_csm_disable();
    sdio_command_response_config(SD_CMD_STOP_TRANSMISSION, (uint32_t)0x0, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    /* check if some error occurs */
    status = r1_error_check(SD_CMD_STOP_TRANSMISSION);
    return status;
}

/*!
    \brief      lock or unlock a card
    \param[in]  lockstate: the lock state
      \arg        SD_LOCK: lock the SD card
      \arg        SD_UNLOCK: unlock the SD card
    \param[out] none
    \retval     sd_error_enum
*/
sd_error_enum sd_lock_unlock(uint8_t lockstate)
{
    sd_error_enum status = SD_OK;
    uint8_t cardstate = 0, tempbyte = 0;
    uint32_t pwd1 = 0, pwd2 = 0, response = 0;
    __IO uint32_t timeout = 0;
    uint16_t tempccc = 0;

    /* get the card command classes from CSD */
    tempbyte = (uint8_t)((sd_csd[1] & SD_MASK_24_31BITS) >> 24);
    tempccc = (uint16_t)((uint16_t)tempbyte << 4);
    tempbyte = (uint8_t)((sd_csd[1] & SD_MASK_16_23BITS) >> 16);
    tempccc |= (uint16_t)((uint16_t)(tempbyte & 0xF0) >> 4);

    if(0 == (tempccc & SD_CCC_LOCK_CARD)) {
        /* don't support the lock command */
        status = SD_FUNCTION_UNSUPPORTED;
        return status;
    }
    /* password pattern */
    pwd1 = (0x01020600 | lockstate);
    pwd2 = 0x03040506;

    /* clear all DSM configuration */
    sdio_data_config(0, 0, SDIO_DATABLOCKSIZE_1BYTE);
    sdio_data_transfer_config(SDIO_TRANSMODE_BLOCK, SDIO_TRANSDIRECTION_TOCARD);
    sdio_dsm_disable();
    sdio_dma_disable();

    /* send CMD16(SET_BLOCKLEN) to set the block length */
    sdio_csm_disable();
    sdio_command_response_config(SD_CMD_SET_BLOCKLEN, (uint32_t)8, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    /* check if some error occurs */
    status = r1_error_check(SD_CMD_SET_BLOCKLEN);
    if(SD_OK != status) {
        return status;
    }

    /* send CMD13(SEND_STATUS), addressed card sends its status register */
    sdio_csm_disable();
    sdio_command_response_config(SD_CMD_SEND_STATUS, (uint32_t)sd_rca << SD_RCA_SHIFT, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    /* check if some error occurs */
    status = r1_error_check(SD_CMD_SEND_STATUS);
    if(SD_OK != status) {
        return status;
    }

    response = sdio_response_get(SDIO_RESPONSE0);
    timeout = 100000;
    while((0 == (response & SD_R1_READY_FOR_DATA)) && (timeout > 0)) {
        /* continue to send CMD13 to polling the state of card until buffer empty or timeout */
        --timeout;
        /* send CMD13(SEND_STATUS), addressed card sends its status registers */
        sdio_csm_disable();
        sdio_command_response_config(SD_CMD_SEND_STATUS, (uint32_t)sd_rca << SD_RCA_SHIFT, SDIO_RESPONSETYPE_SHORT);
        sdio_wait_type_set(SDIO_WAITTYPE_NO);
        sdio_csm_enable();
        /* check if some error occurs */
        status = r1_error_check(SD_CMD_SEND_STATUS);
        if(SD_OK != status) {
            return status;
        }
        response = sdio_response_get(SDIO_RESPONSE0);
    }
    if(0 == timeout) {
        return SD_ERROR;
    }

    /* send CMD42(LOCK_UNLOCK) to set/reset the password or lock/unlock the card */
    sdio_csm_disable();
    sdio_command_response_config(SD_CMD_LOCK_UNLOCK, (uint32_t)0x0, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    /* check if some error occurs */
    status = r1_error_check(SD_CMD_LOCK_UNLOCK);
    if(SD_OK != status) {
        return status;
    }

    response = sdio_response_get(SDIO_RESPONSE0);

    /* configure the SDIO data transmission */
    sdio_data_config(SD_DATATIMEOUT, (uint32_t)8, SDIO_DATABLOCKSIZE_8BYTES);
    sdio_data_transfer_config(SDIO_TRANSMODE_BLOCK, SDIO_TRANSDIRECTION_TOCARD);
    sdio_dsm_enable();

    /* write password pattern */
    sdio_data_write(pwd1);
    sdio_data_write(pwd2);

    /* whether some error occurs and return it */
    if(RESET != sdio_flag_get(SDIO_FLAG_DTCRCERR)) {
        status = SD_DATA_CRC_ERROR;
        sdio_flag_clear(SDIO_FLAG_DTCRCERR);
        return status;
    } else if(RESET != sdio_flag_get(SDIO_FLAG_DTTMOUT)) {
        status = SD_DATA_TIMEOUT;
        sdio_flag_clear(SDIO_FLAG_DTTMOUT);
        return status;
    } else if(RESET != sdio_flag_get(SDIO_FLAG_TXURE)) {
        status = SD_TX_UNDERRUN_ERROR;
        sdio_flag_clear(SDIO_FLAG_TXURE);
        return status;
    } else if(RESET != sdio_flag_get(SDIO_FLAG_STBITE)) {
        status = SD_START_BIT_ERROR;
        sdio_flag_clear(SDIO_FLAG_STBITE);
        return status;
    }

    /* clear the SDIO_INTC flags */
    sdio_flag_clear(SDIO_MASK_INTC_FLAGS);
    /* get the card state and wait the card is out of programming and receiving state */
    status = sd_card_state_get(&cardstate);
    while((SD_OK == status) && ((SD_CARDSTATE_PROGRAMMING == cardstate) || (SD_CARDSTATE_RECEIVING == cardstate))) {
        status = sd_card_state_get(&cardstate);
    }
    return status;
}

/*!
    \brief      get the data transfer state
    \param[in]  none
    \param[out] none
    \retval     sd_error_enum
*/
sd_transfer_state_enum sd_transfer_state_get(void)
{
    sd_transfer_state_enum transtate = SD_NO_TRANSFER;
    if(RESET != sdio_flag_get(SDIO_FLAG_TXRUN | SDIO_FLAG_RXRUN)) {
        transtate = SD_TRANSFER_IN_PROGRESS;
    }
    return transtate;
}

/*!
    \brief      get SD card capacity
    \param[in]  none
    \param[out] none
    \retval     capacity of the card(KB)
*/
uint32_t sd_card_capacity_get(void)
{
    uint8_t tempbyte = 0, devicesize_mult = 0, readblklen = 0;
    uint32_t capacity = 0, devicesize = 0;
    if((SDIO_STD_CAPACITY_SD_CARD_V1_1 == cardtype) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == cardtype)) {
        /* calculate the c_size(device size) */
        tempbyte = (uint8_t)((sd_csd[1] & SD_MASK_8_15BITS) >> 8);
        devicesize = (uint32_t)((uint32_t)(tempbyte & 0x03) << 10);
        tempbyte = (uint8_t)(sd_csd[1] & SD_MASK_0_7BITS);
        devicesize |= (uint32_t)((uint32_t)tempbyte << 2);
        tempbyte = (uint8_t)((sd_csd[2] & SD_MASK_24_31BITS) >> 24);
        devicesize |= (uint32_t)((uint32_t)(tempbyte & 0xC0) >> 6);

        /* calculate the c_size_mult(device size multiplier) */
        tempbyte = (uint8_t)((sd_csd[2] & SD_MASK_16_23BITS) >> 16);
        devicesize_mult = (tempbyte & 0x03) << 1;
        tempbyte = (uint8_t)((sd_csd[2] & SD_MASK_8_15BITS) >> 8);
        devicesize_mult |= (tempbyte & 0x80) >> 7;

        /* calculate the read_bl_len */
        tempbyte = (uint8_t)((sd_csd[1] & SD_MASK_16_23BITS) >> 16);
        readblklen = tempbyte & 0x0F;

        /* capacity = BLOCKNR*BLOCK_LEN, BLOCKNR = (C_SIZE+1)*MULT, MULT = 2^(C_SIZE_MULT+2), BLOCK_LEN = 2^READ_BL_LEN */
        capacity = (devicesize + 1) * (1 << (devicesize_mult + 2));
        capacity *= (1 << readblklen);

        /* change the unit of capacity to KByte */
        capacity /= 1024;
    } else if(SDIO_HIGH_CAPACITY_SD_CARD == cardtype) {
        /* calculate the c_size */
        tempbyte = (uint8_t)(sd_csd[1] & SD_MASK_0_7BITS);
        devicesize = (uint32_t)((uint32_t)(tempbyte & 0x3F) << 16);
        tempbyte = (uint8_t)((sd_csd[2] & SD_MASK_24_31BITS) >> 24);
        devicesize |= (uint32_t)((uint32_t)tempbyte << 8);
        tempbyte = (uint8_t)((sd_csd[2] & SD_MASK_16_23BITS) >> 16);
        devicesize |= (uint32_t)tempbyte;

        /* capacity = (c_size+1)*512KByte */
        capacity = (devicesize + 1) * 512;
    }
    return capacity;
}

sd_error_enum sd_card_information_get_short(sdio_card_type_enum *card_type, uint16_t *card_rca)
{
    *card_type = cardtype;
    *card_rca = sd_rca;
    return SD_OK;
}

/*!
    \brief      get the detailed information of the SD card based on received CID and CSD
    \param[in]  none
    \param[out] pcardinfo: a pointer that store the detailed card information
    \retval     sd_error_enum
*/
sd_error_enum sd_card_information_get(sd_card_info_struct *pcardinfo)
{
    sd_error_enum status = SD_OK;
    uint8_t tempbyte = 0;

    if(NULL == pcardinfo) {
        status = SD_PARAMETER_INVALID;
        return status;
    }

    /* store the card type and RCA */
    pcardinfo->card_type = cardtype;
    pcardinfo->card_rca = sd_rca;

    /* CID byte 0 */
    tempbyte = (uint8_t)((sd_cid[0] & SD_MASK_24_31BITS) >> 24);
    pcardinfo->card_cid.mid = tempbyte;

    /* CID byte 1 */
    tempbyte = (uint8_t)((sd_cid[0] & SD_MASK_16_23BITS) >> 16);
    pcardinfo->card_cid.oid = (uint16_t)((uint16_t)tempbyte << 8);

    /* CID byte 2 */
    tempbyte = (uint8_t)((sd_cid[0] & SD_MASK_8_15BITS) >> 8);
    pcardinfo->card_cid.oid |= (uint16_t)tempbyte;

    /* CID byte 3 */
    tempbyte = (uint8_t)(sd_cid[0] & SD_MASK_0_7BITS);
    pcardinfo->card_cid.pnm0 = (uint32_t)((uint32_t)tempbyte << 24);

    /* CID byte 4 */
    tempbyte = (uint8_t)((sd_cid[1] & SD_MASK_24_31BITS) >> 24);
    pcardinfo->card_cid.pnm0 |= (uint32_t)((uint32_t)tempbyte << 16);

    /* CID byte 5 */
    tempbyte = (uint8_t)((sd_cid[1] & SD_MASK_16_23BITS) >> 16);
    pcardinfo->card_cid.pnm0 |= (uint32_t)((uint32_t)tempbyte << 8);

    /* CID byte 6 */
    tempbyte = (uint8_t)((sd_cid[1] & SD_MASK_8_15BITS) >> 8);
    pcardinfo->card_cid.pnm0 |= (uint32_t)(tempbyte);

    /* CID byte 7 */
    tempbyte = (uint8_t)(sd_cid[1] & SD_MASK_0_7BITS);
    pcardinfo->card_cid.pnm1 = tempbyte;

    /* CID byte 8 */
    tempbyte = (uint8_t)((sd_cid[2] & SD_MASK_24_31BITS) >> 24);
    pcardinfo->card_cid.prv = tempbyte;

    /* CID byte 9 */
    tempbyte = (uint8_t)((sd_cid[2] & SD_MASK_16_23BITS) >> 16);
    pcardinfo->card_cid.psn = (uint32_t)((uint32_t)tempbyte << 24);

    /* CID byte 10 */
    tempbyte = (uint8_t)((sd_cid[2] & SD_MASK_8_15BITS) >> 8);
    pcardinfo->card_cid.psn |= (uint32_t)((uint32_t)tempbyte << 16);

    /* CID byte 11 */
    tempbyte = (uint8_t)(sd_cid[2] & SD_MASK_0_7BITS);
    pcardinfo->card_cid.psn |= (uint32_t)tempbyte;

    /* CID byte 12 */
    tempbyte = (uint8_t)((sd_cid[3] & SD_MASK_24_31BITS) >> 24);
    pcardinfo->card_cid.psn |= (uint32_t)tempbyte;

    /* CID byte 13 */
    tempbyte = (uint8_t)((sd_cid[3] & SD_MASK_16_23BITS) >> 16);
    pcardinfo->card_cid.mdt = (uint16_t)((uint16_t)(tempbyte & 0x0F) << 8);

    /* CID byte 14 */
    tempbyte = (uint8_t)((sd_cid[3] & SD_MASK_8_15BITS) >> 8);
    pcardinfo->card_cid.mdt |= (uint16_t)tempbyte;

    /* CID byte 15 */
    tempbyte = (uint8_t)(sd_cid[3] & SD_MASK_0_7BITS);
    pcardinfo->card_cid.cid_crc = (tempbyte & 0xFE) >> 1;

    /* CSD byte 0 */
    tempbyte = (uint8_t)((sd_csd[0] & SD_MASK_24_31BITS) >> 24);
    pcardinfo->card_csd.csd_struct = (tempbyte & 0xC0) >> 6;

    /* CSD byte 1 */
    tempbyte = (uint8_t)((sd_csd[0] & SD_MASK_16_23BITS) >> 16);
    pcardinfo->card_csd.taac = tempbyte;

    /* CSD byte 2 */
    tempbyte = (uint8_t)((sd_csd[0] & SD_MASK_8_15BITS) >> 8);
    pcardinfo->card_csd.nsac = tempbyte;

    /* CSD byte 3 */
    tempbyte = (uint8_t)(sd_csd[0] & SD_MASK_0_7BITS);
    pcardinfo->card_csd.tran_speed = tempbyte;

    /* CSD byte 4 */
    tempbyte = (uint8_t)((sd_csd[1] & SD_MASK_24_31BITS) >> 24);
    pcardinfo->card_csd.ccc = (uint16_t)((uint16_t)tempbyte << 4);

    /* CSD byte 5 */
    tempbyte = (uint8_t)((sd_csd[1] & SD_MASK_16_23BITS) >> 16);
    pcardinfo->card_csd.ccc |= (uint16_t)((uint16_t)(tempbyte & 0xF0) >> 4);
    pcardinfo->card_csd.read_bl_len = tempbyte & 0x0F;

    /* CSD byte 6 */
    tempbyte = (uint8_t)((sd_csd[1] & SD_MASK_8_15BITS) >> 8);
    pcardinfo->card_csd.read_bl_partial = (tempbyte & 0x80) >> 7;
    pcardinfo->card_csd.write_blk_misalign = (tempbyte & 0x40) >> 6;
    pcardinfo->card_csd.read_blk_misalign = (tempbyte & 0x20) >> 5;
    pcardinfo->card_csd.dsp_imp = (tempbyte & 0x10) >> 4;

    if((SDIO_STD_CAPACITY_SD_CARD_V1_1 == cardtype) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == cardtype)) {
        /* card is SDSC card, CSD version 1.0 */
        pcardinfo->card_csd.c_size = (uint32_t)((uint32_t)(tempbyte & 0x03) << 10);

        /* CSD byte 7 */
        tempbyte = (uint8_t)(sd_csd[1] & SD_MASK_0_7BITS);
        pcardinfo->card_csd.c_size |= (uint32_t)((uint32_t)tempbyte << 2);

        /* CSD byte 8 */
        tempbyte = (uint8_t)((sd_csd[2] & SD_MASK_24_31BITS) >> 24);
        pcardinfo->card_csd.c_size |= (uint32_t)((uint32_t)(tempbyte & 0xC0) >> 6);
        pcardinfo->card_csd.vdd_r_curr_min = (tempbyte & 0x38) >> 3;
        pcardinfo->card_csd.vdd_r_curr_max = tempbyte & 0x07;

        /* CSD byte 9 */
        tempbyte = (uint8_t)((sd_csd[2] & SD_MASK_16_23BITS) >> 16);
        pcardinfo->card_csd.vdd_w_curr_min = (tempbyte & 0xE0) >> 5;
        pcardinfo->card_csd.vdd_w_curr_max = (tempbyte & 0x1C) >> 2;
        pcardinfo->card_csd.c_size_mult = (tempbyte & 0x03) << 1;

        /* CSD byte 10 */
        tempbyte = (uint8_t)((sd_csd[2] & SD_MASK_8_15BITS) >> 8);
        pcardinfo->card_csd.c_size_mult |= (tempbyte & 0x80) >> 7;

        /* calculate the card block size and capacity */
        pcardinfo->card_blocksize = 1 << (pcardinfo->card_csd.read_bl_len);
        pcardinfo->card_capacity = pcardinfo->card_csd.c_size + 1;
        pcardinfo->card_capacity *= (1 << (pcardinfo->card_csd.c_size_mult + 2));
        pcardinfo->card_capacity *= pcardinfo->card_blocksize;
    } else if(SDIO_HIGH_CAPACITY_SD_CARD == cardtype) {
        /* card is SDHC card, CSD version 2.0 */
        /* CSD byte 7 */
        tempbyte = (uint8_t)(sd_csd[1] & SD_MASK_0_7BITS);
        pcardinfo->card_csd.c_size = (uint32_t)((uint32_t)(tempbyte & 0x3F) << 16);

        /* CSD byte 8 */
        tempbyte = (uint8_t)((sd_csd[2] & SD_MASK_24_31BITS) >> 24);
        pcardinfo->card_csd.c_size |= (uint32_t)((uint32_t)tempbyte << 8);

        /* CSD byte 9 */
        tempbyte = (uint8_t)((sd_csd[2] & SD_MASK_16_23BITS) >> 16);
        pcardinfo->card_csd.c_size |= (uint32_t)tempbyte;

        /* calculate the card block size and capacity */
        pcardinfo->card_blocksize = 512;
        pcardinfo->card_capacity = (pcardinfo->card_csd.c_size + 1) * 512 * 1024;
    }

    pcardinfo->card_csd.erase_blk_en = (tempbyte & 0x40) >> 6;
    pcardinfo->card_csd.sector_size = (tempbyte & 0x3F) << 1;

    /* CSD byte 11 */
    tempbyte = (uint8_t)(sd_csd[2] & SD_MASK_0_7BITS);
    pcardinfo->card_csd.sector_size |= (tempbyte & 0x80) >> 7;
    pcardinfo->card_csd.wp_grp_size = (tempbyte & 0x7F);

    /* CSD byte 12 */
    tempbyte = (uint8_t)((sd_csd[3] & SD_MASK_24_31BITS) >> 24);
    pcardinfo->card_csd.wp_grp_enable = (tempbyte & 0x80) >> 7;
    pcardinfo->card_csd.r2w_factor = (tempbyte & 0x1C) >> 2;
    pcardinfo->card_csd.write_bl_len = (tempbyte & 0x03) << 2;

    /* CSD byte 13 */
    tempbyte = (uint8_t)((sd_csd[3] & SD_MASK_16_23BITS) >> 16);
    pcardinfo->card_csd.write_bl_len |= (tempbyte & 0xC0) >> 6;
    pcardinfo->card_csd.write_bl_partial = (tempbyte & 0x20) >> 5;

    /* CSD byte 14 */
    tempbyte = (uint8_t)((sd_csd[3] & SD_MASK_8_15BITS) >> 8);
    pcardinfo->card_csd.file_format_grp = (tempbyte & 0x80) >> 7;
    pcardinfo->card_csd.copy_flag = (tempbyte & 0x40) >> 6;
    pcardinfo->card_csd.perm_write_protect = (tempbyte & 0x20) >> 5;
    pcardinfo->card_csd.tmp_write_protect = (tempbyte & 0x10) >> 4;
    pcardinfo->card_csd.file_format = (tempbyte & 0x0C) >> 2;

    /* CSD byte 15 */
    tempbyte = (uint8_t)(sd_csd[3] & SD_MASK_0_7BITS);
    pcardinfo->card_csd.csd_crc = (tempbyte & 0xFE) >> 1;

    return status;
}

void sd_cid_get(uint8_t *cid)
{
    // SdFat expects the data in big endian format.
    for (int i = 0; i < 16; i++)
    {
        cid[i] = (sd_cid[i / 4] >> (24 - (i % 4) * 8)) & 0xFF;
    }
}

void sd_csd_get(uint8_t *csd)
{
    for (int i = 0; i < 16; i++)
    {
        csd[i] = (sd_csd[i / 4] >> (24 - (i % 4) * 8)) & 0xFF;
    }
}

/*!
    \brief      check if the command sent error occurs
    \param[in]  none
    \param[out] none
    \retval     sd_error_enum
*/
static sd_error_enum cmdsent_error_check(void)
{
    sd_error_enum status = SD_OK;
    __IO uint32_t timeout = 100000;
    /* check command sent flag */
    while((RESET == sdio_flag_get(SDIO_FLAG_CMDSEND)) && (timeout > 0)) {
        --timeout;
    }
    /* command response is timeout */
    if(0 == timeout) {
        status = SD_CMD_RESP_TIMEOUT;
        return status;
    }
    /* if the command is sent, clear the SDIO_INTC flags */
    sdio_flag_clear(SDIO_MASK_INTC_FLAGS);
    return status;
}

/*!
    \brief      check if error type for R1 response
    \param[in]  resp: content of response
    \param[out] none
    \retval     sd_error_enum
*/
static sd_error_enum r1_error_type_check(uint32_t resp)
{
    sd_error_enum status = SD_ERROR;
    /* check which error occurs */
    if(resp & SD_R1_OUT_OF_RANGE) {
        status = SD_OUT_OF_RANGE;
    } else if(resp & SD_R1_ADDRESS_ERROR) {
        status = SD_ADDRESS_ERROR;
    } else if(resp & SD_R1_BLOCK_LEN_ERROR) {
        status = SD_BLOCK_LEN_ERROR;
    } else if(resp & SD_R1_ERASE_SEQ_ERROR) {
        status = SD_ERASE_SEQ_ERROR;
    } else if(resp & SD_R1_ERASE_PARAM) {
        status = SD_ERASE_PARAM;
    } else if(resp & SD_R1_WP_VIOLATION) {
        status = SD_WP_VIOLATION;
    } else if(resp & SD_R1_LOCK_UNLOCK_FAILED) {
        status = SD_LOCK_UNLOCK_FAILED;
    } else if(resp & SD_R1_COM_CRC_ERROR) {
        status = SD_COM_CRC_ERROR;
    } else if(resp & SD_R1_ILLEGAL_COMMAND) {
        status = SD_ILLEGAL_COMMAND;
    } else if(resp & SD_R1_CARD_ECC_FAILED) {
        status = SD_CARD_ECC_FAILED;
    } else if(resp & SD_R1_CC_ERROR) {
        status = SD_CC_ERROR;
    } else if(resp & SD_R1_GENERAL_UNKNOWN_ERROR) {
        status = SD_GENERAL_UNKNOWN_ERROR;
    } else if(resp & SD_R1_CSD_OVERWRITE) {
        status = SD_CSD_OVERWRITE;
    } else if(resp & SD_R1_WP_ERASE_SKIP) {
        status = SD_WP_ERASE_SKIP;
    } else if(resp & SD_R1_CARD_ECC_DISABLED) {
        status = SD_CARD_ECC_DISABLED;
    } else if(resp & SD_R1_ERASE_RESET) {
        status = SD_ERASE_RESET;
    } else if(resp & SD_R1_AKE_SEQ_ERROR) {
        status = SD_AKE_SEQ_ERROR;
    }
    return status;
}

/*!
    \brief      check if error occurs for R1 response
    \param[in]  cmdindex: the index of command
    \param[out] none
    \retval     sd_error_enum
*/
static sd_error_enum r1_error_check(uint8_t cmdindex)
{
    sd_error_enum status = SD_OK;
    uint32_t reg_status = 0, resp_r1 = 0;
    __IO uint32_t timeout = 100000;

    /* store the content of SDIO_STAT */
    reg_status = SDIO_STAT;
    while(!(reg_status & (SDIO_FLAG_CCRCERR | SDIO_FLAG_CMDTMOUT | SDIO_FLAG_CMDRECV)) && (timeout > 0)) {
        reg_status = SDIO_STAT;
        --timeout;
    }
    /* check whether an error or timeout occurs or command response received */
    if(reg_status & SDIO_FLAG_CCRCERR) {
        status = SD_CMD_CRC_ERROR;
        sdio_flag_clear(SDIO_FLAG_CCRCERR);
        return status;
    } else if((reg_status & SDIO_FLAG_CMDTMOUT) || (0 == timeout)) {
        status = SD_CMD_RESP_TIMEOUT;
        sdio_flag_clear(SDIO_FLAG_CMDTMOUT);
        return status;
    }

    /* check whether the last response command index is the desired one */
    if(sdio_command_index_get() != cmdindex) {
        status = SD_ILLEGAL_COMMAND;
        return status;
    }
    /* clear all the SDIO_INTC flags */
    sdio_flag_clear(SDIO_MASK_INTC_FLAGS);
    /* get the SDIO response register 0 for checking */
    resp_r1 = sdio_response_get(SDIO_RESPONSE0);
    if(SD_ALLZERO == (resp_r1 & SD_R1_ERROR_BITS)) {
        /* no error occurs, return SD_OK */
        status = SD_OK;
        return status;
    }

    /* if some error occurs, return the error type */
    status = r1_error_type_check(resp_r1);
    return status;
}

/*!
    \brief      check if error occurs for R2 response
    \param[in]  none
    \param[out] none
    \retval     sd_error_enum
*/
static sd_error_enum r2_error_check(void)
{
    sd_error_enum status = SD_OK;
    uint32_t reg_status = 0;
    __IO uint32_t timeout = 100000;

    /* store the content of SDIO_STAT */
    reg_status = SDIO_STAT;
    while(!(reg_status & (SDIO_FLAG_CCRCERR | SDIO_FLAG_CMDTMOUT | SDIO_FLAG_CMDRECV)) && (timeout > 0)) {
        reg_status = SDIO_STAT;
        --timeout;
    }
    /* check whether an error or timeout occurs or command response received */
    if(reg_status & SDIO_FLAG_CCRCERR) {
        status = SD_CMD_CRC_ERROR;
        sdio_flag_clear(SDIO_FLAG_CCRCERR);
        return status;
    } else if((reg_status & SDIO_FLAG_CMDTMOUT) || (0 == timeout)) {
        status = SD_CMD_RESP_TIMEOUT;
        sdio_flag_clear(SDIO_FLAG_CMDTMOUT);
        return status;
    }
    /* clear all the SDIO_INTC flags */
    sdio_flag_clear(SDIO_MASK_INTC_FLAGS);
    return status;
}

/*!
    \brief      check if error occurs for R3 response
    \param[in]  none
    \param[out] none
    \retval     sd_error_enum
*/
static sd_error_enum r3_error_check(void)
{
    sd_error_enum status = SD_OK;
    uint32_t reg_status = 0;
    __IO uint32_t timeout = 100000;

    /* store the content of SDIO_STAT */
    reg_status = SDIO_STAT;
    while(!(reg_status & (SDIO_FLAG_CCRCERR | SDIO_FLAG_CMDTMOUT | SDIO_FLAG_CMDRECV)) && (timeout > 0)) {
        reg_status = SDIO_STAT;
        --timeout;
    }
    if((reg_status & SDIO_FLAG_CMDTMOUT) || (0 == timeout)) {
        status = SD_CMD_RESP_TIMEOUT;
        sdio_flag_clear(SDIO_FLAG_CMDTMOUT);
        return status;
    }
    /* clear all the SDIO_INTC flags */
    sdio_flag_clear(SDIO_MASK_INTC_FLAGS);
    return status;
}

/*!
    \brief      check if error occurs for R6 response
    \param[in]  cmdindex: the index of command
    \param[out] prca: a pointer that store the RCA of card
    \retval     sd_error_enum
*/
static sd_error_enum r6_error_check(uint8_t cmdindex, uint16_t *prca)
{
    sd_error_enum status = SD_OK;
    uint32_t reg_status = 0, response = 0;
    __IO uint32_t timeout = 100000;

    /* store the content of SDIO_STAT */
    reg_status = SDIO_STAT;
    while(!(reg_status & (SDIO_FLAG_CCRCERR | SDIO_FLAG_CMDTMOUT | SDIO_FLAG_CMDRECV)) && (timeout > 0)) {
        reg_status = SDIO_STAT;
        --timeout;
    }
    /* check whether an error or timeout occurs or command response received */
    if(reg_status & SDIO_FLAG_CCRCERR) {
        status = SD_CMD_CRC_ERROR;
        sdio_flag_clear(SDIO_FLAG_CCRCERR);
        return status;
    } else if((reg_status & SDIO_FLAG_CMDTMOUT) || (0 == timeout)) {
        status = SD_CMD_RESP_TIMEOUT;
        sdio_flag_clear(SDIO_FLAG_CMDTMOUT);
        return status;
    }

    /* check whether the last response command index is the desired one */
    if(sdio_command_index_get() != cmdindex) {
        status = SD_ILLEGAL_COMMAND;
        return status;
    }
    /* clear all the SDIO_INTC flags */
    sdio_flag_clear(SDIO_MASK_INTC_FLAGS);
    /* get the SDIO response register 0 for checking */
    response = sdio_response_get(SDIO_RESPONSE0);

    if(SD_ALLZERO == (response & (SD_R6_COM_CRC_ERROR | SD_R6_ILLEGAL_COMMAND | SD_R6_GENERAL_UNKNOWN_ERROR))) {
        *prca = (uint16_t)(response >> 16);
        return status;
    }
    /* if some error occurs, return the error type */
    if(response & SD_R6_COM_CRC_ERROR) {
        status = SD_COM_CRC_ERROR;
    } else if(response & SD_R6_ILLEGAL_COMMAND) {
        status = SD_ILLEGAL_COMMAND;
    } else if(response & SD_R6_GENERAL_UNKNOWN_ERROR) {
        status = SD_GENERAL_UNKNOWN_ERROR;
    }
    return status;
}

/*!
    \brief      check if error occurs for R7 response
    \param[in]  none
    \param[out] none
    \retval     sd_error_enum
*/
static sd_error_enum r7_error_check(void)
{
    sd_error_enum status = SD_ERROR;
    uint32_t reg_status = 0;
    __IO uint32_t timeout = 100000;

    /* store the content of SDIO_STAT */
    reg_status = SDIO_STAT;
    while(!(reg_status & (SDIO_FLAG_CCRCERR | SDIO_FLAG_CMDTMOUT | SDIO_FLAG_CMDRECV)) && (timeout > 0)) {
        reg_status = SDIO_STAT;
        --timeout;
    }

    /* check the flags */
    if((reg_status & SDIO_FLAG_CMDTMOUT) || (0 == timeout)) {
        status = SD_CMD_RESP_TIMEOUT;
        sdio_flag_clear(SDIO_FLAG_CMDTMOUT);
        return status;
    }
    if(reg_status & SDIO_FLAG_CMDRECV) {
        status = SD_OK;
        sdio_flag_clear(SDIO_FLAG_CMDRECV);
        return status;
    }
    return status;
}

/*!
    \brief      get the state which the card is in
    \param[in]  none
    \param[out] pcardstate: a pointer that store the card state
      \arg        SD_CARDSTATE_IDLE: card is in idle state
      \arg        SD_CARDSTATE_READY: card is in ready state
      \arg        SD_CARDSTATE_IDENTIFICAT: card is in identification state
      \arg        SD_CARDSTATE_STANDBY: card is in standby state
      \arg        SD_CARDSTATE_TRANSFER: card is in transfer state
      \arg        SD_CARDSTATE_DATA: card is in data state
      \arg        SD_CARDSTATE_RECEIVING: card is in receiving state
      \arg        SD_CARDSTATE_PROGRAMMING: card is in programming state
      \arg        SD_CARDSTATE_DISCONNECT: card is in disconnect state
      \arg        SD_CARDSTATE_LOCKED: card is in locked state
    \retval     sd_error_enum
*/
static sd_error_enum sd_card_state_get(uint8_t *pcardstate)
{
    sd_error_enum status = SD_OK;
    __IO uint32_t reg_status = 0, response = 0;
    __IO uint32_t timeout = 100000;

    /* send CMD13(SEND_STATUS), addressed card sends its status register */
    sdio_csm_disable();
    sdio_command_response_config(SD_CMD_SEND_STATUS, (uint32_t)sd_rca << SD_RCA_SHIFT, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();

    /* store the content of SDIO_STAT */
    reg_status = SDIO_STAT;
    while(!(reg_status & (SDIO_FLAG_CCRCERR | SDIO_FLAG_CMDTMOUT | SDIO_FLAG_CMDRECV)) && (timeout > 0)) {
        reg_status = SDIO_STAT;
        --timeout;
    }
    /* check whether an error or timeout occurs or command response received */
    if(reg_status & SDIO_FLAG_CCRCERR) {
        status = SD_CMD_CRC_ERROR;
        sdio_flag_clear(SDIO_FLAG_CCRCERR);
        return status;
    } else if((reg_status & SDIO_FLAG_CMDTMOUT) || (0 == timeout)) {
        status = SD_CMD_RESP_TIMEOUT;
        sdio_flag_clear(SDIO_FLAG_CMDTMOUT);
        return status;
    }

    /* command response received, store the response command index */
    reg_status = (uint32_t)sdio_command_index_get();
    if(reg_status != (uint32_t)SD_CMD_SEND_STATUS) {
        status = SD_ILLEGAL_COMMAND;
        return status;
    }
    /* clear all the SDIO_INTC flags */
    sdio_flag_clear(SDIO_MASK_INTC_FLAGS);
    /* get the SDIO response register 0 for checking */
    response = sdio_response_get(SDIO_RESPONSE0);
    *pcardstate = (uint8_t)((response >> 9) & 0x0000000F);

    if(SD_ALLZERO == (response & SD_R1_ERROR_BITS)) {
        /* no error occurs, return SD_OK */
        status = SD_OK;
        return status;
    }

    /* if some error occurs, return the error type */
    status = r1_error_type_check(response);
    return status;
}

/*!
    \brief      configure the bus width mode
    \param[in]  buswidth: the bus width
      \arg        SD_BUS_WIDTH_1BIT: 1-bit bus width
      \arg        SD_BUS_WIDTH_4BIT: 4-bit bus width
    \param[out] none
    \retval     sd_error_enum
*/
static sd_error_enum sd_bus_width_config(uint32_t buswidth)
{
    sd_error_enum status = SD_OK;
    /* check whether the card is locked */
    if(sdio_response_get(SDIO_RESPONSE0) & SD_CARDSTATE_LOCKED) {
        status = SD_LOCK_UNLOCK_FAILED;
        return status;
    }
    /* get the SCR register */
    status = sd_scr_get(sd_rca, sd_scr);
    if(SD_OK != status) {
        return status;
    }

    if(SD_BUS_WIDTH_1BIT == buswidth) {
        if(SD_ALLZERO != (sd_scr[1] & buswidth)) {
            /* send CMD55(APP_CMD) to indicate next command is application specific command */
            sdio_csm_disable();
            sdio_command_response_config(SD_CMD_APP_CMD, (uint32_t)sd_rca << SD_RCA_SHIFT, SDIO_RESPONSETYPE_SHORT);
            sdio_wait_type_set(SDIO_WAITTYPE_NO);
            sdio_csm_enable();
            /* check if some error occurs */
            status = r1_error_check(SD_CMD_APP_CMD);
            if(SD_OK != status) {
                return status;
            }

            /* send ACMD6(SET_BUS_WIDTH) to define the data bus width */
            sdio_csm_disable();
            sdio_command_response_config(SD_APPCMD_SET_BUS_WIDTH, (uint32_t)0x0, SDIO_RESPONSETYPE_SHORT);
            sdio_wait_type_set(SDIO_WAITTYPE_NO);
            sdio_csm_enable();
            /* check if some error occurs */
            status = r1_error_check(SD_APPCMD_SET_BUS_WIDTH);
            if(SD_OK != status) {
                return status;
            }
        } else {
            status = SD_OPERATION_IMPROPER;
        }
        return status;
    } else if(SD_BUS_WIDTH_4BIT == buswidth) {
        if(SD_ALLZERO != (sd_scr[1] & buswidth)) {
            /* send CMD55(APP_CMD) to indicate next command is application specific command */
            sdio_csm_disable();
            sdio_command_response_config(SD_CMD_APP_CMD, (uint32_t)sd_rca << SD_RCA_SHIFT, SDIO_RESPONSETYPE_SHORT);
            sdio_wait_type_set(SDIO_WAITTYPE_NO);
            sdio_csm_enable();
            /* check if some error occurs */
            status = r1_error_check(SD_CMD_APP_CMD);
            if(SD_OK != status) {
                return status;
            }

            /* send ACMD6(SET_BUS_WIDTH) to define the data bus width */
            sdio_csm_disable();
            sdio_command_response_config(SD_APPCMD_SET_BUS_WIDTH, (uint32_t)0x2, SDIO_RESPONSETYPE_SHORT);
            sdio_wait_type_set(SDIO_WAITTYPE_NO);
            sdio_csm_enable();
            /* check if some error occurs */
            status = r1_error_check(SD_APPCMD_SET_BUS_WIDTH);
            if(SD_OK != status) {
                return status;
            }
        } else {
            status = SD_OPERATION_IMPROPER;
        }
        return status;
    } else {
        status = SD_PARAMETER_INVALID;
        return status;
    }
}

/*!
    \brief      get the SCR of corresponding card
    \param[in]  rca: RCA of a card
    \param[out] pscr: a pointer that store the SCR content
    \retval     sd_error_enum
*/
static sd_error_enum sd_scr_get(uint16_t rca, uint32_t *pscr)
{
    sd_error_enum status = SD_OK;
    uint32_t temp_scr[2] = {0, 0}, idx_scr = 0;
    /* send CMD16(SET_BLOCKLEN) to set block length */
    sdio_csm_disable();
    sdio_command_response_config(SD_CMD_SET_BLOCKLEN, (uint32_t)8, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    /* check if some error occurs */
    status = r1_error_check(SD_CMD_SET_BLOCKLEN);
    if(SD_OK != status) {
        return status;
    }

    /* send CMD55(APP_CMD) to indicate next command is application specific command */
    sdio_csm_disable();
    sdio_command_response_config(SD_CMD_APP_CMD, (uint32_t)rca << SD_RCA_SHIFT, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    /* check if some error occurs */
    status = r1_error_check(SD_CMD_APP_CMD);
    if(SD_OK != status) {
        return status;
    }

    /* configure SDIO data */
    sdio_data_config(SD_DATATIMEOUT, (uint32_t)8, SDIO_DATABLOCKSIZE_8BYTES);
    sdio_data_transfer_config(SDIO_TRANSMODE_BLOCK, SDIO_TRANSDIRECTION_TOSDIO);
    sdio_dsm_enable();

    /* send ACMD51(SEND_SCR) to read the SD configuration register */
    sdio_csm_disable();
    sdio_command_response_config(SD_APPCMD_SEND_SCR, (uint32_t)0x0, SDIO_RESPONSETYPE_SHORT);
    sdio_wait_type_set(SDIO_WAITTYPE_NO);
    sdio_csm_enable();
    /* check if some error occurs */
    status = r1_error_check(SD_APPCMD_SEND_SCR);
    if(SD_OK != status) {
        return status;
    }

    /* store the received SCR */
    while(!sdio_flag_get(SDIO_FLAG_DTCRCERR | SDIO_FLAG_DTTMOUT | SDIO_FLAG_RXORE | SDIO_FLAG_DTBLKEND | SDIO_FLAG_STBITE)) {
        if(RESET != sdio_flag_get(SDIO_FLAG_RXDTVAL)) {
            *(temp_scr + idx_scr) = sdio_data_read();
            ++idx_scr;
        }
    }

    /* check whether some error occurs */
    if(RESET != sdio_flag_get(SDIO_FLAG_DTCRCERR)) {
        status = SD_DATA_CRC_ERROR;
        sdio_flag_clear(SDIO_FLAG_DTCRCERR);
        return status;
    } else if(RESET != sdio_flag_get(SDIO_FLAG_DTTMOUT)) {
        status = SD_DATA_TIMEOUT;
        sdio_flag_clear(SDIO_FLAG_DTTMOUT);
        return status;
    } else if(RESET != sdio_flag_get(SDIO_FLAG_RXORE)) {
        status = SD_RX_OVERRUN_ERROR;
        sdio_flag_clear(SDIO_FLAG_RXORE);
        return status;
    } else if(RESET != sdio_flag_get(SDIO_FLAG_STBITE)) {
        status = SD_START_BIT_ERROR;
        sdio_flag_clear(SDIO_FLAG_STBITE);
        return status;
    }

    /* clear all the SDIO_INTC flags */
    sdio_flag_clear(SDIO_MASK_INTC_FLAGS);
    /* readjust the temp SCR value */
    *(pscr) = ((temp_scr[1] & SD_MASK_0_7BITS) << 24) | ((temp_scr[1] & SD_MASK_8_15BITS) << 8) |
              ((temp_scr[1] & SD_MASK_16_23BITS) >> 8) | ((temp_scr[1] & SD_MASK_24_31BITS) >> 24);
    *(pscr + 1) = ((temp_scr[0] & SD_MASK_0_7BITS) << 24) | ((temp_scr[0] & SD_MASK_8_15BITS) << 8) |
                  ((temp_scr[0] & SD_MASK_16_23BITS) >> 8) | ((temp_scr[0] & SD_MASK_24_31BITS) >> 24);
    return status;
}

/*!
    \brief      get the data block size
    \param[in]  bytesnumber: the number of bytes
    \param[out] none
    \retval     data block size
      \arg        SDIO_DATABLOCKSIZE_1BYTE: block size = 1 byte
      \arg        SDIO_DATABLOCKSIZE_2BYTES: block size = 2 bytes
      \arg        SDIO_DATABLOCKSIZE_4BYTES: block size = 4 bytes
      \arg        SDIO_DATABLOCKSIZE_8BYTES: block size = 8 bytes
      \arg        SDIO_DATABLOCKSIZE_16BYTES: block size = 16 bytes
      \arg        SDIO_DATABLOCKSIZE_32BYTES: block size = 32 bytes
      \arg        SDIO_DATABLOCKSIZE_64BYTES: block size = 64 bytes
      \arg        SDIO_DATABLOCKSIZE_128BYTES: block size = 128 bytes
      \arg        SDIO_DATABLOCKSIZE_256BYTES: block size = 256 bytes
      \arg        SDIO_DATABLOCKSIZE_512BYTES: block size = 512 bytes
      \arg        SDIO_DATABLOCKSIZE_1024BYTES: block size = 1024 bytes
      \arg        SDIO_DATABLOCKSIZE_2048BYTES: block size = 2048 bytes
      \arg        SDIO_DATABLOCKSIZE_4096BYTES: block size = 4096 bytes
      \arg        SDIO_DATABLOCKSIZE_8192BYTES: block size = 8192 bytes
      \arg        SDIO_DATABLOCKSIZE_16384BYTES: block size = 16384 bytes
*/
static uint32_t sd_datablocksize_get(uint16_t bytesnumber)
{
    uint8_t exp_val = 0;
    /* calculate the exponent of 2 */
    while(1 != bytesnumber) {
        bytesnumber >>= 1;
        ++exp_val;
    }
    return DATACTL_BLKSZ(exp_val);
}

/*!
    \brief      configure the DMA1 channel 3 for transferring data
    \param[in]  srcbuf: a pointer point to a buffer which will be transferred
    \param[in]  bufsize: the size of buffer(not used in flow controller is peripheral)
    \param[out] none
    \retval     none
*/
static void dma_transfer_config(uint32_t *srcbuf, uint32_t bufsize)
{
    dma_parameter_struct dma_struct;
    /* clear all the interrupt flags */
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_G);
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_FTF);
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_HTF);
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_ERR);
    dma_channel_disable(DMA1, DMA_CH3);
    dma_deinit(DMA1, DMA_CH3);

    /* configure the DMA1 channel3 */
    dma_struct.periph_addr = (uint32_t)SDIO_FIFO_ADDR;
    dma_struct.memory_addr = (uint32_t)srcbuf;
    dma_struct.direction = DMA_MEMORY_TO_PERIPHERAL;
    dma_struct.number = bufsize / 4;
    dma_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_struct.periph_width = DMA_PERIPHERAL_WIDTH_32BIT;
    dma_struct.memory_width = DMA_MEMORY_WIDTH_32BIT;
    dma_struct.priority = DMA_PRIORITY_MEDIUM;
    dma_init(DMA1, DMA_CH3, &dma_struct);

    dma_circulation_disable(DMA1, DMA_CH3);
    dma_channel_enable(DMA1, DMA_CH3);
}

/*!
    \brief      configure the DMA1 channel 3 for receiving data
    \param[in]  dstbuf: a pointer point to a buffer which will receive data
    \param[in]  bufsize: the size of buffer(not used in flow controller is peripheral)
    \param[out] none
    \retval     none
*/
static void dma_receive_config(uint32_t *dstbuf, uint32_t bufsize)
{
    dma_parameter_struct dma_struct;
    /* clear all the interrupt flags */
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_G);
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_FTF);
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_HTF);
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_ERR);
    dma_channel_disable(DMA1, DMA_CH3);
    dma_deinit(DMA1, DMA_CH3);

    /* configure the DMA1 channel 3 */
    dma_struct.periph_addr = (uint32_t)SDIO_FIFO_ADDR;
    dma_struct.memory_addr = (uint32_t)dstbuf;
    dma_struct.direction = DMA_PERIPHERAL_TO_MEMORY;
    dma_struct.number = bufsize / 4;
    dma_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_struct.periph_width = DMA_PERIPHERAL_WIDTH_32BIT;
    dma_struct.memory_width = DMA_MEMORY_WIDTH_32BIT;
    dma_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_init(DMA1, DMA_CH3, &dma_struct);

    dma_circulation_disable(DMA1, DMA_CH3);
    dma_channel_enable(DMA1, DMA_CH3);
}
