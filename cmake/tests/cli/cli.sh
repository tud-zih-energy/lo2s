#!/usr/bin/env bash

function run_test()
{
lo2s="./lo2s"
what_str=$1
lo2s_cmd=$2
jq_cmd=$3
eq_val=$4

config_output=$(eval "$lo2s --dump-config $lo2s_cmd")
lo2s_output=$(echo $config_output | eval "jq $jq_cmd")

if test "$lo2s_output" != "$eq_val";
then
    echo "CLI test for $lo2s $lo2s_cmd failed"
    echo "test case was: $what_str"
    echo "expected $jq_cmd value: $eq_val"
    echo "actual   $jq_cmd value: $lo2s_output"

    exit 1
else
    echo "Test \"$what_str\" succeeded"
    echo "Tested command: $lo2s $lo2s_cmd"
fi
}

function process_mode_default_on()
{
component=$1
field=$2
run_test "Should use $component by default in process mode" "-- true" "$field" 'true'
}
function process_mode_default_off()
{
component=$1
field=$2
run_test "Should not use $component by default in process mode" "-- true" "$field" 'false'
}

run_test "Shouldn't use perf" "-a --no-process-recording" "'.perf.use_perf'"  'false'

run_test "Command should be 'true" "-- true" "'.program_under_test.command[0]'" "\"true\""

run_test "Default DWARF mode should be 'local'" "-- true" ".dwarf.usage" "\"local\""

process_mode_default_off "HIP" ".accel.hip"
process_mode_default_off "NEC" ".accel.nec"
process_mode_default_off "CUDA" ".accel.nvidia"
process_mode_default_off "OpenMP" ".accel.openmp"
process_mode_default_off "x86_adapt" ".x86_adapt.enabled"
process_mode_default_off "x86_energy" ".x86_energy.enabled"
process_mode_default_off "drop_root" ".program_under_test.as_user.drop_root"
process_mode_default_off "quiet" ".general.quiet"
process_mode_default_off "userspace counters" ".perf.userspace.enabled"
process_mode_default_off "group counters" ".perf.group.enabled"

process_mode_default_on "sampling" ".perf.sampling.enabled"
process_mode_default_on "Python Sampling" ".program_under_test.use_python"
