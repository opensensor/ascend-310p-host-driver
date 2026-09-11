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
 * Create: 2023-12-21
 */
#ifndef DBL_SPOD_INFO_H
#define DBL_SPOD_INFO_H

struct spod_info {
    unsigned int sdid;
    unsigned int scale_type;
    unsigned int super_pod_id;
    unsigned int server_id;
    unsigned int reserve[8];
};

struct sdid_parse_info {
    unsigned int server_id;
    unsigned int chip_id;
    unsigned int die_id;
    unsigned int udevid;
    unsigned int reserve[8];
};

int dbl_get_spod_info(unsigned int udevid, struct spod_info *s);
int dbl_set_spod_info(unsigned int udevid, struct spod_info *s);
int dbl_parse_sdid (unsigned int sdid, struct sdid_parse_info *s);
int dbl_make_sdid (struct sdid_parse_info *s, unsigned int *sdid);


#endif /* DBL_SPOD_INFO_H */

