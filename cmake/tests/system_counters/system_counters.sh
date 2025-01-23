#!/usr/bin/env bash
set -euo pipefail

rm -rf test_trace
./lo2s -a --metric-count 1 --metric-leader instructions --standard-metrics  --output-trace test_trace -- sleep 1

if  otf2-print test_trace/traces.otf2 | grep  "METRIC" | grep  "cpu-cycles" > /dev/null;
then
    exit 0
else
    echo "Trace does not contain counter metrics in process mode!"
    exit 1
fi
