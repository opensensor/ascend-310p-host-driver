/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
* Create: 2023-10-10
*/

#ifndef FEATURE_LOADER_H__
#define FEATURE_LOADER_H__

#ifndef AOS_LLVM_BUILD

#include <linux/module.h>
#include <linux/version.h>
#include <linux/seq_file.h>
#include "ka_kernel_def_pub.h"

#include "securec.h"

/*
   usage : surport feature global/device/task scope auto init and uninit
           feature can define init or uninit function use this macros:
               global: DECLAER_FEATURE_AUTO_INIT / DECLAER_FEATURE_AUTO_UNINIT
               device: DECLAER_FEATURE_AUTO_INIT_DEV / DECLAER_FEATURE_AUTO_UNINIT_DEV
               task: DECLAER_FEATURE_AUTO_INIT_TASK / DECLAER_FEATURE_AUTO_UNINIT_TASK
           global scope init/uninit call module_feature_auto_init/module_feature_auto_uninit in module_init/module_exit
           device scope init/uninit call module_feature_auto_init_dev/module_feature_auto_uninit_dev
                  when device online or offline
           task scope init/uninit call module_feature_auto_init_task/module_feature_auto_uninit_task
                  when proc add or remove
       When there are multiple auto-initialization features in the module and you want to contrl the sequence,
       you can use stage param.
           stage : scope 0-9, stage 0 has the highest priority.
                  init: functions are invoked in the sequence of stages.
                  uninit: functions are invoked in the oppside sequence of stages.
*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
struct kernel_symbol {
#ifdef CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
    int value_offset;
    int name_offset;
    int namespace_offset;
#else
    unsigned long value;
    const char *name;
    const char *namespace;
#endif
};
#endif

#define PREFIX_MAX_LEN 64

enum {
    FEATURE_SCOPE_GLOBAL = 0,
    FEATURE_SCOPE_DEV,
    FEATURE_SCOPE_TASK,
    FEATURE_SCOPE_MAX
};

enum {
    FEATURE_LOADER_STAGE_0 = 0,
    FEATURE_LOADER_STAGE_1,
    FEATURE_LOADER_STAGE_2,
    FEATURE_LOADER_STAGE_3,
    FEATURE_LOADER_STAGE_4,
    FEATURE_LOADER_STAGE_5,
    FEATURE_LOADER_STAGE_6,
    FEATURE_LOADER_STAGE_7,
    FEATURE_LOADER_STAGE_8,
    FEATURE_LOADER_STAGE_9,
    FEATURE_LOADER_STAGE_MAX,
};

typedef int (*_fearture_init_func)(void);
typedef void (*_fearture_uninit_func)(void);

typedef int (*_fearture_init_dev_func)(u32 udevid);
typedef void (*_fearture_uninit_dev_func)(u32 udevid);

typedef int (*_fearture_init_task_func)(int pid);
typedef void (*_fearture_uninit_task_func)(int pid);


/* the function decleared will be auto call in module insert
   init_fn : shoule like "int _fearture_init_func(void)" */
