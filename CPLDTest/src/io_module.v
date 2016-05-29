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

	
	// registers map
	wire [7:0] regs_wire_map[0:15];
	reg [7:0] regs_map[0:7];

	// IO data sync logic
	parameter CMD_IDLE 			= 4'b0000;
	parameter CMD_READ_REG 		= 4'b0001;
	parameter CMD_WRITE_REG 	= 4'b1???; // ??? - address of the writable register
	parameter CMD_RX_BYTE 		= 4'b0100; 
	parameter CMD_TX_BYTE 		= 4'b0101;
	parameter CMD_RX_TX_BYTE 	= 4'b0110;

	parameter IO_STATE_IDLE 	= 0;
	parameter IO_STATE_IO 		= 1;

	reg [2:0] tx_state;
	reg [2:0] rx_state;
	reg [1:0] io_state;
	reg sync;
	reg [3:0] cmd;
	reg [7:0] input_data;
	reg [7:0] output_data;

	reg [7:0] invalid_commands_count; 

	always @(negedge i_clk or negedge i_rst_n) begin
		if (!i_rst_n) begin
			io_state <= IO_STATE_IDLE;
			sync <= 1'b0;
			cmd <= 4'b0;
			input_data <= 8'b0;
			output_data <= 8'b0;
			invalid_commands_count <= 8'b0;
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
					case(cmd)
						CMD_IDLE: begin
							
						end
						CMD_READ_REG: begin
							output_data <= regs_wire_map[input_data[3:0]];
						end
						CMD_WRITE_REG: begin
							output_data <= input_data;
							regs_map[cmd[2:0]] <= input_data;
						end
						CMD_RX_BYTE: begin
							
						end
						CMD_TX_BYTE: begin

						end
						CMD_TX_RX_BYTE: begin

						end
						default: begin
							invalid_commands_count <= invalid_commands_count + 1'b1;
						end
					endcase // cmd
				end
				default: begin
					io_state <= IO_STATE_IDLE;
				end
			endcase // io_state
		end
	end

	// regs_wire_map[15] - 
	// regs_wire_map[14] - 
	// regs_wire_map[13] - 
	// regs_wire_map[12] - 
	// regs_wire_map[11] - 
	// regs_wire_map[10] - 
	// regs_wire_map[9]  - 
	// regs_wire_map[8]  - 
	// regs_wire_map[7]  - 
	// regs_wire_map[6]  - 
	// regs_wire_map[5]  - 
	// regs_wire_map[4]  - 
	// regs_wire_map[3]  - 
	// regs_wire_map[2]  - 
	// regs_wire_map[1]  - 
	// regs_wire_map[0]  - 

	genvar i;
	generate
		for (i = 0; i < 8; i = i + 1) begin : REGS_ASSIGN
			assign regs_wire_map[i] = regs_map;
		end
	endgenerate

	assign regs_wire_map[15] = 0;
	assign regs_wire_map[14] = 0;
	assign regs_wire_map[13] = 0;
	assign regs_wire_map[12] = 0;
	assign regs_wire_map[11] = 0;
	assign regs_wire_map[10] = 0;
	assign regs_wire_map[9]  = 0;
	assign regs_wire_map[8]  = 0;

endmodule // io_module