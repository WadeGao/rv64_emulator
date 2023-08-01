#!/bin/bash

# working dirs
ROOT_DIR="$(realpath $(dirname $0)/..)"
BUILD_KERNEL_DIR="$ROOT_DIR/build/kernel"

# source code version tags
LINUX_TAG="5.19.1"
BUSYBOX_TAG="1.35.0"
OPENSBI_TAG="1.3"

# source code filename
LINUX_SRC="linux-$LINUX_TAG.tar.gz"
BUSYBOX_SRC="busybox-$BUSYBOX_TAG.tar.bz2"
OPENSBI_SRC="opensbi-$OPENSBI_TAG.tar.gz"

# create and goto build dir
cd $ROOT_DIR
mkdir -p $BUILD_KERNEL_DIR
cd $BUILD_KERNEL_DIR

# download source code
if [ ! -f "$LINUX_SRC" ]; then
    wget "https://mirrors.aliyun.com/linux-kernel/v5.x/$LINUX_SRC"
fi

if [ ! -f "$BUSYBOX_SRC" ]; then
    wget "https://busybox.net/downloads/$BUSYBOX_SRC"
fi

if [ ! -f "$OPENSBI_SRC" ]; then
    wget -O "$OPENSBI_SRC" "https://github.com/riscv-software-src/opensbi/archive/refs/tags/v$OPENSBI_TAG.tar.gz"
fi

# step 1: build device tree
dtc "$ROOT_DIR/dts/rv64_emulator.dts" -o "rv64_emulator.dtb"

# step 2: build busybox and create rootfs
## 2.1: untar and compile
tar xf $BUSYBOX_SRC
mkdir -p rootfs
cd "busybox-$BUSYBOX_TAG/"
ARCH=riscv CROSS_COMPILE=riscv64-unknown-linux-gnu- LDFLAGS=--static make defconfig
ARCH=riscv CROSS_COMPILE=riscv64-unknown-linux-gnu- LDFLAGS=--static make -C . install CONFIG_PREFIX=../rootfs

## 2.2: create essential dirs
cd ../rootfs/
mkdir proc sys dev etc etc/init.d

## 2.3: create init rc file
echo '#!/bin/sh' >etc/init.d/rcS
echo 'mount -t proc none /proc' >>etc/init.d/rcS
echo 'mount -t sysfs none /sys' >>etc/init.d/rcS
echo '/sbin/mdev -s' >>etc/init.d/rcS
chmod +x etc/init.d/rcS

## 2.4: make essential device node
cd dev/
sudo mknod console c 5 1
sudo mknod null c 1 3
sudo mknod random c 1 8
sudo mknod urandom c 1 9
sudo mknod zero c 1 5

## 2.5： pack rootfs into cpio format
cd ..
find . -print0 | cpio --null -ov --format=newc >../rootfs.cpio
cd ..

# step 3: compile linux kernel
tar zxf "$LINUX_SRC"
cd "linux-$LINUX_TAG"
make ARCH=riscv CROSS_COMPILE=riscv64-unknown-linux-gnu- defconfig
cp "$ROOT_DIR/config/kernel_config" .config
make ARCH=riscv CROSS_COMPILE=riscv64-unknown-linux-gnu- -j $(nproc)
cd ..

# step 4: build opensbi
tar -zxf $OPENSBI_SRC
cd "opensbi-$OPENSBI_TAG"
make CROSS_COMPILE=riscv64-unknown-elf- PLATFORM=generic PLATFORM_RISCV_ISA=rv64ima FW_FDT_PATH=../rv64_emulator.dtb FW_PAYLOAD_PATH=../linux-$LINUX_TAG/arch/riscv/boot/Image
cp build/platform/generic/firmware/fw_payload.bin ..
cd ..
