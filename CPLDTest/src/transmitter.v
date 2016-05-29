module transmitter(
	i_clk,
	i_clk_2x,
	i_rst_n,
	i_push_write_index,
	i_pop_write_index,
	i_data,
	i_data_we,
	i_push_frame,
	o_data_size,
	o_frames_count,
	o_status,
	o_tx
);

input i_clk;
input i_clk_2x;
input i_rst_n;
input i_push_write_index;
input i_pop_write_index;
input [7:0] i_data;
input i_data_we;
input i_push_frame;
output [15:0] o_data_size;
output [7:0] o_frames_count;
output [15:0] o_status;
output o_tx;

// status register
wire [15:0] status_wires;
reg [15:0] status;

always @(posedge i_clk or negedge i_rst_n) begin
	if (!i_rst_n) begin
		status <= 16'b0;
	end else begin
		status <= status_wires;
	end
end

parameter PREAMBLE_SIZE = 64;
parameter START_SIZE 	= 2;
parameter DATA_SIZE 	= 8;
parameter GAP_SIZE 		= 16;

reg cb_read_en;
wire [7:0] cb_o_data;
wire [15:0] cb_o_data_size;
wire cb_underrun;
wire cb_overrun;
wire cb_stack_overrun;
wire cb_invalid_operation;
wire cb_invalid_write_index;
 
circullar_buffer cb_inst(
	.i_clk(i_clk),
	.i_rst_n(i_rst_n),
	.i_push_write_index(i_push_write_index),
	.i_pop_write_index(i_pop_write_index),
	.i_write_en(i_data_we),
	.i_read_en(cb_read_en),
	.i_data(i_data),
	.o_data(cb_o_data),
	.o_data_size(cb_o_data_size),
	.o_underrun(cb_underrun),
	.o_overrun(cb_overrun),
	.o_stack_overrun(cb_stack_overrun),
	.o_invalid_operation(cb_invalid_operation),
	.o_invalid_write_index(cb_invalid_write_index)
  ); 

reg [7:0] frames_count;
reg [7:0] processed_frames_count;
reg [7:0] tx_buffer;
reg [7:0] tx_counter;
reg [15:0] data_size;
reg [15:0] data_size_counter;
reg tx_en;
reg tx;
reg bad_state;

assign tx_bit = tx_buffer[0];

// TODO: posedge i_clk_2x or i_clk??
always @(posedge i_clk_2x or negedge i_rst_n) begin
	if (!i_rst_n) begin
		frames_count <= 8'b0;
	end else begin
		if (i_push_frame) begin
			frames_count <= frames_count + 1'b1;
		end else begin
			frames_count <= frames_count;
		end
	end
end

reg [3:0] state;
localparam 	STATE_IDLE = 0, STATE_PREAMBLE = 1, STATE_START = 2, STATE_DATA_SIZE_0 = 3, 
		   	STATE_DATA_SIZE_1 = 4, STATE_DATA_BYTE = 5, STATE_GAP = 6;

