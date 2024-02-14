#!/bin/sh
${CVI_SHOPTS}

# Copyright (c) 2015, ARM Limited and Contributors. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
#
# Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# Neither the name of ARM nor the names of its contributors may be used
# to endorse or promote products derived from this software without specific
# prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

#Mount filesystems needed by mount and mdev
/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sysfs /sys
echo "init_ubuntu.sh"

#Create all the symlinks to /bin/busybox
/bin/busybox --install -s
chown root:root /bin/busybox
chown root:root /bin/su
chmod 4755 /bin/su

#Create things under /dev
#mask "mdev -s" for PLD
#mdev -s
/bin/busybox mdev -s

until [ -e "/dev/mmcblk0p3" ]; do
  /bin/busybox mdev -s
  echo "wait for mmcblk0p3 created!"
done
until [ -e "/dev/mmcblk0p4" ]; do
  /bin/busybox mdev -s
  echo "wait for mmcblk0p4 created!"
done
until [ -e "/dev/mmcblk0p5" ]; do
  /bin/busybox mdev -s
  echo "wait for mmcblk0p5 created!"
done

#Check and repair rootfs
/bin/busybox touch /etc/mtab
e2fsck -p /dev/mmcblk0p2
e2fsck -p /dev/mmcblk0p3
e2fsck -p /dev/mmcblk0p5
e2fsck -p /dev/mmcblk0p6

#mount the /dev/mmcblk0p2 to /mnt
if [ -e "/dev/mmcblk0p4" ]; then
  e2fsck -p /dev/mmcblk0p4
fi

/bin/busybox mkdir -p /media/root-ro
/bin/busybox mount -o ro /dev/mmcblk0p4 /media/root-ro

/bin/busybox mkdir -p /media/root-rw
/bin/busybox mount /dev/mmcblk0p5 /media/root-rw

rm -rf  /media/root-rw/overlay-workdir/index/*

/bin/busybox mkdir -p /media/root-rw/overlay
/bin/busybox mkdir -p /media/root-rw/overlay-workdir
/bin/mount -t overlay -o lowerdir=/media/root-ro,upperdir=/media/root-rw/overlay,workdir=/media/root-rw/overlay-workdir overlay /mnt

/bin/busybox cp /mnt/etc/fstab.emmc.ro /mnt/etc/fstab

#clean up
/bin/busybox umount /proc
/bin/busybox umount /sys

#Redirect output
exec 0</dev/console
exec 1>/dev/console
exec 2>/dev/console

#Go!
#Switch to the real root file system, which is mounted on /mnt
exec switch_root /mnt /init
#exec /sbin/init $*

