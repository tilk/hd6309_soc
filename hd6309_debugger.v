module hd6309_debugger(
	input clk,
	input rst_n,
	input [15:0] addr,
	input [7:0] data_in,
	input [7:0] data_out,
	input e,
	input q,
	input bs,
	input ba,
	input rw,
	output reg st_valid,
	output reg [31:0] st_data
);

	reg done;
	
	reg [2:0] e_buf;
	reg [2:0] q_buf;
	
	reg ba_buf;
	
	wire cinit = ~e_buf[1] & ~q_buf[1];
	wire crun = ~e_buf[2] & ~q_buf[2];
	
	// synchronizer
	always @(posedge clk)
	begin
		e_buf <= {e_buf[1:0], e};
		q_buf <= {q_buf[1:0], q};
	end
	
	// latch
	always @(negedge e)
	begin
		st_data <= {addr, rw ? data_in : data_out, rw, bs, 6'b0};
		ba_buf <= ba;
	end

	always @(posedge clk or negedge rst_n)
		if (~rst_n) begin
			done <= 1'b0;
			st_valid <= 1'b0;
		end else if (cinit & ~crun)
			done <= 1'b0;
		else if (crun && st_valid) 
			st_valid <= 1'b0;
		else if (crun && ~done && ~ba_buf) begin
			done <= 1'b1;
			st_valid <= 1'b1;
		end

endmodule
