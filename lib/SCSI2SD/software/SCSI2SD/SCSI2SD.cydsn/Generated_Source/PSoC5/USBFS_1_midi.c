/*******************************************************************************
* File Name: USBFS_1_midi.c
* Version 2.60
*
* Description:
*  MIDI Streaming request handler.
*  This file contains routines for sending and receiving MIDI
*  messages, and handles running status in both directions.
*
********************************************************************************
* Copyright 2008-2013, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "USBFS_1.h"

#if defined(USBFS_1_ENABLE_MIDI_STREAMING)

#include "USBFS_1_midi.h"
#include "USBFS_1_pvt.h"


/***************************************
*    MIDI Constants
***************************************/

#if (USBFS_1_MIDI_EXT_MODE >= USBFS_1_ONE_EXT_INTRF)
    /* The Size of the MIDI messages (MIDI Table 4-1) */
    static const uint8 CYCODE USBFS_1_MIDI_SIZE[] = {
    /*  Miscellaneous function codes(Reserved)  */ 0x03u,
    /*  Cable events (Reserved)                 */ 0x03u,
    /*  Two-byte System Common messages         */ 0x02u,
    /*  Three-byte System Common messages       */ 0x03u,
    /*  SysEx starts or continues               */ 0x03u,
    /*  Single-byte System Common Message or
        SysEx ends with following single byte   */ 0x01u,
    /*  SysEx ends with following two bytes     */ 0x02u,
    /*  SysEx ends with following three bytes   */ 0x03u,
    /*  Note-off                                */ 0x03u,
    /*  Note-on                                 */ 0x03u,
    /*  Poly-KeyPress                           */ 0x03u,
    /*  Control Change                          */ 0x03u,
    /*  Program Change                          */ 0x02u,
    /*  Channel Pressure                        */ 0x02u,
    /*  PitchBend Change                        */ 0x03u,
    /*  Single Byte                             */ 0x01u
    };
#endif  /* USBFS_1_MIDI_EXT_MODE >= USBFS_1_ONE_EXT_INTRF */



/***************************************
*  Global variables
***************************************/

#if (USBFS_1_MIDI_IN_BUFF_SIZE > 0)
    #if (USBFS_1_MIDI_IN_BUFF_SIZE >= 256)
        volatile uint16 USBFS_1_midiInPointer;                            /* Input endpoint buffer pointer */
    #else
        volatile uint8 USBFS_1_midiInPointer;                             /* Input endpoint buffer pointer */
    #endif /* End USBFS_1_MIDI_IN_BUFF_SIZE >=256 */
    volatile uint8 USBFS_1_midi_in_ep;                                    /* Input endpoint number */
    uint8 USBFS_1_midiInBuffer[USBFS_1_MIDI_IN_BUFF_SIZE];       /* Input endpoint buffer */
#endif /* USBFS_1_MIDI_IN_BUFF_SIZE > 0 */

#if (USBFS_1_MIDI_OUT_BUFF_SIZE > 0)
    volatile uint8 USBFS_1_midi_out_ep;                                   /* Output endpoint number */
    uint8 USBFS_1_midiOutBuffer[USBFS_1_MIDI_OUT_BUFF_SIZE];     /* Output endpoint buffer */
#endif /* USBFS_1_MIDI_OUT_BUFF_SIZE > 0 */

#if (USBFS_1_MIDI_EXT_MODE >= USBFS_1_ONE_EXT_INTRF)
    static USBFS_1_MIDI_RX_STATUS USBFS_1_MIDI1_Event;            /* MIDI RX status structure */
    static volatile uint8 USBFS_1_MIDI1_TxRunStat;                         /* MIDI Output running status */
    volatile uint8 USBFS_1_MIDI1_InqFlags;                                 /* Device inquiry flag */

    #if (USBFS_1_MIDI_EXT_MODE >= USBFS_1_TWO_EXT_INTRF)
        static USBFS_1_MIDI_RX_STATUS USBFS_1_MIDI2_Event;        /* MIDI RX status structure */
        static volatile uint8 USBFS_1_MIDI2_TxRunStat;                     /* MIDI Output running status */
        volatile uint8 USBFS_1_MIDI2_InqFlags;                             /* Device inquiry flag */
    #endif /* End USBFS_1_MIDI_EXT_MODE >= USBFS_1_TWO_EXT_INTRF */
#endif /* End USBFS_1_MIDI_EXT_MODE >= USBFS_1_ONE_EXT_INTRF */


/***************************************
* Custom Declarations
***************************************/

/* `#START MIDI_CUSTOM_DECLARATIONS` Place your declaration here */

/* `#END` */


/***************************************
* Optional MIDI APIs
***************************************/
#if (USBFS_1_ENABLE_MIDI_API != 0u)


/*******************************************************************************
* Function Name: USBFS_1_MIDI_EP_Init
********************************************************************************
*
* Summary:
*  This function initializes the MIDI interface and UART(s) to be ready to
*   receive data from the PC and MIDI ports.
*
* Parameters:
*  None
*
* Return:
*  None
*
* Global variables:
*  USBFS_1_midiInBuffer: This buffer is used for saving and combining
*    the received data from UART(s) and(or) generated internally by
*    PutUsbMidiIn() function messages. USBFS_1_MIDI_IN_EP_Service()
*    function transfers the data from this buffer to the PC.
*  USBFS_1_midiOutBuffer: This buffer is used by the
*    USBFS_1_MIDI_OUT_EP_Service() function for saving the received
*    from the PC data, then the data are parsed and transferred to UART(s)
*    buffer and to the internal processing by the
*    USBFS_1_callbackLocalMidiEvent function.
*  USBFS_1_midi_out_ep: Used as an OUT endpoint number.
*  USBFS_1_midi_in_ep: Used as an IN endpoint number.
*   USBFS_1_midiInPointer: Initialized to zero.
*
* Reentrant:
*  No
*
*******************************************************************************/
void USBFS_1_MIDI_EP_Init(void) 
{
    #if (USBFS_1_MIDI_IN_BUFF_SIZE > 0)
       USBFS_1_midiInPointer = 0u;
    #endif  /* USBFS_1_MIDI_IN_BUFF_SIZE > 0 */

    #if(USBFS_1_EP_MM == USBFS_1__EP_DMAAUTO)
        #if (USBFS_1_MIDI_IN_BUFF_SIZE > 0)
            /* Init DMA configurations for IN EP*/
            USBFS_1_LoadInEP(USBFS_1_midi_in_ep, USBFS_1_midiInBuffer,
                                                                                USBFS_1_MIDI_IN_BUFF_SIZE);
                                                                                
        #endif  /* USBFS_1_MIDI_IN_BUFF_SIZE > 0 */
        #if (USBFS_1_MIDI_OUT_BUFF_SIZE > 0)
            /* Init DMA configurations for OUT EP*/
            (void)USBFS_1_ReadOutEP(USBFS_1_midi_out_ep, USBFS_1_midiOutBuffer,
                                                                                USBFS_1_MIDI_OUT_BUFF_SIZE);
        #endif /*USBFS_1_MIDI_OUT_BUFF_SIZE > 0 */
    #endif  /* End USBFS_1__EP_DMAAUTO */

    #if (USBFS_1_MIDI_OUT_BUFF_SIZE > 0)
        USBFS_1_EnableOutEP(USBFS_1_midi_out_ep);
    #endif /* USBFS_1_MIDI_OUT_BUFF_SIZE > 0 */

    /* Initialize the MIDI port(s) */
    #if (USBFS_1_MIDI_EXT_MODE >= USBFS_1_ONE_EXT_INTRF)
        USBFS_1_MIDI_Init();
    #endif /* USBFS_1_MIDI_EXT_MODE >= USBFS_1_ONE_EXT_INTRF */
}

