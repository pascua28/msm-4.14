#!/bin/bash

export ARCH=arm64

if [ ! -f out/.config ]; then
	make O=out rmx2170_defconfig
else
	make O=out oldconfig
fi

make O=out CC=~/clang/bin/clang CLANG_TRIPLE=aarch64-linux-gnu- \
     CROSS_COMPILE=~/gcc-7.4.1/bin/aarch64-linux-gnu- \
     TARGET_PRODUCT=atoll -j4 Image.gz
