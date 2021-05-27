#!/bin/bash

if [ ! -f out/.config ]; then
	make O=out rmx2170_defconfig
else
	make O=out oldconfig
fi

make CROSS_COMPILE=aarch64-linux-gnu- \
     TARGET_PRODUCT=atoll -j8 Image.gz dtbs

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
