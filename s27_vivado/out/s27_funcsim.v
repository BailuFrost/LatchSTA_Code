// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// --------------------------------------------------------------------------------
// Tool Version: Vivado v.2024.2 (win64) Build 5239630 Fri Nov 08 22:35:27 MST 2024
// Date        : Mon Sep 21 15:02:32 2026
// Host        : XiaoB running 64-bit major release  (build 9200)
// Command     : write_verilog -force -mode funcsim e:/LatchSTA_Code/code/s27_vivado/out/s27_funcsim.v
// Design      : s27
// Purpose     : This verilog netlist is a functional simulation representation of the design and should not be modified
//               or synthesized. This netlist cannot be used for SDF annotated simulation.
// Device      : xc7a35tcpg236-1
// --------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

(* ECO_CHECKSUM = "af860ea5" *) 
(* NotValidForBitStream *)
(* \DesignAttr:ENABLE_NOC_NETLIST_VIEW  *) 
(* \DesignAttr:ENABLE_AIE_NETLIST_VIEW  *) 
module s27
   (clk,
    G0,
    G1,
    G2,
    G3,
    G17);
  input clk;
  input G0;
  input G1;
  input G2;
  input G3;
  output G17;

  wire G0;
  wire G0_IBUF;
  wire G1;
  (* DONT_TOUCH *) (* RTL_KEEP = "true" *) wire G10;
  (* DONT_TOUCH *) (* RTL_KEEP = "true" *) wire G11;
  (* DONT_TOUCH *) (* RTL_KEEP = "true" *) wire G12;
  (* DONT_TOUCH *) (* RTL_KEEP = "true" *) wire G13;
  (* DONT_TOUCH *) (* RTL_KEEP = "true" *) wire G14;
  (* DONT_TOUCH *) (* RTL_KEEP = "true" *) wire G15;
  (* DONT_TOUCH *) (* RTL_KEEP = "true" *) wire G16;
  wire G17;
  wire G17_OBUF;
  wire G1_IBUF;
  wire G2;
  wire G2_IBUF;
  wire G3;
  wire G3_IBUF;
  (* DONT_TOUCH *) (* RTL_KEEP = "true" *) wire G5;
  (* DONT_TOUCH *) (* RTL_KEEP = "true" *) wire G6;
  (* DONT_TOUCH *) (* RTL_KEEP = "true" *) wire G7;
  (* DONT_TOUCH *) (* RTL_KEEP = "true" *) wire G8;
  (* DONT_TOUCH *) (* RTL_KEEP = "true" *) wire G9;
  wire clk;
  wire clk_IBUF;
  wire clk_IBUF_BUFG;

  IBUF G0_IBUF_inst
       (.I(G0),
        .O(G0_IBUF));
  LUT2 #(
    .INIT(4'h1)) 
    G10_inferred_i_1
       (.I0(G14),
        .I1(G11),
        .O(G10));
  LUT2 #(
    .INIT(4'h1)) 
    G11_inferred_i_1
       (.I0(G5),
        .I1(G9),
        .O(G11));
  LUT2 #(
    .INIT(4'h1)) 
    G12_inferred_i_1
       (.I0(G1_IBUF),
        .I1(G7),
        .O(G12));
  LUT2 #(
    .INIT(4'h1)) 
    G13_inferred_i_1
       (.I0(G2_IBUF),
        .I1(G12),
        .O(G13));
  LUT1 #(
    .INIT(2'h1)) 
    G14_inferred_i_1
       (.I0(G0_IBUF),
        .O(G14));
  LUT2 #(
    .INIT(4'hE)) 
    G15_inferred_i_1
       (.I0(G12),
        .I1(G8),
        .O(G15));
  LUT2 #(
    .INIT(4'hE)) 
    G16_inferred_i_1
       (.I0(G3_IBUF),
        .I1(G8),
        .O(G16));
  OBUF G17_OBUF_inst
       (.I(G17_OBUF),
        .O(G17));
  LUT1 #(
    .INIT(2'h1)) 
    G17_OBUF_inst_i_1
       (.I0(G11),
        .O(G17_OBUF));
  IBUF G1_IBUF_inst
       (.I(G1),
        .O(G1_IBUF));
  IBUF G2_IBUF_inst
       (.I(G2),
        .O(G2_IBUF));
  IBUF G3_IBUF_inst
       (.I(G3),
        .O(G3_IBUF));
  (* DONT_TOUCH *) 
  (* KEEP = "yes" *) 
  FDRE #(
    .INIT(1'b0)) 
    G5_reg
       (.C(clk_IBUF_BUFG),
        .CE(1'b1),
        .D(G10),
        .Q(G5),
        .R(1'b0));
  (* DONT_TOUCH *) 
  (* KEEP = "yes" *) 
  FDRE #(
    .INIT(1'b0)) 
    G6_reg
       (.C(clk_IBUF_BUFG),
        .CE(1'b1),
        .D(G11),
        .Q(G6),
        .R(1'b0));
  (* DONT_TOUCH *) 
  (* KEEP = "yes" *) 
  FDRE #(
    .INIT(1'b0)) 
    G7_reg
       (.C(clk_IBUF_BUFG),
        .CE(1'b1),
        .D(G13),
        .Q(G7),
        .R(1'b0));
  LUT2 #(
    .INIT(4'h8)) 
    G8_inferred_i_1
       (.I0(G14),
        .I1(G6),
        .O(G8));
  LUT2 #(
    .INIT(4'h7)) 
    G9_inferred_i_1
       (.I0(G16),
        .I1(G15),
        .O(G9));
  BUFG clk_IBUF_BUFG_inst
       (.I(clk_IBUF),
        .O(clk_IBUF_BUFG));
  IBUF clk_IBUF_inst
       (.I(clk),
        .O(clk_IBUF));
endmodule
`ifndef GLBL
`define GLBL
`timescale  1 ps / 1 ps

module glbl ();

    parameter ROC_WIDTH = 100000;
    parameter TOC_WIDTH = 0;
    parameter GRES_WIDTH = 10000;
    parameter GRES_START = 10000;

//--------   STARTUP Globals --------------
    wire GSR;
    wire GTS;
    wire GWE;
    wire PRLD;
    wire GRESTORE;
    tri1 p_up_tmp;
    tri (weak1, strong0) PLL_LOCKG = p_up_tmp;

    wire PROGB_GLBL;
    wire CCLKO_GLBL;
    wire FCSBO_GLBL;
    wire [3:0] DO_GLBL;
    wire [3:0] DI_GLBL;
   
    reg GSR_int;
    reg GTS_int;
    reg PRLD_int;
    reg GRESTORE_int;

//--------   JTAG Globals --------------
    wire JTAG_TDO_GLBL;
    wire JTAG_TCK_GLBL;
    wire JTAG_TDI_GLBL;
    wire JTAG_TMS_GLBL;
    wire JTAG_TRST_GLBL;

    reg JTAG_CAPTURE_GLBL;
    reg JTAG_RESET_GLBL;
    reg JTAG_SHIFT_GLBL;
    reg JTAG_UPDATE_GLBL;
    reg JTAG_RUNTEST_GLBL;

    reg JTAG_SEL1_GLBL = 0;
    reg JTAG_SEL2_GLBL = 0 ;
    reg JTAG_SEL3_GLBL = 0;
    reg JTAG_SEL4_GLBL = 0;

    reg JTAG_USER_TDO1_GLBL = 1'bz;
    reg JTAG_USER_TDO2_GLBL = 1'bz;
    reg JTAG_USER_TDO3_GLBL = 1'bz;
    reg JTAG_USER_TDO4_GLBL = 1'bz;

    assign (strong1, weak0) GSR = GSR_int;
    assign (strong1, weak0) GTS = GTS_int;
    assign (weak1, weak0) PRLD = PRLD_int;
    assign (strong1, weak0) GRESTORE = GRESTORE_int;

    initial begin
	GSR_int = 1'b1;
	PRLD_int = 1'b1;
	#(ROC_WIDTH)
	GSR_int = 1'b0;
	PRLD_int = 1'b0;
    end

    initial begin
	GTS_int = 1'b1;
	#(TOC_WIDTH)
	GTS_int = 1'b0;
    end

    initial begin 
	GRESTORE_int = 1'b0;
	#(GRES_START);
	GRESTORE_int = 1'b1;
	#(GRES_WIDTH);
	GRESTORE_int = 1'b0;
    end

endmodule
`endif
