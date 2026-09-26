#!/bin/sh

export PATH=/bin:/sbin:/usr/bin:/usr/sbin

mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t tmpfs tmpfs /tmp
mount -t devtmpfs devtmpfs /dev

mdev -s

[ -x /etc/init.d/rcS ] && /etc/init.d/rcS

if [ -c /dev/console ]; then
	exec 0</dev/console
	exec 1>/dev/console
	exec 2>/dev/console
fi

if [ -c /dev/ttyS0 ]; then
	exec /sbin/getty -n -l /bin/sh 115200 ttyS0 vt100
fi

exec /sbin/init