#if (USBFS_1_MIDI_OUT_BUFF_SIZE > 0)


    /*******************************************************************************
    * Function Name: USBFS_1_MIDI_OUT_EP_Service
    ********************************************************************************
    *
    * Summary:
    *  Services the USB MIDI OUT endpoints.
    *  This function is called from OUT EP ISR. It transfers the received from PC
    *  data to the external MIDI port(UART TX buffer) and calls the
    *  USBFS_1_callbackLocalMidiEvent() function to internal process
    *  of the MIDI data.
    *  This function is blocked by UART, if not enough space is available in UART
    *  TX buffer. Therefore it is recommended to use large UART TX buffer size.
    *
    * Parameters:
    *  None
    *
    * Return:
    *  None
    *
    * Global variables:
    *  USBFS_1_midiOutBuffer: Used as temporary buffer between USB internal
    *   memory and UART TX buffer.
    *  USBFS_1_midi_out_ep: Used as an OUT endpoint number.
    *
    * Reentrant:
    *  No
    *
    *******************************************************************************/
    void USBFS_1_MIDI_OUT_EP_Service(void) 
    {
        #if USBFS_1_MIDI_OUT_BUFF_SIZE >= 256
            uint16 outLength;
            uint16 outPointer;
        #else
            uint8 outLength;
            uint8 outPointer;
        #endif /* End USBFS_1_MIDI_OUT_BUFF_SIZE >=256 */

        uint8 dmaState = 0u;

        /* Service the USB MIDI output endpoint */
        if (USBFS_1_GetEPState(USBFS_1_midi_out_ep) == USBFS_1_OUT_BUFFER_FULL)
        {
            #if USBFS_1_MIDI_OUT_BUFF_SIZE >= 256
                outLength = USBFS_1_GetEPCount(USBFS_1_midi_out_ep);
            #else
                outLength = (uint8)USBFS_1_GetEPCount(USBFS_1_midi_out_ep);
            #endif /* End USBFS_1_MIDI_OUT_BUFF_SIZE >= 256 */
            #if(USBFS_1_EP_MM != USBFS_1__EP_DMAAUTO)
                #if USBFS_1_MIDI_OUT_BUFF_SIZE >= 256
                    outLength = USBFS_1_ReadOutEP(USBFS_1_midi_out_ep,
                                                                    USBFS_1_midiOutBuffer, outLength);
                #else
                    outLength = (uint8)USBFS_1_ReadOutEP(USBFS_1_midi_out_ep,
                                                                    USBFS_1_midiOutBuffer, (uint16)outLength);
                #endif /* End USBFS_1_MIDI_OUT_BUFF_SIZE >= 256 */
                #if(USBFS_1_EP_MM == USBFS_1__EP_DMAMANUAL)
                    do  /* wait for DMA transfer complete */
                    {
                        (void)CyDmaChStatus(USBFS_1_DmaChan[USBFS_1_midi_out_ep], NULL, &dmaState);
                    }while((dmaState & (STATUS_TD_ACTIVE | STATUS_CHAIN_ACTIVE)) != 0u);
                #endif /* End USBFS_EP_MM == USBFS__EP_DMAMANUAL */
            #endif  /* End USBFS_1_EP_MM != USBFS_1__EP_DMAAUTO */
            if(dmaState != 0u)
            {
                /* Suppress compiler warning */
            }
            if (outLength >= USBFS_1_EVENT_LENGTH)
            {
                outPointer = 0u;
                while (outPointer < outLength)
                {
                    /* In some OS OUT packet could be appended by nulls which could be skipped */
                    if (USBFS_1_midiOutBuffer[outPointer] == 0u)
                    {
                        break;
                    }
                    /* Route USB MIDI to the External connection */
                    #if (USBFS_1_MIDI_EXT_MODE >= USBFS_1_ONE_EXT_INTRF)
                        if ((USBFS_1_midiOutBuffer[outPointer] & USBFS_1_CABLE_MASK) ==
                            USBFS_1_MIDI_CABLE_00)
                        {
                            USBFS_1_MIDI1_ProcessUsbOut(&USBFS_1_midiOutBuffer[outPointer]);
                        }
                        else if ((USBFS_1_midiOutBuffer[outPointer] & USBFS_1_CABLE_MASK) ==
                                 USBFS_1_MIDI_CABLE_01)
                        {
                            #if (USBFS_1_MIDI_EXT_MODE >= USBFS_1_TWO_EXT_INTRF)
                                USBFS_1_MIDI2_ProcessUsbOut(&USBFS_1_midiOutBuffer[outPointer]);
                            #endif /* End USBFS_1_MIDI_EXT_MODE >= USBFS_1_TWO_EXT_INTRF */
                        }
                        else
                        {
                            /* `#START CUSTOM_MIDI_OUT_EP_SERV` Place your code here */

                            /* `#END` */
                        }
                    #endif /* End USBFS_1_MIDI_EXT_MODE >= USBFS_1_ONE_EXT_INTRF */

                    /* Process any local MIDI output functions */
                    USBFS_1_callbackLocalMidiEvent(
                        USBFS_1_midiOutBuffer[outPointer] & USBFS_1_CABLE_MASK,
                        &USBFS_1_midiOutBuffer[outPointer + USBFS_1_EVENT_BYTE1]);
                    outPointer += USBFS_1_EVENT_LENGTH;
                }
            }
            #if(USBFS_1_EP_MM == USBFS_1__EP_DMAAUTO)
                /* Enable Out EP*/
                USBFS_1_EnableOutEP(USBFS_1_midi_out_ep);
            #endif  /* End USBFS_1_EP_MM == USBFS_1__EP_DMAAUTO */
        }
    }

#endif /* #if (USBFS_1_MIDI_OUT_BUFF_SIZE > 0) */

