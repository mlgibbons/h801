#!/bin/sh
esptool.py --port /dev/tty.wchusbserial144720 write_flash 0 ./h801.ino.d1_mini.bin

