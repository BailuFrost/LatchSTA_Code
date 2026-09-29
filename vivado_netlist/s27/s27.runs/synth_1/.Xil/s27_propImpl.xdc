set_property SRC_FILE_INFO {cfile:E:/LatchSTA_Code/code/vivado_netlist/s27/s27.srcs/constrs_1/imports/s27_vivado/s27.xdc rfile:../../../s27.srcs/constrs_1/imports/s27_vivado/s27.xdc id:1} [current_design]
set_property src_info {type:XDC file:1 line:6 export:INPUT save:INPUT read:READ} [current_design]
set data_in   [remove_from_collection [all_inputs] [get_ports clk]]
set_property src_info {type:XDC file:1 line:8 export:INPUT save:INPUT read:READ} [current_design]
set_input_delay  -clock clk 0.000 $data_in
set_property src_info {type:XDC file:1 line:12 export:INPUT save:INPUT read:READ} [current_design]
set_max_delay 20.000 -from $data_in -to [all_outputs]
set_property src_info {type:XDC file:1 line:13 export:INPUT save:INPUT read:READ} [current_design]
set_min_delay  0.000 -from $data_in -to [all_outputs]
