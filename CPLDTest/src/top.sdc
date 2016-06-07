#create the 50 MHz (20 ns) clock
create_clock -period 20 [get_ports i_clk]

derive_pll_clocks