#define DECLAER_FEATURE_AUTO_INIT(init_fn, stage) \
    int stage##_init_##init_fn(void); \
    int stage##_init_##init_fn(void) \
    { \
        return init_fn(); \
    } \
    EXPORT_SYMBOL(stage##_init_##init_fn)

/* the function decleared will be auto call in module remove
   init_fn : shoule like "void _fearture_uninit_func(void)" */
#define DECLAER_FEATURE_AUTO_UNINIT(uninit_fn, stage) \
    void stage##_uninit_##uninit_fn(void); \
    void stage##_uninit_##uninit_fn(void) \
    { \
        uninit_fn(); \
    } \
    EXPORT_SYMBOL(stage##_uninit_##uninit_fn)

/* the function decleared will be auto call when dev online
   init_fn : shoule like "int _fearture_init_dev_func(u32 udevid)" */
#define DECLAER_FEATURE_AUTO_INIT_DEV(init_fn, stage) \
    int stage##_dev_init_##init_fn(u32 udevid); \
    int stage##_dev_init_##init_fn(u32 udevid) \
    { \
        return init_fn(udevid); \
    } \
    EXPORT_SYMBOL(stage##_dev_init_##init_fn)

/* the function decleared will be auto call when dev offline
   init_fn : shoule like "void _fearture_uninit_dev_func(u32 udevid)" */
#define DECLAER_FEATURE_AUTO_UNINIT_DEV(uninit_fn, stage) \
    void stage##_dev_uninit_##uninit_fn(u32 udevid); \
    void stage##_dev_uninit_##uninit_fn(u32 udevid) \
    { \
        uninit_fn(udevid); \
    } \
    EXPORT_SYMBOL(stage##_dev_uninit_##uninit_fn)

/* the function decleared will be auto call when task init
   init_fn : shoule like "int _fearture_init_task_func(int pid)" */
#define DECLAER_FEATURE_AUTO_INIT_TASK(init_fn, stage) \
    int stage##_task_init_##init_fn(int pid); \
    int stage##_task_init_##init_fn(int pid) \
    { \
        return init_fn(pid); \
    } \
    EXPORT_SYMBOL(stage##_task_init_##init_fn)

/* the function decleared will be auto call when task uninit
   init_fn : shoule like "void _fearture_uninit_task_func(int pid)" */
#define DECLAER_FEATURE_AUTO_UNINIT_TASK(uninit_fn, stage) \
    void stage##_task_uninit_##uninit_fn(int pid); \
    void stage##_task_uninit_##uninit_fn(int pid) \
    { \
        uninit_fn(pid); \
    } \
    EXPORT_SYMBOL(stage##_task_uninit_##uninit_fn)

static inline void *get_symbol_fun(const struct kernel_symbol *sym)
{
#ifdef CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
    return (void *)(uintptr_t)offset_to_ptr(&sym->value_offset);
#else
    return (void *)(uintptr_t)sym->value;
#endif
}

static inline const char *get_symbol_name(const struct kernel_symbol *sym)
{
#ifdef CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
    return offset_to_ptr(&sym->name_offset);
#else
    return sym->name;
#endif
}

static inline void *get_symbol_prefix_func(const struct kernel_symbol *sym, char *prefix)
{
    const char *sym_name = get_symbol_name(sym);
    if (strstr(sym_name, prefix) != sym_name) {
        return NULL;
    }

    return get_symbol_fun(sym);
}

static inline int module_feature_init_call(int feature_scope, void *fn, u32 param)
{
    if (feature_scope == FEATURE_SCOPE_GLOBAL) {
        return ((_fearture_init_func)fn)();
    } else if (feature_scope == FEATURE_SCOPE_DEV) {
        return ((_fearture_init_dev_func)fn)(param);
    } else if (feature_scope == FEATURE_SCOPE_TASK) {
        return ((_fearture_init_task_func)fn)((int)param);
    }
    return 0;
}

static inline int module_feature_try_to_init(int feature_scope, u32 param,
    const struct kernel_symbol *sym, char *prefix)
{
    void *fn = get_symbol_prefix_func(sym, prefix);
    return (fn != NULL) ? module_feature_init_call(feature_scope, fn, param) : 0;
}

static inline void module_feature_uninit_call(int feature_scope, void *fn, u32 param)
{
    if (feature_scope == FEATURE_SCOPE_GLOBAL) {
        ((_fearture_uninit_func)fn)();
    } else if (feature_scope == FEATURE_SCOPE_DEV) {
        ((_fearture_uninit_dev_func)fn)(param);
    } else if (feature_scope == FEATURE_SCOPE_TASK) {
        ((_fearture_uninit_task_func)fn)((int)param);
    }
}

static inline void module_feature_try_to_uninit(int feature_scope, u32 param,
    const struct kernel_symbol *sym, char *prefix)
{
    void *fn = get_symbol_prefix_func(sym, prefix);
    if (fn != NULL) {
        module_feature_uninit_call(feature_scope, fn, param);
    }
}

static inline void module_feature_uninit_prefix_range(int feature_scope, u32 param,
    struct module *module, char *prefix, u32 num_syms)
{
    u32 i;

    for (i = 0; i < num_syms; i++) {
        module_feature_try_to_uninit(feature_scope, param, &module->syms[i], prefix);
    }
}

static inline int module_feature_init_prefix(int feature_scope, u32 param, char *init_prefix, char *uninit_prefix)
{
    struct module *module = KA_THIS_MODULE;
    u32 i;

    for (i = 0; i < module->num_syms; i++) {
        int ret = module_feature_try_to_init(feature_scope, param, &module->syms[i], init_prefix);
        if (ret != 0) {
            module_feature_uninit_prefix_range(feature_scope, param, module, uninit_prefix, i);
            return ret;
        }
    }

    return 0;
}

static inline void module_feature_uninit_prefix(int feature_scope, u32 param, char *prefix)
{
    struct module *module = KA_THIS_MODULE;
    module_feature_uninit_prefix_range(feature_scope, param, module, prefix, module->num_syms);
}

static inline void module_feature_format_init_prefix(int feature_scope, char *prefix, int len, int stage)
{
    if (feature_scope == FEATURE_SCOPE_GLOBAL) {
        (void)sprintf_s(prefix, PREFIX_MAX_LEN, "FEATURE_LOADER_STAGE_%d_init_", stage);
    } else if (feature_scope == FEATURE_SCOPE_DEV) {
        (void)sprintf_s(prefix, PREFIX_MAX_LEN, "FEATURE_LOADER_STAGE_%d_dev_init_", stage);
    } else if (feature_scope == FEATURE_SCOPE_TASK) {
        (void)sprintf_s(prefix, PREFIX_MAX_LEN, "FEATURE_LOADER_STAGE_%d_task_init_", stage);
    }
}

static inline void module_feature_format_uninit_prefix(int feature_scope, char *prefix, int len, int stage)
{
    if (feature_scope == FEATURE_SCOPE_GLOBAL) {
        (void)sprintf_s(prefix, PREFIX_MAX_LEN, "FEATURE_LOADER_STAGE_%d_uninit_", stage);
    } else if (feature_scope == FEATURE_SCOPE_DEV) {
        (void)sprintf_s(prefix, PREFIX_MAX_LEN, "FEATURE_LOADER_STAGE_%d_dev_uninit_", stage);
    } else if (feature_scope == FEATURE_SCOPE_TASK) {
        (void)sprintf_s(prefix, PREFIX_MAX_LEN, "FEATURE_LOADER_STAGE_%d_task_uninit_", stage);
    }
}

static inline int module_feature_init_stage(int feature_scope, u32 param, int stage)
{
    char init_prefix[PREFIX_MAX_LEN];
    char uninit_prefix[PREFIX_MAX_LEN];

    module_feature_format_init_prefix(feature_scope, init_prefix, PREFIX_MAX_LEN, stage);
    module_feature_format_uninit_prefix(feature_scope, uninit_prefix, PREFIX_MAX_LEN, stage);
    return module_feature_init_prefix(feature_scope, param, init_prefix, uninit_prefix);
}

static inline void module_feature_uninit_stage(int feature_scope, u32 param, int stage)
{
    char uninit_prefix[PREFIX_MAX_LEN];

    module_feature_format_uninit_prefix(feature_scope, uninit_prefix, PREFIX_MAX_LEN, stage);
    module_feature_uninit_prefix(feature_scope, param, uninit_prefix);
}

static inline void module_feature_uninit_stage_range(int feature_scope, u32 param, int min_stage, int max_stage)
{
    int stage;

    for (stage = max_stage; stage >= min_stage; stage--) {
        module_feature_uninit_stage(feature_scope, param, stage);
    }
}

static inline int module_feature_unified_init(int feature_scope, u32 param)
{
    int stage;

    for (stage = FEATURE_LOADER_STAGE_0; stage <= FEATURE_LOADER_STAGE_9; stage++) {
        int ret = module_feature_init_stage(feature_scope, param, stage);
        if (ret != 0) {
            module_feature_uninit_stage_range(feature_scope, param, FEATURE_LOADER_STAGE_0, stage - 1);
            return ret;
        }
    }

    return 0;
}

static inline void module_feature_unified_uninit(int feature_scope, u32 param)
{
    module_feature_uninit_stage_range(feature_scope, param, FEATURE_LOADER_STAGE_0, FEATURE_LOADER_STAGE_9);
}

static inline int module_feature_auto_init(void)
{
    return module_feature_unified_init(FEATURE_SCOPE_GLOBAL, 0); /* fill meaningless parameters 0 */
}

static inline void module_feature_auto_uninit(void)
{
    module_feature_unified_uninit(FEATURE_SCOPE_GLOBAL, 0);  /* fill meaningless parameters 0 */
}

static inline int module_feature_auto_init_dev(u32 udevid)
{
    return module_feature_unified_init(FEATURE_SCOPE_DEV, udevid);
}

static inline void module_feature_auto_uninit_dev(u32 udevid)
{
    module_feature_unified_uninit(FEATURE_SCOPE_DEV, udevid);
}

static inline int module_feature_auto_init_task(int pid)
{
    return module_feature_unified_init(FEATURE_SCOPE_TASK, (u32)pid);
}

static inline void module_feature_auto_uninit_task(int pid)
{
    module_feature_unified_uninit(FEATURE_SCOPE_TASK, (u32)pid);
}

typedef void (*_fearture_show_task_func)(int pid, struct seq_file *seq);

/* the function decleared will be auto call when show task
   init_fn : shoule like "void _fearture_uninit_task_func(int pid)" */
#define DECLAER_FEATURE_AUTO_SHOW_TASK(show_task_fn, stage) \
    void stage##_task_show_##show_task_fn(int pid, struct seq_file *seq) \
    { \
        show_task_fn(pid, seq); \
    } \
    EXPORT_SYMBOL(stage##_task_show_##show_task_fn)

static inline void module_feature_show_task_prefix(char *prefix, int pid, struct seq_file *seq)
{
    struct module *module = KA_THIS_MODULE;
    u32 i;

    for (i = 0; i < module->num_syms; i++) {
        void *fn = get_symbol_prefix_func(&module->syms[i], prefix);
        if (fn != NULL) {
            ((_fearture_show_task_func)fn)(pid, seq);
        }
    }
}

static inline void module_feature_format_show_task_prefix(char *prefix, int len, int stage)
{
    (void)sprintf_s(prefix, PREFIX_MAX_LEN, "FEATURE_LOADER_STAGE_%d_task_show_", stage);
}

static inline void module_feature_show_task_stage(int stage, int pid, struct seq_file *seq)
{
    char show_prefix[PREFIX_MAX_LEN];

    module_feature_format_show_task_prefix(show_prefix, PREFIX_MAX_LEN, stage);
    module_feature_show_task_prefix(show_prefix, pid, seq);
}

/* pid=0, show global info */
static inline void module_feature_auto_show_task(int pid, struct seq_file *seq)
{
    int stage;

    for (stage = FEATURE_LOADER_STAGE_0; stage <= FEATURE_LOADER_STAGE_9; stage++) {
        module_feature_show_task_stage(stage, pid, seq);
    }
}

#else

static inline int module_feature_auto_init(void)
{
    return 0;
}

static inline void module_feature_auto_uninit(void)
{
}

static inline int module_feature_auto_init_dev(u32 udevid)
{
    return 0;
}

static inline void module_feature_auto_uninit_dev(u32 udevid)
{
}

static inline int module_feature_auto_init_task(int pid)
{
    return 0;
}

static inline void module_feature_auto_uninit_task(int pid)
{
}

static inline void module_feature_auto_show_task(int pid, struct seq_file *seq)
{
}

#endif

#endif

