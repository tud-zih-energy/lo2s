set -euo pipefail

paranoid=$(cat /proc/sys/kernel/perf_event_paranoid)

echo "perf_event_paranoid is: $paranoid"

if [ $paranoid -gt -1 ]
then
    # skip
    exit 100
fi

if test ! -d "/sys/kernel/debug/tracing/" || test ! -x "/sys/kernel/debug/tracing"
then
    exit 100
fi

rm -rf test_trace
./lo2s -t syscalls/sys_enter_write -o test_trace -- echo "FOOBAR"

sensor=$(sensors | head -n 1)

if  otf2-print test_trace/traces.otf2 | grep  "METRIC" | grep  "sys_enter_write::__syscall_nr" > /dev/null
then
    exit 0
else
    echo "Trace did not contain write() syscall events!"
    exit 1
fi
rm -rf test_trace

