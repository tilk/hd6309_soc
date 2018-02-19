module address_translator(
	input [15:0] hd6309_addr,
	input hd6309_rw,
	output reg [19:0] hd6309_addr_ext
);

   always @(hd6309_addr or hd6309_rw)
		if (hd6309_addr >= 16'hF800) 
			hd6309_addr_ext = 20'h10800 + hd6309_addr - 16'hF800;
		else if (hd6309_addr == 16'hF7FE && hd6309_rw)
			hd6309_addr_ext = 20'h10100;
		else if (hd6309_addr == 16'hF7FE && ~hd6309_rw)
			hd6309_addr_ext = 20'h10200;
		else
			hd6309_addr_ext = 20'h00000 + hd6309_addr;

endmodule
