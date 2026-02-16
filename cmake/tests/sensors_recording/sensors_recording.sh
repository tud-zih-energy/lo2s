set -euo pipefail

if test ! $(command -v sensors); then
	echo "No 'sensors' command found" >&2
	exit 127
fi

sensors_output=$(sensors 2>&1)

if echo $sensors_output | grep -q "No sensors found"; then
	echo "No lm-sensors sensors found" >&2
	exit 127
fi
rm -rf test_trace

./lo2s --no-instruction-sampling -S -o test_trace -- sleep 1

sensor=$(sensors | head -n 1)

if otf2-print test_trace/traces.otf2 | grep "METRIC" | grep ${sensor} >/dev/null; then
	exit 0
else
	echo "Trace did not contain any sensor readings!"
	exit 1
fi
