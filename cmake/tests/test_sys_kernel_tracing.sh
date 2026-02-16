if test ! -r "/sys/kernel/tracing/events/syscalls/sys_enter_write/format"; then
	exit -1
else
	exit 0
fi
