
//`#start header` -- edit after this line, do not edit this line
//	Copyright (C) 2013 Michael McMaster <michael@codesrc.com>
//
//	This file is part of SCSI2SD.
//
//	SCSI2SD is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	SCSI2SD is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with SCSI2SD.  If not, see <http://www.gnu.org/licenses/>.
`include "cypress.v"
//`#end` -- edit above this line, do not edit this line
// Generated on 10/16/2013 at 00:01
// Component: scsiTarget
module scsiTarget (
	output [7:0] DBx_out, // Active High, connected to SCSI bus via inverter
	output  REQ, // Active High, connected to SCSI bus via inverter
	input   nACK, // Active LOW, connected directly to SCSI bus.
	input  [7:0] nDBx_in, // Active LOW, connected directly to SCSI bus.
	input   IO, // Active High, set by CPU via status register.
	input   nRST, // Active LOW, connected directly to SCSI bus.
	input   clk
);


//`#start body` -- edit after this line, do not edit this line

/////////////////////////////////////////////////////////////////////////////
// Force Clock Sync
/////////////////////////////////////////////////////////////////////////////
// The udb_clock_enable primitive component is used to indicate that the input
// clock must always be synchronous and if not implement synchronizers to make
// it synchronous.
wire op_clk;
cy_psoc3_udb_clock_enable_v1_0 #(.sync_mode(`TRUE)) ClkSync
(
    .clock_in(clk),
    .enable(1'b1),
    .clock_out(op_clk)
);

/////////////////////////////////////////////////////////////////////////////
// FIFO Status Register
/////////////////////////////////////////////////////////////////////////////
// Status Register: scsiTarget_StatusReg__STATUS_REG
//     Bit 0: Tx FIFO not full
//     Bit 1: Rx FIFO not empty
//     Bit 2: Tx FIFO empty
//     Bit 3: Rx FIFO full
//
// TX FIFO Register: scsiTarget_scsiTarget_u0__F0_REG
// RX FIFO Register: scsiTarget_scsiTarget_u0__F1_REG
// Use with CY_GET_REG8 and CY_SET_REG8
wire f0_bus_stat;   // Tx FIFO not full
wire f0_blk_stat;	// Tx FIFO empty
wire f1_bus_stat;	// Rx FIFO not empty
wire f1_blk_stat;	// Rx FIFO full
cy_psoc3_status #(.cy_force_order(1), .cy_md_select(8'h00)) StatusReg
(
    /* input          */  .clock(op_clk),
    /* input  [04:00] */  .status({4'b0, f1_blk_stat, f0_blk_stat, f1_bus_stat, f0_bus_stat})
);

/////////////////////////////////////////////////////////////////////////////
// CONSTANTS
/////////////////////////////////////////////////////////////////////////////
localparam IO_WRITE = 1'b1;
localparam IO_READ = 1'b0;

/////////////////////////////////////////////////////////////////////////////
// STATE MACHINE
/////////////////////////////////////////////////////////////////////////////
// TX States:
// IDLE
//     Wait for an entry in the FIFO, and for the SCSI Initiator to be ready
// FIFOLOAD
//     Load F0 into A0. Feed (old) A0 into the ALU SRCA.
// TX
//     Load data register from PO. PO is fed by A0 going into the ALU via SRCA
//     A0 must remain unchanged.
// DESKEW_INIT
//     DBx output signals will be output in this state
//     Load deskew clock count into A0 from D0
// DESKEW
//     DBx output signals will be output in this state
//     Wait for the SCSI deskew time of 55ms. (DEC A0).
//     A1 must be fed into SRCA, so PO is now useless.
// READY
//     REQ and DBx output signals will be output in this state
//     Wait for acknowledgement from the SCSI initiator.
// RX
//     Dummy state for flow control.
//     REQ signal will be output in this state
//     PI enabled for input into ALU "PASS" operation, storing into F1.
//
// RX States:
// IDLE
//     Wait for a dummy "enabling" entry in the input FIFO, and wait for space
//     in output the FIFO, and for the SCSI Initiator to be ready
// FIFOLOAD
//     Load F0 into A0.
//     The input FIFO is used to control the number of bytes we attempt to
//     read from the SCSI bus.
// READY
//     REQ signal will be output in this state
//     Wait for the initiator to send a byte on the SCSI bus.
// RX
//     REQ signal will be output in this state
//     PI enabled for input into ALU "PASS" operation, storing into F1.


localparam STATE_IDLE = 3'b000;
localparam STATE_FIFOLOAD = 3'b001;
localparam STATE_TX = 3'b010;
localparam STATE_DESKEW_INIT = 3'b011;
localparam STATE_DESKEW = 3'b100;
// This state intentionally not used.
localparam STATE_READY = 3'b110;
localparam STATE_RX = 3'b111;

// state selects the datapath register.
reg[2:0] state;

// Data being read/written from/to the SCSI bus
reg[7:0] data;

// Set by the datapath zero detector (z1). High when A1 counts down to zero.
wire deskewComplete;

// Parallel input to the datapath SRCA.
// Selected for input through to the ALU if CFB EN bit set for the datapath
// state and enabled by PI DYN bit in CFG15-14
wire[7:0] pi;

// Parallel output from the selected SRCA value (A0 or A1) to the ALU.
wire[7:0] po;

// Set true to trigger storing A1 into F1.
wire fifoStore;

// Set Output Pins
assign REQ = state[1] & state[2]; // STATE_READY & STATE_RX
assign DBx_out[7:0] = data;
assign pi[7:0] = ~nDBx_in[7:0]; // Invert active low scsi bus
assign fifoStore = (state == STATE_RX) ? 1'b1 : 1'b0;

always @(posedge op_clk) begin
	case (state)
		STATE_IDLE:
		begin
			// Check that SCSI initiator is ready, and input FIFO is not empty,
			// and output FIFO is not full.
			// Note that output FIFO is unused in TX mode.
			if (nACK & !f0_blk_stat && !f1_blk_stat)
				state <= STATE_FIFOLOAD;
			else
				state <= STATE_IDLE;

			// Clear our output pins
			data <= 8'b0;
		end

		STATE_FIFOLOAD:
			state <= IO == IO_WRITE ? STATE_TX : STATE_READY;

		STATE_TX:
		begin
			state <= STATE_DESKEW_INIT;
			data <= po;
		end

		STATE_DESKEW_INIT: state <= STATE_DESKEW;

		STATE_DESKEW:
			if(deskewComplete) state <= STATE_READY;
			else state <= STATE_DESKEW;

		STATE_READY:
			if (~nACK) state <= STATE_RX;
			else state <= STATE_READY;

		STATE_RX: state <= STATE_IDLE;

		default: state <= STATE_IDLE;
	endcase
end

// D1 is used for the deskew count.
// The data output is valid during the DESKEW_INIT phase as well,
// so we subtract 1.
// D1 = [0.000000055 / (1 / clk)] - 1
cy_psoc3_dp #(.d1_init(3), 
.cy_dpconfig(
{
    `CS_ALU_OP_PASS, `CS_SRCA_A0, `CS_SRCB_D0,
    `CS_SHFT_OP_PASS, `CS_A0_SRC_NONE, `CS_A1_SRC_NONE,
    `CS_FEEDBACK_DSBL, `CS_CI_SEL_CFGA, `CS_SI_SEL_CFGA,
    `CS_CMP_SEL_CFGA, /*CFGRAM0:         IDLE*/
    `CS_ALU_OP_PASS, `CS_SRCA_A0, `CS_SRCB_D0,
    `CS_SHFT_OP_PASS, `CS_A0_SRC___F0, `CS_A1_SRC_NONE,
    `CS_FEEDBACK_DSBL, `CS_CI_SEL_CFGA, `CS_SI_SEL_CFGA,
    `CS_CMP_SEL_CFGA, /*CFGRAM1:         FIFO Load*/
    `CS_ALU_OP_PASS, `CS_SRCA_A0, `CS_SRCB_D0,
    `CS_SHFT_OP_PASS, `CS_A0_SRC_NONE, `CS_A1_SRC_NONE,
    `CS_FEEDBACK_DSBL, `CS_CI_SEL_CFGA, `CS_SI_SEL_CFGA,
    `CS_CMP_SEL_CFGA, /*CFGRAM2:         TX*/
    `CS_ALU_OP_PASS, `CS_SRCA_A0, `CS_SRCB_D0,
    `CS_SHFT_OP_PASS, `CS_A0_SRC___D0, `CS_A1_SRC_NONE,
    `CS_FEEDBACK_DSBL, `CS_CI_SEL_CFGA, `CS_SI_SEL_CFGA,
    `CS_CMP_SEL_CFGA, /*CFGRAM3:         DESKEW INIT*/
    `CS_ALU_OP__DEC, `CS_SRCA_A0, `CS_SRCB_D0,
    `CS_SHFT_OP_PASS, `CS_A0_SRC__ALU, `CS_A1_SRC_NONE,
    `CS_FEEDBACK_DSBL, `CS_CI_SEL_CFGA, `CS_SI_SEL_CFGA,
    `CS_CMP_SEL_CFGA, /*CFGRAM4:         DESKEW*/
    `CS_ALU_OP_PASS, `CS_SRCA_A0, `CS_SRCB_D0,
    `CS_SHFT_OP_PASS, `CS_A0_SRC_NONE, `CS_A1_SRC_NONE,
    `CS_FEEDBACK_DSBL, `CS_CI_SEL_CFGA, `CS_SI_SEL_CFGA,
    `CS_CMP_SEL_CFGA, /*CFGRAM5:   Not used*/
    `CS_ALU_OP_PASS, `CS_SRCA_A0, `CS_SRCB_D0,
    `CS_SHFT_OP_PASS, `CS_A0_SRC_NONE, `CS_A1_SRC_NONE,
    `CS_FEEDBACK_DSBL, `CS_CI_SEL_CFGA, `CS_SI_SEL_CFGA,
    `CS_CMP_SEL_CFGA, /*CFGRAM6:         READY*/
    `CS_ALU_OP_PASS, `CS_SRCA_A0, `CS_SRCB_D0,
    `CS_SHFT_OP_PASS, `CS_A0_SRC_NONE, `CS_A1_SRC_NONE,
    `CS_FEEDBACK_ENBL, `CS_CI_SEL_CFGA, `CS_SI_SEL_CFGA,
    `CS_CMP_SEL_CFGA, /*CFGRAM7:         RX*/
    8'hFF, 8'h00,  /*CFG9:            */
    8'hFF, 8'hFF,  /*CFG11-10:            */
    `SC_CMPB_A1_D1, `SC_CMPA_A1_D1, `SC_CI_B_ARITH,
    `SC_CI_A_ARITH, `SC_C1_MASK_DSBL, `SC_C0_MASK_DSBL,
    `SC_A_MASK_DSBL, `SC_DEF_SI_0, `SC_SI_B_DEFSI,
    `SC_SI_A_DEFSI, /*CFG13-12:            */
    `SC_A0_SRC_ACC, `SC_SHIFT_SL, `SC_PI_DYN_EN,
    1'h0, `SC_FIFO1_ALU, `SC_FIFO0_BUS,
    `SC_MSB_DSBL, `SC_MSB_BIT0, `SC_MSB_NOCHN,
    `SC_FB_NOCHN, `SC_CMP1_NOCHN,
    `SC_CMP0_NOCHN, /*CFG15-14:            */
    10'h00, `SC_FIFO_CLK__DP,`SC_FIFO_CAP_AX,
    `SC_FIFO_LEVEL,`SC_FIFO__SYNC,`SC_EXTCRC_DSBL,
    `SC_WRK16CAT_DSBL /*CFG17-16:            */
}
)) datapath(
        /*  input                   */  .reset(1'b0),
        /*  input                   */  .clk(op_clk),
        /*  input   [02:00]         */  .cs_addr(state),
        /*  input                   */  .route_si(1'b0),
        /*  input                   */  .route_ci(1'b0),
        /*  input                   */  .f0_load(1'b0),
        /*  input                   */  .f1_load(fifoStore),
        /*  input                   */  .d0_load(1'b0),
        /*  input                   */  .d1_load(1'b0),
        /*  output                  */  .ce0(),
        /*  output                  */  .cl0(),
        /*  output                  */  .z0(deskewComplete),
        /*  output                  */  .ff0(),
        /*  output                  */  .ce1(),
        /*  output                  */  .cl1(),
        /*  output                  */  .z1(),
        /*  output                  */  .ff1(),
        /*  output                  */  .ov_msb(),
        /*  output                  */  .co_msb(),
        /*  output                  */  .cmsb(),
        /*  output                  */  .so(),
        /*  output                  */  .f0_bus_stat(f0_bus_stat),
        /*  output                  */  .f0_blk_stat(f0_blk_stat),
        /*  output                  */  .f1_bus_stat(f1_bus_stat),
        /*  output                  */  .f1_blk_stat(f1_blk_stat),
        
        /* input                    */  .ci(1'b0),     // Carry in from previous stage
        /* output                   */  .co(),         // Carry out to next stage
        /* input                    */  .sir(1'b0),    // Shift in from right side
        /* output                   */  .sor(),        // Shift out to right side
        /* input                    */  .sil(1'b0),    // Shift in from left side
        /* output                   */  .sol(),        // Shift out to left side
        /* input                    */  .msbi(1'b0),   // MSB chain in
        /* output                   */  .msbo(),       // MSB chain out
        /* input [01:00]            */  .cei(2'b0),    // Compare equal in from prev stage
        /* output [01:00]           */  .ceo(),        // Compare equal out to next stage
        /* input [01:00]            */  .cli(2'b0),    // Compare less than in from prv stage
        /* output [01:00]           */  .clo(),        // Compare less than out to next stage
        /* input [01:00]            */  .zi(2'b0),     // Zero detect in from previous stage
        /* output [01:00]           */  .zo(),         // Zero detect out to next stage
        /* input [01:00]            */  .fi(2'b0),     // 0xFF detect in from previous stage
        /* output [01:00]           */  .fo(),         // 0xFF detect out to next stage
        /* input [01:00]            */  .capi(2'b0),   // Software capture from previous stage
        /* output [01:00]           */  .capo(),       // Software capture to next stage
        /* input                    */  .cfbi(1'b0),   // CRC Feedback in from previous stage
        /* output                   */  .cfbo(),       // CRC Feedback out to next stage
        /* input [07:00]            */  .pi(pi),     // Parallel data port
        /* output [07:00]           */  .po(po)          // Parallel data port
);
//`#end` -- edit above this line, do not edit this line
endmodule
//`#start footer` -- edit after this line, do not edit this line
//`#end` -- edit above this line, do not edit this line
