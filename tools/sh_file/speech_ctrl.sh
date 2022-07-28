#!/bin/sh
source /usr/bin/config
VOS_CLEANER_LOG=/data/log/vos_cleaner.log
USB_LOG=/tmp/usb.log
USB_FLAG=/tmp/usb.flag

if [ -f /speech/vos_cleaner ]; then
    killall -9 vos_cleaner
    control_gpio.sh 44 1
 sleep 3
    lsusb | awk '{print $6}' > $USB_LOG
 cat $USB_LOG | while read line
 do
 if [ "X$line" == "X03fe:0301" ] || [ "X$line" == "X5448:152e" ]; then
     if [ -f "${DEBUG_VERSION_FILE}" ]; then
      backup_log_file ${VOS_CLEANER_LOG} 512000 yes
      cd /speech
      ./vos_cleaner >  ${VOS_CLEANER_LOG} &
      log "open speech success!!!!"
  else
   cd /speech
   ./vos_cleaner &
   log "open speech success!!!!"
  fi
  touch $USB_FLAG
  break;
 fi
 done
 if [ ! -f "${USB_FLAG}" ]; then
  control_gpio.sh 44 0
 fi
 rm -rf $USB_LOG
 rm -rf $USB_FLAG
else
 control_gpio.sh 44 0
    log "open speech failed!!!!"
fi