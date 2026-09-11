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
 * Create: 2024-11-11
 */

#ifndef KA_BASE_PUB_H
#define KA_BASE_PUB_H

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
#include <linux/stdarg.h>
#else
#include <stdarg.h>
#endif
#include <linux/bitmap.h>
#include <linux/err.h>
#include <linux/spinlock_types.h>
#include <linux/atomic.h>
#include <linux/slab.h>
#include <linux/string.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
#include <linux/irq_poll.h>
#endif
#include <linux/crc16.h>
#include <linux/skbuff.h>
#include <linux/ctype.h>
#include <linux/genalloc.h>
#include <linux/cdev.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/idr.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/preempt.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/kfifo.h>
#include <linux/namei.h>
#include <linux/rbtree.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/refcount.h>
#endif
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/crc32.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/random.h>
#include <linux/scatterlist.h>
#include <linux/sort.h>
#include <linux/interrupt.h>
#include <linux/capability.h>
#include <linux/hash.h>
#include <linux/kref.h>
#include <linux/jhash.h>
#include <linux/stat.h>
#include <linux/posix_types.h>

#include "ka_macro_pub.h"
#include "ka_common_pub.h"

#define KA_INT_MAX          INT_MAX
#define KA_UINT_MAX         UINT_MAX
#define KA_U32_MAX          U32_MAX
#define KA_S32_MAX          S32_MAX
#define KA_U64_MAX          U64_MAX
#define KA_LONG_MAX         LONG_MAX
#define KA_ULLONG_MAX       ULLONG_MAX

#define KA_HZ               HZ

#define KA_CAP_SYS_ADMIN        CAP_SYS_ADMIN
#define KA_STATX_BASIC_STATS    STATX_BASIC_STATS
#define KA_AT_NO_AUTOMOUNT      AT_NO_AUTOMOUNT

typedef struct cpu_rmap ka_cpu_rmap_t;
typedef struct rb_node ka_rb_node_t;
typedef struct rb_root_cached ka_rb_root_cached_t;
typedef struct rb_root ka_rb_root_t;
typedef struct idr ka_idr_t;
typedef struct ida ka_ida_t;

typedef struct __kfifo __ka_kfifo_t;

typedef struct dql ka_dql_t;
typedef struct fasync_struct ka_fasync_struct_t;
typedef struct proc_dir_entry ka_proc_dir_entry_t;
typedef struct seq_operations ka_seq_operations_t;
typedef struct poll_table_struct ka_poll_table_struct_t;

typedef struct module ka_module_t;
#define __ka_poll_t __poll_t
typedef struct kref ka_kref_t;
typedef struct refcount_struct ka_refcount_struct_t;
typedef struct completion ka_completion_t;
typedef struct cpumask ka_cpumask_t;
#define ka_gfp_t gfp_t
#define ka_clockid_t clockid_t
#define ka_dev_t dev_t
typedef struct device_type ka_device_type_t;

#define KA_BASE_ARRAY_SIZE(x)  ARRAY_SIZE(x)
#define KA_BASE_ATOMIC_INIT(i) ATOMIC_INIT(i)

#define ka_atomic_t atomic_t
#define ka_atomic64_t atomic64_t

#define KA_RB_ROOT RB_ROOT

#define ka_base_min(x, y)                   min(x, y)
#define ka_base_min_t(type, x, y)           min_t(type, x, y)
#define ka_base_max_t(type, x, y)           max_t(type, x, y)

#define KA_BITS_PER_BYTE                    BITS_PER_BYTE
#define KA_BASE_BITS_PER_TYPE(type)         BITS_PER_TYPE(type)
#define KA_BASE_BITS_TO_LONGS(nr)           BITS_TO_LONGS(nr)
#define KA_BASE_DECLARE_BITMAP(name,bits)   DECLARE_BITMAP(name,bits)

#define KA_BASE_RB_EMPTY_ROOT(root)         RB_EMPTY_ROOT(root)
#define KA_BASE_RB_EMPTY_NODE(node)         RB_EMPTY_NODE(node)
#define KA_BASE_RB_CLEAR_NODE(node)         RB_CLEAR_NODE(node)
#define KA_BASE_DEFINE_RATELIMIT_STATE(name, interval_init, burst_init) DEFINE_RATELIMIT_STATE(name, interval_init, burst_init)

