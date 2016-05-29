module m_transmitter_tb; 
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

	wire [15:0] data_size;
	wire [7:0] frames_count;
	wire [7:0] status;
	wire tx;

	m_transmitter tx_inst(
		.i_clk(clk),
		.i_clk_2x(clk2x),
		.i_rst_n(rst_n),
		.i_data(data),
		.i_data_we(data_we),
		.i_push_frame(push_frame),
		.o_data_size(data_size),
		.o_frames_count(frames_count),
		.o_status(status),
		.o_tx(tx)
	);

	initial begin
		data = 0;
		data_we = 0;
		push_frame = 0;
		#6;
		data = 8'b00000001;
		@(posedge clk);
		data_we = 1;
		@(posedge clk);
		data = 8'b00000000;
		@(posedge clk);
		data = 8'b11110100;
		@(posedge clk);
		data_we = 0;
		push_frame = 1;
		@(posedge clk);
		push_frame = 0;

		#1000;
		$stop;
	end
    
endmodule 