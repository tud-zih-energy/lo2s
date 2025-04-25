#!/usr/bin/env bash
set -euo pipefail

rm -rf test_trace

# select first pfm event (sed -n '3 p')
# That is not process mode only (grep -v *)

pfm_event=$(./lo2s --list-events | grep -v \* | grep pfm -A 5 | sed -n '3 p')

./lo2s -a --metric-count 1 --metric-leader cpu-cycles -E $pfm_event  --output-trace test_trace -- sleep 1

if  otf2-print test_trace/traces.otf2 | grep  "METRIC" | grep $pfm_event > /dev/null;
then
    exit 0
else
    echo "Trace does not contain counter metrics for ${pfm_event}!"
    exit 1
fi