#define ka_base_round_up(x, y) round_up(x, y)
#define ka_base_round_down(x, y) round_down(x, y)
#define ka_base_rounddown(x, y) rounddown(x, y)
#define KA_BASE_DIV_ROUND_UP(n, d) DIV_ROUND_UP(n, d)

#define ka_base_simple_strtoull(cp, endp, base) simple_strtoull(cp, endp, base)
#define ka_base_simple_strtoul(cp, endp, base) simple_strtoul(cp, endp, base)
#define ka_base_simple_strtol(cp, endp, base) simple_strtol(cp, endp, base)
#define ka_base_simple_strtoll(cp, endp, base) simple_strtoll(cp, endp, base)
#define ka_base_vscnprintf(buf, size, fmt, args) vscnprintf(buf, size, fmt, args)
#define ka_base_find_next_bit(addr, size, offset) find_next_bit(addr, size, offset)
#define ka_base_find_next_zero_bit(addr, size, offset) find_next_zero_bit(addr, size, offset)
#define ka_base_find_next_zero_bit_le(addr, size, offset) find_next_zero_bit_le(addr, size, offset)
#define ka_base_find_next_bit_le(addr, size, offset) find_next_bit_le(addr, size, offset)
#define ka_base_find_first_bit(addr, size) find_first_bit(addr, size)
#define _ka_base_copy_from_user(to, from, n) _copy_from_user(to, from, n)
#define _ka_base_copy_to_user(to, from, n) _copy_to_user(to, from, n)

#define ka_base_rb_entry(ptr, type, member) rb_entry(ptr, type, member)
#define ka_base_rb_insert_color(node, root) rb_insert_color(node, root)
#define ka_base_rb_erase(node, root) rb_erase(node, root)
#define ka_base_rb_insert_color_cached(node, root, leftmost) rb_insert_color_cached(node, root, leftmost)
#define ka_base_rb_erase_cached(node, root) rb_erase_cached(node, root)
#define ka_base_rb_first(root) rb_first(root)
#define ka_base_rb_next(node) rb_next(node)
#define ka_base_rb_prev(node) rb_prev(node)
#define ka_base_rb_replace_node(victim, rb_new, root) rb_replace_node(victim, rb_new, root)
#define ka_base_rb_replace_node_cached(victim, rb_new, root) rb_replace_node_cached(victim, rb_new, root)
#define ka_base_rb_replace_node_rcu(victim, rb_new, root) rb_replace_node_rcu(victim, rb_new, root)
#define ka_base_rb_next_postorder(node) rb_next_postorder(node)
#define ka_base_rb_first_postorder(root) rb_first_postorder(root)
#define ka_base_rb_link_node(node, parent, rb_link) rb_link_node(node, parent, rb_link)
static inline ka_rb_node_t *ka_base_get_rb_root_node(ka_rb_root_t *root)
{
    return root->rb_node;
}
static inline ka_rb_node_t *ka_base_get_rb_node_left(ka_rb_node_t *node)
{
    return node->rb_left;
}
static inline ka_rb_node_t *ka_base_get_rb_node_right(ka_rb_node_t *node)
{
    return node->rb_right;
}
static inline ka_rb_node_t **ka_base_get_rb_root_node_addr(ka_rb_root_t *root)
{
    return &root->rb_node;
}
static inline ka_rb_node_t **ka_base_get_rb_node_left_addr(ka_rb_node_t *node)
{
    return &node->rb_left;
}
static inline ka_rb_node_t **ka_base_get_rb_node_right_addr(ka_rb_node_t *node)
{
    return &node->rb_right;
}

