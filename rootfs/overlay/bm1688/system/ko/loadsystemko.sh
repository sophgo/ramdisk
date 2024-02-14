#!/bin/sh
${CVI_SHOPTS}
#
# Start to insert kernel modules
#
insmod /mnt/system/ko/soph_base.ko
insmod /mnt/system/ko/soph_sys.ko
insmod /mnt/system/ko/soph_mipi_rx.ko
insmod /mnt/system/ko/soph_snsr_i2c.ko
insmod /mnt/system/ko/soph_vi.ko
insmod /mnt/system/ko/soph_vpss.ko
insmod /mnt/system/ko/soph_dwa.ko
insmod /mnt/system/ko/soph_ldc.ko
insmod /mnt/system/ko/soph_rgn.ko
insmod /mnt/system/ko/soph_vo.ko
insmod /mnt/system/ko/soph_mipi_tx.ko
insmod /mnt/system/ko/soph_hdmi.ko
insmod /mnt/system/ko/soph_dpu.ko
insmod /mnt/system/ko/soph_stitch.ko
insmod /mnt/system/ko/soph_ive.ko
insmod /mnt/system/ko/soph_2d_engine.ko

#insmod /mnt/system/ko/soph_wdt.ko
insmod /mnt/system/ko/soph_clock_cooling.ko

insmod /mnt/system/ko/bmtpu.ko
insmod /mnt/system/ko/soph_vc_drv.ko
#insmod /mnt/system/ko/soph_rtc.ko

echo 3 > /proc/sys/vm/drop_caches
dmesg -n 4

#usb hub control
#/etc/uhubon.sh host

exit $?
