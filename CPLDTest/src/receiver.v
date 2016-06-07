module receiver(
	i_clk,
	i_rst_n,
	i_rec_clk,
	o_rec_clk,
	i_rx,
	i_data_re,
	i_pop_frame,
	o_local_rx_locked,
	o_remote_rx_locked,
	o_data,
	o_data_size,
	o_frames_count,
	o_status,
);

	input i_clk;
	input i_rst_n;
	input i_rec_clk; // done
	output o_rec_clk; // done
	input i_rx;
	input i_data_re;
	input i_pop_frame;
	output o_local_rx_locked; // done
	output o_remote_rx_locked;
	output [7:0] o_data; // done
	output [15:0] o_data_size; // done
	output [7:0] o_frames_count;
	output [15:0] o_status; // done


	

	// generate rec_clk
	reg rec_clk; // done
	wire neg_rx_wire; // done
	reg neg_rx;
	assign neg_rx_wire = i_rx ^ neg_rx;
	always @(negedge neg_rx_wire or negedge i_rst_n) begin
		if (!i_rst_n) begin
			rec_clk <= 1'b0;
		end else begin
			rec_clk <= ~rec_clk;
		end
	end

	assign o_rec_clk = rec_clk;

	wire pll_pfdena;
	wire pll_ena;
	assign pll_pfdena = 1'b1; // done
	assign pll_ena = 1'b1; // done
	wire pll_locked;

	wire rx_clk;
	wire rx_clk2x;

	rx_pll	rx_pll_inst(
		.areset(!i_rst_n),
		.inclk0(i_rec_clk),
		.pfdena(pll_pfdena),
		.pllena(pll_ena),
		.c0(rx_clk),
		.c1(rx_clk2x),
		.locked(pll_locked)
	);

	// perform reset of the receiver if pll falls into unlocked state
	reg rx_sync;
	reg pll_sync;
	always @(posedge i_clk or negedge i_rst_n) begin
		if (!i_rst_n) begin
			rx_sync <= 1'b0;
		end else begin
			if ((rx_sync == pll_sync) && (!pll_locked)) begin
				rx_sync <= ~rx_sync;
			end
		end
	end

	reg [15:0] rx_reg;
	wire [7:0] rx_data;

	wire rx_valid;
	assign rx_valid = !(	(rx_reg[15] ^ rx_reg[14]) || (rx_reg[13] ^ rx_reg[12]) || 
							(rx_reg[11] ^ rx_reg[10]) || (rx_reg[ 9] ^ rx_reg[ 8]) || 
							(rx_reg[ 7] ^ rx_reg[ 6]) || (rx_reg[ 5] ^ rx_reg[ 4]) || 
							(rx_reg[ 3] ^ rx_reg[ 2]) || (rx_reg[ 1] ^ rx_reg[ 0])	);

	genvar i;
	generate
		for (i = 0; i < 8; i = i + 1) begin: ASSIGN_RX_DATA
			assign rx_data[i] = rx_reg[i * 2];
		end
	endgenerate

	always @(negedge rx_clk2x or negedge i_rst_n) begin
		if (!i_rst_n) begin
			rx_reg <= 16'b0;
		end else begin
			rx_reg <= {rx_reg[15:1], rx_clk ^ i_rx};
		end
	end


	wire cb_push_write_index;
	wire cb_pop_write_index;
	wire cb_write_en;
	wire [7:0] cb_i_data; // done
	wire cb_underrun; // done
	wire cb_overrun; // done
	wire cb_stack_overrun; // done
	wire cb_invalid_operation; // done
	wire cb_invalid_write_index; // done
	 
	circullar_buffer cb_inst(
		.i_clk(i_clk),
		.i_rst_n(i_rst_n),
		.i_cb_push_write_index(cb_push_write_index),
		.i_cb_pop_write_index(cb_pop_write_index),
		.i_write_en(cb_write_en),
		.i_read_en(i_data_re),
		.i_data(cb_i_data),
		.o_data(o_data),
		.o_data_size(o_data_size),
		.o_underrun(cb_underrun),
		.o_overrun(cb_overrun),
		.o_stack_overrun(cb_stack_overrun),
		.o_invalid_operation(cb_invalid_operation),
		.o_invalid_write_index(cb_invalid_write_index)
	);

	assign cb_i_data = rx_data;


	// status register
	wire [15:0] status_wires;
	reg [15:0] status; // done

	always @(posedge i_clk or negedge i_rst_n) begin
		if (!i_rst_n) begin
			status <= 16'b0;
		end else begin
			status <= status_wires;
		end
	end

	assign o_status = status;

	assign o_local_rx_locked = pll_locked;

	assign status_wires[15] = data_size != 0;			// 1 - Data available, 0 - no data
	assign status_wires[14] = frames_count != 0; 		// 1 - frames are available, 0 - no frames
	// assign status_wires[13] = (state != STATE_SYNC) && 	// 1 - transmitting, 0 - idle
	// 						  (state != STATE_SYNCED);
	assign status_wires[14] = 1'b0;

	assign status_wires[12] = 1'b0;
	// assign status_wires[11] = state[3];					// state bit 3
	// assign status_wires[10] = state[2];					// state bit 2
	// assign status_wires[9] = state[1];					// state bit 1
	// assign status_wires[8] = state[0];					// state bit 0

	assign status_wires[11] = 1'b0;
	assign status_wires[10] = 1'b0;
	assign status_wires[9] = 1'b0;
	assign status_wires[8] = 1'b0;

	assign status_wires[7] = 1'b0;
	assign status_wires[6] = 1'b0;
	assign status_wires[5] = bad_state;					// 1 - returned from the undefined state 
	assign status_wires[4] = cb_underrun;				// 1 - buffer underrun error
	assign status_wires[3] = cb_overrun;				// 1 - buffer overrun error
	assign status_wires[2] = cb_stack_overrun;			// 1 - write stack pointer overrun
	assign status_wires[1] = cb_invalid_operation;		// 1 - invalid operation (pushing and popping write index at the same time)
	assign status_wires[0] = cb_invalid_write_index; 	// 1 - popped invalid write index

endmodule