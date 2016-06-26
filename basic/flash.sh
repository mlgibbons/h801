#!/bin/sh
echo Flashing basic firmware
esptool.py --port /dev/tty.wchusbserial144720 write_flash 0 ./basic.ino.d1_mini.bin