// FSM
always @(posedge i_clk or negedge i_rst_n) begin
	if (!i_rst_n) begin
		state <= STATE_IDLE;
		processed_frames_count <= 8'b0;
		tx_buffer <= 8'b0;
		data_size <= 16'b0;
		tx_counter <= 8'b0;
		data_size_counter <= 16'b0;
		tx_en <= 1'b0;
		bad_state <= 1'b0;
		cb_read_en <= 1'b0;
	end else begin
		case (state)
			STATE_IDLE: begin
				if (frames_count == processed_frames_count) begin
					state <= STATE_IDLE;
					tx_en <= 1'b0;
				end else begin
					state <= STATE_PREAMBLE;
					processed_frames_count <= processed_frames_count + 1;
					tx_en <= 1'b1;
				end
				tx_buffer <= 8'b0;
				data_size <= 16'b0;
				tx_counter <= 1'b1;
				data_size_counter <= 16'b0;
				cb_read_en <= 1'b0;
			end
			STATE_PREAMBLE: begin
				if (tx_counter == PREAMBLE_SIZE) begin
					tx_counter <= 1'b1; 
					state <= STATE_START;
					tx_buffer <= 8'b01;
				end else begin
					tx_counter <= tx_counter + 1'b1;
					state <= STATE_PREAMBLE;
					tx_buffer <= 8'b0;
				end
				tx_en <= 1'b1;	
				data_size <= 16'b0;
				data_size_counter <= 16'b0;
				cb_read_en <= 1'b0;
			end
			STATE_START: begin
				if (tx_counter == START_SIZE) begin
					tx_counter <= 1'b1; 
					state <= STATE_DATA_SIZE_0;
					data_size <= {8'b0, cb_o_data};
					tx_buffer <= cb_o_data;
					cb_read_en <= 1'b1;
				end else begin
					tx_counter <= tx_counter + 1'b1;
					state <= STATE_START;
					tx_buffer <= 8'b00000001;
					cb_read_en <= 1'b0;
				end
				tx_en <= 1'b1;
				data_size_counter <= 16'b0;
			end
			STATE_DATA_SIZE_0: begin
				if (tx_counter == DATA_SIZE) begin
					tx_counter <= 1'b1; 
					state <= STATE_DATA_SIZE_1;
					data_size <= {cb_o_data, data_size[7:0]};
					tx_buffer <= cb_o_data;
					cb_read_en <= 1'b1;
				end else begin
					tx_counter <= tx_counter + 1'b1;
					state <= STATE_DATA_SIZE_0;
					tx_buffer <= {1'b0, tx_buffer[7:1]};
					cb_read_en <= 1'b0;
				end
				tx_en <= 1'b1;
				data_size_counter <= 16'b0;
			end
			STATE_DATA_SIZE_1: begin
				if (tx_counter == DATA_SIZE) begin
					tx_counter <= 8'b01;
					data_size_counter <= 16'b01;
					state <= STATE_DATA_BYTE;
					tx_buffer <= cb_o_data;
					cb_read_en <= 1'b1;
				end else begin
					tx_counter <= tx_counter + 1'b1;
					data_size_counter <= 16'b0;
					state <= STATE_DATA_SIZE_1;
					tx_buffer <= {1'b0, tx_buffer[7:1]};
					cb_read_en <= 1'b0;
				end
				tx_en <= 1'b1;
			end
			STATE_DATA_BYTE: begin
				if (tx_counter == DATA_SIZE) begin
					tx_counter <= 8'b01; 
					if (data_size_counter == data_size) begin	
						state <= STATE_GAP;
						tx_en <= 1'b0;
						cb_read_en <= 1'b0;
					end else begin 
						data_size_counter <= data_size_counter + 1'b1;
						state <= STATE_DATA_BYTE;
						tx_en <= 1'b1;
						tx_buffer <= cb_o_data;
						cb_read_en <= 1'b1;
					end
				end else begin
					state <= STATE_DATA_BYTE;
					tx_counter <= tx_counter + 1'b1;
					tx_en <= 1'b1;
					cb_read_en <= 1'b0;
					tx_buffer <= {1'b0, tx_buffer[7:1]};
				end
			end
			STATE_GAP: begin
				if (tx_counter == GAP_SIZE) begin
					state <= STATE_IDLE;
				end else begin
					state <= STATE_GAP;
				end
				tx_en <= 1'b0;
				tx_counter <= tx_counter + 1'b1;
				cb_read_en <= 1'b0;
			end
			default: begin
				state <= STATE_IDLE; // return from the undefined state
				bad_state <= 1'b1;
			end
		endcase
	end
end

// tx output driver
always @(negedge i_clk_2x or negedge i_rst_n) begin
	if (!i_rst_n) begin
		tx <= 0;
	end else begin
		if (tx_en == 1'b1) begin
			tx <= i_clk ^ tx_buffer[0];
		end else begin
			tx <= 1'b0;
		end
	end
end

assign o_frames_count = frames_count;
assign o_tx = tx;
assign o_data_size = cb_o_data_size;
assign o_status = status;

assign status_wires[15] = data_size == 0;			// 1 - No data to transmit, 0 - data is available
assign status_wires[14] = frames_count != 0; 		// 1 - frames are available, 0 - no frames
assign status_wires[13] = state != STATE_IDLE; 		// 1 - transmitting, 0 - idle
assign status_wires[12] = 0;
assign status_wires[11] = state[3];					// state bit 3
assign status_wires[10] = state[2];					// state bit 2
assign status_wires[9] = state[1];					// state bit 1
assign status_wires[8] = state[0];					// state bit 0

assign status_wires[7] = 0;
assign status_wires[6] = 0;
assign status_wires[5] = bad_state;					// 1 - returned from the undefined state 
assign status_wires[4] = cb_underrun;				// 1 - buffer underrun error
assign status_wires[3] = cb_overrun;				// 1 - buffer overrun error
assign status_wires[2] = cb_stack_overrun;			// 1 - write stack pointer overrun
assign status_wires[1] = cb_invalid_operation;		// 1 - invalid operation (pushing and popping write index at the same time)
assign status_wires[0] = cb_invalid_write_index; 	// 1 - popped invalid write index

endmodule