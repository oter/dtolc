module top(
	i_clk,
	i_rst_n,
	i_sync,
	i_cmd,
	i_data,
	o_data,
	o_sync,
	o_rx_int,
	o_tx_int,

	o_led_0,
	o_led_1
);

	input i_clk;
	input i_rst_n;
	output o_rx_int;
	output o_tx_int;
	output o_led_0;
	output o_led_1;


	assign o_data = output_data;
	assign o_sync = sync;


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

	wire push_write_index;
	wire pop_write_index;
	wire push_frame;

	transmitter tx_inst(
		.i_clk(i_clk),
		.i_clk_2x(c0),
		.i_rst_n(i_rst_n),
		.i_push_write_index(push_write_index),
		.i_pop_write_index(pop_write_index),
		.i_data(tx_i_data),
		.i_data_we(tx_i_data_we),
		.i_push_frame(push_frame),
		.o_data_size(tx_o_data_count),
		.o_frames_count(tx_o_frames_count),
		.o_status(tx_o_status),
		.o_tx(tx_o_tx)
	);


endmodule