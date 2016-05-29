module top(
	i_clk,
	i_rst_n,
	i_en,
	i_sync,
	i_cmd,
	i_data,
	o_data,
	o_sync,
	o_int,
	o_led_0,
	o_led_1
);

input i_clk;
input i_rst_n;
input i_en;
input i_sync; // Done
input [3:0] i_cmd; // Done
input [7:0] i_data; // Done
output [7:0] o_data; // Done
ouput o_sync; // Done
output o_int;
output o_led_0;
output o_led_1;

wire [7:0] regs_map[0:15];


// Enable logic
reg en;

always @(posedge i_clk or negedge i_rst_n) begin
	if (!i_rst_n) begin
		en <= 1'b0;
	end else begin
		if(i_en == 1'b1) begin
			en <= 1'b1;
		end else begin
			en <= 1'b0;
		end
	end
end

// IO data sync logic
parameter CMD_IDLE 			= 4'b0000;
parameter CMD_READ_REG 		= 4'b0001;
parameter CMD_WRITE_REG 	= 4'b1xxx; // xxx - address of the writable register
parameter CMD_READ_REG 		= 4'b0010; 
parameter CMD_READ_REG 		= 4'b0010; 

parameter IO_STATE_IDLE 	= 0;
parameter IO_STATE_IO 		= 1;

reg [2:0] tx_state;
reg [2:0] rx_state;


reg [1:0] io_state;
reg sync;
reg [3:0] cmd;
reg [7:0] input_data;
reg [7:0] output_data;

always @(posedge i_clk or negedge i_rst_n) begin
	if (!i_rst_n) begin
		io_state <= 2'b0;
		sync <= 1'b0;
		cmd <= 4'b0;
		input_data <= 8'b0;
		output_data <= 8'b0;
	end else begin
		if(i_en == 1'b1) begin
			case (io_state)
				IO_STATE_IDLE: begin
					if (sync != i_sync) begin
						io_state <= IO_STATE_READ;
						cmd <= i_cmd;
						input_data <= i_data;
					end else begin
						io_state <= io_state;
					end
					sync <= sync;
					output_data <= output_data;
				end
				IO_STATE_IO: begin
					sync <= i_sync;
					cmd <= cmd;
					input_data <= input_data;
					case(cmd)
						CMD_IDLE: begin
							io_state <= IO_STATE_IDLE;
							
							output_data <= output_data;
						end
						default: begin
							io_state <= IO_STATE_IDLE;
						end
					endcase // cmd
				end
				default: begin
					io_state <= IO_STATE_IDLE;
				end
			endcase // io_state
		end else begin
			io_state <= 2'b0;
			sync <= 1'b0;
			cmd <= 4'b0;
			input_data <= 8'b0;
			output_data <= 8'b0;
		end
	end
end

assign o_data = output_data;
assign o_sync = sync;





reg [7:0] 


reg led;
reg [31:0] counter;







assign o_led_0 = led;

wire pfdena;
wire pllena_sig;
wire c0;
pll	pll_inst (
	.areset 	(!i_rst_n),
	.inclk0 ( i_clk ),
	.pfdena ( pfdena ),
	.pllena ( pllena_sig ),
	.c0 		( c0 ),
	.locked ( o_led_1 )
	);
	
assign pfdena = 1;
assign pllena_sig = 1;


always @(posedge c0 or negedge i_rst_n) begin
	if (!i_rst_n) begin
	  counter <= 0;
	  led <= 0;
	end else begin
		if (counter == 29) begin
			counter <= 0;
			led <= !led;
		end else begin
			counter <= counter + 1;
		end
	end
end

wire [7:0] tx_i_data;
wire tx_i_data_we;
wire [15:0] tx_o_data_count;
wire [7:0] tx_o_frames_count;
wire [7:0] tx_o_status;
wire [7:0] tx_o_tx;

m_transmitter tx_inst(
	.i_clk(i_clk),
	.i_clk_2x(x0),
	.i_rst_n(i_rst_n),
	.i_data(tx_i_data),
	.i_data_we(tx_i_data_we),
	.o_data_count(tx_o_data_count),
	.o_frames_count(tx_o_frames_count),
	.o_status(tx_o_status),
	.o_tx(tx_o_tx)
);


endmodule