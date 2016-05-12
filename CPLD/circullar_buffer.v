module circullar_buffer (
	i_clk,
	i_rst_n,
	i_write_en,
	i_read_en,
	i_data,
	o_data,
	o_data_size,
	o_underrun,
	o_overrun
);
	`include "utils.vh"

	parameter DATA_WIDTH = 8;
	parameter BUFFER_SIZE = 256;
	localparam ADDRESS_WIDTH = u_log2(BUFFER_SIZE);
	input i_clk,
	input i_rst_n,
	input i_write_en,
	input i_read_en,
	input [DATA_WIDTH - 1:0] i_data,

	output [DATA_WIDTH - 1:0] o_data,
	output [ADDRESS_WIDTH - 1:0] o_data_size,
	output o_underrun;
	output o_overrun;

	reg [DATA_WIDTH - 1:0] mem[0:BUFFER_SIZE- 1];
	reg [ADDRESS_WIDTH - 1:0] data_size;
	reg [ADDRESS_WIDTH - 1:0] write_index;
	reg [ADDRESS_WIDTH - 1:0] read_index;


	always @ (posedge i_clk or negedge i_rst_n) begin
		if (!i_rst_n) begin 

		end else begin

		end
	end

	assign o_data = mem[read_index];
	assign o_data_size = data_size;

endmodule