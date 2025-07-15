#!/bin/bash
#
# build and install team/vrf driver
#

set -e

source /etc/os-release

trim() {
    local var="$*"
    # remove leading whitespace characters
    var="${var#"${var%%[![:space:]]*}"}"
    # remove trailing whitespace characters
    var="${var%"${var##*[![:space:]]}"}"
    printf '%s' "$var"
}


build_and_install_kmodule()
{
    if sudo modprobe team 2>/dev/null && sudo modprobe vrf 2>/dev/null && sudo modprobe macsec 2>/dev/null; then
        echo "The module team, vrf and macsec exist."
        return 0
    fi

    [ -z "$WORKDIR" ] && WORKDIR=$(mktemp -d)
    cd $WORKDIR

    KERNEL_RELEASE=$(uname -r)
    KERNEL_MAINVERSION=$(echo $KERNEL_RELEASE | cut -d- -f1)
    EXTRAVERSION=$(echo $KERNEL_RELEASE | cut -d- -f2)
    LOCALVERSION=$(echo $KERNEL_RELEASE | cut -d- -f3)
    VERSION=$(echo $KERNEL_MAINVERSION | cut -d. -f1)
    PATCHLEVEL=$(echo $KERNEL_MAINVERSION | cut -d. -f2)
    SUBLEVEL=$(echo $KERNEL_MAINVERSION | cut -d. -f3)

    # Install the required debian packages to build the kernel modules
    apt-get update
    apt-get install -y build-essential linux-headers-${KERNEL_RELEASE} autoconf pkg-config fakeroot
    apt-get install -y flex bison libssl-dev libelf-dev dwarves
    apt-get install -y libnl-route-3-200 libnl-route-3-dev libnl-cli-3-200 libnl-cli-3-dev libnl-3-dev

    # Add the apt source mirrors and download the linux image source code
    cp /etc/apt/sources.list /etc/apt/sources.list.bk
    sed -i "s/^# deb-src/deb-src/g" /etc/apt/sources.list
    apt-get update
    KERNEL_PACKAGE_SOURCE=$(trim $(apt-cache show linux-image-unsigned-${KERNEL_RELEASE} | grep ^Source: | cut -d':' -f 2))
    KERNEL_PACKAGE_VERSION=$(trim $(apt-cache show linux-image-unsigned-${KERNEL_RELEASE} | grep ^Version: | cut -d':' -f 2))
    SOURCE_PACKAGE_VERSION=$(apt-cache showsrc "${KERNEL_PACKAGE_SOURCE}" | grep ^Version: | cut -d':' -f 2 | tr '\n' ' ')
    if ! echo "${SOURCE_PACKAGE_VERSION}" | grep "\b${KERNEL_PACKAGE_VERSION}\b"; then
        echo "ERROR: the running kernel version (${KERNEL_PACKAGE_VERSION}) doesn't match any of the available source " \
            "package versions (${SOURCE_PACKAGE_VERSION}) being downloaded. There's no guarantee any of the available " \
            "source packages can be loaded into the kernel or function correctly. Please update your kernel and reboot " \
            "your system so that it's running a matching kernel version." >&2
        return 1
    fi
    apt-get source "linux-image-unsigned-${KERNEL_RELEASE}=${KERNEL_PACKAGE_VERSION}"

    # Recover the original apt sources list
    cp /etc/apt/sources.list.bk /etc/apt/sources.list
    apt-get update

    # Build the Linux kernel module drivers/net/team and vrf
    cd ${KERNEL_PACKAGE_SOURCE}-*
    if [ -e debian/debian.env ]; then
        source debian/debian.env
        if [ -n "${DEBIAN}" -a -e ${DEBIAN}/reconstruct ]; then
            bash ${DEBIAN}/reconstruct
        fi
    fi
    make  allmodconfig
    mv .config .config.bk
    cp /boot/config-$(uname -r) .config
    grep NET_TEAM .config.bk >> .config
    make VERSION=$VERSION PATCHLEVEL=$PATCHLEVEL SUBLEVEL=$SUBLEVEL EXTRAVERSION=-${EXTRAVERSION} LOCALVERSION=-${LOCALVERSION} modules_prepare
    cp /usr/src/linux-headers-$(uname -r)/Module.symvers .
    make -j$(nproc) M=drivers/net/team
    mv drivers/net/Makefile drivers/net/Makefile.bak
    echo 'obj-$(CONFIG_NET_VRF) += vrf.o' > drivers/net/Makefile
    echo 'obj-$(CONFIG_MACSEC) += macsec.o' >> drivers/net/Makefile
    make -j$(nproc) M=drivers/net

    # Install the module
    SONIC_MODULES_DIR=/lib/modules/$(uname -r)/updates/sonic
    mkdir -p $SONIC_MODULES_DIR
    cp drivers/net/team/*.ko drivers/net/vrf.ko drivers/net/macsec.ko $SONIC_MODULES_DIR/
    depmod
    modinfo team vrf macsec
    modprobe team
    modprobe vrf
    modprobe macsec

    cd /tmp
    rm -rf $WORKDIR
}

build_and_install_kmodule
