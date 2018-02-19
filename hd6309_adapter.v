module hd6309_adapter(
	inout [35:0] gpio,
	input clk,
	input rst_n,
	input nmi_n,
	input irq_n,
	input firq_n,
	input halt_n,
	input mrdy,
	input dma_n,
	output q,
	output e,
	output bs,
	output ba,
	output rw,
	input [7:0] data_in,
	output [7:0] data_out,
	output [15:0] addr_out
);

  assign gpio[1] = nmi_n;
  assign gpio[2] = rst_n ? halt_n : 1'b0;
  assign gpio[3] = clk;
  assign gpio[4] = irq_n;
  assign gpio[5] = rst_n ? firq_n : 1'b0;
  assign gpio[6] = mrdy;
  assign gpio[7] = dma_n;
  assign ba = gpio[16];
  assign bs = gpio[17];
  assign e = gpio[18];
  assign q = gpio[19];
  assign rw = gpio[0];
  assign gpio[0] = ba ? 1'b1 : 1'bZ;
  assign addr_out = gpio[35:20];
  assign data_out = gpio[15:8];
  assign gpio[15:8] = (rw | ba) ? data_in : 8'bZ;
  assign gpio[35:20] = ba ? 16'b0 : 16'bZ;

endmodule
