#!/usr/bin/env bash
set -euo pipefail

rm -rf test_trace

./lo2s -s write -o test_trace -- echo "FOOBAR"

if  otf2-print test_trace/traces.otf2 | grep  "ENTER" | grep  "write" > /dev/null
then
    exit 0
else
    echo "Trace does not contain write() syscall events!"
    exit 1
fi
