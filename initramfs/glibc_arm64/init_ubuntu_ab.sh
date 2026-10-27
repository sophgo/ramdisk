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

# Mount filesystems required by mount and mdev
/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sysfs /sys
echo "init_ubuntu.sh (AB Partition support)"

# Create all symlinks to /bin/busybox
/bin/busybox --install -s
chown root:root /bin/busybox
chown root:root /bin/su
chmod 4755 /bin/su

# Create device nodes under /dev
/bin/busybox mdev -s

# Wait for required partitions to be ready
until [ -e "/dev/mmcblk0p4" ]; do
  /bin/busybox mdev -s
  echo "Waiting for /dev/mmcblk0p4 to be created..."
done
until [ -e "/dev/mmcblk0p5" ]; do
  /bin/busybox mdev -s
  echo "Waiting for /dev/mmcblk0p5 to be created..."
done
until [ -e "/dev/mmcblk0p6" ]; do
  /bin/busybox mdev -s
  echo "Waiting for /dev/mmcblk0p6 to be created..."
done
until [ -e "/dev/mmcblk0p7" ]; do
  /bin/busybox mdev -s
  echo "Waiting for /dev/mmcblk0p7 to be created..."
done
until [ -e "/dev/mmcblk0p8" ]; do
  /bin/busybox mdev -s
  echo "Waiting for /dev/mmcblk0p8 to be created..."
done

BOOT_SLOT="a"

# Read kernel command line and parse boot_slot parameter
if [ -f "/proc/cmdline" ]; then
  CMDLINE=$(/bin/busybox cat /proc/cmdline)
  # Extract boot_slot parameter (supports boot_slot=a or boot_slot=b)
  for arg in $CMDLINE; do
    case $arg in
      boot_slot=*)
        BOOT_SLOT=${arg#*=}
        # Validate boot_slot value, fall back to 'a' if invalid
        if [ "$BOOT_SLOT" != "a" ] && [ "$BOOT_SLOT" != "b" ]; then
          echo "Invalid boot_slot '$BOOT_SLOT', falling back to 'a'"
          BOOT_SLOT="a"
        fi
        echo "Detected boot_slot: $BOOT_SLOT"
        ;;
    esac
  done
else
  echo "/proc/cmdline not found, using default boot_slot=a"
fi

# Select rootfs partitions based on boot slot
if [ "$BOOT_SLOT" = "a" ]; then
  ROOTFS_PART="/dev/mmcblk0p5"    # ROOTFS_A (default)
  ROOTFS_RW_PART="/dev/mmcblk0p7" # ROOTFS_RW_A
  FSTAB_FILE="fstab.emmc_a.ro"
else
  ROOTFS_PART="/dev/mmcblk0p6"    # ROOTFS_B
  ROOTFS_RW_PART="/dev/mmcblk0p8" # ROOTFS_RW_B
  FSTAB_FILE="fstab.emmc_b.ro"
fi

# Check and repair root filesystems
/bin/busybox touch /etc/mtab
e2fsck -p $ROOTFS_PART
e2fsck -p $ROOTFS_RW_PART
e2fsck -p /dev/mmcblk0p9

# Mount read-only root partition
/bin/busybox mkdir -p /media/root-ro
/bin/busybox mount -o ro $ROOTFS_PART /media/root-ro

# ====================== Check if root-ro mount succeeded ======================
if [ $? -ne 0 ] || [ ! -d /media/root-ro/sbin ]; then
    echo "============================================================"
    echo "CRITICAL ERROR: Mount root-ro FAILED!"
    echo "System will reboot in 5 seconds and switch to backup slot..."
    echo "============================================================"
    sleep 5
    reboot -f
    exit 1
fi

# Mount read-write partition for overlay
/bin/busybox mkdir -p /media/root-rw
/bin/busybox mount $ROOTFS_RW_PART /media/root-rw

if [ $? -ne 0 ]; then
    echo "============================================================"
    echo "CRITICAL ERROR: Mount root-rw FAILED!"
    echo "System will reboot in 5 seconds and switch to backup slot..."
    echo "============================================================"
    sleep 5
    reboot -f
    exit 1
fi

# Clean up old overlay workdir
/bin/busybox rm -rf /media/root-rw/overlay-workdir

# Prepare overlay filesystem directories
/bin/busybox mkdir -p /media/root-rw/overlay
/bin/busybox mkdir -p /media/root-rw/overlay-workdir

# Mount overlay rootfs (ro + rw)
/bin/mount -t overlay -o lowerdir=/media/root-ro,upperdir=/media/root-rw/overlay,workdir=/media/root-rw/overlay-workdir overlay /mnt

# ====================== CRITICAL: Check if overlay mount succeeded ======================
if [ $? -ne 0 ] || [ ! -d /mnt/sbin ]; then
    echo "============================================================"
    echo "CRITICAL ERROR: Rootfs overlay mount FAILED!!!"
    echo "Slot $BOOT_SLOT is corrupted, rebooting to rollback..."
    echo "============================================================"
    sleep 5
    reboot -f  # Force reboot; bootloader will switch to backup slot
    exit 1
fi

echo "== Rootfs overlay mounted SUCCESSFULLY =="
echo "== Marking slot $BOOT_SLOT as SUCCESSFUL =="

# Mark current slot as successfully booted (misc partition: /dev/mmcblk0p4)
/bin/cvi_ab_boot -d /dev/mmcblk0p4 -m

# Install correct fstab for current boot slot
/bin/busybox cp /mnt/etc/$FSTAB_FILE /mnt/etc/fstab

# Clean up temporary mounts
/bin/busybox umount /proc
/bin/busybox umount /sys

# Redirect stdio to console
exec 0</dev/console
exec 1>/dev/console
exec 2>/dev/console

# Switch to real root filesystem mounted at /mnt
exec switch_root /mnt /init
