include fbtp-image.inc

symlinkfunction() {
    ln -sf /mnt/data/etc/network/interfaces "${IMAGE_ROOTFS}/etc/network/interfaces"
}

removeDhcp6() {
    rm -f "${IMAGE_ROOTFS}/etc/network/if-up.d/dhcpv6_up"
    rm -f "${IMAGE_ROOTFS}/etc/network/if-up.d/dhcpv6_down"
    rm -f "${IMAGE_ROOTFS}/etc/init.d/setup-dhc6.sh"
    rm -rf "${IMAGE_ROOTFS}/etc/sv/dhc6"
    rm -f "${IMAGE_ROOTFS}/etc/rc5.d/S03setup-dhc6.sh"
}

ROOTFS_POSTPROCESS_COMMAND += "symlinkfunction;removeDhcp6;"