/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
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
* Create: 2023-12-19
*/

#include "ka_kernel_def_pub.h"
#include "ka_barrier_pub.h"
#include "uda_pub_def.h"
#include "spod_info.h"

struct spod_info g_spod_info[UDA_MAX_PHY_DEV_NUM];
bool g_spod_info_been_set[UDA_MAX_PHY_DEV_NUM];

int dbl_get_spod_info(unsigned int udevid, struct spod_info *s)
{
    if (udevid >= UDA_MAX_PHY_DEV_NUM || s == NULL) {
        return -EINVAL;
    }
    if (!g_spod_info_been_set[udevid]) {
        return -EAGAIN;
    }
    *s = g_spod_info[udevid];
    return 0;
}
KA_EXPORT_SYMBOL(dbl_get_spod_info);

int dbl_set_spod_info(unsigned int udevid, struct spod_info *s)
{
    if (udevid >= UDA_MAX_PHY_DEV_NUM || s == NULL) {
        return -EINVAL;
    }
    g_spod_info[udevid] = *s;
    ka_mb();
    g_spod_info_been_set[udevid] = true;
    return 0;
}
KA_EXPORT_SYMBOL(dbl_set_spod_info);

/* SDID total 32 bits, low to high: */
#define UDEVID_BIT_LEN 16
#define DIE_ID_BIT_LEN 2
#define CHIP_ID_BIT_LEN 4
#define SERVER_ID_BIT_LEN 10

static inline void parse_bit_shift(unsigned int *rcv, unsigned int *src, unsigned int bit_width)
{
    *rcv = (*src) & GENMASK(bit_width - 1, 0);
    (*src) >>= bit_width;
}

int dbl_parse_sdid (unsigned int sdid, struct sdid_parse_info *s)
{
    if (s == NULL) {
        return -EINVAL;
    }

    parse_bit_shift(&s->udevid, &sdid, UDEVID_BIT_LEN);
    parse_bit_shift(&s->die_id, &sdid, DIE_ID_BIT_LEN);
    parse_bit_shift(&s->chip_id, &sdid, CHIP_ID_BIT_LEN);
    parse_bit_shift(&s->server_id, &sdid, SERVER_ID_BIT_LEN);
    return 0;
}
KA_EXPORT_SYMBOL(dbl_parse_sdid);

int dbl_make_sdid (struct sdid_parse_info *s, unsigned int *sdid)
{
    unsigned int tmp_sdid;

    if (s == NULL || sdid == NULL) {
        return -EINVAL;
    }

    tmp_sdid = s->server_id;

    tmp_sdid <<= CHIP_ID_BIT_LEN;
    tmp_sdid |= s->chip_id;

    tmp_sdid <<= DIE_ID_BIT_LEN;
    tmp_sdid |= s->die_id;

    tmp_sdid <<= UDEVID_BIT_LEN;
    tmp_sdid |= s->udevid;

    *sdid = tmp_sdid;
    return 0;
}
KA_EXPORT_SYMBOL(dbl_make_sdid);

