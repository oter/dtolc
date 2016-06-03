module crc_tb; 
	reg          clk;
	reg          rst_n;

	initial begin
		forever #1 clk = ~clk;
	end

	wire [7:0]   test_data [15:0];
	wire [127:0] test_data_unpacked; 

	assign test_data_unpacked = 'h80_55_90_ba_87_28_98_16_34_82_ce_9e_c7_0e_f4_bb;

	wire [31:0] test_expected_crc = 'hBE53A968;

	genvar i1;
    generate
        for (i1 = 0; i1 < 16; i1 = i1 + 1) begin : GEN_CRC_OUT
            assign test_data[i1] = test_data_unpacked[i1 * 8 + 7 : i1 * 8];
        end
    endgenerate

    reg          crc_init;
	reg  [7:0]   crc_data;
	reg          crc_data_en;
	reg          crc_rd;
	wire [7:0]   crc_out;

	crc_gen cg_inst
	(
		.i_clk(clk),
		.i_rst_n(rst_n),
		.i_init(crc_init),
		.i_data(crc_data),
		.i_data_en(crc_data_en),
		.i_data_rd(crc_rd),
		.o_crc(crc_out)
	);

	reg [7:0] result_crc[3:0];

	integer i2;

	initial begin
		crc_init = 0;
		crc_data = 0;
		crc_data_en = 0;
		crc_rd = 0;

		clk = 0;
		rst_n = 0;
		#2;
		rst_n = 1;

		@(posedge clk);
		crc_init = 1;
		
		for (i2 = 0; i2 < 16; i2 = i2 + 1) begin
			@(posedge clk);
			crc_data_en = 1;
			crc_init = 0;
			crc_data = test_data[i2];		
		end
		@(posedge clk);
		crc_data_en = 0;
		crc_rd = 1;

		result_crc[0] = crc_out;

		@(posedge clk);
		result_crc[1] = crc_out;

		@(posedge clk);
		result_crc[2] = crc_out;

		@(posedge clk);
		result_crc[3] = crc_out;
		crc_rd = 0;


		@(posedge clk);
		if ({result_crc[3], result_crc[2], result_crc[1], result_crc[0]} != test_expected_crc) begin
			$display("CRC test failed!");
		end else begin
			$display("CRC test passed!");
		end
		
		@(posedge clk);
		@(posedge clk);
		$finish;

   	end

endmodule // crc_tb