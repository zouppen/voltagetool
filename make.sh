#!/bin/bash
#
# FIXME Implement a real compile script.

gcc -g -Wall $(pkg-config --libs libusb) $(pkg-config --cflags libusb) usbtools.c
