# Batch:
#   vivado -mode batch -source run_s27.tcl
#
# Output:
#   s27_vivado.blif.out   timing graph in LatchSTA format
#   s27_vivado.dcp        implemented design
#
# This reproduces s27's node set and file format. Delay numbers will NOT
# match benchmark/s27.blif.out (different cell library / FPGA architecture).

set script_dir [file dirname [file normalize [info script]]]
set out_dir    [file join $script_dir out]
file mkdir $out_dir

# Change the part if needed; OOC synthesis does not require a board.
set part xc7a35tcpg236-1

create_project -in_memory -part $part
read_verilog [file join $script_dir s27.v]
read_xdc     [file join $script_dir s27.xdc]

synth_design -top s27 -flatten_hierarchy none -mode out_of_context
opt_design
place_design
route_design

write_checkpoint -force [file join $out_dir s27_vivado.dcp]
report_timing_summary -file [file join $out_dir s27_timing_summary.rpt]
report_utilization    -file [file join $out_dir s27_util.rpt]

# --- map Vivado objects onto LatchSTA node names -----------------------
# INPUT/OUTPUT collapse all PIs/POs, matching getDelayGraph().
# Sequential cells G5/G6/G7 keep those names (Vivado may append _reg).

proc node_name {obj} {
    set cls [get_property CLASS $obj]
    if {$cls eq "port"} {
        set dir [get_property DIRECTION $obj]
        if {$dir eq "IN"} {
            return "INPUT"
        }
        return "OUTPUT"
    }
    set n [get_property NAME $obj]
    # G5_reg -> G5
    if {[regsub {_reg$} $n n]} {
        return $n
    }
    # hierarchical leftover: foo/G5_reg
    set tail [lindex [split $n /] end]
    regsub {_reg$} $tail tail
    return $tail
}

proc arc_delay {from to dtype} {
    set paths [get_timing_paths -from $from -to $to -delay_type $dtype \
                   -nworst 1 -max_paths 1 -quiet]
    if {$paths eq ""} {
        return ""
    }
    # Combinational datapath only (Q->D or port->D), not slack.
    return [get_property DATAPATH_DELAY $paths]
}

set clk_ports [get_ports clk]
set starts [list]
set ends   [list]

foreach p [get_ports -filter {DIRECTION == IN}] {
    if {[lsearch -exact $clk_ports $p] >= 0} {
        continue
    }
    lappend starts $p
}
foreach c [all_registers] {
    lappend starts $c
    lappend ends   $c
}
foreach p [get_ports -filter {DIRECTION == OUT}] {
    lappend ends $p
}

array unset maxd
array unset mind
set keys [list]

foreach s $starts {
    foreach e $ends {
        if {$s eq $e && [get_property CLASS $s] eq "port"} {
            continue
        }
        set dmax [arc_delay $s $e max]
        set dmin [arc_delay $s $e min]
        if {$dmax eq "" && $dmin eq ""} {
            continue
        }
        if {$dmax eq ""} { set dmax $dmin }
        if {$dmin eq ""} { set dmin $dmax }
        set ns [node_name $s]
        set ne [node_name $e]
        set k  "$ns $ne"
        if {![info exists maxd($k)]} {
            set maxd($k) $dmax
            set mind($k) $dmin
            lappend keys $k
        } else {
            if {$dmax > $maxd($k)} { set maxd($k) $dmax }
            if {$dmin < $mind($k)} { set mind($k) $dmin }
        }
    }
}

set ofile [file join $out_dir s27_vivado.blif.out]
set fp [open $ofile w]
puts $fp "Network s27.bench"
puts $fp "latch latch max_delay min_delay"
puts $fp "INPUT latch max_delay min_delay"
puts $fp "latch OUTPUT max_delay min_delay"
foreach k $keys {
    puts $fp [format "%s %.6f %.6f" $k $maxd($k) $mind($k)]
}
close $fp

puts "Wrote $ofile"

source [file join $script_dir report_s27_timing.tcl]
