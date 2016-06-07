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
	o_tx,

	o_led_0,
	o_led_1
);

	input i_clk;
	input i_rst_n;
	input i_sync;
	input [3:0] i_cmd;
	input [7:0] i_data;
	output [7:0] o_data;
	output o_sync;
	output o_rx_int;
	output o_tx_int;
	output o_tx; 

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

	wire tx_rst_n;
	wire tx_push_write_index;
	wire tx_pop_write_index;
	wire [7:0] tx_data;
	wire tx_data_we;
	wire tx_push_frame;
	wire [15:0] tx_data_count;
	wire [7:0] tx_frames_count;
	wire [7:0] tx_status;

	wire local_rx_locked;
	wire remote_rx_locked;
	assign local_rx_locked = 1'b1;
	assign remote_rx_locked = 1'b1;
	
	transmitter tx_inst(
		.i_clk(i_clk),
		.i_clk_2x(c0),
		.i_rst_n(tx_rst_n),
		.i_local_rx_locked(local_rx_locked),
		.i_remote_rx_locked(remote_rx_locked),
		.i_push_write_index(tx_push_write_index),
		.i_pop_write_index(tx_pop_write_index),
		.i_data(tx_data),
		.i_data_we(tx_data_we),
		.i_push_frame(tx_push_frame),
		.o_data_size(tx_data_count),
		.o_frames_count(tx_frames_count),
		.o_status(tx_status),
		.o_tx(o_tx)
	);


	io_module im_inst(
		.i_clk(i_clk),
		.i_rst_n(i_rst_n),
		.i_sync(i_sync),
		.i_cmd(i_cmd),
		.i_data(i_data),
		.o_data(o_data),
		.o_sync(o_sync),
		.o_rx_int(o_rx_int),
		.o_tx_int(o_tx_int),

		// transmitter
		.i_tx_data_size(tx_data_count),
		.i_tx_frames_count(tx_frames_count),
		.i_tx_status(tx_status),
		.o_tx_rst_n(tx_rst_n),
		.o_tx_push_write_index(tx_push_write_index),
		.o_tx_pop_write_index(tx_pop_write_index),
		.o_tx_data(tx_data),
		.o_tx_data_we(tx_data_we),
		.o_tx_push_frame(tx_push_frame)
	);

endmodule