module pll_test(
	i_clk,
	i_rst_n,
	i_btn_0,

	i_tx,
	o_tx,

	o_led_0
	);

	input i_clk;
	input i_rst_n;
	input i_btn_0;
	input i_tx;
	output o_tx;

	output o_led_0;

	reg led_0;

	assign o_led_0 = led_0;

	wire [63:0]test_data;
	wire [127:0]data_manchester;
	wire [1:0]clk_manchester;
	wire [3:0]start_manchester;

	assign test_data = 64'b10110001_01100100_11001100_10100110_10011001_00110001_10001101_10011010;
	assign data_manchester = 128'b01100101101010011001011010011010010110100101101001100110100101100110100101101001101001011010100101101010010110010110100101100110;
	assign clk_manchester = 2'b10;
	assign start_manchester = 4'b0101;
	
	reg tx;
	reg [7:0] counter;
	reg [3:0] state;

	localparam PREAMBLE_SIZE = 64 * 2;
	localparam START_SIZE = 2 * 2;
	localparam DATA_SIZE = 64 * 2;

	localparam STATE_IDLE = 0;
	localparam STATE_TX_PREAMBLE = 1;
	localparam STATE_TX_START = 2;
	localparam STATE_TX_DATA = 3;


	always @(posedge i_clk or negedge i_rst_n) begin
		if(!i_rst_n) begin
			counter <= 8'b0;
			state <= STATE_IDLE;
			tx <= 1'b0;
		end else begin
			if (!i_btn_0) begin
				state <= STATE_IDLE;
				counter <= 8'b0;
				tx <= 1'b0; 
			end else begin
				case (state)
					STATE_IDLE: begin
						if (i_btn_0) begin
							counter <= counter + 1'b1;
							if (counter == 63) begin
								state <= STATE_TX_PREAMBLE;
								counter <= 8'b0;
							end
						end
					end
					STATE_TX_PREAMBLE: begin
						counter <= counter + 1'b1;
						if (counter == PREAMBLE_SIZE - 1) begin
							state <= STATE_TX_START;
							counter <= 0;
						end else begin
							tx <= clk_manchester[counter[0]];
						end
					end
					STATE_TX_START: begin
						counter <= counter + 1'b1;
						if (counter == START_SIZE - 1) begin
							state <= STATE_TX_DATA;
							counter <= 0;
						end else begin
							tx <= start_manchester[counter[0]];
						end
					end
					STATE_TX_DATA: begin
						counter <= counter + 1'b1;
						if (counter == DATA_SIZE - 1) begin
							state <= STATE_TX_DATA;
							counter <= 0;
						end else begin
							tx <= data_manchester[counter[6:0]];
						end
					end
					default: begin
						state <= STATE_IDLE;
					end
				endcase
			end
		end
	end

	// receiver
	reg clkd2;
	reg invert_tx;
	wire xor_tx;
	assign xor_tx = tx ^ invert_tx;

	always @(negedge xor_tx or negedge i_rst_n) begin
		if(!i_rst_n) begin
			clkd2 <= 1'b0;
		end else begin
			clkd2 <= !clkd2;
		end
	end

	assign o_tx = clkd2;

	wire pll_en;
	wire pll_clk;
	wire pll_clk2x;
	wire pll_locked;

	assign pll_en = 1'b1;

	m_pll m_pll_inst
	(
		.areset(!i_rst_n),
		.inclk0(i_tx),
		.pllena(pllena),
		.c0(pll_clk),
		.c1(pll_clk2x) ,
		.locked(pll_locked)
	);

	reg [31:0]test_counter;

	reg [1:0] data;
	reg [31:0] bad_counter;
	wire decoded_data;
	assign decoded_data = tx ^ pll_clk;
	always @(negedge pll_clk2x or negedge i_rst_n or negedge pll_locked) begin
		if(!i_rst_n) begin
			data <= 2'b0;
			invert_tx <= 1'b0;
			bad_counter <= 32'b0;
			test_counter <= 32'b0;
			led_0 <= 1'b0;
		end else if (!pll_locked) begin
			data <= 2'b0;
			invert_tx <= 1'b0;
			test_counter <= 32'b0;
			led_0 <= 1'b0;
		end else begin
			test_counter <= test_counter + 1'b1;
			if (test_counter == 25000000) begin
				test_counter <= 32'b0;
				led_0 <= !led_0;
			end
			data <= {data[0], decoded_data};
			if (decoded_data == 1'b1) begin
				invert_tx <= 1'b1;
			end else begin
				invert_tx <= 1'b0;
			end
		end
	end


endmodule // pll_test