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

wire [7:0] tx_i_data;
wire tx_i_data_we;
wire [15:0] tx_o_data_count;
wire [7:0] tx_o_frames_count;
wire [7:0] tx_o_status;
wire [7:0] tx_o_tx;

m_transmitter tx_inst(
	.i_clk(i_clk),
	.i_clk_2x(x0),
	.i_rst_n(i_rst_n),
	.i_data(tx_i_data),
	.i_data_we(tx_i_data_we),
	.o_data_count(tx_o_data_count),
	.o_frames_count(tx_o_frames_count),
	.o_status(tx_o_status),
	.o_tx(tx_o_tx)
);


endmodule