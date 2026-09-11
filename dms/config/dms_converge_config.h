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

#ifndef __DMS_CONVERGE_CONFIG_H__
#define __DMS_CONVERGE_CONFIG_H__

/* true:on, false:off */
#ifdef CFG_SOC_PLATFORM_MDC_V51
#  define DMS_EVENT_CONVERGE_SWITCH false
#else
#  define DMS_EVENT_CONVERGE_SWITCH false
#endif

EVENT_CONVERGE_DECLARATION_BEGIN()
/* convergent diagrams of SOC ERROR declaration */
EVENT_CONVERGE_ITEM_DECLARATION(0x80818000U, 0x80B78000U,  /* SLLC    ERROR */
                                            0x80B78008U,  /* SLLC    MBECC */
                                            0x80B38000U,  /* CS      ERROR */
                                            0x80DF8000U,  /* DDR     ERROR */
                                            0x80F38000U,  /* DHA     ERROR */
                                            0x80F38008U,  /* DHA     MBECC */
                                            0x80F38001U,  /* DHA     ERROR_NF */
                                            0x80C78008U,  /* TS      MBECC */
                                            0x80AD8008U,  /* GIC     MBECC */
                                            0x80E18005U,  /* HBM     PARITY */
                                            0x80E18000U,  /* HBM     ERROR */
                                            0x80E1800AU,  /* HBM     TIMEOUT_ERR */
                                            0x80F18000U,  /* HHA     ERROR */
                                            0x80F18008U,  /* HHA     MBECC */
                                            0x80BD8000U,  /* NIC     ERROR */
                                            0x80BD8008U,  /* NIC     MBECC */
                                            0x80F78000U,  /* HWTS/Stars-TS ERROR */
                                            0x80E38000U,  /* LPM     ERROR */
                                            0x80CD8000U,  /* L2BUF   ERROR */
                                            0x80CD8008U,  /* L2BUF   MBECC */
                                            0x80A38000U,  /* L3D     ERROR */
                                            0x80A38008U,  /* L3D     MBECC */
                                            0x80A58000U,  /* L3T     ERROR */
                                            0x80A58008U,  /* L3T     MBECC */
                                            0x80A78000U,  /* MESH    ERROR */
                                            0x80B18000U,  /* MN      ERROR */
                                            0x80B58000U,  /* SIOE    ERROR */
                                            0x80A18008U,  /* CPUcore MBECC */
                                            0x80B98000U,  /* pcie local ERROR */
                                            0x80B98008U,  /* pcie local MBECC */
                                            0x80FB8000U,  /* TSCPU(A55/R52) ERROR */
                                            0x80FB8005U); /* TSCPU(A55/R52) PARITY */
/* convergent diagrams of SOC ERROR_NF declaration */
EVENT_CONVERGE_ITEM_DECLARATION(0x80818001U, 0x80BB8001U); /* ROCE    ERROR_NF */
/* convergent diagrams of SOC IN_CFG_ERR declaration */
EVENT_CONVERGE_ITEM_DECLARATION(0x80818003U, 0x80B78003U,  /* SLLC    IN_CFG_ERR */
                                            0x80F38003U,  /* DHA     IN_CFG_ERR */
                                            0x80D18003U,  /* DVPP    IN_CFG_ERR */
                                            0x80F18003U,  /* HHA     IN_CFG_ERR */
                                            0x80BD8003U,  /* NIC     IN_CFG_ERR */
                                            0x80CD8003U,  /* L2BUF   IN_CFG_ERR */
                                            0x80A38003U,  /* L3D     IN_CFG_ERR */
                                            0x80A58003U,  /* L3T     IN_CFG_ERR */
                                            0x80B18003U,  /* MN      IN_CFG_ERR */
                                            0x80BB8003U); /* ROCE    IN_CFG_ERR */
EVENT_CONVERGE_ITEM_INIT(0x80818000U);
EVENT_CONVERGE_ITEM_INIT(0x80818001U);
EVENT_CONVERGE_ITEM_INIT(0x80818003U);
EVENT_CONVERGE_DECLARATION_END();
#endif
