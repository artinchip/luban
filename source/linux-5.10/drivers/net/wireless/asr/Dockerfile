FROM ubuntu:16.04
COPY ./sources.list /etc/apt/sources.list
RUN apt update
RUN apt-get install -y linux-headers-$(uname -r) apt-utils build-essential vim net-tools iperf usbutils \
wireless-tools wpasupplicant hostapd  git  isc-dhcp-server isc-dhcp-client module-init-tools psmisc
WORKDIR /root/asrwifidrv
CMD [ "sh", "-c", "echo $HOME" ]
