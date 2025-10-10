if test ! -d "/sys/kernel/tracing/" || test ! -x "/sys/kernel/tracing/events/syscalls/sys_enter_write"
then
    exit -1
else
    exit 0
fi
