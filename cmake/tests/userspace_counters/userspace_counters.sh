#!/usr/bin/env bash
set -euo pipefail

rm -rf test_trace

./lo2s --userspace-metric-event cpu-cycles  --output-trace test_trace -- sleep 1

if  otf2-print test_trace/traces.otf2 | grep  "METRIC" | grep  "cpu-cycles" > /dev/null
then
    exit 0
else
    echo "Trace does not contain counter metrics read as userspace-metric-event!"
    exit 1
fi
