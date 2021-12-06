#!/bin/bash

set -e

cd "$(dirname $0)"

export WORKSPACE="$PWD"
export PACKAGES_PATH=$PWD/edk2:$PWD/edk2-platforms:$PWD/edk2-non-osi:$PWD/edk2-rockchip
export GCC5_AARCH64_PREFIX=aarch64-linux-gnu-

BL31=rkbin/bin/rk35/rk3568_bl31_v1.24.elf


fetch_deps() {
	git submodule update --init --recursive
}

build_uefitools() {
	[ -r .uefitools_done ] && return
	echo " => Building UEFI tools"
	make -C edk2/BaseTools -j$(getconf _NPROCESSORS_ONLN) && touch .uefitools_done
}

build_uefi() {
	memsize=$1
	echo " => Building UEFI (PcdSystemMemorySize=${memsize})"
	build -n $(getconf _NPROCESSORS_ONLN) -b DEBUG -a AARCH64 -t GCC5 \
	    --pcd gArmTokenSpaceGuid.PcdSystemMemorySize=${memsize} \
	    -p Platform/Pine64/Quartz64/Quartz64.dsc
}

build_idblock() {
	echo " => Building idblock.bin"
	FLASHFILES="FlashHead.bin FlashData.bin FlashBoot.bin"
	rm -f idblock.bin rk3566_ddr_*.bin rk356x_usbplug*.bin UsbHead.bin ${FLASHFILES}
	(cd rkbin && ./tools/boot_merger RKBOOT/RK3566MINIALL.ini)
	./rkbin/tools/boot_merger unpack --loader rkbin/rk356x_spl_loader_*.bin --output .
	cat ${FLASHFILES} > idblock.bin
	rm -f rkbin/rk356x_spl_loader_*.bin
	rm -f rk3566_ddr_*.bin rk356x_usbplug*.bin UsbHead.bin ${FLASHFILES}
}

build_fit() {
	tag=$1
	echo " => Building FIT (${tag})"
	./scripts/extractbl31.py ${BL31}
	./rkbin/tools/mkimage -f uefi.its -E QUARTZ64_EFI_${tag}.itb
	rm -f bl31_0x*.bin
}

fetch_deps

test -r ${BL31} || (echo "${BL31} not found"; false)
. edk2/edksetup.sh

build_uefitools
build_uefi 0xF0000000
build_fit 4GB
build_uefi 0x200000000
build_fit 8GB
build_idblock

echo
ls -l `pwd`/idblock.bin `pwd`/QUARTZ64_EFI_*.itb
echo
