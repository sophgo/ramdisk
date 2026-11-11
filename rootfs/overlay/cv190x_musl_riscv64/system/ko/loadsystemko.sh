#!/bin/sh
${CVI_SHOPTS}
#
# Start to insert kernel modules
#
insmod /system/ko/cv190x_osal.ko
insmod /system/ko/cv190x_base.ko
insmod /system/ko/cv190x_sys.ko

insmod /system/ko/cv190x_rtos_cmdqu.ko
insmod /system/ko/cv190x_fast_image.ko
insmod /system/ko/cv190x_mipi_rx.ko
insmod /system/ko/cv190x_snsr_i2c.ko
insmod /system/ko/cv190x_vi.ko
insmod /system/ko/cv190x_vpss.ko
insmod /system/ko/cv190x_rgn.ko
insmod /system/ko/cv190x_ldc.ko

#insmod /system/ko/cv190x_wdt.ko
insmod /system/ko/cv190x_thermal.ko
insmod /system/ko/cv190x_clock_cooling.ko

insmod /system/ko/cv190x_tpu.ko
insmod /system/ko/cv190x_vc_drv.ko
#insmod /system/ko/cv190x_rtc.ko
#insmod /system/ko/cv190x_ive.ko

echo 3 > /proc/sys/vm/drop_caches
dmesg -n 4

#usb hub control
#/etc/uhubon.sh host

exit $?
