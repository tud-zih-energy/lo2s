#!/usr/bin/env bash
set -euo pipefail

perf list --json | jq '.[] | select(.EventType=="Hardware event" or .EventType=="Software event" or .EventType=="Hardware cache event") | .EventName'| while read ev;do
    rm -rf test_trace
    ev=$(echo $ev | tr -d \")
    ./lo2s -a --userspace-metric-event  $ev --output-trace test_trace -- sleep 1
    if  otf2-print test_trace/traces.otf2| grep  "METRIC" | grep $ev;
    then
        echo "Succesfully recorded $ev!"
    else
        echo "Trace does not contain \"$ev\" metrics in process mode!"
    exit 1
    fi
done