#define KA_BASE_DEFINE_IDR(name) DEFINE_IDR(name)
#define ka_base_idr_alloc(idr, ptr, start, end, gfp) idr_alloc(idr, ptr, start, end, gfp)
#define ka_base_idr_alloc_cyclic(idr, ptr, start, end, gfp) idr_alloc_cyclic(idr, ptr, start, end, gfp)
#define ka_base_idr_for_each(idr, fn, data) idr_for_each(idr, fn, data)
#define ka_base_idr_get_next_ul(idr, nextid) idr_get_next_ul(idr, nextid)
#define ka_base_idr_get_next(idr, nextid) idr_get_next(idr, nextid)
#define ka_base_idr_find(idr, id) idr_find(idr, id)
#define ka_base_idr_remove(idr, id) idr_remove(idr, id)
#define ka_base_idr_init_base(idr, base) idr_init_base(idr, base)
#define ka_base_idr_is_empty(idr) idr_is_empty(idr)


#define ka_base_ida_destroy(ida) ida_destroy(ida)
#define ka_base_ida_alloc_range(ida, min, max, gfp) ida_alloc_range(ida, min, max, gfp)
#define ka_base_ida_free(ida, id) ida_free(ida, id)
#define ka_base_strncpy_from_user(dst, src, count) strncpy_from_user(dst, src, count)
#define ka_base_strnlen_user(str, count) strnlen_user(str, count)
#define ka_base_print_hex_dump(level, prefix_str, prefix_type, rowsize, groupsize, buf, len, ascii) print_hex_dump(level, prefix_str, prefix_type, rowsize, groupsize, buf, len, ascii)
#define ka_base_print_hex_dump_bytes(prefix_str, prefix_type, buf, len) print_hex_dump_bytes(prefix_str, prefix_type, buf, len)


#define __ka_base_kfifo_alloc(fifo, size, esize, gfp_mask) __kfifo_alloc(fifo, size, esize, gfp_mask)
#define __ka_base_kfifo_free(fifo) __kfifo_free(fifo)
#define __ka_base_kfifo_init(fifo, buffer, size, esize) __kfifo_init(fifo, buffer, size, esize)
#define __ka_base_kfifo_in(fifo, buf, len) __kfifo_in(fifo, buf, len)
#define __ka_base_kfifo_out_peek(fifo, buf, len) __kfifo_out_peek(fifo, buf, len)
#define __ka_base_kfifo_out(fifo, buf, len) __kfifo_out(fifo, buf, len)
#define __ka_base_kfifo_in_r(fifo, buf, len, recsize) __kfifo_in_r(fifo, buf, len, recsize)
#define __ka_base_udelay(usecs) __udelay(usecs)
#define __ka_base_const_udelay(xloops) __const_udelay(xloops)


#define ka_base_dql_completed(dql, count) dql_completed(dql, count)
#define ka_base_dql_reset(dql) dql_reset(dql)


static inline void ka_base_set_kobj_parent(ka_kobject_t *kobj, ka_kobject_t *parent)
{
    kobj->parent = parent;
}
#define ka_base_kobject_put(kobj) kobject_put(kobj)
#define ka_base_kobject_name(kobj) kobject_name(kobj)


#define ka_base_fasync_helper(fd, filp, on, fap) fasync_helper(fd, filp, on, fap)
#define ka_base_kill_fasync(fp, sig, band) kill_fasync(fp, sig, band)

#define ka_base_bitmap_and(dst, bitmap1, bitmap2, nbits) bitmap_and(dst, bitmap1, bitmap2, nbits)
#define ka_base_bitmap_andnot(dst, bitmap1, bitmap2, nbits) bitmap_andnot(dst, bitmap1, bitmap2, nbits)
#define ka_base_bitmap_subset(bitmap1, bitmap2, nbits) bitmap_subset(bitmap1, bitmap2, nbits)
#define ka_base_bitmap_weight(bitmap, nbits) bitmap_weight(bitmap, nbits)
#define ka_base_bitmap_set(map, start, len) bitmap_set(map, start, len)
#define ka_base_bitmap_clear(map, start, len) bitmap_clear(map, start, len)
#define ka_base_bitmap_find_next_zero_area_off(map, size, start, nr, align_mask, align_offset) bitmap_find_next_zero_area_off(map, size, start, nr, align_mask, align_offset)
#define ka_base_bitmap_parselist(buf, maskp, nmaskbits) bitmap_parselist(buf, maskp, nmaskbits)
#define ka_base_bitmap_parselist_user(ubuf, ulen, dst, nbits) bitmap_parselist_user(ubuf, ulen, dst, nbits)

