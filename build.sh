#!/bin/bash

CLANG_DIR=~/clang/bin/clang

if [ ! -f out/.config ]; then
	make O=out rmx2170_defconfig
else
	make O=out oldconfig
fi

export KBUILD_COMPILER_STRING=$($CLANG_DIR --version | head -n 1 | perl -pe 's/\(http.*?\)//gs' | sed -e 's/  */ /g' -e 's/[[:space:]]*$//')

make CC="ccache $CLANG_DIR" CLANG_TRIPLE=aarch64-linux-gnu- \
     CROSS_COMPILE=~/gcc-7.4.1/bin/aarch64-linux-gnu- \
     TARGET_PRODUCT=atoll -j4 Image.gz dtbs

ATOLL="out/arch/arm64/boot/dts/qcom/atoll.dtb"
ATOLL_AB="out/arch/arm64/boot/dts/qcom/atoll-ab.dtb"
DTB_OUT="out/arch/arm64/boot/dts/qcom/206B1.dtb"

if [[ -f "$ATOLL" && -f "$ATOLL_AB" ]]; then
     cat "$ATOLL" "$ATOLL_AB" > $DTB_OUT
fi
