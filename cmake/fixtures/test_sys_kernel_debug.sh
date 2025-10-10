if test ! -d "/sys/kernel/tracing/" || test ! -x "/sys/kernel/tracing"
then
    exit -1
else
    exit 0
fi
