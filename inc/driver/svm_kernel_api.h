/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 * Description:
 * Author: huawei
 * Create: 2025-07-19
 */
 
#ifndef __SVM_KERNEL_API_H__
#define __SVM_KERNEL_API_H__
#include <linux/types.h>
#include <linux/mm.h>

extern int svm_get_pasid(pid_t vpid, int dev_id);

#ifdef CONFIG_SVSP
extern unsigned long svm_svsp_mmap(unsigned long _addr, unsigned long len, int pasid);
extern int svm_svsp_remap_range(unsigned long va, unsigned long iova,
               size_t size, int pasid, pgprot_t prot, u64 page_size);
extern void svm_svsp_munmap(unsigned long start, unsigned long len, int pasid);
extern struct mm_struct *svm_svsp_of_mm(struct mm_struct *mm);
#else
static inline unsigned long svm_svsp_mmap(unsigned long len, int pasid)
{
       return -EINVAL;
}

static inline int svm_svsp_remap_range(unsigned long va, unsigned long iova,
               size_t size, int pasid, pgprot_t prot, u64 page_size)
{
       return -EINVAL;
}

static inline void svm_svsp_munmap(unsigned long start, unsigned long len, int pasid)
{
       return;
}

static inline struct mm_struct *svm_svsp_of_mm(struct mm_struct *mm)
{
       return NULL;
}
#endif

#endif
