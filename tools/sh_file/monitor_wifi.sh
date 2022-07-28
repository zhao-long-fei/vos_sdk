#!/bin/sh

wpa_state="wpa_state=COMPLETED"
while true ; do
	result=`wpa_cli -i wlan0 status | grep -r $wpa_state`
	if [ "$wpa_state"x = "$result"x ]
	then
		echo "wifi is connected"
	else
		echo "wifi is disconnected"
	fi
	sleep 1
done
exit 0
