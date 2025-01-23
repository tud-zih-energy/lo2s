#!/usr/bin/env bash

paranoid=$(cat /proc/sys/kernel/perf_event_paranoid)

if [ $paranoid -gt $1 ]
then
    exit -1
else
    exit 0
fi
