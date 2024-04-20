include yosemite-image.inc

symlinkfunction() {
    ln -sf /mnt/data/etc/network/interfaces "${IMAGE_ROOTFS}/etc/network/interfaces"
}

ROOTFS_POSTPROCESS_COMMAND += "symlinkfunction;"