typedef struct gen_pool ka_gen_pool_t;

#define ka_base_gen_pool_create(min_alloc_order, nid) gen_pool_create(min_alloc_order, nid)
#define ka_base_gen_pool_destroy(pool) gen_pool_destroy(pool)
#define ka_base_gen_pool_virt_to_phys(pool, addr) gen_pool_virt_to_phys(pool, addr)
#define ka_base_gen_pool_add_virt(pool, virt, phys, size, nid) gen_pool_add_virt(pool, virt, phys, size, nid)
#define ka_base_gen_pool_avail(pool) gen_pool_avail(pool)
#define ka_base_gen_pool_size(pool) gen_pool_size(pool)
#define ka_base_gen_pool_alloc(pool, size) gen_pool_alloc(pool, size)
#define ka_base_gen_pool_free(pool, addr, size) gen_pool_free(pool, addr, size)

#define ka_base_kasprintf kasprintf

#define KA_IRQ_NONE         IRQ_NONE
#define KA_IRQ_HANDLED      IRQ_HANDLED
#define KA_IRQ_WAKE_THREAD  IRQ_WAKE_THREAD

#define ka_irqreturn_t irqreturn_t
#define ka_irq_handler_t irq_handler_t
typedef struct irq_poll ka_irq_poll_t;
#define ka_base_irq_poll_init(iop, weight, poll_fn) irq_poll_init(iop, weight, poll_fn)
#define ka_base_irq_poll_destroy(iop) irq_poll_destroy(iop)
#define ka_base_irq_poll_sched(iop) irq_poll_sched(iop)
#define ka_base_irq_poll_complete(iop) irq_poll_complete(iop)
#define ka_base_irq_poll_disable(iop) irq_poll_disable(iop)

unsigned short const *ka_base_get_crc16_table(void);
unsigned char const *_ka_base_get_ctype(void);
#define ka_base_crc16(crc, buffer, len) crc16(crc, buffer, len)

typedef struct resource ka_resource_t;
typedef struct radix_tree_root ka_radix_tree_root_t;
typedef struct radix_tree_iter ka_radix_tree_iter_t;
typedef struct rnd_state ka_rnd_state_t;
typedef struct ratelimit_state ka_ratelimit_state_t;
typedef struct sg_table ka_sg_table_t;
typedef struct scatterlist ka_scatterlist_t;

#define ka_base_cpumask_local_spread(i, node) cpumask_local_spread(i, node)
#define ka_base_cpumask_next(n, srcp) cpumask_next(n, srcp)
#define ka_base_cpumask_next_and(n, src1p, src2p) cpumask_next_and(n, src1p, src2p)
#define ka_base_cpumask_next_wrap(n, mask, start, wrap) cpumask_next_wrap(n, mask, start, wrap)
#define ka_base_crc32_le(crc, p, len) crc32_le(crc, p, len)
#define ka_base_crc32_le_shift(crc, len) crc32_le_shift(crc, len)
#define ka_base_devm_ioremap(dev, resource_offset, resource_size) devm_ioremap(dev, resource_offset, resource_size)
#define ka_base_devm_ioremap_nocache(dev, resource_offset, resource_size) devm_ioremap_nocache(dev, resource_offset, resource_size)
#define ka_base_devm_ioremap_resource(dev, res) devm_ioremap_resource(dev, res)
#define ka_base_devm_ioremap_wc(dev, resource_offset, resource_size) devm_ioremap_wc(dev, resource_offset, resource_size)
#define ka_base_devm_iounmap(dev, addr) devm_iounmap(dev, addr)
#define ka_base_pcim_iomap(pdev, bar, maxlen) pcim_iomap(pdev, bar, maxlen)
#define ka_base_pcim_iomap_regions(pdev, mask, name) pcim_iomap_regions(pdev, mask, name)
#define ka_base_pcim_iomap_regions_request_all(pdev, mask, name) pcim_iomap_regions_request_all(pdev, mask, name)
#define ka_base_pcim_iomap_table(pdev) pcim_iomap_table(pdev)
#define ka_base_pcim_iounmap(pdev, addr) pcim_iounmap(pdev, addr)
#define ka_base_pcim_iounmap_regions(pdev, mask) pcim_iounmap_regions(pdev, mask)
#define ka_base_dump_stack() dump_stack()
#define __ka_base_sw_hweight32(w) __sw_hweight32(w)
#define __ka_base_sw_hweight64(w) __sw_hweight64(w)
#define __ka_base_kfifo_out_peek_r(fifo, buf, len, recsize) __kfifo_out_peek_r(fifo, buf, len, recsize)
#define __ka_base_kfifo_out_r(fifo, buf, len, recsize) __kfifo_out_r(fifo, buf, len, recsize)
#define __ka_base_kfifo_int_must_check_helper(val) __kfifo_int_must_check_helper(val)
#define __ka_base_kfifo_max_r(len, recsize) __kfifo_max_r(len, recsize)

