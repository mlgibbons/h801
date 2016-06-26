#!/bin/sh
echo Flashing HUE firmware
esptool.py --port /dev/tty.wchusbserial144720 write_flash 0 ./hue.ino.d1_mini.bin

