module m_transmitter(i_clk_2x, i_rst_n, i_data, i_data_we, o_data_left, o_m_status, o_m_tx);

input i_clk_2x;
input i_rst_n;
input [7:0] i_data;
input i_data_we;
output [7:0] o_data_left;
output o_m_status;
output o_m_tx;

reg m_tx;
assign o_m_tx = m_tx;

reg [7:0] tx_buffer[0:255];

reg [2:0] shift;

reg [3:0] state, next_state;
localparam STATE_IDLE = 0, STATE_TX = 1;






// Next state logic
always @(posedge c0 or negedge i_rst_n) begin
	if (!i_rst_n) begin
		m_tx <= 0;
		next_state <= STATE_IDLE;
	end else begin
		case (state)
			STATE_IDLE: begin

			end
			STATE_TX: begin

			end
			default: begin
				next_state <= STATE_IDLE; // Return from the undefined state
			end
		endcase
	end
end

always @(posedge clk or negedge i_rst_n) begin
	if (!i_rst_n) begin
		state <= IDLE; // Initial state
	end else begin
		state <= next_state; // Switch to the next state
	end
end 

endmodule