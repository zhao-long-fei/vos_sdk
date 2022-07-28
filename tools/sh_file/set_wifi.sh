#!/bin/sh
# set -x

source /usr/bin/config

LOG_FILE=/data/log/wifi.log

GATEWAY=192.168.10.1
NETMASK=255.255.255.0
AP_IF_NAME=wlan1
IP_ADDR="192.168.10.100,192.168.10.255"
SSID_NAME="test_p2275"
wpa=2
wpa_passphrase="12345678"
wpa_key_mgmt=WPA-PSK

echo `date` "start ${AP_IF_NAME} AP mode" >> ${LOG_FILE}

hostapdpid=`ps | grep hostapd | grep wlan1 | cut -d " " -f 1`
if [ x"$hostapdpid" != x ]; then
    kill -9 hostapdpid > /dev/null 2>&1
fi
killall -9 dnsmasq

ipaddr flush ${AP_IF_NAME}
sleep 0.5

ifconfig ${AP_IF_NAME} down
sleep 1
ifconfig ${AP_IF_NAME} up

CHANNEL=6

sed -e "s/AP_IF_NAME/${AP_IF_NAME}/" -e "s/SSID_NAME/${SSID_NAME}/" -e "s/CHANNEL/${CHANNEL}/" /etc/wifi/hostapd.conf > /tmp/hostapd_${AP_IF_NAME}.conf
sed -i '$a\'"wpa=2" /tmp/hostapd_${AP_IF_NAME}.conf
sed -i '$a\'"wpa_passphrase=12345678" /tmp/hostapd_${AP_IF_NAME}.conf
sed -i '$a\'"wpa_key_mgmt=WPA-PSK" /tmp/hostapd_${AP_IF_NAME}.conf

hostapd -B /tmp/hostapd_${AP_IF_NAME}.conf
sleep 0.5

ifconfig ${AP_IF_NAME} ${GATEWAY} netmask ${NETMASK}

sed -e "s/AP_IF_NAME/${AP_IF_NAME}/" -e "s/192.168.5.100,192.168.5.255/${IP_ADDR}/" /etc/wifi/dnsmasq.conf > /tmp/dnsmasq_${AP_IF_NAME}.conf

echo -n "" > /tmp/dnsmasq_${AP_IF_NAME}.leases
dnsmasq -C /tmp/dnsmasq_${AP_IF_NAME}.conf -x /tmp/dnsmasq_${AP_IF_NAME}.pid

echo `date` "finish ${AP_IF_NAME} AP ssid=${SSID_NAME}" >> ${LOG_FILE}

IP_ROUTE=ip route show | awk '/default /{print $3}'
iptables -t nat -A PREROUTING -p udp --dport 53 -j DNAT --to-destination ${IP_ROUTE}:53

echo 1 > /proc/sys/net/ipv4/conf/wlan0/forwarding
echo 1 > /proc/sys/net/ipv4/conf/wlan1/forwarding

iptables -t nat -A POSTROUTING -s ${GATEWAY}/${NETMASK} -o wlan0 -j MASQUERADE
