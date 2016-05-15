module m_transmitter(
	i_clk,
	i_clk_2x,
	i_rst_n,
	i_data,
	i_data_we,
	o_data_count,
	o_frames_count,
	o_status,
	o_tx
);

input i_clk;
input i_clk_2x;
input i_rst_n;
input [7:0] i_data;
input i_data_we;
output [7:0] o_data_count;
output [7:0] o_frames_count; // done
output [7:0] o_status; // done
output o_tx; // done

parameter PREAMBLE_SIZE = 64;

wire cb_write_en;
wire cb_read_en;
wire [7:0] cb_i_data;
wire [7:0] cb_o_data;
wire [15:0] cb_o_data_size;
wire cb_underrun;
wire cb_overrun;
 
circullar_buffer cb_inst(
	.i_clk(i_clk),
	.i_rst_n(i_rst_n),
	.i_write_en(cb_write_en),
	.i_read_en(cb_read_en),
	.i_data(cb_i_data),
	.o_data(cb_o_data),
	.o_data_size(cb_o_data_size),
	.o_underrun(cb_underrun),
	.o_overrun(cb_overrun)
  ); 


reg [7:0] frames_count;
reg [7:0] processed_frames_count;
reg [7:0] tx_buffer;
reg [2:0] tx_counter;
reg tx_en;
reg bad_state;

reg tx; // done





reg [4:0] state, next_state;
localparam STATE_IDLE = 0, STATE_PREAMBLE = 1, STATE_START = 2, STATE_DATA = 3, STATE_GAP = 4;

// FSM
always @(posedge i_clk or negedge i_rst_n) begin
	if (!i_rst_n) begin
		state <= STATE_IDLE;

		processed_frames_count <= 0;
		tx_buffer <= 0;
		tx_counter <= 0;
		tx_en <= 0;
		bad_state <= 0;
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
				tx_buffer <= 0;
				tx_counter <= 0;
				bad_state <= bad_state;
			end
			STATE_PREAMBLE: begin

			end
			STATE_START: begin

			end
			STATE_DATA: begin

			end
			STATE_GAP: begin

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
			tx <= i_clk ^ tx_buffer;
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


endmodule