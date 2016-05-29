module m_transmitter(
	i_clk,
	i_clk_2x,
	i_rst_n,
	i_data,
	i_data_we,
	i_push_frame,
	o_data_size,
	o_frames_count,
	o_status,
	o_tx
);

input i_clk; // done
input i_clk_2x; // done
input i_rst_n; // done
input [7:0] i_data; // done
input i_data_we; // done
input i_push_frame;
output [15:0] o_data_size; // done
output [7:0] o_frames_count; // done
output [7:0] o_status; // TODO
output o_tx; // done

parameter PREAMBLE_SIZE = 64;
parameter START_SIZE 	= 2;
parameter DATA_SIZE 	= 8;
parameter GAP_SIZE 		= 16;

reg cb_read_en; // done
//wire [7:0] cb_i_data; 
wire [7:0] cb_o_data; // done
wire [15:0] cb_o_data_size; // done
wire cb_underrun;
wire cb_overrun;
 
circullar_buffer cb_inst(
	.i_clk(i_clk),
	.i_rst_n(i_rst_n),
	.i_write_en(i_data_we),
	.i_read_en(cb_read_en),
	.i_data(i_data),
	.o_data(cb_o_data),
	.o_data_size(cb_o_data_size),
	.o_underrun(cb_underrun),
	.o_overrun(cb_overrun)
  ); 

reg [7:0] frames_count;
reg [7:0] processed_frames_count; // done
reg [7:0] tx_buffer; // done
reg [7:0] tx_counter; // done
reg [15:0] data_size; // done
reg [15:0] data_size_counter; // done
reg tx_en; // done
reg bad_state; // done

reg tx; // done

assign tx_bit = tx_buffer[0];

always @(posedge i_clk or negedge i_rst_n) begin
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

// state
// processed_frames_count
// tx_en
// tx_buffer
// tx_counter
// bad_state

reg [3:0] state, next_state;
localparam 	STATE_IDLE = 0, STATE_PREAMBLE = 1, STATE_START = 2, STATE_DATA_SIZE_0 = 3, 
		   	STATE_DATA_SIZE_1 = 4, STATE_DATA = 5, STATE_DATA_BYTE = 6, STATE_GAP = 7;

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
					processed_frames_count <= processed_frames_count;
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
				bad_state <= bad_state;
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
				processed_frames_count <= processed_frames_count;
				tx_en <= 1'b1;	
				data_size <= 16'b0;
				data_size_counter <= 16'b0;
				bad_state <= bad_state;
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
				processed_frames_count <= processed_frames_count;
				tx_en <= 1'b1;
				data_size_counter <= 16'b0;			
				bad_state <= bad_state;
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
				processed_frames_count <= processed_frames_count;
				tx_en <= 1'b1;
				data_size_counter <= 16'b0;		
				bad_state <= bad_state;
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
				processed_frames_count <= processed_frames_count;
				tx_en <= 1'b1;				
				bad_state <= bad_state;
			end
			// STATE_DATA: begin
				
			// end
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
					data_size_counter <= data_size_counter;
					tx_en <= 1'b1;
					cb_read_en <= 1'b0;
					tx_buffer <= {1'b0, tx_buffer[7:1]};
				end					
				processed_frames_count <= processed_frames_count;				
				bad_state <= bad_state;	
			end
			STATE_GAP: begin
				if (tx_counter == GAP_SIZE) begin
					state <= STATE_IDLE;
				end else begin
					state <= STATE_GAP;
				end
				tx_en <= 1'b0;
				data_size_counter <= data_size_counter;
				tx_counter <= tx_counter + 1'b1;
				processed_frames_count <= processed_frames_count;			
				bad_state <= bad_state;
				cb_read_en <= 1'b0;
			end
			default: begin
				state <= STATE_IDLE; // Return from the undefined state
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

// always @(posedge clk or negedge i_rst_n) begin
// 	if (!i_rst_n) begin
// 		state <= STATE_IDLE; // Initial state
// 	end else begin
// 		state <= next_state; // Switch to the next state
// 	end
// end

assign o_frames_count = frames_count;
//assign o_status = status;
assign o_tx = tx;

assign o_data_size = cb_o_data_size;


endmodule