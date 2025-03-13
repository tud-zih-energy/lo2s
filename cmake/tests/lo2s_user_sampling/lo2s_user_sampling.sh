set -euo pipefail

paranoid=$(cat /proc/sys/kernel/perf_event_paranoid)

echo "perf_event_paranoid is: $paranoid"
if [ $paranoid -gt 2 ]
then
    # skip
    exit 100
fi

rm -rf test_trace
./lo2s -c 1  --output-trace test_trace -- sleep 1

if  otf2-print test_trace/traces.otf2 | grep  "SAMPLE" > /dev/null
then
    exit 0
else
    echo "Trace did not contain calling context samples!"
    exit 1
fi
rm -rf test_trace