#define ka_base_kfifo_size(fifo) kfifo_size(fifo)
#define ka_base_kfifo_len(fifo) kfifo_len(fifo)
#define ka_base_kfifo_reset(fifo) kfifo_reset(fifo)
#define ka_base_kfifo_alloc(fifo, size, gfp_mask) kfifo_alloc(fifo, size, gfp_mask)
#define ka_base_kfifo_free(fifo) kfifo_free(fifo)
#define ka_base_kfifo_is_empty(fifo) kfifo_is_empty(fifo)
#define	ka_base_kfifo_is_full(fifo) kfifo_is_full(fifo)
#define ka_base_kfifo_out_peek(fifo, buf, n) kfifo_out_peek(fifo, buf, n)
#define ka_base_kfifo_in(fifo, buf, n) kfifo_in(fifo, buf, n)
#define ka_base_kfifo_out(fifo, buf, n) kfifo_out(fifo, buf, n)
#define ka_base_kfifo_avail(fifo) kfifo_avail(fifo)

#define ka_base_kstrtobool(s, res) kstrtobool(s, res)
#define ka_base_kstrtobool_from_user(s, count, res) kstrtobool_from_user(s, count, res)
#define ka_base_kstrtoint(s, base, res) kstrtoint(s, base, res)
#define ka_base_kstrtol(s, base, res) kstrtol(s, base, res)
#define ka_base_kstrtoll(s, base, res) kstrtoll(s, base, res)
#define ka_base_kstrtou16(s, base, res) kstrtou16(s, base, res)
#define ka_base_kstrtou8(s, base, res) kstrtou8(s, base, res)
#define ka_base_kstrtou32(s, base, res) kstrtou32(s, base, res)
#define ka_base_kstrtouint(s, base, res) kstrtouint(s, base, res)
#define ka_base_kstrtoull(s, base, res) kstrtoull(s, base, res)
#define ka_base_idr_destroy(idr) idr_destroy(idr)
#define ka_base_idr_init(idr) idr_init(idr)
#define ka_base_radix_tree_insert(root, index, item) radix_tree_insert(root, index, item)
#define ka_base_radix_tree_delete(root, index) radix_tree_delete(root, index)
#define ka_base_radix_tree_delete_item(root, index, item) radix_tree_delete_item(root, index, item)
#define ka_base_radix_tree_iter_delete(root, iter, slot) radix_tree_iter_delete(root, iter, slot)
#define ka_base_radix_tree_lookup(root, index) radix_tree_lookup(root, index)
#define ka_base_radix_tree_lookup_slot(root, index) radix_tree_lookup_slot(root, index)
#define ka_base_radix_tree_next_chunk(root, iter, flags) radix_tree_next_chunk(root, iter, flags)
#define ka_base_radix_tree_tagged(root, tag) radix_tree_tagged(root, tag)
#define ka_base_prandom_u32_state(state) prandom_u32_state(state)

#define __ka_base_ratelimit(state) __ratelimit(state)

