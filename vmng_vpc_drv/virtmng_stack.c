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

#include "ka_memory_pub.h"

#include "virtmng_stack.h"
#include "virtmng_public_def.h"

void vmng_stack_init(struct vmng_stack *stack, u32 len)
{
    u32 i;

    for (i = 0; i < len; i++) {
        stack->data[i] = i;
    }
    stack->top = (int)len;
}

bool vmng_stack_empty(const struct vmng_stack *stack)
{
    if (stack->top == 0) {
        return 1;
    } else {
        return 0;
    }
}

bool vmng_stack_full(const struct vmng_stack *stack, u32 len)
{
    if (stack->top == len) {
        return 1;
    } else {
        return 0;
    }
}

int vmng_stack_push(struct vmng_stack *stack, u32 len, u32 data)
{
    if (stack == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    if (vmng_stack_full(stack, len) != 0) {
        vmng_err("Call vmng_stack_full error.\n");
        return -EINVAL;
    }
    stack->data[stack->top] = data;
    stack->top++;

    return 0;
}

int vmng_stack_pop(struct vmng_stack *stack, u32 len, u32 *data)
{
    if (stack == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    if (vmng_stack_empty(stack) != 0) {
        vmng_err("Call vmng_stack_empty error.\n");
        return -EINVAL;
    }

    *data = stack->data[stack->top - 1];
    stack->top--;
    return 0;
}