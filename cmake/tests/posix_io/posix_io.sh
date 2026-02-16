#!/usr/bin/env bash
set -euo pipefail

if ! test $UID = 0; then
	echo "The Posix I/O test needs superuser rights"
	exit 127
fi

rm -rf test_trace

./lo2s -c 1 --posix-io --output-trace test_trace -- bash -c "echo "TEST!" > test_file && sleep 1 && sync"

rm test_file

if otf2-print test_trace/traces.otf2 | grep "IO_OPERATION" >/dev/null; then
	exit 0
else
	echo "Trace did not contain IO operations!"
	exit 1
fi
