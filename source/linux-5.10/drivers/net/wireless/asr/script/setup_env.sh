#!/bin/sh

# install make gcc vim git wpa_supplicant hostapd dhcpd dhclient
apt install linux-headers-$(uname -r) apt-utils build-essential vim net-tools iperf usbutils wireless-tools wpasupplicant hostapd git isc-dhcp-server isc-dhcp-client psmisc

#copy 5531 fw to linux firmware path
[ ! -d /lib/firmware ]&& rm /lib/firmware;mkdir -p /lib/firmware
cp ../fw_bin/fmacfw_asr5531_usb.bin /lib/firmware


#copy dhcp config to /etc/dhcp path
cp ./asr_dhcpd.conf /etc/dhcp/asr_dhcpd.conf
