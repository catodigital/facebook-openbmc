LINUX_VERSION_EXTENSION = "-yosemite"

COMPATIBLE_MACHINE = "yosemite"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://yosemite.cfg \
            file://defconfig \
           "

KERNEL_MODULE_AUTOLOAD += " \
"
