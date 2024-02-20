#!/bin/sh
PWD_PATH=`pwd`
[ `pidof dhcpd` ] &&killall -9 dhcpd
[ `pidof wpa_supplicant` ] && killall -9 wpa_supplicant
[ `pidof hostapd` ] && killall -9 hostapd
dcpid=`ps -ef|grep dhclient |grep wlan0| awk '{print $2}'`
[ $dcpid ]&& kill -9 $dcpid
dmesg -C
asrmodule=`lsmod|grep asr`
[ "$asrmodule" ]&& rmmod asr5531
insmod ${PWD_PATH}/../out/asr5531.ko nss=2
sleep 5
ifconfig wlan0 up
ifconfig wlan0 192.168.0.1/24
hostapd -B ${PWD_PATH}/asr_hostapd.conf >/dev/null
CONFIG_FILE=/etc/dhcp/asr_dhcpd.conf
[ -e /var/lib/dhcp/dhcpd.leases ] || touch /var/lib/dhcp/dhcpd.leases
chown root:dhcpd /var/lib/dhcp /var/lib/dhcp/dhcpd.leases
chmod 775 /var/lib/dhcp ; chmod 664 /var/lib/dhcp/dhcpd.leases
dhcpd -user dhcpd -group dhcpd -f -4 -cf $CONFIG_FILE wlan0 >/dev/null 2>&1 &
ifconfig wlan0
