#!/bin/sh

# AIC8800 WiFi auto-start (AP mode)

USERDATAPATH=/mnt/data
SYSTEMPATH=/mnt/system
FW_DIR=${SYSTEMPATH}/firmware/aic8800
CFG_FILE=${FW_DIR}/rwnx_settings.ini
WIFI_IF=wlan0
AP_CONF=/etc/network/hostapd.conf
UDHCPD_CONF=/etc/network/udhcpd.conf
WIFI_IP=192.168.1.1/24
AIC_CHIP=

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
      if check_fw; then
            echo "AIC8800 firmware detected: $AIC_CHIP"
      fi

      load_ko "$SYSTEMPATH/ko/cfg80211.ko" cfg80211
      load_ko "$SYSTEMPATH/ko/aic8800_bsp.ko" aic8800_bsp
      load_ko "$SYSTEMPATH/ko/aic8800_fdrv.ko" aic8800_fdrv

      # Bring interface up with static IP for AP
      if ip link show "$WIFI_IF" >/dev/null 2>&1; then
            ip link set "$WIFI_IF" up || true
            ip addr flush dev "$WIFI_IF" || true
            ip addr add "$WIFI_IP" dev "$WIFI_IF" || true
      fi

      # Start hostapd
      if [ -f "$AP_CONF" ]; then
            if pidof hostapd >/dev/null 2>&1; then
                  killall hostapd 2>/dev/null || true
                  sleep 1
            fi
            hostapd -i "$WIFI_IF" -B "$AP_CONF" || true
      fi

      mkdir -p /var/lib/misc

      # Start DHCP server
      if [ -f "$UDHCPD_CONF" ] && command -v udhcpd >/dev/null 2>&1; then
            if pidof udhcpd >/dev/null 2>&1; then
                  killall udhcpd 2>/dev/null || true
                  sleep 1
            fi
            killall dnsmasq 2>/dev/null || true
            udhcpd -fS "$UDHCPD_CONF" >/dev/null 2>&1 &
      fi
      ;;
  stop)
      # Stop hostapd and DHCP server, bring iface down
      if pidof hostapd >/dev/null 2>&1; then
            killall hostapd 2>/dev/null || true
      fi
      if pidof udhcpd >/dev/null 2>&1; then
            killall udhcpd 2>/dev/null || true
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