#if (USBFS_1_MIDI_IN_BUFF_SIZE > 0)


    /*******************************************************************************
    * Function Name: USBFS_1_MIDI_IN_EP_Service
    ********************************************************************************
    *
    * Summary:
    *  Services the USB MIDI IN endpoint. Non-blocking.
    *  Checks that previous packet was processed by HOST, otherwise service the
    *  input endpoint on the subsequent call. It is called from the
    *  USBFS_1_MIDI_IN_Service() and from the
    *  USBFS_1_PutUsbMidiIn() function.
    *
    * Parameters:
    *  None
    *
    * Return:
    *  None
    *
    * Global variables:
    *  USBFS_1_midi_in_ep: Used as an IN endpoint number.
    *  USBFS_1_midiInBuffer: Function loads the data from this buffer to
    *    the USB IN endpoint.
    *   USBFS_1_midiInPointer: Cleared to zero when data are sent.
    *
    * Reentrant:
    *  No
    *
    *******************************************************************************/
    void USBFS_1_MIDI_IN_EP_Service(void) 
    {
        /* Service the USB MIDI input endpoint */
        /* Check that previous packet was processed by HOST, otherwise service the USB later */
        if (USBFS_1_midiInPointer != 0u)
        {
            if(USBFS_1_GetEPState(USBFS_1_midi_in_ep) == USBFS_1_EVENT_PENDING)
            {
            #if(USBFS_1_EP_MM != USBFS_1__EP_DMAAUTO)
                USBFS_1_LoadInEP(USBFS_1_midi_in_ep, USBFS_1_midiInBuffer,
                                                                                (uint16)USBFS_1_midiInPointer);
            #else /* USBFS_1_EP_MM != USBFS_1__EP_DMAAUTO */
                /* rearm IN EP */
                USBFS_1_LoadInEP(USBFS_1_midi_in_ep, NULL, (uint16)USBFS_1_midiInPointer);
            #endif /* End USBFS_1_EP_MM != USBFS_1__EP_DMAAUTO*/

            /* Clear the midiInPointer. For DMA mode, clear this pointer in the ARB ISR when data are moved by DMA */
            #if(USBFS_1_EP_MM == USBFS_1__EP_MANUAL)
                USBFS_1_midiInPointer = 0u;
            #endif /* USBFS_1_EP_MM == USBFS_1__EP_MANUAL */
            }
        }
    }


    /*******************************************************************************
    * Function Name: USBFS_1_MIDI_IN_Service
    ********************************************************************************
    *
    * Summary:
    *  Services the traffic from the MIDI input ports (RX UART) and prepare data
    *  in USB MIDI IN endpoint buffer.
    *  Calls the USBFS_1_MIDI_IN_EP_Service() function to sent the
    *  data from buffer to PC. Non-blocking. Should be called from main foreground
    *  task.
    *  This function is not protected from the reentrant calls. When it is required
    *  to use this function in UART RX ISR to guaranty low latency, care should be
    *  taken to protect from reentrant calls.
    *
    * Parameters:
    *  None
    *
    * Return:
    *  None
    *
    * Global variables:
    *   USBFS_1_midiInPointer: Cleared to zero when data are sent.
    *
    * Reentrant:
    *  No
    *
    *******************************************************************************/
    void USBFS_1_MIDI_IN_Service(void) 
    {
        /* Service the MIDI UART inputs until either both receivers have no more
        *  events or until the input endpoint buffer fills up.
        */
        #if (USBFS_1_MIDI_EXT_MODE >= USBFS_1_ONE_EXT_INTRF)
            uint8 m1 = 0u;
            uint8 m2 = 0u;
            do
            {
                if (USBFS_1_midiInPointer <= (USBFS_1_MIDI_IN_BUFF_SIZE - USBFS_1_EVENT_LENGTH))
                {
                    /* Check MIDI1 input port for a complete event */
                    m1 = USBFS_1_MIDI1_GetEvent();
                    if (m1 != 0u)
                    {
                        USBFS_1_PrepareInBuffer(m1, (uint8 *)&USBFS_1_MIDI1_Event.msgBuff[0],
                                                    USBFS_1_MIDI1_Event.size, USBFS_1_MIDI_CABLE_00);
                    }
                }

            #if (USBFS_1_MIDI_EXT_MODE >= USBFS_1_TWO_EXT_INTRF)
                if (USBFS_1_midiInPointer <= (USBFS_1_MIDI_IN_BUFF_SIZE - USBFS_1_EVENT_LENGTH))
                {
                    /* Check MIDI2 input port for a complete event */
                    m2 = USBFS_1_MIDI2_GetEvent();
                    if (m2 != 0u)
                    {
                        USBFS_1_PrepareInBuffer(m2, (uint8 *)&USBFS_1_MIDI2_Event.msgBuff[0],
                                                    USBFS_1_MIDI2_Event.size, USBFS_1_MIDI_CABLE_01);
                    }
                }
            #endif /* End USBFS_1_MIDI_EXT_MODE >= USBFS_1_TWO_EXT_INTRF */

            }while( (USBFS_1_midiInPointer <= (USBFS_1_MIDI_IN_BUFF_SIZE - USBFS_1_EVENT_LENGTH))
                   && ((m1 != 0u) || (m2 != 0u)) );
        #endif /* End USBFS_1_MIDI_EXT_MODE >= USBFS_1_ONE_EXT_INTRF */

        /* Service the USB MIDI input endpoint */
        USBFS_1_MIDI_IN_EP_Service();
    }


    /*******************************************************************************
    * Function Name: USBFS_1_PutUsbMidiIn
    ********************************************************************************
    *
    * Summary:
    *  Puts one MIDI messages into the USB MIDI In endpoint buffer. These are
    *  MIDI input messages to the host. This function is only used if the device
    *  has internal MIDI input functionality. USBMIDI_MIDI_IN_Service() function
    *  should additionally be called to send the message from local buffer to
    *  IN endpoint.
    *
    * Parameters:
    *  ic:   0 = No message (should never happen)
    *        1 - 3 = Complete MIDI message in midiMsg
    *        3 - IN EP LENGTH = Complete SySEx message(without EOSEX byte) in
    *            midiMsg. The length is limited by the max BULK EP size(64)
    *        MIDI_SYSEX = Start or continuation of SysEx message
    *                     (put event bytes in midiMsg buffer)
    *        MIDI_EOSEX = End of SysEx message
    *                     (put event bytes in midiMsg buffer)
    *        MIDI_TUNEREQ = Tune Request message (single byte system common msg)
    *        0xf8 - 0xff = Single byte real-time message
    *  midiMsg: pointer to MIDI message.
    *  cable:   cable number.
    *
    * Return:
    *  USBFS_1_TRUE if error.
    *  USBFS_1_FALSE if success.
    *
    * Global variables:
    *  USBFS_1_midi_in_ep: MIDI IN endpoint number used for sending data.
    *  USBFS_1_midiInPointer: Checked this variable to see if there is
    *    enough free space in the IN endpoint buffer. If buffer is full, initiate
    *    sending to PC.
    *
    * Reentrant:
    *  No
    *
    *******************************************************************************/
    uint8 USBFS_1_PutUsbMidiIn(uint8 ic, const uint8 midiMsg[], uint8 cable)
                                                                
    {
        uint8 retError = USBFS_1_FALSE;
        uint8 msgIndex;

        /* Protect PrepareInBuffer() function from concurrent calls */
        #if (USBFS_1_MIDI_EXT_MODE >= USBFS_1_ONE_EXT_INTRF)
            MIDI1_UART_DisableRxInt();
            #if (USBFS_1_MIDI_EXT_MODE >= USBFS_1_TWO_EXT_INTRF)
                MIDI2_UART_DisableRxInt();
            #endif /* End USBFS_1_MIDI_EXT_MODE >= USBFS_1_TWO_EXT_INTRF */
        #endif /* End USBFS_1_MIDI_EXT_MODE >= USBFS_1_ONE_EXT_INTRF */

        if (USBFS_1_midiInPointer >
                    (USBFS_1_EP[USBFS_1_midi_in_ep].bufferSize - USBFS_1_EVENT_LENGTH))
        {
            USBFS_1_MIDI_IN_EP_Service();
        }
        if (USBFS_1_midiInPointer <=
                    (USBFS_1_EP[USBFS_1_midi_in_ep].bufferSize - USBFS_1_EVENT_LENGTH))
        {
            if((ic < USBFS_1_EVENT_LENGTH) || (ic >= USBFS_1_MIDI_STATUS_MASK))
            {
                USBFS_1_PrepareInBuffer(ic, midiMsg, ic, cable);
            }
            else
            {   /* Only SysEx message is greater than 4 bytes */
                msgIndex = 0u;
                do
                {
                    USBFS_1_PrepareInBuffer(USBFS_1_MIDI_SYSEX, &midiMsg[msgIndex],
                                                     USBFS_1_EVENT_BYTE3, cable);
                    ic -= USBFS_1_EVENT_BYTE3;
                    msgIndex += USBFS_1_EVENT_BYTE3;
                    if (USBFS_1_midiInPointer >
                        (USBFS_1_EP[USBFS_1_midi_in_ep].bufferSize - USBFS_1_EVENT_LENGTH))
                    {
                        USBFS_1_MIDI_IN_EP_Service();
                        if (USBFS_1_midiInPointer >
                            (USBFS_1_EP[USBFS_1_midi_in_ep].bufferSize - USBFS_1_EVENT_LENGTH))
                        {
                            /* Error condition. HOST is not ready to receive this packet. */
                            retError = USBFS_1_TRUE;
                            break;
                        }
                    }
                }while(ic > USBFS_1_EVENT_BYTE3);

                if(retError == USBFS_1_FALSE)
                {
                    USBFS_1_PrepareInBuffer(USBFS_1_MIDI_EOSEX, midiMsg, ic, cable);
                }
            }
        }
        else
        {
            /* Error condition. HOST is not ready to receive this packet. */
            retError = USBFS_1_TRUE;
        }

        #if (USBFS_1_MIDI_EXT_MODE >= USBFS_1_ONE_EXT_INTRF)
            MIDI1_UART_EnableRxInt();
            #if (USBFS_1_MIDI_EXT_MODE >= USBFS_1_TWO_EXT_INTRF)
                MIDI2_UART_EnableRxInt();
            #endif /* End USBFS_1_MIDI_EXT_MODE >= USBFS_1_TWO_EXT_INTRF */
        #endif /* End USBFS_1_MIDI_EXT_MODE >= USBFS_1_ONE_EXT_INTRF */

        return (retError);
    }


    /*******************************************************************************
    * Function Name: USBFS_1_PrepareInBuffer
    ********************************************************************************
    *
    * Summary:
    *  Builds a USB MIDI event in the input endpoint buffer at the current pointer.
    *  Puts one MIDI message into the USB MIDI In endpoint buffer.
    *
    * Parameters:
    *  ic:   0 = No message (should never happen)
    *        1 - 3 = Complete MIDI message at pMdat[0]
    *        MIDI_SYSEX = Start or continuation of SysEx message
    *                     (put eventLen bytes in buffer)
    *        MIDI_EOSEX = End of SysEx message
    *                     (put eventLen bytes in buffer,
    *                      and append MIDI_EOSEX)
    *        MIDI_TUNEREQ = Tune Request message (single byte system common msg)
    *        0xf8 - 0xff = Single byte real-time message
    *
    *  srcBuff: pointer to MIDI data
    *  eventLen: number of bytes in MIDI event
    *  cable: MIDI source port number
    *
    * Return:
    *  None
    *
    * Global variables:
    *  USBFS_1_midiInBuffer: This buffer is used for saving and combine the
    *    received from UART(s) and(or) generated internally by
    *    USBFS_1_PutUsbMidiIn() function messages.
    *  USBFS_1_midiInPointer: Used as an index for midiInBuffer to
    *     write data.
    *
    * Reentrant:
    *  No
    *
    *******************************************************************************/
    void USBFS_1_PrepareInBuffer(uint8 ic, const uint8 srcBuff[], uint8 eventLen, uint8 cable)
                                                                 
    {
        uint8 srcBuffZero;
        uint8 srcBuffOne;

        srcBuffZero = srcBuff[0u];
        srcBuffOne  = srcBuff[1u];

        if (ic >= (USBFS_1_MIDI_STATUS_MASK | USBFS_1_MIDI_SINGLE_BYTE_MASK))
        {
            USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = USBFS_1_SINGLE_BYTE | cable;
            USBFS_1_midiInPointer++;
            USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = ic;
            USBFS_1_midiInPointer++;
            USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = 0u;
            USBFS_1_midiInPointer++;
            USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = 0u;
            USBFS_1_midiInPointer++;
        }
        else if((ic < USBFS_1_EVENT_LENGTH) || (ic == USBFS_1_MIDI_SYSEX))
        {
            if(ic == USBFS_1_MIDI_SYSEX)
            {
                USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = USBFS_1_SYSEX | cable;
                USBFS_1_midiInPointer++;
            }
            else if (srcBuffZero < USBFS_1_MIDI_SYSEX)
            {
                USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = (srcBuffZero >> 4u) | cable;
                USBFS_1_midiInPointer++;
            }
            else if (srcBuffZero == USBFS_1_MIDI_TUNEREQ)
            {
                USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = USBFS_1_1BYTE_COMMON | cable;
                USBFS_1_midiInPointer++;
            }
            else if ((srcBuffZero == USBFS_1_MIDI_QFM) || (srcBuffZero == USBFS_1_MIDI_SONGSEL))
            {
                USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = USBFS_1_2BYTE_COMMON | cable;
                USBFS_1_midiInPointer++;
            }
            else if (srcBuffZero == USBFS_1_MIDI_SPP)
            {
                USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = USBFS_1_3BYTE_COMMON | cable;
                USBFS_1_midiInPointer++;
            }
            else
            {
            }

            USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = srcBuffZero;
            USBFS_1_midiInPointer++;
            USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = srcBuffOne;
            USBFS_1_midiInPointer++;
            USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = srcBuff[2u];
            USBFS_1_midiInPointer++;
        }
        else if (ic == USBFS_1_MIDI_EOSEX)
        {
            switch (eventLen)
            {
                case 0u:
                    USBFS_1_midiInBuffer[USBFS_1_midiInPointer] =
                                                                        USBFS_1_SYSEX_ENDS_WITH1 | cable;
                    USBFS_1_midiInPointer++;
                    USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = USBFS_1_MIDI_EOSEX;
                    USBFS_1_midiInPointer++;
                    USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = 0u;
                    USBFS_1_midiInPointer++;
                    USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = 0u;
                    USBFS_1_midiInPointer++;
                    break;
                case 1u:
                    USBFS_1_midiInBuffer[USBFS_1_midiInPointer] =
                                                                        USBFS_1_SYSEX_ENDS_WITH2 | cable;
                    USBFS_1_midiInPointer++;
                    USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = srcBuffZero;
                    USBFS_1_midiInPointer++;
                    USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = USBFS_1_MIDI_EOSEX;
                    USBFS_1_midiInPointer++;
                    USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = 0u;
                    USBFS_1_midiInPointer++;
                    break;
                case 2u:
                    USBFS_1_midiInBuffer[USBFS_1_midiInPointer] =
                                                                        USBFS_1_SYSEX_ENDS_WITH3 | cable;
                    USBFS_1_midiInPointer++;
                    USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = srcBuffZero;
                    USBFS_1_midiInPointer++;
                    USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = srcBuffOne;
                    USBFS_1_midiInPointer++;
                    USBFS_1_midiInBuffer[USBFS_1_midiInPointer] = USBFS_1_MIDI_EOSEX;
                    USBFS_1_midiInPointer++;
                    break;
                default:
                    break;
            }
        }
        else
        {
        }
    }

