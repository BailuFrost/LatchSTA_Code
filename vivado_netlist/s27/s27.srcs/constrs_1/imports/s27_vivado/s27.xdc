# Period only needs to be larger than any combo path so STA still reports
# a complete timing graph. 20 ns is plenty for this tiny circuit on 7-series.
create_clock -name clk -period 20.000 [get_ports clk]

set clk_ports [get_ports clk]
set data_in   [remove_from_collection [all_inputs] $clk_ports]

set_input_delay  -clock clk 0.000 $data_in
set_output_delay -clock clk 0.000 [all_outputs]

# Keep PI->PO combinational arcs in the timing graph.
set_max_delay 20.000 -from $data_in -to [all_outputs]
set_min_delay  0.000 -from $data_in -to [all_outputs]
