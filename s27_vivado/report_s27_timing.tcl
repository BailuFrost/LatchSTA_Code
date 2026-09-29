# Sourced from run_s27.tcl after route_design.
# Writes:
#   s27_ff_params.txt      Tsu / Thd / Tcq of G5 G6 G7
#   s27_combo_paths.txt    LUT-mapped combo max/min per arc
#   s27_paths_max.rpt      full setup-corner path reports
#   s27_paths_min.rpt      full hold-corner path reports

proc ff_base {cell} {
    set n [get_property NAME $cell]
    regsub {_reg$} $n n
    return [lindex [split $n /] end]
}

proc arc_num {arc names} {
    foreach p $names {
        set v [get_property -quiet $p $arc]
        if {$v ne "" && [string is double -strict $v]} {
            return $v
        }
    }
    return ""
}

proc worst_arc_delay {arcs {corner max}} {
    if {$arcs eq ""} {
        return ""
    }
    if {$corner eq "max"} {
        set props {DELAY_MAX DELAY_MAX_RISE DELAY_MAX_FALL DELAY}
    } else {
        set props {DELAY_MIN DELAY_MIN_RISE DELAY_MIN_FALL DELAY}
    }
    set worst ""
    foreach a $arcs {
        set v [arc_num $a $props]
        if {$v eq ""} {
            continue
        }
        if {$worst eq ""} {
            set worst $v
            continue
        }
        if {$corner eq "max" && $v > $worst} { set worst $v }
        if {$corner eq "min" && $v < $worst} { set worst $v }
    }
    return $worst
}

proc lut_names {path} {
    set cells [get_cells -quiet -of_objects $path \
        -filter {PRIMITIVE_GROUP == LUT || REF_NAME =~ "LUT*" || REF_NAME =~ "LUT"}]
    set names [list]
    foreach c $cells {
        lappend names "[get_property NAME $c]([get_property REF_NAME $c])"
    }
    if {[llength $names] == 0} {
        return "(none / routed-only)"
    }
    return [join $names " -> "]
}

set ff_file [file join $out_dir s27_ff_params.txt]
set ff_fp [open $ff_file w]
puts $ff_fp "Flip-flop library timing after implementation (ns)"
puts $ff_fp "Device part: [get_property PART [current_design]]"
puts $ff_fp "These come from the speed file of the FDRE/FDSE primitive, not from RTL."
puts $ff_fp "Tsu/Thd: D vs C. Tcq: C vs Q. Rise/fall collapsed to worst of the corner."
puts $ff_fp ""
puts $ff_fp [format "%-8s %-10s %-12s %-12s %-12s %-12s %-12s %-12s %s" \
    name site bel ref Tsu Tcq_max Tcq_min Thd]

foreach cell [lsort -dictionary [all_registers]] {
    set dpin [get_pins -quiet $cell/D]
    set cpin [get_pins -quiet $cell/C]
    set qpin [get_pins -quiet $cell/Q]
    set setup_arcs [get_timing_arcs -quiet -from $cpin -to $dpin -filter {TYPE =~ "*setup*" || TYPE == "setup"}]
    if {$setup_arcs eq ""} {
        set setup_arcs [get_timing_arcs -quiet -to $dpin -filter {TYPE =~ "*setup*"}]
    }
    set hold_arcs [get_timing_arcs -quiet -from $cpin -to $dpin -filter {TYPE =~ "*hold*" || TYPE == "hold"}]
    if {$hold_arcs eq ""} {
        set hold_arcs [get_timing_arcs -quiet -to $dpin -filter {TYPE =~ "*hold*"}]
    }
    set cq_arcs [get_timing_arcs -quiet -from $cpin -to $qpin]
    set tsu  [worst_arc_delay $setup_arcs max]
    set thd  [worst_arc_delay $hold_arcs max]
    set tcqM [worst_arc_delay $cq_arcs max]
    set tcqm [worst_arc_delay $cq_arcs min]
    foreach x {tsu thd tcqM tcqm} {
        if {[set $x] eq ""} { set $x "n/a" } else { set $x [format "%.4f" [set $x]] }
    }
    puts $ff_fp [format "%-8s %-10s %-12s %-12s %-12s %-12s %-12s %-12s %s" \
        [ff_base $cell] \
        [get_property SITE $cell] \
        [get_property BEL $cell] \
        [get_property REF_NAME $cell] \
        $tsu $tcqM $tcqm $thd \
        ""]
}
close $ff_fp

