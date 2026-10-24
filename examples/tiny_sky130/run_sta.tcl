# Fixed TT, ideal clock, no SPEF: cell loads include connected input capacitance.
cd [file dirname [file normalize [info script]]]
set liberty ../../sky130_pdk/sky130_fd_sc_hd__tt_025C_1v80.lib
if {![file isfile $liberty] || ![file isfile tiny_mapped.v]} {
    error "Missing Liberty or tiny_mapped.v; run Yosys export.ys first"
}

read_liberty $liberty
read_verilog tiny_mapped.v
if {![link_design tiny]} {
    error "Cannot link tiny"
}
foreach cell {sky130_fd_sc_hd__dlxtp_1 sky130_fd_sc_hd__buf_1 sky130_fd_sc_hd__and2_1} {
    if {[llength [get_lib_cells */$cell]] != 1} {
        error "Missing or ambiguous library cell: $cell"
    }
}
if {[llength [get_cells *]] != 11 ||
    [llength [all_registers -level_sensitive -cells]] != 3} {
    error "Expected eleven instances including three latches"
}
foreach pin {L1/D L1/GATE L1/Q L2/D L2/GATE L2/Q L3/D L3/GATE L3/Q} {
    if {[llength [get_pins $pin]] != 1} {
        error "Missing or ambiguous latch pin: $pin"
    }
}

read_sdc tiny.sdc
file mkdir reports
file copy -force tiny.sdc reports/constraints.sdc
log_begin reports/units.rpt
report_units
log_end
log_begin reports/clocks.rpt
report_clock_properties [get_clocks clk]
log_end
check_setup -verbose > reports/check_setup.rpt

report_checks -path_delay max -format full_clock_expanded \
    -fields {slew capacitance input_pin net} \
    -group_path_count 100 -endpoint_path_count 100 -digits 6 \
    > reports/setup.rpt
report_checks -path_delay min -format full_clock_expanded \
    -fields {slew capacitance input_pin net} \
    -group_path_count 100 -endpoint_path_count 100 -digits 6 \
    > reports/hold.rpt
report_check_types -min_pulse_width -digits 6 > reports/pulse_width.rpt

foreach latch {L1 L2 L3} {
    report_instance $latch > reports/${latch}_instance.rpt
    log_begin reports/${latch}_arrival.rpt
    report_arrival -digits 6 [get_pins $latch/D]
    log_end
    report_net -digits 6 [get_full_name [get_nets -of_objects [get_pins $latch/Q]]] \
        > reports/${latch}_load.rpt
    log_begin reports/${latch}_dq_min.rpt
    report_dcalc -from [get_pins $latch/D] -to [get_pins $latch/Q] -min -digits 6
    log_end
    log_begin reports/${latch}_dq_max.rpt
    report_dcalc -from [get_pins $latch/D] -to [get_pins $latch/Q] -max -digits 6
    log_end
    log_begin reports/${latch}_cq_min.rpt
    report_dcalc -from [get_pins $latch/GATE] -to [get_pins $latch/Q] -min -digits 6
    log_end
    log_begin reports/${latch}_cq_max.rpt
    report_dcalc -from [get_pins $latch/GATE] -to [get_pins $latch/Q] -max -digits 6
    log_end
}
puts "Reports written to [file normalize reports]; inspect setup, hold, clock edge mapping and pulse width"
