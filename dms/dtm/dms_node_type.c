/*
* Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* Description:
* Author: huawei
* Create: 2019-10-15
*/

#include "ka_kernel_def_pub.h"

#include "dms_node_type.h"
#include "dms_define.h"

struct dms_node_type_string {
    unsigned short node_type;
    const char *string;
};

static struct dms_node_type_string g_dms_node_type[] = {
    {DMS_DEV_TYPE_SOC, "SoC"},
    {DMS_DEV_TYPE_PCIE_LINK, "PCIE_LINK"},
    {DMS_DEV_TYPE_HCCS_LINK, "HCCS_LINK"},
    {DMS_DEV_TYPE_CPU_CORE, "CPU core"},
    {DMS_DEV_TYPE_L3D, "L3D"},
    {DMS_DEV_TYPE_L3T, "L3T"},
    {DMS_DEV_TYPE_MESH, "MESH"},
    {DMS_DEV_TYPE_RING, "RING"},
    {DMS_DEV_TYPE_RBRG, "RBRG"},
    {DMS_DEV_TYPE_GIC, "GIC"},
    {DMS_DEV_TYPE_ITS, "ITS"},
    {DMS_DEV_TYPE_MN, "MN"},
    {DMS_DEV_TYPE_CS, "CS"},
    {DMS_DEV_TYPE_SIOE, "SIOE"},
    {DMS_DEV_TYPE_SLLC, "SLLC"},
    {DMS_DEV_TYPE_PCIE, "PCIE"},
    {DMS_DEV_TYPE_ROCE, "RoCE"},
    {DMS_DEV_TYPE_NIC, "NIC"},
    {DMS_DEV_TYPE_UFS, "UFS"},
    {DMS_DEV_TYPE_SPI, "SPI"},
    {DMS_DEV_TYPE_XGMAC, "xGMac"},
    {DMS_DEV_TYPE_SAFETYISLAND, "SafetyIsland"},
    {DMS_DEV_TYPE_TS, "TS"},
    {DMS_DEV_TYPE_AIC, "AIC"},
    {DMS_DEV_TYPE_AIV, "AIV"},
    {DMS_DEV_TYPE_L2BUF, "L2BUFF"},
    {DMS_DEV_TYPE_SDMA, "SDMA"},
    {DMS_DEV_TYPE_DVPP, "DVPP"},
    {DMS_DEV_TYPE_JPEGD, "JPEGD"},
    {DMS_DEV_TYPE_JPEGE, "JPEGE"},
    {DMS_DEV_TYPE_PNGD, "PNGD"},
    {DMS_DEV_TYPE_VDEC, "VDEC"},
    {DMS_DEV_TYPE_VENC, "VENC"},
    {DMS_DEV_TYPE_VPC, "VPC"},
    {DMS_DEV_TYPE_DDR, "DDR"},
    {DMS_DEV_TYPE_HBM, "HBM/3DRAM"},
    {DMS_DEV_TYPE_LPM, "LPM"},
    {DMS_DEV_TYPE_HSM, "HSM"},
    {DMS_DEV_TYPE_SAFE_SEC, "TEEDrv"},
    {DMS_DEV_TYPE_GPU, "GPU"},
    {DMS_DEV_TYPE_DISPLAY, "DISPLAY"},
    {DMS_DEV_TYPE_AUDIO, "AUDIO"},
    {DMS_DEV_TYPE_CAMERA, "Camera"},
    {DMS_DEV_TYPE_DDRA, "DDRA"},
    {DMS_DEV_TYPE_HBMA, "HBMA"},
    {DMS_DEV_TYPE_ISP, "ISP"},
    {DMS_DEV_TYPE_HWTS_S_TS, "HWTS/Stars-TS"},
    {DMS_DEV_TYPE_BASE_SERVCIE, "BaseService"},
    {DMS_DEV_TYPE_TSCPU, "TSCPU(A55/R52)"},
    {DMS_DEV_TYPE_MCU, "MCU"},
    {DMS_DEV_TYPE_VR, "VR"},
    {HAL_DMS_DEV_TYPE_TSD_DAEMON, "tsdaemon"},
    {HAL_DMS_DEV_TYPE_DMP_DAEMON, "dmp_daemon"},
    {HAL_DMS_DEV_TYPE_ADDA, "adda"},
    {HAL_DMS_DEV_TYPE_SLOGD, "slogd"},
    {HAL_DMS_DEV_TYPE_LOG_DAEMON, "log-daemon"},
    {HAL_DMS_DEV_TYPE_SKLOGD, "sklogd"},
    {HAL_DMS_DEV_TYPE_HDCD, "hdcd"},
    {HAL_DMS_DEV_TYPE_AICPU_SCH, "aicpu_scheduler"},
    {HAL_DMS_DEV_TYPE_QUEUE_SCH, "queue_scheduler"},
    {HAL_DMS_DEV_TYPE_AICPU_CUST_SCH, "aicpu_cust_scheduler"},
    {HAL_DMS_DEV_TYPE_HCCP, "hccp"},
    {HAL_DMS_DEV_TYPE_TIMER_SERVER, "timer_server"},
    {HAL_DMS_DEV_TYPE_DATA_MASTER, "data_master"},
    {HAL_DMS_DEV_TYPE_CFG_MGR, "cfg_mgr"},
    {HAL_DMS_DEV_TYPE_DATA_GW, "data_gw"},
    {HAL_DMS_DEV_TYPE_RESMGR, "resource_mgr"},
    {HAL_DMS_DEV_TYPE_DRV_KERNEL, "driver_kernel"},
    {HAL_DMS_DEV_TYPE_BASE_SERVCIE, "BaseService"},
    {HAL_DMS_DEV_TYPE_PROC_MGR, "process-manager"},
    {HAL_DMS_DEV_TYPE_PROC_LAUNCHER, "proc_launcher"},
    {HAL_DMS_DEV_TYPE_IAMMGR, "iammgr"},
    {HAL_DMS_DEV_TYPE_OS_LINUX, "OS_Linux"},
    {DMS_DEV_TYPE_PORT, "PORT"},
    {DMS_DEV_TYPE_CAN, "CAN"},
    {DMS_DEV_TYPE_SENSORHUB, "SENSORHUB"},
    {DMS_DEV_TYPE_PMU, "PMU"},
    {DMS_DEV_TYPE_AO_SUBSYS, "AO subsys"},
    {DMS_DEV_TYPE_HILINK, "HILINK"},
    {DMS_DEV_TYPE_CPU_CLUSTER, "CPU cluster"},
    {DMS_DEV_TYPE_IO_SUBSYS, "IO subsys"},
    {DMS_DEV_TYPE_HAC_SUBSYS, "HAC subsys"},
    {DMS_DEV_TYPE_PERI_SUBSYS, "PERI subsys"},
    {DMS_DEV_TYPE_SAFETYISLAND, "SILS subsys"},
    {DMS_DEV_TYPE_HWTSCPU, "RISC-V/HWTSCPU"},
    {DMS_DEV_TYPE_TSFW, "TSFW"},
    {DMS_DEV_TYPE_DSA, "DSA"},
    {DMS_DEV_TYPE_ISP_SUB_SYS, "ISP_subsys"},
    {DMS_DEV_TYPE_MEDIA_SUB_SYS, "Media_subsys"},
    /* subsys */
    {DMS_DEV_TYPE_DVPPSUB_DISP, "DVPP subsys disp"},
    {DMS_DEV_TYPE_DVPPSUB_AA, "DVPP subsys aa"},
    {DMS_DEV_TYPE_DVPPSUB_SCHE, "DVPP subsys sche"},
    {DMS_DEV_TYPE_DVPPSUB_SMMU, "DVPP subsys smmu"},
    {DMS_DEV_TYPE_AOSUB_DISP, "AO subsys disp"},
    {DMS_DEV_TYPE_AOSUB_AA, "AO subsys aa"},
    {DMS_DEV_TYPE_AOSUB_SCHE, "AO subsys sche"},
    {DMS_DEV_TYPE_AOSUB_SMMU, "AO subsys smmu"},
    {DMS_DEV_TYPE_ISPSUB_DISP, "ISP subsys disp"},
    {DMS_DEV_TYPE_ISPSUB_AA, "ISP subsys AA"},
    {DMS_DEV_TYPE_ISPSUB_SCHE, "ISP subsys sche"},
    {DMS_DEV_TYPE_ISPSUB_SMMU, "ISP subsys smmu"},
    {DMS_DEV_TYPE_PERI_DISP, "PERI subsys disp"},
    {DMS_DEV_TYPE_PERI_AA, "PERI subsys AA"},
    {DMS_DEV_TYPE_PERI_SCHE, "PERI subsys sche"},
    {DMS_DEV_TYPE_PERI_SMMU, "PERI subsys smmu"},
    {DMS_DEV_TYPE_HAC_DISP, "HAC subsys disp"},
    {DMS_DEV_TYPE_HAC_AA, "HAC subsys AA"},
    {DMS_DEV_TYPE_HAC_SCHE, "HAC subsys sche"},
    {DMS_DEV_TYPE_HAC_SMMU, "HAC subsys smmu"},
    {DMS_DEV_TYPE_IO_DISP, "IO subsys disp"},
    {DMS_DEV_TYPE_IO_AA, "IO subsys AA"},
    {DMS_DEV_TYPE_IO_SCHE, "IO subsys sche"},
    {DMS_DEV_TYPE_IO_SMMU, "IO subsys smmu"},
    {DMS_DEV_TYPE_SILS_DISP, "SILS subsys disp"},
    {DMS_DEV_TYPE_SILS_AA, "SILS subsys AA"},
    {DMS_DEV_TYPE_SILS_SCHE, "SILS subsys sche"},
    {DMS_DEV_TYPE_SILS_SMMU, "SILS subsys smmu"},
    {DMS_DEV_TYPE_TS_DISP, "TS subsys disp"},
    {DMS_DEV_TYPE_TS_AA, "TS subsys aa"},
    {DMS_DEV_TYPE_TS_SCHE, "TS subsys sche"},
    {DMS_DEV_TYPE_TS_SMMU, "TS subsys smmu"},
    {DMS_DEV_TYPE_AIC_DISP, "AIC subsys disp"},
    {DMS_DEV_TYPE_AIC_AA, "aic subsys aa"},
    {DMS_DEV_TYPE_AIC_SMMU, "aic subsys smmu"},
    {DMS_DEV_TYPE_AIV_DISP, "aiv subsys disp"},
    {DMS_DEV_TYPE_AIV_AA, "aiv subsys aa"},
    {DMS_DEV_TYPE_AIV_SMMU, "aiv subsys smmu"},
    {DMS_DEV_TYPE_POWER_GLITCH_SENSOR, "PGsensor"},
    {DMS_DEV_TYPE_DSA_DISP, "DSA subsys disp"},
    {DMS_DEV_TYPE_TEE_OS, "TEE OS"},
    {DMS_DEV_TYPE_NIC_SMMU, "NIC subsys smmu"},
    {DMS_DEV_TYPE_NIC_DISP, "NIC subsys disp"},
    {DMS_DEV_TYPE_PCIE_SMMU, "PCIE subsys smmu"},
    {DMS_DEV_TYPE_PCIE_DISP, "PCIE subsys disp"},
    {DMS_DEV_TYPE_HCCS, "HCCS"},
    {DMS_DEV_TYPE_EMMC, "EMMC"},
    {DMS_DEV_TYPE_SDMAM, "SDMAM"},
    {DMS_DEV_TYPE_ROH, "ROH"},
    {DMS_DEV_TYPE_HCCS_PORT, "HCCS_PORT"},
    {DMS_DEV_TYPE_ROH_PORT, "ROH_PORT"},
    {DMS_DEV_TYPE_ZIP, "ZIP"},
    {DMS_DEV_TYPE_SEC, "SEC"},
    {DMS_DEV_TYPE_DSA_SMMU, "DSA subsys smmu"},
};

int dms_get_node_type_str(unsigned short node_type,
    char *node_str, unsigned int str_len)
{
    int cnt = sizeof(g_dms_node_type) / sizeof(struct dms_node_type_string);
    int i;

    if (node_str == NULL) {
        dms_err("Invalid parameter, node_str=NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    for (i = 0; i < cnt; i++) {
        if (g_dms_node_type[i].node_type != node_type) {
            continue;
        }
        if (strlen(g_dms_node_type[i].string) >= str_len) {
            dms_err("Invalid parameter. (node_str=%lu; str_len=%u)\n",
                    strlen(g_dms_node_type[i].string), str_len);
            return DRV_ERROR_PARA_ERROR;
        }
        if (strcpy_s(node_str, str_len, g_dms_node_type[i].string) != 0) {
            dms_err("Call strcpy_s failed. (node_str=\"%s\")\n",
                    g_dms_node_type[i].string);
            return DRV_ERROR_PARA_ERROR;
        }
        return DRV_ERROR_NONE;
    }

    dms_err("Cannot find this node_type in table. (node_type=%u)\n", node_type);
    return DRV_ERROR_INVALID_HANDLE;
}
KA_EXPORT_SYMBOL(dms_get_node_type_str);


