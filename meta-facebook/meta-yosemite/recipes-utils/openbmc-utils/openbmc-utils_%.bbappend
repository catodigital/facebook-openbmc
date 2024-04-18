# Copyright 2020-present Facebook. All Rights Reserved.
#
# This program file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program in a file named COPYING; if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301 USA

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

PACKAGECONFIG += "disable-watchdog"
PACKAGECONFIG += "boot-info"

LOCAL_URI += "\
    file://init-interfaces.sh \
    "

OPENBMC_UTILS_FILES += " \
    init-interfaces.sh \
    "

DEPENDS:append = " update-rc.d-native"

do_install_board() {
    # for backward compatible, create /usr/local/fbpackages/utils/ast-functions
    # olddir="/usr/local/fbpackages/utils"
    # install -d ${D}${olddir}
    # ln -s "/usr/local/bin/openbmc-utils.sh" "${D}${olddir}/ast-functions"

    # init
    install -d ${D}${sysconfdir}/init.d
    install -d ${D}${sysconfdir}/rcS.d
    # the script to check for /etc/network/interfaces symlink
    install -m 0755 ${S}/init-interfaces.sh ${D}${sysconfdir}/init.d/init-interfaces.sh
    update-rc.d -r ${D} init-interfaces.sh start 06 S .
}

do_install:append() {
  do_install_board
}

FILES:${PN} += "${sysconfdir}"
