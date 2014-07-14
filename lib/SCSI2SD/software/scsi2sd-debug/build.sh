#!/bin/sh

make && \
	make TARGET=Win32 &&
	make TARGET=Win64