#define ka_base_sg_set_page(sg, page, len, offset) sg_set_page(sg, page, len, offset)
#define ka_base_sg_alloc_table(table, nents, gfp_mask) sg_alloc_table(table, nents, gfp_mask)
#define ka_base_sg_alloc_table_from_pages(sgt, pages, n_pages, offset, size, gfp_mask) sg_alloc_table_from_pages(sgt, pages, n_pages, offset, size, gfp_mask)
#define ka_base_sg_copy_from_buffer(sgl, nents, buf, buflen) sg_copy_from_buffer(sgl, nents, buf, buflen)
#define ka_base_sg_copy_to_buffer(sgl, nents, buf, buflen) sg_copy_to_buffer(sgl, nents, buf, buflen)
#define ka_base_sg_free_table(table) sg_free_table(table)
#define ka_base_sg_init_table(sgl, nents) sg_init_table(sgl, nents)
#define ka_base_sg_nents(sg) sg_nents(sg)
#define ka_base_sg_nents_for_len(sg, len) sg_nents_for_len(sg, len)
#define ka_base_sg_next(sg) sg_next(sg)
#define ka_base_sg_pcopy_from_buffer(sgl, nents, buf, buflen, skip) sg_pcopy_from_buffer(sgl, nents, buf, buflen, skip)
#define ka_base_sg_pcopy_to_buffer(sgl, nents, buf, buflen, skip) sg_pcopy_to_buffer(sgl, nents, buf, buflen, skip)
#define ka_base_sort(base, num, size, cmp_func, swap_func) sort(base, num, size, cmp_func, swap_func)
#define ka_base_fortify_panic(name) fortify_panic(name)
#define ka_base_memchr_inv(start, c, bytes) memchr_inv(start, c, bytes)
#define ka_base_memcmp(cs, ct, count) memcmp(cs, ct, count)
#define ka_base_memset16(s, c, count) memset16(s, c, count)
#define ka_base_memset32(s, c, count) memset32(s, c, count)
#define ka_base_memset64(s, c, count) memset64(s, c, count)
#define ka_base_strchr(s, c) strchr(s, c)
#define ka_base_strchrnul(s, c) strchrnul(s, c)
#define ka_base_strcmp(cs, ct) strcmp(cs, ct)
#define ka_base_strlcat(dest, src, count) strlcat(dest, src, count)
#define ka_base_strlen(s) strlen(s)
#define ka_base_strncmp(cs, ct, count) strncmp(cs, ct, count)
#define ka_base_strnlen(s, count) strnlen(s, count)
#define ka_base_strrchr(s, c) strrchr(s, c)
#define ka_base_strsep(s, ct) strsep(s, ct)
#define ka_base_strstr(s1, s2) strstr(s1, s2)
#define ka_base_copy_from_user(to, from, n) copy_from_user(to, from, n)
#define ka_base_copy_to_user(to, from, n) copy_to_user(to, from, n)
#define ka_base_get_user(x, ptr) get_user(x, ptr)
#define ka_base_put_user(x, ptr) put_user(x, ptr)

u32 ka_base_get_random_u32(void);
#define ka_base_device_initialize(dev) device_initialize(dev)
#define ka_base_kref_init(ka_kref_t) kref_init(ka_kref_t)
#define ka_base_kref_get(ka_kref_t) kref_get(ka_kref_t)
#define ka_base_kref_put(ka_kref_t, release) kref_put(ka_kref_t, release)
#define ka_base_kref_read(ka_kref_t) kref_read(ka_kref_t)
#define ka_base_kref_get_unless_zero(ka_kref_t) kref_get_unless_zero(ka_kref_t)

#define ka_base_irq_set_affinity_hint(irq, mask) irq_set_affinity_hint(irq, mask)
void *ka_base_pde_data(const ka_inode_t *inode);
#define ka_base_set_bit(nr, addr) set_bit(nr, addr)
#define ka_base_clear_bit(nr, addr) clear_bit(nr, addr)
#define ka_base_test_bit(nr, addr) test_bit(nr, addr)
#define ka_base_test_and_set_bit(nr, addr) test_and_set_bit(nr, addr)
#define KA_BASE_RB_CLEAR_NODE(node) RB_CLEAR_NODE(node)
#define ka_base_reinit_completion(x) reinit_completion(x)

