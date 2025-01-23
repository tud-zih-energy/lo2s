#!/usr/bin/env bash
set -euo pipefail

rm -rf test_trace

./lo2s -c 1 -A --output-trace test_trace -- sleep 1

if  otf2-print test_trace/traces.otf2 | grep  "SAMPLE" > /dev/null
then
    exit 0
else
    echo "Trace did not contain calling context samples in system mode!"
    exit 1
fi
