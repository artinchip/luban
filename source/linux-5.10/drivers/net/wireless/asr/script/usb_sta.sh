#!/bin/sh
PWD_PATH=`pwd`
[ `pidof dhcpd` ] && killall -9 dhcpd
[ `pidof wpa_supplicant` ] &&killall -9 wpa_supplicant
[ `pidof hostapd` ] && killall -9 hostapd
dcpid=`ps -ef|grep dhclient |grep wlan0| awk '{print $2}'`
[ $dcpid ]&&kill -9 $dcpid
dmesg -C
asrmodule=`lsmod|grep asr`
[ "$asrmodule" ]&& rmmod asr5531
insmod ${PWD_PATH}/../out/asr5531.ko nss=2
sleep 5
ifconfig wlan0 up
wpa_supplicant -Dnl80211 -iwlan0 -c${PWD_PATH}/asr_wpa_supplicant.conf -B -d -t -f /tmp/asr_wpa.txt
dhclient -i wlan0
ifconfig wlan0