#endif /* #if (USBFS_1_MIDI_IN_BUFF_SIZE > 0) */


/* The implementation for external serial input and output connections
*  to route USB MIDI data to and from those connections.
*/
#if (USBFS_1_MIDI_EXT_MODE >= USBFS_1_ONE_EXT_INTRF)


    /*******************************************************************************
    * Function Name: USBFS_1_MIDI_Init
    ********************************************************************************
    *
    * Summary:
    *  Initializes MIDI variables and starts the UART(s) hardware block(s).
    *
    * Parameters:
    *  None
    *
    * Return:
    *  None
    *
    * Side Effects:
    *  Change the priority of the UART(s) TX interrupts to be higher than the
    *  default EP ISR priority.
    *
    * Global variables:
    *   USBFS_1_MIDI_Event: initialized to zero.
    *   USBFS_1_MIDI_TxRunStat: initialized to zero.
    *
    *******************************************************************************/
    void USBFS_1_MIDI_Init(void) 
    {
        USBFS_1_MIDI1_Event.length = 0u;
        USBFS_1_MIDI1_Event.count = 0u;
        USBFS_1_MIDI1_Event.size = 0u;
        USBFS_1_MIDI1_Event.runstat = 0u;
        USBFS_1_MIDI1_TxRunStat = 0u;
        USBFS_1_MIDI1_InqFlags = 0u;
        /* Start UART block */
        MIDI1_UART_Start();
        /* Change the priority of the UART TX and RX interrupt */
        CyIntSetPriority(MIDI1_UART_TX_VECT_NUM, USBFS_1_CUSTOM_UART_TX_PRIOR_NUM);
        CyIntSetPriority(MIDI1_UART_RX_VECT_NUM, USBFS_1_CUSTOM_UART_RX_PRIOR_NUM);

        #if (USBFS_1_MIDI_EXT_MODE >= USBFS_1_TWO_EXT_INTRF)
            USBFS_1_MIDI2_Event.length = 0u;
            USBFS_1_MIDI2_Event.count = 0u;
            USBFS_1_MIDI2_Event.size = 0u;
            USBFS_1_MIDI2_Event.runstat = 0u;
            USBFS_1_MIDI2_TxRunStat = 0u;
            USBFS_1_MIDI2_InqFlags = 0u;
            /* Start second UART block */
            MIDI2_UART_Start();
            /* Change the priority of the UART TX interrupt */
            CyIntSetPriority(MIDI2_UART_TX_VECT_NUM, USBFS_1_CUSTOM_UART_TX_PRIOR_NUM);
            CyIntSetPriority(MIDI2_UART_RX_VECT_NUM, USBFS_1_CUSTOM_UART_RX_PRIOR_NUM);
        #endif /* End USBFS_1_MIDI_EXT_MODE >= USBFS_1_TWO_EXT_INTRF*/

        /* `#START MIDI_INIT_CUSTOM` Init other extended UARTs here */

        /* `#END` */

    }


    /*******************************************************************************
    * Function Name: USBFS_1_ProcessMidiIn
    ********************************************************************************
    *
    * Summary:
    *  Processes one byte of incoming MIDI data.
    *
    * Parameters:
    *   mData = current MIDI input data byte
    *   *rxStat = pointer to a MIDI_RX_STATUS structure
    *
    * Return:
    *   0, if no complete message
    *   1 - 4, if message complete
    *   MIDI_SYSEX, if start or continuation of system exclusive
    *   MIDI_EOSEX, if end of system exclusive
    *   0xf8 - 0xff, if single byte real time message
    *
    *******************************************************************************/
    uint8 USBFS_1_ProcessMidiIn(uint8 mData, USBFS_1_MIDI_RX_STATUS *rxStat)
                                                                
    {
        uint8 midiReturn = 0u;

        /* Check for a MIDI status byte.  All status bytes, except real time messages,
        *  which are a single byte, force the start of a new buffer cycle.
        */
        if ((mData & USBFS_1_MIDI_STATUS_BYTE_MASK) != 0u)
        {
            if ((mData & USBFS_1_MIDI_STATUS_MASK) == USBFS_1_MIDI_STATUS_MASK)
            {
                if ((mData & USBFS_1_MIDI_SINGLE_BYTE_MASK) != 0u) /* System Real-Time Messages(single byte) */
                {
                    midiReturn = mData;
                }
                else                              /* System Common Messages */
                {
                    switch (mData)
                    {
                        case USBFS_1_MIDI_SYSEX:
                            rxStat->msgBuff[0u] = USBFS_1_MIDI_SYSEX;
                            rxStat->runstat = USBFS_1_MIDI_SYSEX;
                            rxStat->count = 1u;
                            rxStat->length = 3u;
                            break;
                        case USBFS_1_MIDI_EOSEX:
                            rxStat->runstat = 0u;
                            rxStat->size = rxStat->count;
                            rxStat->count = 0u;
                            midiReturn = USBFS_1_MIDI_EOSEX;
                            break;
                        case USBFS_1_MIDI_SPP:
                            rxStat->msgBuff[0u] = USBFS_1_MIDI_SPP;
                            rxStat->runstat = 0u;
                            rxStat->count = 1u;
                            rxStat->length = 3u;
                            break;
                        case USBFS_1_MIDI_SONGSEL:
                            rxStat->msgBuff[0u] = USBFS_1_MIDI_SONGSEL;
                            rxStat->runstat = 0u;
                            rxStat->count = 1u;
                            rxStat->length = 2u;
                            break;
                        case USBFS_1_MIDI_QFM:
                            rxStat->msgBuff[0u] = USBFS_1_MIDI_QFM;
                            rxStat->runstat = 0u;
                            rxStat->count = 1u;
                            rxStat->length = 2u;
                            break;
                        case USBFS_1_MIDI_TUNEREQ:
                            rxStat->msgBuff[0u] = USBFS_1_MIDI_TUNEREQ;
                            rxStat->runstat = 0u;
                            rxStat->size = 1u;
                            rxStat->count = 0u;
                            midiReturn = rxStat->size;
                            break;
                        default:
                            break;
                    }
                }
            }
            else /* Channel Messages */
            {
                rxStat->msgBuff[0u] = mData;
                rxStat->runstat = mData;
                rxStat->count = 1u;
                switch (mData & USBFS_1_MIDI_STATUS_MASK)
                {
                    case USBFS_1_MIDI_NOTE_OFF:
                    case USBFS_1_MIDI_NOTE_ON:
                    case USBFS_1_MIDI_POLY_KEY_PRESSURE:
                    case USBFS_1_MIDI_CONTROL_CHANGE:
                    case USBFS_1_MIDI_PITCH_BEND_CHANGE:
                        rxStat->length = 3u;
                        break;
                    case USBFS_1_MIDI_PROGRAM_CHANGE:
                    case USBFS_1_MIDI_CHANNEL_PRESSURE:
                        rxStat->length = 2u;
                        break;
                    default:
                        rxStat->runstat = 0u;
                        rxStat->count = 0u;
                        break;
                }
            }
        }

        /* Otherwise, it's a data byte */
        else
        {
            if (rxStat->runstat == USBFS_1_MIDI_SYSEX)
            {
                rxStat->msgBuff[rxStat->count] = mData;
                rxStat->count++;
                if (rxStat->count >= rxStat->length)
                {
                    rxStat->size = rxStat->count;
                    rxStat->count = 0u;
                    midiReturn = USBFS_1_MIDI_SYSEX;
                }
            }
            else if (rxStat->count > 0u)
            {
                rxStat->msgBuff[rxStat->count] = mData;
                rxStat->count++;
                if (rxStat->count >= rxStat->length)
                {
                    rxStat->size = rxStat->count;
                    rxStat->count = 0u;
                    midiReturn = rxStat->size;
                }
            }
            else if (rxStat->runstat != 0u)
            {
                rxStat->msgBuff[0u] = rxStat->runstat;
                rxStat->msgBuff[1u] = mData;
                rxStat->count = 2u;
                switch (rxStat->runstat & USBFS_1_MIDI_STATUS_MASK)
                {
                    case USBFS_1_MIDI_NOTE_OFF:
                    case USBFS_1_MIDI_NOTE_ON:
                    case USBFS_1_MIDI_POLY_KEY_PRESSURE:
                    case USBFS_1_MIDI_CONTROL_CHANGE:
                    case USBFS_1_MIDI_PITCH_BEND_CHANGE:
                        rxStat->length = 3u;
                        break;
                    case USBFS_1_MIDI_PROGRAM_CHANGE:
                    case USBFS_1_MIDI_CHANNEL_PRESSURE:
                        rxStat->size =rxStat->count;
                        rxStat->count = 0u;
                        midiReturn = rxStat->size;
                        break;
                    default:
                        rxStat->count = 0u;
                    break;
                }
            }
            else
            {
            }
        }
        return (midiReturn);
    }


    /*******************************************************************************
    * Function Name: USBFS_1_MIDI1_GetEvent
    ********************************************************************************
    *
    * Summary:
    *  Checks for incoming MIDI data, calls the MIDI event builder if so.
    *  Returns either empty or with a complete event.
    *
    * Parameters:
    *  None
    *
    * Return:
    *   0, if no complete message
    *   1 - 4, if message complete
    *   MIDI_SYSEX, if start or continuation of system exclusive
    *   MIDI_EOSEX, if end of system exclusive
    *   0xf8 - 0xff, if single byte real time message
    *
    * Global variables:
    *  USBFS_1_MIDI1_Event: RX status structure used to parse received
    *    data.
    *
    *******************************************************************************/
    uint8 USBFS_1_MIDI1_GetEvent(void) 
    {
        uint8 msgRtn = 0u;
        uint8 rxData;
        #if (MIDI1_UART_RXBUFFERSIZE >= 256u)
            uint16 rxBufferRead;
            #if CY_PSOC3 /* This local variable is required only for PSOC3 and large buffer */
                uint16 rxBufferWrite;
            #endif /* end CY_PSOC3 */
        #else
            uint8 rxBufferRead;
        #endif /* End MIDI1_UART_RXBUFFERSIZE >= 256 */
        uint8 rxBufferLoopDetect;
        /* Read buffer loop condition to the local variable */
        rxBufferLoopDetect = MIDI1_UART_rxBufferLoopDetect;

        if ( (MIDI1_UART_rxBufferRead != MIDI1_UART_rxBufferWrite) || (rxBufferLoopDetect != 0u) )
        {
            /* Protect variables that could change on interrupt by disabling Rx interrupt.*/
            #if ((MIDI1_UART_RXBUFFERSIZE >= 256u) && (CY_PSOC3))
                CyIntDisable(MIDI1_UART_RX_VECT_NUM);
            #endif /* End MIDI1_UART_RXBUFFERSIZE >= 256 */
            rxBufferRead = MIDI1_UART_rxBufferRead;
            #if ((MIDI1_UART_RXBUFFERSIZE >= 256u) && (CY_PSOC3))
                rxBufferWrite = MIDI1_UART_rxBufferWrite;
                CyIntEnable(MIDI1_UART_RX_VECT_NUM);
            #endif /* End MIDI1_UART_RXBUFFERSIZE >= 256 */

            /* Stay here until either the buffer is empty or we have a complete message
            *  in the message buffer. Note that we must use a temporary buffer pointer
            *  since it takes two instructions to increment with a wrap, and we can't
            *  risk doing that with the real pointer and getting an interrupt in between
            *  instructions.
            */

            #if ((MIDI1_UART_RXBUFFERSIZE >= 256u) && (CY_PSOC3))
                while ( ((rxBufferRead != rxBufferWrite) || (rxBufferLoopDetect != 0u)) && (msgRtn == 0u) )
            #else
                while ( ((rxBufferRead != MIDI1_UART_rxBufferWrite) || (rxBufferLoopDetect != 0u)) && (msgRtn == 0u) )
            #endif /* End MIDI1_UART_RXBUFFERSIZE >= 256 && CY_PSOC3 */
                {
                    rxData = MIDI1_UART_rxBuffer[rxBufferRead];
                    /* Increment pointer with a wrap */
                    rxBufferRead++;
                    if(rxBufferRead >= MIDI1_UART_RXBUFFERSIZE)
                    {
                        rxBufferRead = 0u;
                    }
                    /* If loop condition was set - update real read buffer pointer
                    *  to avoid overflow status
                    */
                    if(rxBufferLoopDetect != 0u )
                    {
                        MIDI1_UART_rxBufferLoopDetect = 0u;
                        #if ((MIDI1_UART_RXBUFFERSIZE >= 256u) && (CY_PSOC3))
                            CyIntDisable(MIDI1_UART_RX_VECT_NUM);
                        #endif /* End MIDI1_UART_RXBUFFERSIZE >= 256 */
                        MIDI1_UART_rxBufferRead = rxBufferRead;
                        #if ((MIDI1_UART_RXBUFFERSIZE >= 256u) && (CY_PSOC3))
                            CyIntEnable(MIDI1_UART_RX_VECT_NUM);
                        #endif /* End MIDI1_UART_RXBUFFERSIZE >= 256 */
                    }

                    msgRtn = USBFS_1_ProcessMidiIn(rxData,
                                                    (USBFS_1_MIDI_RX_STATUS *)&USBFS_1_MIDI1_Event);

                    /* Read buffer loop condition to the local variable */
                    rxBufferLoopDetect = MIDI1_UART_rxBufferLoopDetect;
                }

            /* Finally, update the real output pointer, then return with
            *  an indication as to whether there's a complete message in the buffer.
            */
            #if ((MIDI1_UART_RXBUFFERSIZE >= 256u) && (CY_PSOC3))
                CyIntDisable(MIDI1_UART_RX_VECT_NUM);
            #endif /* End MIDI1_UART_RXBUFFERSIZE >= 256 */
            MIDI1_UART_rxBufferRead = rxBufferRead;
            #if ((MIDI1_UART_RXBUFFERSIZE >= 256u) && (CY_PSOC3))
                CyIntEnable(MIDI1_UART_RX_VECT_NUM);
            #endif /* End MIDI1_UART_RXBUFFERSIZE >= 256 */
        }

        return (msgRtn);
    }


    /*******************************************************************************
    * Function Name: USBFS_1_MIDI1_ProcessUsbOut
    ********************************************************************************
    *
    * Summary:
    *  Process a USB MIDI output event.
    *  Puts data into the MIDI TX output buffer.
    *
    * Parameters:
    *  *epBuf: pointer on MIDI event.
    *
    * Return:
    *   None
    *
    * Global variables:
    *  USBFS_1_MIDI1_TxRunStat: This variable used to save the MIDI
    *    status byte and skip to send the repeated status byte in subsequent event.
    *  USBFS_1_MIDI1_InqFlags: The following flags are set when SysEx
    *    message comes.
    *    USBFS_1_INQ_SYSEX_FLAG: Non-Real Time SySEx message received.
    *    USBFS_1_INQ_IDENTITY_REQ_FLAG: Identity Request received.
    *      This bit should be cleared by user when Identity Reply message generated.
    *
    *******************************************************************************/
    void USBFS_1_MIDI1_ProcessUsbOut(const uint8 epBuf[])
                                                            
    {
        uint8 cmd;
        uint8 len;
        uint8 i;

        /* User code is required at the beginning of the procedure */
        /* `#START MIDI1_PROCESS_OUT_BEGIN` */

        /* `#END` */

        cmd = epBuf[USBFS_1_EVENT_BYTE0] & USBFS_1_CIN_MASK;
        if((cmd != USBFS_1_RESERVED0) && (cmd != USBFS_1_RESERVED1))
        {
            len = USBFS_1_MIDI_SIZE[cmd];
            i = USBFS_1_EVENT_BYTE1;
            /* Universal System Exclusive message parsing */
            if(cmd == USBFS_1_SYSEX)
            {
                if((epBuf[USBFS_1_EVENT_BYTE1] == USBFS_1_MIDI_SYSEX) &&
                   (epBuf[USBFS_1_EVENT_BYTE2] == USBFS_1_MIDI_SYSEX_NON_REAL_TIME))
                {   /* Non-Real Time SySEx starts */
                    USBFS_1_MIDI1_InqFlags |= USBFS_1_INQ_SYSEX_FLAG;
                }
                else
                {
                    USBFS_1_MIDI1_InqFlags &= (uint8)~USBFS_1_INQ_SYSEX_FLAG;
                }
            }
            else if(cmd == USBFS_1_SYSEX_ENDS_WITH1)
            {
                USBFS_1_MIDI1_InqFlags &= (uint8)~USBFS_1_INQ_SYSEX_FLAG;
            }
            else if(cmd == USBFS_1_SYSEX_ENDS_WITH2)
            {
                USBFS_1_MIDI1_InqFlags &= (uint8)~USBFS_1_INQ_SYSEX_FLAG;
            }
            else if(cmd == USBFS_1_SYSEX_ENDS_WITH3)
            {
                /* Identify Request support */
                if((USBFS_1_MIDI1_InqFlags & USBFS_1_INQ_SYSEX_FLAG) != 0u)
                {
                    USBFS_1_MIDI1_InqFlags &= (uint8)~USBFS_1_INQ_SYSEX_FLAG;
                    if((epBuf[USBFS_1_EVENT_BYTE1] == USBFS_1_MIDI_SYSEX_GEN_INFORMATION) &&
                    (epBuf[USBFS_1_EVENT_BYTE2] == USBFS_1_MIDI_SYSEX_IDENTITY_REQ))
                    {   /* Set the flag about received the Identity Request.
                        *  The Identity Reply message may be send by user code.
                        */
                        USBFS_1_MIDI1_InqFlags |= USBFS_1_INQ_IDENTITY_REQ_FLAG;
                    }
                }
            }
            else /* Do nothing for other command */
            {
            }
            /* Running Status for Voice and Mode messages only. */
            if((cmd >= USBFS_1_NOTE_OFF) && ( cmd <= USBFS_1_PITCH_BEND_CHANGE))
            {
                if(USBFS_1_MIDI1_TxRunStat == epBuf[USBFS_1_EVENT_BYTE1])
                {   /* Skip the repeated Status byte */
                    i++;
                }
                else
                {   /* Save Status byte for next event */
                    USBFS_1_MIDI1_TxRunStat = epBuf[USBFS_1_EVENT_BYTE1];
                }
            }
            else
            {   /* Clear Running Status */
                USBFS_1_MIDI1_TxRunStat = 0u;
            }
            /* Puts data into the MIDI TX output buffer.*/
            do
            {
                MIDI1_UART_PutChar(epBuf[i]);
                i++;
            } while (i <= len);
        }

        /* User code is required at the end of the procedure */
        /* `#START MIDI1_PROCESS_OUT_END` */

        /* `#END` */
    }

