#!/bin/sh

rm -rf build/
make && \
	make TARGET=Win32 &&
	make TARGET=Win64

