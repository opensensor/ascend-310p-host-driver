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

#ifndef VIRTMNG_STACK_H
#define VIRTMNG_STACK_H

struct vmng_stack {
    int top;
    u32 data[];
};

void vmng_stack_init(struct vmng_stack *stack, u32 len);
bool vmng_stack_empty(const struct vmng_stack *stack);
bool vmng_stack_full(const struct vmng_stack *stack, u32 len);
int vmng_stack_push(struct vmng_stack *stack, u32 len, u32 data);
int vmng_stack_pop(struct vmng_stack *stack, u32 len, u32 *data);
#endif