/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * Create: 2025-02-11
 */

#ifndef KA_HASHTABLE_PUB_H
#define KA_HASHTABLE_PUB_H
#include <linux/hashtable.h>

#define KA_DEFINE_HASHTABLE(name, bits) DEFINE_HASHTABLE(name, bits)

#define KA_DEFINE_READ_MOSTLY_HASHTABLE(name, bits) DEFINE_READ_MOSTLY_HASHTABLE(name, bits)

#define KA_DECLARE_HASHTABLE(name, bits) DECLARE_HASHTABLE(name, bits)

#define KA_HASH_SIZE(name) HASH_SIZE(name)
#define KA_HASH_BITS(name) HASH_BITS(name)

/* Use hash_32 when possible to allow for fast 32bit hashing in 64bit kernels. */
#define ka_hash_min(val, bits) hash_min(val, bits)

/**
 * ka_hash_init - initialize a hash table
 * @hashtable: hashtable to be initialized
 *
 * Calculates the size of the hashtable from the given parameter, otherwise
 * same as hash_init_size.
 *
 * This has to be a macro since HASH_BITS() will not work on pointers since
 * it calculates the size during preprocessing.
 */
#define ka_hash_init(hashtable) hash_init(hashtable)

/**
 * ka_hash_add - add an object to a hashtable
 * @hashtable: hashtable to add to
 * @node: the &ka_hlist_node_t of the object to be added
 * @key: the key of the object to be added
 */
#define ka_hash_add(hashtable, node, key) hash_add(hashtable, node, key)

/**
 * ka_hash_hashed - check whether an object is in any hashtable
 * @node: the &ka_hlist_node_t of the object to be checked
 */
#define ka_hash_hashed(node) hash_hashed(node)

/**
 * ka_hash_empty - check whether a hashtable is empty
 * @hashtable: hashtable to check
 *
 * This has to be a macro since HASH_BITS() will not work on pointers since
 * it calculates the size during preprocessing.
 */
#define ka_hash_empty(hashtable) hash_empty(hashtable)

/**
 * ka_hash_del - remove an object from a hashtable
 * @node: &ka_hlist_node_t of the object to remove
 */
#define ka_hash_del(node) hash_del(node)

/**
 * ka_hash_for_each - iterate over a hashtable
 * @name: hashtable to iterate
 * @bkt: integer to use as bucket loop cursor
 * @obj: the type * to use as a loop cursor for each entry
 * @member: the name of the ka_hlist_node within the struct
 */
#define ka_hash_for_each(name, bkt, obj, member) hash_for_each(name, bkt, obj, member)

/**
 * ka_hash_for_each_safe - iterate over a hashtable safe against removal of
 * hash entry
 * @name: hashtable to iterate
 * @bkt: integer to use as bucket loop cursor
 * @tmp: a &ka_hlist_node_t used for temporary storage
 * @obj: the type * to use as a loop cursor for each entry
 * @member: the name of the ka_hlist_node within the struct
 */
#define ka_hash_for_each_safe(name, bkt, tmp, obj, member) hash_for_each_safe(name, bkt, tmp, obj, member)

/**
 * ka_hash_for_each_possible - iterate over all possible objects hashing to the
 * same bucket
 * @name: hashtable to iterate
 * @obj: the type * to use as a loop cursor for each entry
 * @member: the name of the ka_hlist_node within the struct
 * @key: the key of the objects to iterate over
 */
#define ka_hash_for_each_possible(name, obj, member, key) hash_for_each_possible(name, obj, member, key)

/**
 * ka_hash_for_each_possible_safe - iterate over all possible objects hashing to the
 * same bucket safe against removals
 * @name: hashtable to iterate
 * @obj: the type * to use as a loop cursor for each entry
 * @tmp: a &ka_hlist_node_t used for temporary storage
 * @member: the name of the ka_hlist_node within the struct
 * @key: the key of the objects to iterate over
 */
#define ka_hash_for_each_possible_safe(name, obj, tmp, member, key) hash_for_each_possible_safe(name, obj, tmp, member, key)

#endif
