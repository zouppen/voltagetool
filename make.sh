#!/bin/bash
gcc -g -Wall $(pkg-config --libs libusb) $(pkg-config --cflags libusb) usbtools.c
