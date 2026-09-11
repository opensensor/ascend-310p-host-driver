# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
# SPDX-License-Identifier: GPL-2.0
ifeq ($(ENABLE_OPEN_SRC), y)
    MODULE_NAME := drv_vascend
    obj-m += ${MODULE_NAME}.o
    ${MODULE_NAME}-objs := dvt.o vdavinci.o cfg_space.o mmio.o interface.o kvmdt.o hw_dvt.o debugfs.o dma_pool.o \
        domain_manage.o dma_pool_map.o dvt_sysfs.o dvt_sriov.o dma_pool_pt.o vdavinci_rdwr.o vfio_ops.o \
        priv_ops.o
else
    ifeq ($(TOP_DIR),)

    DVT_DIR=$(shell pwd)
    UNAME=$(shell uname -r)
    KERNEL_DIR=/lib/modules/$(UNAME)/build
    KERNEL_SOURCE_DIR=/lib/modules/$(UNAME)/source

    ifneq ($(NOT_SUPPORT_SP), y)
        ccflags-y += -fstack-protector-all
    endif

    ifeq ($(DAVINCI_TEST),y)
        ccflags-y += -DDAVINCI_TEST
    endif

    ifeq ($(DAVINCI_DEBUG),y)
        ccflags-y += -DDAVINCI_DEBUG
    endif

    ccflags-y += -Wall -Werror -Wno-unused-variable

    obj-m := drv_vascend.o
    drv_vascend-objs := dvt.o vdavinci.o cfg_space.o mmio.o interface.o kvmdt.o hw_dvt.o debugfs.o dma_pool.o \
            domain_manage.o dma_pool_map.o dvt_sysfs.o dvt_sriov.o dma_pool_pt.o vdavinci_rdwr.o vfio_ops.o \
            priv_ops.o

    default:
		$(MAKE) -C $(KERNEL_DIR) M=$(DVT_DIR) modules

    .PHONY:clean
    clean:
		@$(RM) *.o *.ko *.mod.* Module.* .*.cmd *.order
		$(MAKE) -C $(KERNEL_DIR) M=`pwd` $@

    else

    ifneq ($(NOT_SUPPORT_SP), y)
        ccflags-y += -fstack-protector-all
    endif

    ifeq ($(DAVINCI_TEST),y)
        ccflags-y += -DDAVINCI_TEST
    endif

    ccflags-y += -Wall -Werror -Wno-unused-variable

    obj-m := drv_vascend.o
    drv_vascend-objs := dvt.o vdavinci.o cfg_space.o mmio.o interface.o kvmdt.o hw_dvt.o debugfs.o dma_pool.o \
            domain_manage.o dma_pool_map.o dvt_sysfs.o dvt_sriov.o dma_pool_pt.o vdavinci_rdwr.o vfio_ops.o \
            priv_ops.o

    endif
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function
