#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

# exit immediately on error
set -e
# treat unset variables and parameters as an error when performing expansion
set -u

# *** Dependencies for kernel build and qemu-system-arm ***
# sudo apt update
# sudo apt install -y --no-install-recommends bc u-boot-tools kmod cpio flex bison libssl-dev psmisc
# sudo apt install -y qemu-system-arm
# *********************************************************

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # *** Kernel build steps ***
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j$(grep -c ^processor /proc/cpuinfo) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in ${OUTDIR}"
if [ -e ${OUTDIR}/Image ]; then
    rm -rf ${OUTDIR}/Image
fi
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
    echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# *** Create necessary base directories ***
mkdir rootfs && cd rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # *** Configure busybox ***
    make defconfig
else
    cd busybox
fi

# *** Make and install busybox ***
make -j$(grep -c ^processor /proc/cpuinfo) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
cd ${OUTDIR}/rootfs
echo "${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter""
echo "${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library""

# *** Add library dependencies to rootfs
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter" | awk '{print $4}' | tr -d [] | cut -c2- | xargs -I % cp $(${CROSS_COMPILE}gcc -print-sysroot)/% ${OUTDIR}/rootfs/lib
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library" | awk '{print $5}' | tr -d [] | xargs -I % cp $(${CROSS_COMPILE}gcc -print-sysroot)/lib64/% ${OUTDIR}/rootfs/lib64

# *** Make device nodes
cd ${OUTDIR}/rootfs
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

# *** Clean and build the writer utility
cd $FINDER_APP_DIR
make clean
make CROSS_COMPILE=aarch64-none-linux-gnu-

# *** Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp writer autorun-qemu.sh finder.sh finder-test.sh ${OUTDIR}/rootfs/home
#mkdir -p ${OUTDIR}/rootfs/home/conf
cp -r ../conf ${OUTDIR}/rootfs/home

# *** Chown the root directory
cd ${OUTDIR}/rootfs
sudo chown -R root:root *

# *** Create initramfs.cpio.gz
if [ -e ${OUTDIR}/initramfs.* ]; then
    rm -rf ${OUTDIR}/initramfs.*
fi
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f initramfs.cpio
