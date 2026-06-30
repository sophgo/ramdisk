#!/bin/sh

# AIC8800 WiFi auto-start

USERDATAPATH=/mnt/data
SYSTEMPATH=/mnt/system
FW_DIR=${SYSTEMPATH}/firmware/aic8800
CFG_FILE=${FW_DIR}/rwnx_settings.ini
WIFI_IF=wlan0
WPA_CONF=/etc/init.d/wpa_supplicant.conf
AIC_CHIP=

# What changed: Make module loading skip built-in or already-loaded kernel modules.
# Previous behavior: The legacy script inserted AIC modules directly and assumed cfg80211 was built in.
# Impact: The same STA script supports CONFIG_CFG80211=y, CONFIG_CFG80211=m, and repeated start/restart calls.
load_ko()
{
      ko="$1"
      mod="$2"

      if [ -n "$mod" ] && [ -d "/sys/module/$mod" ]; then
            return 0
      fi
      if [ -f "$ko" ]; then
            insmod "$ko" 2>/dev/null || true
      fi
}

# What changed: Add a reusable firmware list validator for AIC8800 variants.
# Previous behavior: Firmware validation was not aware of the new AIC8800DC package.
# Impact: The same STA script can report DC and D80 firmware package issues without blocking legacy startup.
check_fw_list()
{
      chip="$1"
      shift

      for fw in "$@"
      do
            if [ ! -f "$FW_DIR/$fw" ]; then
                  echo "AIC8800 $chip firmware missing: $FW_DIR/$fw"
                  return 1
            fi
      done

      return 0
}

# What changed: Detect the packaged AIC8800 firmware variant at runtime.
# Previous behavior: The STA script was written for AIC8800D80 and had no DC firmware awareness.
# Impact: Board defconfig can choose AIC8800DC or AIC8800D80 while keeping this STA script common.
check_fw()
{
      if [ -f "$FW_DIR/fw_patch_table_8800dc_u02h.bin" ]; then
            AIC_CHIP=8800dc
            check_fw_list "$AIC_CHIP" \
                  fw_patch_table_8800dc_u02h.bin \
                  fw_adid_8800dc_u02h.bin \
                  fw_patch_8800dc_u02h.bin \
                  fmacfw_patch_8800dc_h_u02.bin \
                  fmacfw_calib_8800dc_h_u02.bin \
                  fmacfw_patch_tbl_8800dc_h_u02.bin \
                  aic_userconfig_8800dc.txt \
                  aic_powerlimit_8800dc.txt \
                  rwnx_settings.ini
            return $?
      fi

      if [ -f "$FW_DIR/fw_patch_table_8800d80_u02.bin" ]; then
            AIC_CHIP=8800d80
            check_fw_list "$AIC_CHIP" \
                  fw_patch_table_8800d80_u02.bin \
                  fw_adid_8800d80_u02.bin \
                  fw_patch_8800d80_u02.bin \
                  fmacfw_8800d80_u02.bin \
                  lmacfw_rf_8800d80_u02.bin \
                  aic_userconfig_8800d80.txt \
                  aic_powerlimit_8800d80.txt \
                  rwnx_settings.ini
            return $?
      fi

      echo "AIC8800 firmware missing under $FW_DIR"
      return 1
}

case "$1" in
  start)
      # What changed: Keep firmware validation as a warning-only compatibility check.
      # Previous behavior: The legacy STA script did not validate the firmware package at all.
      # Impact: DC/D80 packaging issues are visible while the old D80 startup flow remains tolerant.
      if check_fw; then
            echo "AIC8800 firmware detected: $AIC_CHIP"
      fi

      # What changed: Use one module loader for cfg80211 and AIC modules with /sys/module checks.
      # Previous behavior: The legacy script only inserted AIC modules and assumed cfg80211 was built in.
      # Impact: CONFIG_CFG80211=y, already-loaded modules, missing cfg80211.ko, and CONFIG_CFG80211=m are all tolerated.
      load_ko "$SYSTEMPATH/ko/cfg80211.ko" cfg80211
      load_ko "$SYSTEMPATH/ko/aic8800_bsp.ko" aic8800_bsp
      load_ko "$SYSTEMPATH/ko/aic8800_fdrv.ko" aic8800_fdrv

      # Bring interface up
      if ip link show "$WIFI_IF" >/dev/null 2>&1; then
            ip link set "$WIFI_IF" up || true
      fi

      # Start wpa_supplicant if config exists
      if [ -f "$WPA_CONF" ]; then
            if pidof wpa_supplicant >/dev/null 2>&1; then
                  killall wpa_supplicant 2>/dev/null || true
                  sleep 1
            fi
            wpa_supplicant -B -i "$WIFI_IF" -c "$WPA_CONF" -D nl80211 || wpa_supplicant -B -i "$WIFI_IF" -c "$WPA_CONF" -D wext || true
      fi

      # DHCP
      if command -v udhcpc >/dev/null 2>&1; then
            udhcpc -i "$WIFI_IF" -s /usr/share/udhcpc/default.script -t 5 -T 3 -A 5 -q 2>/dev/null &
      fi
      ;;
  stop)
      # Stop supplicant and bring iface down
      if pidof wpa_supplicant >/dev/null 2>&1; then
            killall wpa_supplicant 2>/dev/null || true
      fi
      if ip link show "$WIFI_IF" >/dev/null 2>&1; then
            ip link set "$WIFI_IF" down || true
      fi
      ;;
  restart|reload)
      $0 stop
      sleep 1
      $0 start
      ;;
  *)
      echo "Usage: $0 {start|stop|restart}"
      exit 1
      ;;
esac

exit $?


