module circullar_buffer(
	i_clk,
	i_rst_n,
	i_push_write_index,
	i_pop_write_index,
	i_write_en,
	i_read_en,
	i_data,
	o_data,
	o_data_size,
	o_underrun,
	o_overrun,
	o_stack_overrun,
	o_invalid_operation,
	o_invalid_write_index
);
	`include "utils.vh"

	parameter DATA_WIDTH = 8;
	parameter BUFFER_SIZE = 256;
	localparam ADDRESS_WIDTH = u_log2(BUFFER_SIZE);
	input i_clk;
	input i_rst_n;
	input i_write_en;
	input i_read_en;
	input [DATA_WIDTH - 1:0] i_data;

	output [DATA_WIDTH - 1:0] o_data;
	output [15:0] o_data_size;
	output o_underrun;
	output o_overrun;
	output o_stack_overrun;
	output o_invalid_write_index;

	reg underrun;
	reg overrun;
	reg stack_overrun;
	reg invalid_operation;
	reg invalid_write_index;

	reg [DATA_WIDTH - 1:0] mem[0:BUFFER_SIZE - 1];
	reg [15:0] data_size;
	reg [ADDRESS_WIDTH - 1:0] write_index;
	reg [ADDRESS_WIDTH - 1:0] stack_write_index;
	reg stack_write_index_valid;
	reg [ADDRESS_WIDTH - 1:0] read_index;

	always @ (negedge i_clk or negedge i_rst_n) begin
		if (!i_rst_n) begin
			stack_overrun <= 1'b0;
			invalid_operation <= 1'b0;
			stack_write_index <= 0;
			stack_write_index_valid <= 1'b0;
			invalid_write_index <= 1'b0;
		end else begin
			case ({i_push_write_index, i_pop_write_index})
				2'b00: begin
					stack_overrun <= stack_overrun;
					invalid_operation <= invalid_operation;
					stack_write_index <= stack_write_index;
					invalid_write_index <= invalid_write_index;
				end
				2'b01: begin
					stack_write_index_valid <= 1'b0;
				end
				2'b10: begin
					stack_write_index_valid <= 1'b1;
					stack_write_index <= write_index;
				end
				2'b11: begin
					stack_overrun <= stack_overrun;
					invalid_operation <= 1'b1;
					stack_write_index <= stack_write_index;
				end
			endcase
		end
	end


	always @ (negedge i_clk or negedge i_rst_n) begin
		if (!i_rst_n) begin
			underrun <= 0;
			overrun <= 0;
			data_size <= 0;
			write_index <= 0;
			read_index <= 0;
		end else begin
			if ({i_push_write_index, i_pop_write_index} == 2'b00) begin
				case ({i_write_en, i_read_en})
					2'b00: begin
						underrun <= underrun;
						overrun <= overrun;
						data_size <= data_size;
						write_index <= write_index;
						read_index <= read_index;
					end
					2'b01: begin
						if (data_size == 0) begin
							underrun <= 1'b1;
							overrun <= overrun;
							data_size <= data_size;
							write_index <= write_index;
							read_index <= read_index;
						end else begin
							underrun <= underrun;
							overrun <= overrun;
							data_size <= data_size - 1;
							write_index <= write_index;
							read_index <= read_index + 1;
						end
					end
					2'b10: begin
						if (data_size == (1 << ADDRESS_WIDTH) - 1) begin
							underrun <= underrun;
							overrun <= 1'b1;
							data_size <= data_size;
							write_index <= write_index;
							read_index <= read_index;
						end else begin
							underrun <= underrun;
							overrun <= overrun;
							data_size <= data_size + 1;
							write_index <= write_index + 1;
							read_index <= read_index;
							mem[write_index] <= i_data;
						end
					end
					2'b11: begin
						underrun <= underrun;
						overrun <= overrun;
						data_size <= data_size;
						write_index <= write_index + 1;
						read_index <= read_index + 1;
						mem[write_index] <= i_data;
					end
				endcase // {i_write_en, i_read_en}
			end else begin
				underrun <= underrun;
				overrun <= overrun;
				data_size <= data_size;
				write_index <= write_index;
				read_index <= read_index;
			end
		end
	end

	assign o_data = mem[read_index];
	assign o_data_size = data_size;
	assign o_underrun = underrun;
	assign o_overrun = overrun;
	assign o_stack_overrun = stack_overrun;
	assign o_invalid_operation = invalid_operation;
	assign o_invalid_write_index = invalid_write_index;

endmodule