#!/bin/sh
echo Flashing basic firmware
DEV=`ls /dev/tty.*| grep -v Bluetooth`
echo "Serial device $DEV"
esptool.py --port $DEV write_flash 0 ./*.bin
#screen $DEV
