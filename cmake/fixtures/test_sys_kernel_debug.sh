if test ! -d "/sys/kernel/debug/tracing/" || test ! -x "/sys/kernel/debug/tracing"
then
    exit -1
else
    exit 0
fi
