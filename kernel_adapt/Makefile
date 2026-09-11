ifdef BUILD_OPEN_SRC # for build.sh script build
    TARGET_SRC_DIR := $(DRIVER_KERNEL_OPEN_SOURCE_DIR)/src
    ccflags-y += -I$(DRIVER_KERNEL_OPEN_SOURCE_DIR)/inc/depends/sec
else
    ifeq ($(TOP_DIR),) # for dkms
        KERNEL_DIR :=$(srctree)
        # HIAI_DKMS_DIR = driver/kernel
        DRIVER_SRC_BASE_DIR := $(HIAI_DKMS_DIR)
        TARGET_SRC_DIR := $(HIAI_DKMS_DIR)
        ccflags-y += -I$(TARGET_SRC_DIR)/libc_sec/include/
    else # for ci
        TARGET_SRC_DIR := $(DRIVER_KERNEL_OPEN_SOURCE_DIR)/src
        ccflags-y += -I$(DRIVER_KERNEL_OPEN_SOURCE_DIR)/inc/depends/sec
    endif
endif

ccflags-y += -I$(TARGET_SRC_DIR)/kernel_adapt/
ccflags-y += -I$(TARGET_SRC_DIR)/kernel_adapt/include
ccflags-y += -I$(TARGET_SRC_DIR)/kernel_adapt/base
ccflags-y += -I$(TARGET_SRC_DIR)/kernel_adapt/dfx
ccflags-y += -I$(TARGET_SRC_DIR)/kernel_adapt/driver
ccflags-y += -I$(TARGET_SRC_DIR)/kernel_adapt/fs
ccflags-y += -I$(TARGET_SRC_DIR)/kernel_adapt/memory
ccflags-y += -I$(TARGET_SRC_DIR)/kernel_adapt/net
ccflags-y += -I$(TARGET_SRC_DIR)/kernel_adapt/pci
ccflags-y += -I$(TARGET_SRC_DIR)/kernel_adapt/system
ccflags-y += -I$(TARGET_SRC_DIR)/kernel_adapt/task

ccflags-y += -Wno-error=type-limits # for kernel 5.15
ccflags-y += -Wno-implicit-function-declaration

# check if gcc support -Wno-error=cast-function-type
CHECK_FLAG := $(shell echo "int main() { return 0; }" | ${CC} -x c -Wno-error=cast-function-type -o /dev/null - 2>/dev/null && echo supported || echo unsupported)

# add cflags -Wno-error=cast-function-type
ifeq ($(CHECK_FLAG),supported)
    ccflags-y += -Wno-error=cast-function-type
endif

ifeq ($(TARGET_BUILD_TYPE),debug)
    ccflags-y += -DCFG_BUILD_DEBUG
endif
ifneq ($(NOT_SUPPORT_SP), y)
    ccflags-y += -fstack-protector-all
endif

ifneq ($(filter $(ARCH), arm64 x86), )
$(info ARCH is: $(ARCH))
else
$(error NOT supported ARCH: $(ARCH))
endif

ccflags-y += -Wall -Werror

$(info $(ccflags-y))

ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
ccflags-y += -Wno-unused-parameter -O2 -Wno-missing-field-initializers -Wno-sign-compare -Wno-format-nonliteral

# check if gcc support -Wno-implicit-fallthrough
# -Wno-implicit-fallthrough was first added in gcc-7.
CHECK_FLAG := $(shell echo "int main() { return 0; }" | ${CC} -x c -Wno-implicit-fallthrough -o /dev/null - 2>/dev/null && echo supported || echo unsupported)
# add cflags -Wno-implicit-fallthrough
ifeq ($(CHECK_FLAG),supported)
    ccflags-y += -Wno-implicit-fallthrough
endif

obj-m += ascend_kernel_open_adapt.o

ifeq ($(PRODUCT_SIDE), device)
    ascend_kernel_open_adapt-objs := ka_module_init.o  \
                                     task/ka_task.o \
                                     fs/ka_fs.o \
                                     system/ka_system.o \
                                     base/ka_base.o \
                                     driver/ka_driver.o \
                                     memory/ka_mem.o \
                                     pci/ka_pci.o \
                                     net/ka_net.o \
                                     dfx/ka_dfx.o
else
    ascend_kernel_open_adapt-objs := ka_module_init.o  \
                                     task/ka_task.o \
                                     fs/ka_fs.o \
                                     system/ka_system.o \
                                     base/ka_base.o \
                                     driver/ka_driver.o \
                                     memory/ka_mem.o \
                                     pci/ka_pci.o \
                                     net/ka_net.o \
                                     dfx/ka_dfx.o
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function
