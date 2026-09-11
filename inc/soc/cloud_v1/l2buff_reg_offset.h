// ******************************************************************************
// Copyright     :  Copyright (C) 2020, Hisilicon Technologies Co. Ltd.
// File name     :  l2buff_reg_offset.h
// Project line  :  Platform And Key Technologies Development
// Department    :  CAD Development Department
// Author        :  xxx
// Version       :  1.0
// Date          :  2020/5/25
// Description   :  The description of xxx project
// Others        :  Generated automatically by nManager V4.2
// History       :  xxx 2020/05/25 17:39:05 Create file
// ******************************************************************************

#ifndef __L2BUFF_REG_OFFSET_H__
#define __L2BUFF_REG_OFFSET_H__

/* L2BUFF Base address of Module's Register */
#define L2BUFF_NUM                            (16)
#define SOC_L2BUFF_BASE                       (0x0)
#define L2BUFF_REG_low_ADDR_OFFSET            (0x0000890A0000)
#define L2BUFF_REG_HIGH_ADDR_OFFSET           (0x00008B0A0000)
#define L2BUFF_REG_ADDR_OFFSET                (0x10000)

/******************************************************************************/
/*                      SOC L2BUFF Registers' Definitions                            */
/******************************************************************************/

#define SOC_L2BUFF_L2BUFF_INTM_REG                 (SOC_L2BUFF_BASE + 0x800)
#define SOC_L2BUFF_L2BUFF_RINT_REG                 (SOC_L2BUFF_BASE + 0x804)
#define SOC_L2BUFF_L2BUFF_INTS_REG                 (SOC_L2BUFF_BASE + 0x808)
#define SOC_L2BUFF_L2BUFF_INTC_REG                 (SOC_L2BUFF_BASE + 0x80C)
#define SOC_L2BUFF_L2BUFF_ERR_STATUSL_REG          (SOC_L2BUFF_BASE + 0x2010)

#endif // __L2BUFF_REG_OFFSET_H__
