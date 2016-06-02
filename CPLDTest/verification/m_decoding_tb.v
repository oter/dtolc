module m_decoding_tb; 
	reg clk;
	reg clk2x;
	reg rst_n;

	initial begin
		clk = 0;
		clk2x = 0;
		rst_n = 0;
		#2;
		rst_n = 1;
	end

	initial begin
		#4;
		forever begin
			#1 clk2x = ~clk2x;
		end
	end

	initial begin
		#3;
		forever begin
			#2 clk = ~clk;
		end
	end

	reg [7:0] data;
	reg data_we;
	reg push_frame;

endmodule // m_decoding_tb