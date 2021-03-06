#!/bin/bash

CLANG_DIR=~/clang/bin/clang

if [ ! -f out/.config ]; then
	make O=out rmx2170_defconfig
else
	make O=out oldconfig
fi

export KBUILD_COMPILER_STRING=$($CLANG_DIR --version | head -n 1 | perl -pe 's/\(http.*?\)//gs' | sed -e 's/  */ /g' -e 's/[[:space:]]*$//')

make CC=$CLANG_DIR CLANG_TRIPLE=aarch64-linux-gnu- \
     CROSS_COMPILE=~/gcc-7.4.1/bin/aarch64-linux-gnu- \
     CROSS_COMPILE_ARM32=~/gcc/bin/arm-linux-gnueabihf- \
     TARGET_PRODUCT=atoll -j8 Image.gz-dtb

ATOLL="out/arch/arm64/boot/dts/qcom/atoll.dtb"
ATOLL_AB="out/arch/arm64/boot/dts/qcom/atoll-ab.dtb"
DTB_OUT="out/arch/arm64/boot/dts/qcom/206B1.dtb"
IMAGE="out/arch/arm64/boot/Image.gz"

if [[ -f "$ATOLL" && -f "$ATOLL_AB" ]]; then
     cat "$ATOLL" "$ATOLL_AB" > $DTB_OUT
fi


if [[ -f "$IMAGE" ]]; then
	rm AnyKernel3/Image.gz > /dev/null 2>&1
	rm AnyKernel3/*.zip > /dev/null 2>&1
	rm AnyKernel3/dtb > /dev/null 2>&1
	cp $IMAGE AnyKernel3/Image.gz
	cp $DTB_OUT AnyKernel3/dtb
	cd AnyKernel3
	zip -r release.zip .
fi