report_timing -from [all_registers] -to [all_registers] \
    -delay_type max -max_paths 20 -nworst 1 -input_pins -routable_nets \
    -file [file join $out_dir s27_paths_max.rpt]
report_timing -from [all_registers] -to [all_registers] \
    -delay_type min -max_paths 20 -nworst 1 -input_pins -routable_nets \
    -file [file join $out_dir s27_paths_min.rpt]
report_timing -from [all_inputs] -to [all_outputs] \
    -delay_type min_max -max_paths 20 -nworst 1 -input_pins \
    -file [file join $out_dir s27_paths_io.rpt]

set combo_file [file join $out_dir s27_combo_paths.txt]
set cfp [open $combo_file w]
puts $cfp "LUT-mapped combinational arcs (Q/port -> D/port), delays in ns"
puts $cfp "max = setup corner (slow), min = hold corner (fast)"
puts $cfp "DATAPATH is Q-to-D (or port) and does not include Tsu/Thd."
puts $cfp "Original AND/OR/NOT names are gone after mapping; LUT cells are listed."
puts $cfp ""
puts $cfp [format "%-8s %-8s %-8s %-8s %-8s %-8s %-6s %s" \
    from to max min logic net lvls LUT_chain]

proc dump_combo {from to from_name to_name cfp} {
    set pmax [get_timing_paths -from $from -to $to -delay_type max -nworst 1 -max_paths 1 -quiet]
    set pmin [get_timing_paths -from $from -to $to -delay_type min -nworst 1 -max_paths 1 -quiet]
    if {$pmax eq "" && $pmin eq ""} {
        return
    }
    set path $pmax
    if {$path eq ""} { set path $pmin }
    set dmax [get_property -quiet DATAPATH_DELAY $pmax]
    set dmin [get_property -quiet DATAPATH_DELAY $pmin]
    set logic [get_property -quiet LOGIC_DELAY $path]
    set net   [get_property -quiet NET_DELAY $path]
    set lvls  [get_property -quiet LOGIC_LEVELS $path]
    if {$dmax eq ""} { set dmax "." }
    if {$dmin eq ""} { set dmin "." }
    if {$logic eq ""} { set logic "." }
    if {$net eq ""} { set net "." }
    if {$lvls eq ""} { set lvls "." }
    puts $cfp [format "%-8s %-8s %-8s %-8s %-8s %-8s %-6s %s" \
        $from_name $to_name \
        [expr {$dmax eq "." ? $dmax : [format "%.4f" $dmax]}] \
        [expr {$dmin eq "." ? $dmin : [format "%.4f" $dmin]}] \
        [expr {$logic eq "." ? $logic : [format "%.4f" $logic]}] \
        [expr {$net eq "." ? $net : [format "%.4f" $net]}] \
        $lvls \
        [lut_names $path]]
}

set clk_ports [get_ports clk]
foreach s [all_registers] {
    set sp [get_pins $s/Q]
    foreach e [all_registers] {
        dump_combo $sp [get_pins $e/D] [ff_base $s] [ff_base $e] $cfp
    }
    foreach p [get_ports -filter {DIRECTION == OUT}] {
        dump_combo $sp $p [ff_base $s] [get_property NAME $p] $cfp
    }
}
foreach p [get_ports -filter {DIRECTION == IN}] {
    if {[lsearch -exact $clk_ports $p] >= 0} {
        continue
    }
    foreach e [all_registers] {
        dump_combo $p [get_pins $e/D] [get_property NAME $p] [ff_base $e] $cfp
    }
    foreach o [get_ports -filter {DIRECTION == OUT}] {
        dump_combo $p $o [get_property NAME $p] [get_property NAME $o] $cfp
    }
}
close $cfp

puts "Wrote $ff_file"
puts "Wrote $combo_file"
