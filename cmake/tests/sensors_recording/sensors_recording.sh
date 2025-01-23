set -euo pipefail

rm -rf test_trace

./lo2s -S -o test_trace -- sleep 1

sensor=$(sensors | head -n 1)

if  otf2-print test_trace/traces.otf2 | grep  "METRIC" | grep  ${sensor} > /dev/null
then
    exit 0
else
    echo "Trace did not contain any sensor readings!"
    exit 1
fi
