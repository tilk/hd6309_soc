module hd6309_avalon_master #(
	parameter WIDTH = 16
) (
   input clk,
	input reset,
	
	output reg [WIDTH-1:0] master_address,
	output reg master_read,
	output reg master_write,
	input [7:0] master_readdata,
	output reg [7:0] master_writedata,
	input master_waitrequest,
	input master_readdatavalid,
	input master_writeresponsevalid,
	input [1:0] master_response,
	
	input [WIDTH-1:0] bus_address,
	output reg [7:0] bus_data_in,
	input [7:0] bus_data_out,
	input bus_rw,
	input bus_e,
	input bus_q,
	input bus_bs,
	input bus_ba,
	output bus_mrdy,
	input bus_reset_n
);

	reg [2:0] bus_e_buf;
	reg [2:0] bus_q_buf;
	
	reg read_done, write_done, wait_fffe;
	
	wire idle = ~master_read & ~master_write;
	wire cstart = ~bus_e_buf[2] & bus_q_buf[2];
	wire cinit = ~bus_e_buf[1] & ~bus_q_buf[1];
	
	// mrdy logic
	assign bus_mrdy = ~(master_read | master_write);
	
	// synchronizer
	always @(posedge clk)
	begin
		bus_e_buf <= {bus_e_buf[1:0], bus_e};
		bus_q_buf <= {bus_q_buf[1:0], bus_q};
	end

	// address latching
	always @(posedge clk)
	if (idle & cstart)
		master_address <= bus_address;
	/*
	always @(posedge clk or posedge reset or negedge bus_reset_n)
	if (reset || ~bus_reset_n)
		wait_fffe <= 1;
	else if (idle & cstart & ~bus_ba & bus_address == 'hfffe)
		wait_fffe <= 0;
	*/
	// read logic
	always @(posedge clk or posedge reset or negedge bus_reset_n)
	if (reset || ~bus_reset_n) begin
		master_read <= 0;
		read_done <= 0;
	end else if (idle & cstart & bus_rw & ~bus_ba & ~read_done/* & ~wait_fffe*/)
		master_read <= 1;
	else if (master_read & ~master_waitrequest) begin
	   master_read <= 0;
		bus_data_in <= master_readdata;
		read_done <= 1;
	end else if (cinit)
		read_done <= 0;

	// write logic
	always @(posedge clk or posedge reset or negedge bus_reset_n)
	if (reset || ~bus_reset_n) begin
		master_write <= 0;
		write_done <= 0;
	end else if (idle & cstart & ~bus_rw & ~bus_ba & ~write_done/* & ~wait_fffe*/) begin
		master_write <= 1;
		master_writedata <= bus_data_out;
	end else if (master_write & ~master_waitrequest) begin
		master_write <= 0;
		write_done <= 1;
	end else if (cinit)
		write_done <= 0;
	
endmodule