#if (USBFS_1_MIDI_EXT_MODE >= USBFS_1_TWO_EXT_INTRF)


    /*******************************************************************************
    * Function Name: USBFS_1_MIDI2_GetEvent
    ********************************************************************************
    *
    * Summary:
    *  Checks for incoming MIDI data, calls the MIDI event builder if so.
    *  Returns either empty or with a complete event.
    *
    * Parameters:
    *  None
    *
    * Return:
    *   0, if no complete message
    *   1 - 4, if message complete
    *   MIDI_SYSEX, if start or continuation of system exclusive
    *   MIDI_EOSEX, if end of system exclusive
    *   0xf8 - 0xff, if single byte real time message
    *
    * Global variables:
    *  USBFS_1_MIDI2_Event: RX status structure used to parse received
    *    data.
    *
    *******************************************************************************/
    uint8 USBFS_1_MIDI2_GetEvent(void) 
    {
        uint8 msgRtn = 0u;
        uint8 rxData;
        #if (MIDI2_UART_RXBUFFERSIZE >= 256u)
            uint16 rxBufferRead;
            #if CY_PSOC3 /* This local variable required only for PSOC3 and large buffer */
                uint16 rxBufferWrite;
            #endif /* end CY_PSOC3 */
        #else
            uint8 rxBufferRead;
        #endif /* End MIDI2_UART_RXBUFFERSIZE >= 256 */
        uint8 rxBufferLoopDetect;
        /* Read buffer loop condition to the local variable */
        rxBufferLoopDetect = MIDI2_UART_rxBufferLoopDetect;

        if ( (MIDI2_UART_rxBufferRead != MIDI2_UART_rxBufferWrite) || (rxBufferLoopDetect != 0u) )
        {
            /* Protect variables that could change on interrupt by disabling Rx interrupt.*/
            #if ((MIDI2_UART_RXBUFFERSIZE >= 256u) && (CY_PSOC3))
                CyIntDisable(MIDI2_UART_RX_VECT_NUM);
            #endif /* End MIDI2_UART_RXBUFFERSIZE >= 256 */
            rxBufferRead = MIDI2_UART_rxBufferRead;
            #if ((MIDI2_UART_RXBUFFERSIZE >= 256u) && (CY_PSOC3))
                rxBufferWrite = MIDI2_UART_rxBufferWrite;
                CyIntEnable(MIDI2_UART_RX_VECT_NUM);
            #endif /* End MIDI2_UART_RXBUFFERSIZE >= 256 */

            /* Stay here until either the buffer is empty or we have a complete message
            *  in the message buffer. Note that we must use a temporary output pointer to
            *  since it takes two instructions to increment with a wrap, and we can't
            *  risk doing that with the real pointer and getting an interrupt in between
            *  instructions.
            */

            #if ((MIDI2_UART_RXBUFFERSIZE >= 256u) && (CY_PSOC3))
                while ( ((rxBufferRead != rxBufferWrite) || (rxBufferLoopDetect != 0u)) && (msgRtn == 0u) )
            #else
                while ( ((rxBufferRead != MIDI2_UART_rxBufferWrite) || (rxBufferLoopDetect != 0u)) && (msgRtn == 0u) )
            #endif /* End MIDI2_UART_RXBUFFERSIZE >= 256 && CY_PSOC3 */
                {
                    rxData = MIDI2_UART_rxBuffer[rxBufferRead];
                    rxBufferRead++;
                    if(rxBufferRead >= MIDI2_UART_RXBUFFERSIZE)
                    {
                        rxBufferRead = 0u;
                    }
                    /* If loop condition was set - update real read buffer pointer
                    *  to avoid overflow status
                    */
                    if(rxBufferLoopDetect != 0u )
                    {
                        MIDI2_UART_rxBufferLoopDetect = 0u;
                        #if ((MIDI2_UART_RXBUFFERSIZE >= 256u) && (CY_PSOC3))
                            CyIntDisable(MIDI2_UART_RX_VECT_NUM);
                        #endif /* End MIDI2_UART_RXBUFFERSIZE >= 256 */
                        MIDI2_UART_rxBufferRead = rxBufferRead;
                        #if ((MIDI2_UART_RXBUFFERSIZE >= 256u) && (CY_PSOC3))
                            CyIntEnable(MIDI2_UART_RX_VECT_NUM);
                        #endif /* End MIDI2_UART_RXBUFFERSIZE >= 256 */
                    }

                    msgRtn = USBFS_1_ProcessMidiIn(rxData,
                                                    (USBFS_1_MIDI_RX_STATUS *)&USBFS_1_MIDI2_Event);

                    /* Read buffer loop condition to the local variable */
                    rxBufferLoopDetect = MIDI2_UART_rxBufferLoopDetect;
                }

            /* Finally, update the real output pointer, then return with
            *  an indication as to whether there's a complete message in the buffer.
            */
            #if ((MIDI2_UART_RXBUFFERSIZE >= 256u) && (CY_PSOC3))
                CyIntDisable(MIDI2_UART_RX_VECT_NUM);
            #endif /* End MIDI2_UART_RXBUFFERSIZE >= 256 */
            MIDI2_UART_rxBufferRead = rxBufferRead;
            #if ((MIDI2_UART_RXBUFFERSIZE >= 256u) && (CY_PSOC3))
                CyIntEnable(MIDI2_UART_RX_VECT_NUM);
            #endif /* End MIDI2_UART_RXBUFFERSIZE >= 256 */
        }

        return (msgRtn);
    }


    /*******************************************************************************
    * Function Name: USBFS_1_MIDI2_ProcessUsbOut
    ********************************************************************************
    *
    * Summary:
    *  Process a USB MIDI output event.
    *  Puts data into the MIDI TX output buffer.
    *
    * Parameters:
    *  *epBuf: pointer on MIDI event.
    *
    * Return:
    *   None
    *
    * Global variables:
    *  USBFS_1_MIDI2_TxRunStat: This variable used to save the MIDI
    *    status byte and skip to send the repeated status byte in subsequent event.
    *  USBFS_1_MIDI2_InqFlags: The following flags are set when SysEx
    *    message comes.
    *  USBFS_1_INQ_SYSEX_FLAG: Non-Real Time SySEx message received.
    *  USBFS_1_INQ_IDENTITY_REQ_FLAG: Identity Request received.
    *    This bit should be cleared by user when Identity Reply message generated.
    *
    *******************************************************************************/
    void USBFS_1_MIDI2_ProcessUsbOut(const uint8 epBuf[])
                                                            
    {
        uint8 cmd;
        uint8 len;
        uint8 i;

        /* User code is required at the beginning of the procedure */
        /* `#START MIDI2_PROCESS_OUT_START` */

        /* `#END` */

        cmd = epBuf[USBFS_1_EVENT_BYTE0] & USBFS_1_CIN_MASK;
        if((cmd != USBFS_1_RESERVED0) && (cmd != USBFS_1_RESERVED1))
        {
            len = USBFS_1_MIDI_SIZE[cmd];
            i = USBFS_1_EVENT_BYTE1;
            /* Universal System Exclusive message parsing */
            if(cmd == USBFS_1_SYSEX)
            {
                if((epBuf[USBFS_1_EVENT_BYTE1] == USBFS_1_MIDI_SYSEX) &&
                   (epBuf[USBFS_1_EVENT_BYTE2] == USBFS_1_MIDI_SYSEX_NON_REAL_TIME))
                {   /* SySEx starts */
                    USBFS_1_MIDI2_InqFlags |= USBFS_1_INQ_SYSEX_FLAG;
                }
                else
                {
                    USBFS_1_MIDI2_InqFlags &= (uint8)~USBFS_1_INQ_SYSEX_FLAG;
                }
            }
            else if(cmd == USBFS_1_SYSEX_ENDS_WITH1)
            {
                USBFS_1_MIDI2_InqFlags &= (uint8)~USBFS_1_INQ_SYSEX_FLAG;
            }
            else if(cmd == USBFS_1_SYSEX_ENDS_WITH2)
            {
                USBFS_1_MIDI2_InqFlags &= (uint8)~USBFS_1_INQ_SYSEX_FLAG;
            }
            else if(cmd == USBFS_1_SYSEX_ENDS_WITH3)
            {
                /* Identify Request support */
                if((USBFS_1_MIDI2_InqFlags & USBFS_1_INQ_SYSEX_FLAG) != 0u)
                {
                    USBFS_1_MIDI2_InqFlags &= (uint8)~USBFS_1_INQ_SYSEX_FLAG;
                    if((epBuf[USBFS_1_EVENT_BYTE1] == USBFS_1_MIDI_SYSEX_GEN_INFORMATION) &&
                       (epBuf[USBFS_1_EVENT_BYTE2] == USBFS_1_MIDI_SYSEX_IDENTITY_REQ))
                    {   /* Set the flag about received the Identity Request.
                        *  The Identity Reply message may be send by user code.
                        */
                        USBFS_1_MIDI2_InqFlags |= USBFS_1_INQ_IDENTITY_REQ_FLAG;
                    }
                }
            }
            else /* Do nothing for other command */
            {
            }
            /* Running Status for Voice and Mode messages only. */
            if((cmd >= USBFS_1_NOTE_OFF) && ( cmd <= USBFS_1_PITCH_BEND_CHANGE))
            {
                if(USBFS_1_MIDI2_TxRunStat == epBuf[USBFS_1_EVENT_BYTE1])
                {   /* Skip the repeated Status byte */
                    i++;
                }
                else
                {   /* Save Status byte for next event */
                    USBFS_1_MIDI2_TxRunStat = epBuf[USBFS_1_EVENT_BYTE1];
                }
            }
            else
            {   /* Clear Running Status */
                USBFS_1_MIDI2_TxRunStat = 0u;
            }
            /* Puts data into the MIDI TX output buffer.*/
            do
            {
                MIDI2_UART_PutChar(epBuf[i]);
                i++;
            } while (i <= len);
        }

        /* User code is required at the end of the procedure */
        /* `#START MIDI2_PROCESS_OUT_END` */

        /* `#END` */
    }
#endif /* End USBFS_1_MIDI_EXT_MODE >= USBFS_1_TWO_EXT_INTRF */
#endif /* End USBFS_1_MIDI_EXT_MODE >= USBFS_1_ONE_EXT_INTRF */

#endif  /* End (USBFS_1_ENABLE_MIDI_API != 0u) */


/* `#START MIDI_FUNCTIONS` Place any additional functions here */

/* `#END` */

#endif  /* End defined(USBFS_1_ENABLE_MIDI_STREAMING) */


/* [] END OF FILE */
