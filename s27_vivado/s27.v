// ISCAS'89 s27, structural. Three DFFs named G5/G6/G7 to match s27.blif.out.
// clk is added; the original .bench left the clock implicit.
module s27 (
    input  wire clk,
    input  wire G0,
    input  wire G1,
    input  wire G2,
    input  wire G3,
    output wire G17
);
    (* DONT_TOUCH = "true", KEEP = "true" *) reg G5;
    (* DONT_TOUCH = "true", KEEP = "true" *) reg G6;
    (* DONT_TOUCH = "true", KEEP = "true" *) reg G7;

    (* DONT_TOUCH = "true", KEEP = "true" *) wire G8, G9, G10, G11, G12, G13, G14, G15, G16;

    assign G14 = ~G0;
    assign G17 = ~G11;
    assign G8  = G14 & G6;
    assign G15 = G12 | G8;
    assign G16 = G3  | G8;
    assign G9  = ~(G16 & G15);
    assign G10 = ~(G14 | G11);
    assign G11 = ~(G5  | G9);
    assign G12 = ~(G1  | G7);
    assign G13 = ~(G2  | G12);

    always @(posedge clk) begin
        G5 <= G10;
        G6 <= G11;
        G7 <= G13;
    end
endmodule
