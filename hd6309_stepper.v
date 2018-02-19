module hd6309_stepper(
	input q,
	input halt,
	input step_btn,
	output halt_mpu
);

	reg clicked;
	reg active;
	
	assign halt_mpu = halt & ~active;

	always @(posedge step_btn or negedge q)
	if (step_btn)
		clicked <= 1;
	else if (~q & clicked & ~active) begin
		clicked <= 0;
		active <= 1;
	end else if (~q & active)
		active <= 0;
	
endmodule
