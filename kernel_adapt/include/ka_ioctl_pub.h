// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
 * Create: 2024-11-05
 */

 #ifndef _KA_IOCTL_PUB_H
 #define _KA_IOCTL_PUB_H
#include <asm/ioctl.h>

#define _KA_IOC_NRBITS       _IOC_NRBITS
#define _KA_IOC_TYPEBITS     _IOC_TYPEBITS
#define _KA_IOC_SIZEBITS     _IOC_SIZEBITS
#define _KA_IOC_DIRBITS      _IOC_DIRBITS

#define _KA_IOC_NRMASK       _IOC_NRMASK
#define _KA_IOC_TYPEMASK     _IOC_TYPEMASK
#define _KA_IOC_SIZEMASK     _IOC_SIZEMASK
#define _KA_IOC_DIRMASK      _IOC_DIRMASK

#define _KA_IOC_NRSHIFT      _IOC_NRSHIFT
#define _KA_IOC_TYPESHIFT    _IOC_TYPESHIFT
#define _KA_IOC_SIZESHIFT    _IOC_SIZESHIFT
#define _KA_IOC_DIRSHIFT     _IOC_DIRSHIFT

/*
 * Direction bits _KA_IOC_NONE could be 0, but OSF/1 gives it a bit.
 * And this turns out useful to catch old ioctl numbers in header
 * files for us.
 */
#define _KA_IOC_NONE         _IOC_NONE
#define _KA_IOC_READ         _IOC_READ
#define _KA_IOC_WRITE        _IOC_WRITE

#define _KA_IOC(dir,type,nr,size) _IOC(dir,type,nr,size)

/* used to create numbers */
#define _KA_IO(type,nr)           _IO(type,nr)
#define _KA_IOR(type,nr,size)     _IOR(type,nr,size)
#define _KA_IOW(type,nr,size)     _IOW(type,nr,size)
#define _KA_IOWR(type,nr,size)    _IOWR(type,nr,size)

/* used to decode them.. */
#define _KA_IOC_DIR(nr)           _IOC_DIR(nr)
#define _KA_IOC_TYPE(nr)          _IOC_TYPE(nr)
#define _KA_IOC_NR(nr)            _IOC_NR(nr)
#define _KA_IOC_SIZE(nr)          _IOC_SIZE(nr)

/* ...and for the drivers/sound files... */

#define KA_IOC_IN            IOC_IN
#define KA_IOC_OUT           IOC_OUT
#define KA_IOC_INOUT         IOC_INOUT
#define KA_IOCSIZE_MASK      IOCSIZE_MASK
#define KA_IOCSIZE_SHIFT     IOCSIZE_SHIFT

 #endif