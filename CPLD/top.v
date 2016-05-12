module top(i_clk, i_rst_n, o_led_0, o_led_1);

input i_clk;
input i_rst_n;
output o_led_0;
output o_led_1;



reg led;
reg [31:0] counter;

assign o_led_0 = led;

wire pfdena;
wire pllena_sig;
wire c0;
pll	pll_inst (
	.areset 	(!i_rst_n),
	.inclk0 ( i_clk ),
	.pfdena ( pfdena ),
	.pllena ( pllena_sig ),
	.c0 		( c0 ),
	.locked ( o_led_1 )
	);
	
assign pfdena = 1;
assign pllena_sig = 1;


always @(posedge c0 or negedge i_rst_n) begin
	if (!i_rst_n) begin
	  counter <= 0;
	  led <= 0;
	end else begin
		if (counter == 29) begin
			counter <= 0;
			led <= !led;
		end else begin
			counter <= counter + 1;
		end
	end
end


endmodule