#define ka_base_cpumask_empty(srcp) cpumask_empty(srcp)
#define ka_base_cpumask_clear_cpu(cpu, dstp) cpumask_clear_cpu(cpu, dstp)
#define ka_base_cpumask_set_cpu(cpu, dstp) cpumask_set_cpu(cpu, dstp)
#define ka_base_cpumask_available(mask) cpumask_available(mask)
#define ka_base_set_cpus_allowed_ptr(p, newmask) set_cpus_allowed_ptr(p, newmask)
#define ka_base_cpumask_clear(dstp) cpumask_clear(dstp)
#define ka_base_cpumask_copy(dstp, srcp) cpumask_copy(dstp, srcp)
#define ka_base_zalloc_cpumask_var(mask, flags) zalloc_cpumask_var(mask, flags)
#define ka_base_free_cpumask_var(mask) free_cpumask_var(mask)
#define ka_base_cpulist_parse(buf, dstp) cpulist_parse(buf, dstp)
#define ka_base_ffs(x) ffs(x)
#define ka_base_atomic_set(v, i) atomic_set(v, i)
#define ka_base_atomic_dec_and_test(v) atomic_dec_and_test(v)
#define ka_base_atomic_inc_return(v) atomic_inc_return(v)
#define ka_base_atomic_read(v) atomic_read(v)
#define ka_base_atomic_dec(v) atomic_dec(v)
#define ka_base_atomic_dec_return(v) atomic_dec_return(v)
#define ka_base_atomic_cmpxchg(v, old, newv) atomic_cmpxchg(v, old, newv)
#define ka_base_atomic_xchg(v, i) atomic_xchg(v, i)
#define KA_BASE_ATOMIC64_INIT(v) ATOMIC64_INIT(v)
#define ka_base_atomic64_inc(v) atomic64_inc(v)
#define ka_base_atomic64_dec(v) atomic64_dec(v)
#define ka_base_atomic64_inc_return(v) atomic64_inc_return(v)
#define ka_base_atomic_sub_and_test(i, v) atomic_sub_and_test(i, v)
#define ka_base_in_atomic() in_atomic()
#define ka_base_atomic_and(i, v) atomic_and(i, v)
#define ka_base_atomic_or(i, v) atomic_or(i, v)
#define ka_base_atomic_fetch_or(i, v) atomic_fetch_or(i, v)
#define ka_base_atomic_fetch_inc(v) atomic_fetch_inc(v)

#define ka_base_atomic64_cmpxchg(v, old, newv) atomic64_cmpxchg(v, old, newv)
#define ka_base_atomic64_sub_return(i, v) atomic64_sub_return(i, v)
#define ka_base_atomic64_sub(i, v) atomic64_sub(i, v)
#define ka_base_atomic64_add_return(i, v) atomic64_add_return(i, v)
#define ka_base_atomic64_add(i, v) atomic64_add(i, v)
#define ka_base_atomic64_read(v) atomic64_read(v)
#define ka_base_atomic64_set(v, i) atomic64_set(v, i)
#define ka_base_atomic_inc(v) atomic_inc(v)
#define ka_base_atomic_add(i, v) atomic_add(i, v)
#define ka_base_atomic_add_return(i, v) atomic_add_return(i, v)

static inline void ka_base_set_device_devt(ka_device_t *dev, ka_dev_t devt)
{
    dev->devt = devt;
}

#if (!defined EMU_ST) && (!defined UT_VCAST)
static inline void ka_base_set_device_class(ka_device_t *dev, ka_class_t *class)
{
    dev->class = class;
}
#endif

static inline void ka_base_set_device_type(ka_device_t *dev, ka_device_type_t *type)
{
    dev->type = type;
}

static inline ka_kobject_t *ka_base_get_device_kobj(ka_device_t *dev)
{
    return &dev->kobj;
}

#define ka_base_hash_32(val, bits) hash_32(val, bits)
#define ka_base_hash_long(val, bits) hash_long(val, bits)
#define ka_base_jhash(key, length, initval) jhash(key, length, initval)
#define ka_base_ilog2(x) ilog2(x)

#define KA_BASE_BIT(nr) BIT(nr)

#define ka_base_idr_for_each_entry(idr, entry, id) idr_for_each_entry(idr, entry, id)
#define ka_base_for_each_sg(sglist, sg, nr, __i) for_each_sg(sglist, sg, nr, __i)
#define ka_base_for_each_online_cpu(cpu)    for_each_online_cpu(cpu)

#endif
