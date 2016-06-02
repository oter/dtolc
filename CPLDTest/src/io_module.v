module io_module(
	i_clk,
	i_rst_n,
	i_sync,
	i_cmd,
	i_data,
	o_data,
	o_sync,
	o_rx_int,
	o_tx_int,

	// transmitter
	i_tx_data_size,
	i_tx_frames_count,
	i_tx_status,
	o_tx_rst_n,
	o_tx_push_write_index,
	o_tx_pop_write_index,
	o_tx_data,
	o_tx_data_we,
	o_tx_push_frame,

	);
	
	// IO module
	input i_clk; // done
	input i_rst_n; // done
	input i_sync; // done
	input [3:0] i_cmd; // done
	input [7:0] i_data; // done

	output [7:0] o_data; // done
	output o_sync; // done
	output o_rx_int;
	output o_tx_int;

	// transmitter
	input [15:0] i_tx_data_size; // done
	input [7:0] i_tx_frames_count; // done
	input [15:0] i_tx_status; // done

	output o_tx_rst_n; // done
	output o_tx_push_write_index;
	output o_tx_pop_write_index;
	output [7:0] o_tx_data; // done
	output o_tx_data_we;
	output o_tx_push_frame;

	reg tx_push_write_index;
	reg tx_pop_write_index;
	reg tx_data_we;
	reg tx_push_frame;

	// registers map
	wire [7:0] regs_wire_map[0:15];
	reg [7:0] regs_map[0:3];

	// IO data sync logic
	localparam CMD_IDLE 		= 4'b0000;
	localparam CMD_READ_REG 	= 4'b0001;
	localparam CMD_WRITE_REG 	= 4'b10??; // ?? - address of the writable register
	localparam CMD_RX_BYTE 		= 4'b0101; 
	localparam CMD_TX_BYTE 		= 4'b0110;
	localparam CMD_RX_TX_BYTE 	= 4'b01??;

	localparam IO_STATE_IDLE 	= 0;
	localparam IO_STATE_IO 		= 1;

	localparam TX_STATE_SIZE_0 	= 0;
	localparam TX_STATE_SIZE_1 	= 1;
	localparam TX_STATE_DATA 	= 2;
	
	reg [2:0] rx_state;
	reg [2:0] tx_state;
	reg [15:0] tx_counter;
	reg [1:0] io_state;
	reg sync;
	reg [3:0] cmd;
	reg [7:0] input_data;
	reg [7:0] output_data;

	reg [7:0] invalid_commands_count; // done

	always @(negedge i_clk or negedge i_rst_n) begin
		if (!i_rst_n) begin
			tx_state <= TX_STATE_IDLE;
			io_state <= IO_STATE_IDLE;
			sync <= 1'b0;
			cmd <= 4'b0;
			input_data <= 8'b0;
			output_data <= 8'b0;
			invalid_commands_count <= 8'b0;
			tx_counter <= 16'b0;
			genvar i;
			generate
				for (i = 0; i < 4; i = i + 1) begin : REGS_INIT
					assign regs_wire_map[i] <= 8'b0;
				end
			endgenerate
		end else begin
			casez(io_state)
				IO_STATE_IDLE: begin
					if (sync != i_sync) begin
						io_state <= IO_STATE_IO;
						cmd <= i_cmd;
						input_data <= i_data;
					end
				end
				IO_STATE_IO: begin
					sync <= i_sync;
					io_state <= IO_STATE_IDLE;
					if (o_tx_rst_n /*&& TODO: for rx*/) begin
						casez(cmd)
							CMD_IDLE: begin
							end
							CMD_READ_REG: begin
								output_data <= regs_wire_map[input_data[3:0]];
							end
							CMD_WRITE_REG: begin
								output_data <= input_data;
								regs_map[cmd[1:0]] <= input_data;
							end
							// CMD_RX_BYTE: begin
							// end
							// CMD_TX_BYTE: begin
							// end
							CMD_TX_RX_BYTE: begin
								if (cmd[0]) begin // rx

								end
								if (cmd[1]) begin // tx
									casez(tx_state)
										TX_STATE_SIZE_0: begin
											tx_counter <= 16'b01;
										end
										TX_STATE_SIZE_1: begin

										end
										TX_STATE_DATA: begin

										end
										default: begin
											tx_state <= TX_STATE_IDLE;
										end
									endcase
								end
							end
							default: begin
								invalid_commands_count <= invalid_commands_count + 1'b1;
							end
						endcase // cmd
					end
				end
				default: begin
					io_state <= IO_STATE_IDLE;
				end
			endcase // io_state
		end
	end	

	genvar i;
	generate
		for (i = 0; i < 4; i = i + 1) begin : REGS_ASSIGN
			assign regs_wire_map[i] = regs_map;
		end
	endgenerate

	assign regs_wire_map[15] = invalid_commands_count;
	assign regs_wire_map[14] = {6'b0, io_state};
	assign regs_wire_map[13] = 0;
	assign regs_wire_map[12] = 0;
	assign regs_wire_map[11] = 0;
	assign regs_wire_map[10] = 0;
	assign regs_wire_map[9]  = 0;
	assign regs_wire_map[8]  = i_tx_data_size[15:0];
	assign regs_wire_map[7]  = i_tx_data_size[7:0];
	assign regs_wire_map[6]  = i_tx_frames_count;
	assign regs_wire_map[5]  = i_tx_status[15:8];
	assign regs_wire_map[4]  = i_tx_status[7:0];
	// regs_wire_map[3]  - 
	// regs_wire_map[2]  - 
	// regs_wire_map[1]  - 
	// regs_wire_map[0]  - tranciever control. 0'0'0'0'0'0'rx_rst_n'tx_rst_n.

	assign o_tx_rst_n = regs_wire_map[0][0];

	assign o_data = output_data;
	assign o_sync = sync;
	assign o_tx_data = input_data;

endmodule // io_module