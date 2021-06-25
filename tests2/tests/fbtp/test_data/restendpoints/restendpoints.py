# ",/usr/bin/env python,
# ,
# Copyright 2018-present Facebook. All Rights Reserved.,
# ,
# This program file is free software; you can redistribute it and/or modify it,
# under the terms of the GNU General Public License as published by the,
# Free Software Foundation; version 2 of the License.,
# ,
# This program is distributed in the hope that it will be useful, but WITHOUT,
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or,
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License,
# for more details.,
# ,
# You should have received a copy of the GNU General Public License,
# along with this program in a file named COPYING; if not, write to the,
# Free Software Foundation, Inc.,,
# 51 Franklin Street, Fifth Floor,,
# Boston, MA 02110-1301 USA,
# ,

REST_END_POINTS = {
    "/api": ["Description", "version"],
    "/api/bmc": [
        "uptime",
        "TPM FW version",
        "MTD Parts",
        "SPI1 Vendor",
        "kernel version",
        "u-boot version",
        "Memory Usage",
        "load-1",
        "load-5",
        "Secondary Boot Triggered",
        "Uptime",
        "load-15",
        "open-fds",
        "MAC Addr",
        "Description",
        "OpenBMC Version",
        "Reset Reason",
        "vboot",
        "CPU Usage",
        "SPI0 Vendor",
        "TPM TCG version",
        "memory",
        "At-Scale-Debug Running",
    ],
    "/api/fans": [
        "Fan 0 Speed",
        "Fan 1 Speed",
        "Fan Mode",
        "FSCD Driver",
        "Sensor Fail",
        "Fan Fail",
    ],
    "/api/logs": ["Logs"],
    "/api/sensors": [
        "MB_C4_3_NVME_CTEMP",
        "MB_VR_CPU0_VCCIO_POWER",
        "MB_CPU0_DIMM_GRPA_TEMP",
        "MB_C3_AVA_FTEMP",
        "MB_VR_CPU1_VDDQ_GRPD_POWER",
        "MB_C2_3_NVME_CTEMP",
        "MB_VR_CPU1_VDDQ_GRPC_CURR",
        "MB_C3_3_NVME_CTEMP",
        "MB_VR_CPU0_VCCIN_VOLT",
        "MB_VR_CPU0_VCCIN_CURR",
        "MB_VR_CPU1_VCCIO_TEMP",
        "MB_C3_AVA_RTEMP",
        "MB_VR_CPU1_VDDQ_GRPC_POWER",
        "MB_FAN0_TACH",
        "MB_INLET_REMOTE_TEMP",
        "MB_C2_2_NVME_CTEMP",
        "MB_VR_CPU1_VDDQ_GRPC_VOLT",
        "MB_CONN_P12V_INA230_PWR",
        "MB_VR_CPU0_VCCIN_POWER",
        "MB_VR_CPU1_VCCIO_VOLT",
        "MB_P1V05",
        "MB_VR_CPU1_VSA_POWER",
        "MB_VR_CPU1_VSA_CURR",
        "MB_VR_CPU0_VCCIO_CURR",
        "MB_C2_AVA_FTEMP",
        "MB_C4_0_NVME_CTEMP",
        "MB_CONN_P12V_INA230_CURR",
        "MB_C3_0_NVME_CTEMP",
        "MB_VR_PCH_PVNN_CURR",
        "MB_P3V_BAT",
        "MB_VR_CPU0_VSA_POWER",
        "MB_CPU1_TJMAX",
        "MB_VR_PCH_P1V05_POWER",
        "MB_VR_CPU0_VDDQ_GRPB_VOLT",
        "MB_VR_CPU0_VSA_VOLT",
        "MB_PVNN_PCH_STBY",
        "MB_CPU1_TEMP",
        "MB_VR_CPU0_VDDQ_GRPA_CURR",
        "MB_C4_P12V_INA230_PWR",
        "MEZZ_SENSOR_TEMP",
        "MB_VR_PCH_P1V05_VOLT",
        "MB_CPU1_THERM_MARGIN",
        "MB_HSC_OUT_CURR",
        "MB_VR_PCH_PVNN_POWER",
        "MB_INLET_TEMP",
        "MB_C4_AVA_FTEMP",
        "MB_VR_CPU0_VDDQ_GRPB_POWER",
        "MB_C2_AVA_RTEMP",
        "MB_OUTLET_REMOTE_TEMP",
        "MB_VR_CPU0_VCCIO_VOLT",
        "MB_C3_NVME_CTEMP",
        "MB_CPU0_DIMM_GRPB_TEMP",
        "MB_C4_P12V_INA230_VOL",
        "MB_C2_P12V_INA230_VOL",
        "MB_VR_CPU1_VCCIN_CURR",
        "MB_P5V",
        "MB_C3_2_NVME_CTEMP",
        "MB_C4_NVME_CTEMP",
        "MB_HSC_IN_POWER",
        "MB_VR_CPU0_VCCIO_TEMP",
        "MB_FAN1_TACH",
        "MB_VR_CPU0_VSA_TEMP",
        "MB_HSC_TEMP",
        "MB_VR_CPU0_VDDQ_GRPA_TEMP",
        "MB_C3_1_NVME_CTEMP",
        "MB_VR_CPU1_VSA_TEMP",
        "MB_CPU1_DIMM_GRPC_TEMP",
        "MB_HOST_BOOT_TEMP",
        "MB_VR_CPU1_VDDQ_GRPD_CURR",
        "MB_HSC_IN_VOLT",
        "MB_C4_AVA_RTEMP",
        "MB_VR_PCH_PVNN_TEMP",
        "MB_VR_CPU0_VDDQ_GRPA_VOLT",
        "MB_P3V3",
        "MB_CPU1_DIMM_GRPD_TEMP",
        "MB_C3_P12V_INA230_VOL",
        "MB_VR_CPU1_VDDQ_GRPC_TEMP",
        "MB_C2_0_NVME_CTEMP",
        "MB_VR_CPU0_VSA_CURR",
        "MB_VR_CPU0_VDDQ_GRPA_POWER",
        "MB_VR_CPU0_VDDQ_GRPB_TEMP",
        "MB_P5V_STBY",
        "MB_CPU0_TEMP",
        "MB_VR_CPU1_VCCIN_POWER",
        "MB_C4_2_NVME_CTEMP",
        "MB_C3_P12V_INA230_CURR",
        "MB_PCH_TEMP",
        "MB_VR_CPU1_VCCIN_VOLT",
        "MB_OUTLET_TEMP",
        "MB_C4_P12V_INA230_CURR",
        "MB_VR_CPU1_VCCIN_TEMP",
        "MB_CPU0_THERM_MARGIN",
        "MB_P3V3_STBY",
        "MB_VR_CPU1_VCCIO_POWER",
        "MB_VR_CPU1_VDDQ_GRPD_TEMP",
        "MB_CPU1_PKG_POWER",
        "MB_C3_P12V_INA230_PWR",
        "MB_P12V",
        "MB_CPU0_PKG_POWER",
        "MB_VR_PCH_P1V05_TEMP",
        "SYSTEM_AIRFLOW",
        "MB_VR_CPU1_VCCIO_CURR",
        "MB_C2_P12V_INA230_CURR",
        "MB_VR_CPU0_VDDQ_GRPB_CURR",
        "MB_C2_NVME_CTEMP",
        "MB_VR_CPU1_VDDQ_GRPD_VOLT",
        "MB_CONN_P12V_INA230_VOL",
        "MB_VR_CPU0_VCCIN_TEMP",
        "MB_C2_1_NVME_CTEMP",
        "MB_C2_P12V_INA230_PWR",
        "MB_C4_1_NVME_CTEMP",
        "MB_VR_PCH_PVNN_VOLT",
        "MB_CPU0_TJMAX",
        "MB_VR_PCH_P1V05_CURR",
        "MB_VR_CPU1_VSA_VOLT",
    ],
    "/api/sled": ["Description"],
    "/api/sled/mb": ["Power status"],
    "/api/sled/mb/bmc": [
        "uptime",
        "TPM FW version",
        "MTD Parts",
        "SPI1 Vendor",
        "kernel version",
        "u-boot version",
        "Memory Usage",
        "load-1",
        "load-5",
        "Secondary Boot Triggered",
        "Uptime",
        "load-15",
        "open-fds",
        "MAC Addr",
        "Description",
        "OpenBMC Version",
        "Reset Reason",
        "vboot",
        "CPU Usage",
        "SPI0 Vendor",
        "TPM TCG version",
        "memory",
        "At-Scale-Debug Running",
    ],
    "/api/sled/mb/fruid": [
        "Chassis Part Number",
        "Board Serial",
        "Product Manufacturer",
        "Chassis Custom Data 1",
        "Chassis Custom Data 2",
        "Product Serial",
        "Chassis Serial Number",
        "Product Custom Data 2",
        "Product Custom Data 1",
        "Chassis Type",
        "Product Part Number",
        "Board Mfg",
        "Product Name",
        "Product Version",
        "Board Mfg Date",
        "Board Product",
        "Product Asset Tag",
        "Board Custom Data 1",
        "Board Part Number",
        "Board Custom Data 3",
        "Board Custom Data 2",
    ],
    "/api/sled/mb/logs": ["Logs"],
    "/api/sled/mb/sensors": [
        "MB_VR_CPU1_VCCIN_POWER",
        "MB_CPU0_DIMM_GRPA_TEMP",
        "MB_VR_CPU1_VSA_VOLT",
        "MB_P5V",
        "MB_VR_CPU1_VDDQ_GRPD_POWER",
        "MB_P5V_STBY",
        "MB_VR_CPU1_VCCIO_POWER",
        "MB_VR_CPU0_VCCIO_VOLT",
        "MB_HSC_IN_POWER",
        "MB_VR_CPU0_VCCIN_TEMP",
        "MB_FAN0_TACH",
        "MB_VR_PCH_P1V05_POWER",
        "MB_FAN1_TACH",
        "MB_VR_CPU0_VSA_TEMP",
        "MB_VR_CPU0_VCCIN_VOLT",
        "MB_VR_CPU0_VCCIN_CURR",
        "MB_VR_CPU1_VDDQ_GRPD_TEMP",
        "MB_VR_CPU1_VCCIO_TEMP",
        "MB_VR_CPU1_VSA_TEMP",
        "MB_CONN_P12V_INA230_VOL",
        "MB_CPU0_TEMP",
        "MB_HOST_BOOT_TEMP",
        "MB_VR_CPU0_VDDQ_GRPB_CURR",
        "MB_HSC_IN_VOLT",
        "MB_VR_CPU1_VDDQ_GRPC_TEMP",
        "MB_VR_CPU0_VDDQ_GRPB_VOLT",
        "MB_VR_PCH_PVNN_TEMP",
        "MB_VR_CPU1_VDDQ_GRPC_VOLT",
        "MB_P3V3",
        "MB_PVNN_PCH_STBY",
        "MB_VR_CPU0_VCCIN_POWER",
        "MB_VR_CPU0_VCCIO_POWER",
        "MB_INLET_REMOTE_TEMP",
        "MB_CPU1_DIMM_GRPD_TEMP",
        "MB_VR_CPU0_VDDQ_GRPA_POWER",
        "MB_VR_CPU1_VSA_CURR",
        "MB_P3V_BAT",
        "MB_VR_CPU1_VSA_POWER",
        "MB_CPU1_TEMP",
        "MB_VR_CPU1_VCCIN_VOLT",
        "MB_VR_CPU0_VDDQ_GRPA_VOLT",
        "MB_CPU1_TJMAX",
        "MB_CONN_P12V_INA230_CURR",
        "MB_PCH_TEMP",
        "MB_CPU1_DIMM_GRPC_TEMP",
        "MB_CONN_P12V_INA230_PWR",
        "MB_VR_CPU0_VCCIO_CURR",
        "MB_VR_CPU1_VDDQ_GRPC_CURR",
        "MB_VR_CPU1_VCCIN_TEMP",
        "MB_VR_PCH_PVNN_CURR",
        "MB_P1V05",
        "MB_P3V3_STBY",
        "MB_VR_CPU0_VSA_POWER",
        "MB_VR_CPU0_VDDQ_GRPB_TEMP",
        "MB_CPU0_TJMAX",
        "MB_CPU0_DIMM_GRPB_TEMP",
        "MB_P12V",
        "MB_CPU0_PKG_POWER",
        "MB_VR_PCH_P1V05_TEMP",
        "MB_VR_CPU1_VCCIN_CURR",
        "MB_CPU1_THERM_MARGIN",
        "MB_HSC_OUT_CURR",
        "MB_VR_PCH_P1V05_VOLT",
        "MB_HSC_TEMP",
        "MB_VR_CPU0_VDDQ_GRPA_TEMP",
        "MB_INLET_TEMP",
        "MB_CPU0_THERM_MARGIN",
        "MB_VR_CPU0_VDDQ_GRPA_CURR",
        "MB_VR_PCH_PVNN_POWER",
        "MB_VR_CPU1_VDDQ_GRPD_VOLT",
        "MB_VR_CPU0_VDDQ_GRPB_POWER",
        "MB_VR_CPU0_VSA_VOLT",
        "MB_VR_CPU0_VCCIO_TEMP",
        "MB_OUTLET_REMOTE_TEMP",
        "MB_VR_CPU0_VSA_CURR",
        "MB_VR_CPU1_VCCIO_VOLT",
        "MB_VR_PCH_PVNN_VOLT",
        "MB_CPU1_PKG_POWER",
        "MB_VR_CPU1_VDDQ_GRPD_CURR",
        "MB_VR_PCH_P1V05_CURR",
        "MB_VR_CPU1_VDDQ_GRPC_POWER",
        "MB_OUTLET_TEMP",
        "MB_VR_CPU1_VCCIO_CURR",
    ],
    "/api/sled/mezz": ["Description"],
    "/api/sled/mezz/logs": ["Logs"],
    "/api/sled/mezz/sensors": ["MEZZ_SENSOR_TEMP"],
}
