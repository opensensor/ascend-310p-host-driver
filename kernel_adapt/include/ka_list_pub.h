// SPDX-License-Identifier: GPL-2.0
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
 * Create: 2024-12-10
 */
#ifndef KA_LIST_PUB_H
#define KA_LIST_PUB_H

#include <linux/types.h>
#include <linux/list.h>

#include "ka_common_pub.h"

typedef struct list_head ka_list_head_t;

#define KA_LIST_POISON1 LIST_POISON1
#define KA_LIST_POISON2 LIST_POISON2

#define KA_LIST_HEAD_INIT(name) LIST_HEAD_INIT(name)

#define KA_LIST_HEAD(name) LIST_HEAD(name)

typedef struct hlist_head ka_hlist_head_t;

#define ka_hlist_first_rcu(head) hlist_first_rcu(head)

#define KA_INIT_LIST_HEAD(list) INIT_LIST_HEAD(list)

// Insert a new entry after the specified head.
#define ka_list_add(entry, head) list_add(entry, head)

// Insert a new entry before the specified head.
#define ka_list_add_tail(entry, head) list_add_tail(entry, head)

// Note: ka_list_empty() on entry does not return true after this, the entry is in an undefined state.
#define ka_list_del(entry) list_del(entry)

// replace old entry by new one
// If @old was empty, it will be overwritten.
#define ka_list_replace(old, entry) list_replace(old, entry)

// replace old entry by new one and initialize the old one
// If @old was empty, it will be overwritten.
#define ka_list_replace_init(old, entry) list_replace_init(old, entry)

// replace entry1 with entry2 and re-add entry1 at entry2's position
#define ka_list_swap(entry1, entry2) list_swap(entry1, entry2)

// deletes entry from list and reinitialize it.
#define ka_list_del_init(entry) list_del_init(entry)

// delete the entry from one list and add as another's head
#define ka_list_move(entry, head) list_move(entry, head)

// delete the entry from one list and add as another's tail
#define ka_list_move_tail(entry, head) list_move_tail(entry, head)

// Move all entries between @first and including @last before @list.
// All three entries must belong to the same list.
#define ka_list_bulk_move_tail(list, first, last) list_bulk_move_tail(list, first, last)

// tests whether @entry is the first entry in list @head
#define ka_list_is_first(entry, head) list_is_first(entry, head)

// tests whether @entry is the last entry in list @head
#define ka_list_is_last(entry, head) list_is_last(entry, head)

// tests whether a list is empty
#define ka_list_empty(head) list_empty(head)

/*
 * This is the same as ka_list_del_init(), except designed to be used
 * together with ka_list_empty_careful() in a way to guarantee ordering
 * of other memory operations.
 * Any memory operations done before a ka_list_del_init_careful() are
 * guaranteed to be visible after a ka_list_empty_careful() test.
 */
#define ka_list_del_init_careful(entry) list_del_init_careful(entry)

/*
 * tests whether a list is empty and not being modified
 * Description:
 * tests whether a list is empty _and_ checks that no other CPU might be
 * in the process of modifying either member (next or prev)
 * NOTE: using ka_list_empty_careful() without synchronization
 * can only be safe if the only activity that can happen
 * to the list entry is ka_list_del_init().
 * Eg. it cannot be used if another CPU could re-ka_list_add() it.
 */
#define ka_list_empty_careful(head) list_empty_careful(head)

// ka_list_rotate_left - rotate the list to the left
#define ka_list_rotate_left(head) list_rotate_left(head)

// Rotates list so that @entry becomes the new front of the list.
#define ka_list_rotate_to_front(entry, head) list_rotate_to_front(entry, head)

// tests whether a list has just one entry.
#define ka_list_is_singular(head) list_is_singular(head)

/**
 * cut a list into two
 * @tmp: a new list to add all removed entries
 * @head: a list with entries
 * @entry: an entry within head, could be the head itself, if so we won't cut the list
 * Move the initial part of @head,
 * up to and including @entry, from @head to @tmp.
 * You should pass on @entry an element you know is on @head.
 * @tmp should be an empty list or a list you do not care about losing its data.
 */
#define ka_list_cut_position(tmp, head, entry) list_cut_position(tmp, head, entry)

/**
 * cut a list into two, before the entry
 * @tmp: a new list to add all removed entries
 * @head: a list with entries
 * @entry: an entry within head, could be the head itself
 * Move the initial part of @head, up to but excluding @entry, from @head to @tmp.
 * You should pass in @entry an element you know is on @head.
 * @tmp should be an empty list or a list you do not care about losing its data.
 * If @entry == @head, all entries on @head are moved to @tmp.
 */
#define ka_list_cut_before(tmp, head, entry) list_cut_before(tmp, head, entry)

/**
 * join two lists, this is designed for stacks
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 */
#define ka_list_splice(list, head) list_splice(list, head)

/**
 * join two lists, each list being a queue
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 */
#define ka_list_splice_tail(list, head) list_splice_tail(list, head)

/**
 * join two lists and reinitialise the emptied list.
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 */
#define ka_list_splice_init(list, head) list_splice_init(list, head)

/**
 * join two lists and reinitialise the emptied list
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 * Each of the lists is a queue.
 */
#define ka_list_splice_tail_init(list, head) list_splice_tail_init(list, head)

/**
 * get the struct for this entry
 * @ptr: the &ka_list_head_t pointer.
 * @type: the type of the struct this is embedded in.
 * @member: the name of the list_head within the struct.
 */
#define ka_list_entry(ptr, type, member) list_entry(ptr, type, member)

/**
 * get the first element from a list
 * @ptr: the list head to take the element from.
 * @type: the type of the struct this is embedded in.
 * @member: the name of the list_head within the struct.
 * list is expected to be not empty.
 */
#define ka_list_first_entry(ptr, type, member) list_first_entry(ptr, type, member)

/**
 * get the last element from a list
 * @ptr: the list head to take the element from.
 * @type: the type of the struct this is embedded in.
 * @member: the name of the list_head within the struct.
 *  list is expected to be not empty.
 */
#define ka_list_last_entry(ptr, type, member) list_last_entry(ptr, type, member)

/**
 * get the first element from a list
 * @ptr: the list head to take the element from.
 * @type: the type of the struct this is embedded in.
 * @member: the name of the list_head within the struct.
 * if the list is empty, it returns NULL.
 */
#define ka_list_first_entry_or_null(ptr, type, member) list_first_entry_or_null(ptr, type, member)

/**
 * get the next element in list
 * @pos: the type * to cursor
 * @member: the name of the list_head within the struct.
 */
#define ka_list_next_entry(pos, member) list_next_entry(pos, member)

/**
 * get the prev element in list
 * @pos: the type * to cursor
 * @member: the name of the list_head within the struct.
 */
#define ka_list_prev_entry(pos, member) list_prev_entry(pos, member)

/**
 * iterate over a list
 * @pos: the &ka_list_head_t to use as a loop cursor.
 * @head: the head for your list.
 */
#define ka_list_for_each(pos, head) list_for_each(pos, head)

/**
 * continue iteration over a list
 * @pos: the &ka_list_head_t to use as a loop cursor.
 * @head: the head for your list.
 * Continue to iterate over a list, continuing after the current position.
 */
#define ka_list_for_each_continue(pos, head) list_for_each_continue(pos, head)

/**
 * iterate over a list backwards
 * @pos: the &ka_list_head_t to use as a loop cursor.
 * @head: the head for your list.
 */
#define ka_list_for_each_prev(pos, head) list_for_each_prev(pos, head)

/**
 * iterate over a list safe against removal of list entry
 * @pos: the &ka_list_head_t to use as a loop cursor.
 * @n: another &ka_list_head_t to use as temporary storage
 * @head: the head for your list.
 */
#define ka_list_for_each_safe(pos, n, head) list_for_each_safe(pos, n, head)

/**
 * iterate over a list backwards safe against removal of list entry
 * @pos: the &ka_list_head_t to use as a loop cursor.
 * @n: another &ka_list_head_t to use as temporary storage
 * @head: the head for your list.
 */
#define ka_list_for_each_prev_safe(pos, n, head) list_for_each_prev_safe(pos, n, head)

/**
 * test if the entry points to the head of the list
 * @pos: the type * to cursor
 * @head: the head for your list
 * @member: the name of the list_head within the struct
 */
#define ka_list_entry_is_head(pos, head, member) list_entry_is_head(pos, head, member)

/**
 * iterate over list of given type
 * @pos: the type * to use as a loop cursor.
 * @head: the head for your list.
 * @member: the name of the list_head within the struct.
 */
#define ka_list_for_each_entry(pos, head, member) list_for_each_entry(pos, head, member)

/**
 * iterate backwards over list of given type.
 * @pos: the type * to use as a loop cursor.
 * @head: the head for your list.
 * @member: the name of the list_head within the struct.
 */
#define ka_list_for_each_entry_reverse(pos, head, member) list_for_each_entry_reverse(pos, head, member)

/**
 * prepare a pos entry for use in ka_list_for_each_entry_continue()
 * @pos: the type * to use as a start point
 * @head: the head of the list
 * @member: the name of the list_head within the struct.
 * Prepares a pos entry for use as a start point in ka_list_for_each_entry_continue().
 */
#define ka_list_prepare_entry(pos, head, member) list_prepare_entry(pos, head, member)

/**
 * continue iteration over list of given type
 * @pos: the type * to use as a loop cursor.
 * @head: the head for your list.
 * @member: the name of the list_head within the struct.
 * Continue to iterate over list of given type, continuing after the current position.
 */
#define ka_list_for_each_entry_continue(pos, head, member) list_for_each_entry_continue(pos, head, member)

/**
 * iterate backwards from the given point
 * @pos: the type * to use as a loop cursor.
 * @head: the head for your list.
 * @member: the name of the list_head within the struct.
 * Start to iterate over list of given type backwards, continuing after the current position.
 */
#define ka_list_for_each_entry_continue_reverse(pos, head, member) list_for_each_entry_continue_reverse(pos, head, member)

/**
 * iterate over list of given type from the current point
 * @pos: the type * to use as a loop cursor.
 * @head: the head for your list.
 * @member: the name of the list_head within the struct.
 * Iterate over list of given type, continuing from current position.
 */
#define ka_list_for_each_entry_from(pos, head, member) list_for_each_entry_from(pos, head, member)

/**
 * iterate backwards over list of given type from the current point
 * @pos: the type * to use as a loop cursor.
 * @head: the head for your list.
 * @member: the name of the list_head within the struct.
 * Iterate backwards over list of given type, continuing from current position.
 */
#define ka_list_for_each_entry_from_reverse(pos, head, member) list_for_each_entry_from_reverse(pos, head, member)

/**
 * iterate over list of given type safe against removal of list entry
 * @pos: the type * to use as a loop cursor
 * @n: another type * to use as temporary storage
 * @head: the head for your list
 * @member: the name of the list_head within the struct
 */
#define ka_list_for_each_entry_safe(pos, n, head, member) list_for_each_entry_safe(pos, n, head, member)

/**
 * continue list iteration safe against removal
 * @pos: the type * to use as a loop cursor
 * @n: another type * to use as temporary storage
 * @head: the head for the list
 * @member: the name of the list_head within the struct
 * Iterate over list of given type, continuing after current point, safe against removal of list entry.
 */
#define ka_list_for_each_entry_safe_continue(pos, n, head, member) list_for_each_entry_safe_continue(pos, n, head, member)

/**
 * iterate over list from current point safe against removal
 * @pos: the type * to use as a loop cursor
 * @n: another type * to use as temporary storage
 * @head: the head for the list
 * @member: the name of the list_head within the struct
 * Iterate over list of given type from current point, safe against removal of list entry.
 */
#define ka_list_for_each_entry_safe_from(pos, n, head, member) list_for_each_entry_safe_from(pos, n, head, member)

/**
 * iterate backwards over list safe against removal
 * @pos: the type * to use as a loop cursor
 * @n: another type * to use as temporary storage
 * @head: the head for the list
 * @member: the name of the list_head within the struct
 * Iterate backwards over list of given type, safe against removal of list entry.
 */
#define ka_list_for_each_entry_safe_reverse(pos, n, head, member) list_for_each_entry_safe_reverse(pos, n, head, member)

/**
 * reset a stale ka_list_for_each_entry_safe loop
 * @pos: the loop cursor used in the ka_list_for_each_entry_safe loop
 * @n: temporary storage used in ka_list_for_each_entry_safe
 * @member: the name of the list_head within the struct.
 * ka_list_safe_reset_next is not safe to use in general if the list may be
 * modified concurrently (eg. the lock is dropped in the loop body).
 * An exception to this is if the cursor element (pos) is pinned in the list,
 * and ka_list_safe_reset_next is called after re-taking the lock and before
 * completing the current iteration of the loop body.
 */
#define ka_list_safe_reset_next(pos, n, member) list_safe_reset_next(pos, n, member)


#define KA_HLIST_HEAD_INIT      HLIST_HEAD_INIT
#define KA_HLIST_HEAD(name)     HLIST_HEAD(name)
#define KA_INIT_HLIST_HEAD(ptr) INIT_HLIST_HEAD(ptr)
#define KA_INIT_HLIST_NODE(h)   INIT_HLIST_NODE(h)

/**
 * Check whether the node has been removed from list and reinitialized
 * @h: Node to be checked
 * Note that not all removal functions will leave a node in unhashed state.
 */
#define ka_hlist_unhashed(h) hlist_unhashed(h)

/**
 * Version of ka_hlist_unhashed for lockless use
 * @h: Node to be checked
 * This variant of ka_hlist_unhashed() must be used in lockless contexts
 * to avoid potential load-tearing.
 * The READ_ONCE() is paired with the various WRITE_ONCE() in hlist helpers that are defined below.
 */
#define ka_hlist_unhashed_lockless(h) hlist_unhashed_lockless(h)

// Check whether the specified ka_hlist_head structure is an empty hlist
#define ka_hlist_empty(h) hlist_empty(h)

/**
 * Delete the specified ka_hlist_node from its list
 * @n: Node to delete.
 * Note that this function leaves the node in hashed state.
 * Use ka_hlist_del_init() or similar instead to unhash @n.
 */
#define ka_hlist_del(n) hlist_del(n)

/**
 * Delete the specified ka_hlist_node from its list and initialize
 * @n: Node to delete.
 * Note that this function leaves the node in unhashed state.
 */
#define ka_hlist_del_init(n) hlist_del_init(n)

/**
 * add a new entry at the beginning of the hlist
 * @n: new entry to be added
 * @h: hlist head to add it after
 * Insert a new entry after the specified head.
 */
#define ka_hlist_add_head(n, h) hlist_add_head(n, h)

/**
 * add a new entry before the one specified
 * @n: new entry to be added
 * @next: hlist node to add it before, which must be non-NULL
 */
#define ka_hlist_add_before(n, next) hlist_add_before(n, next)

/**
 * add a new entry after the one specified
 * @n: new entry to be added
 * @prev: hlist node to add it after, which must be non-NULL
 */
#define ka_hlist_add_behind(n, prev) hlist_add_behind(n, prev)

/**
 * create a fake hlist consisting of a single headless node
 * @n: Node to make a fake list out of.
 * This makes @n appear to be its own predecessor on a headless hlist.
 * The point of this is to allow things like ka_hlist_del()
 * to work correctly in cases where there is no list.
 */
#define ka_hlist_add_fake(n) hlist_add_fake(n)

/**
 * Check whether this node is a fake hlist
 * @h: Node to check for being a self-referential fake hlist.
 */
#define ka_hlist_fake(h) hlist_fake(h)

/**
 * Check whether the node is the only element of the specified hlist
 * @n: Node to check for singularity.
 * @h: Header for potentially singular list.
 * Check whether the node is the only node of the head without accessing head,
 * thus avoiding unnecessary cache misses.
 */
#define ka_hlist_is_singular_node(n, h) hlist_is_singular_node(n, h)

// Move a list from one list head to another.
// Fix up the pprev reference of the first entry if it exists.
#define ka_hlist_move_list(old, entry) hlist_move_list(old, entry)

#define ka_hlist_entry(ptr, type, member) hlist_entry(ptr, type, member)

#define ka_hlist_for_each(pos, head) hlist_for_each(pos, head)

#define ka_hlist_for_each_safe(pos, n, head) hlist_for_each_safe(pos, n, head)

#define ka_hlist_entry_safe(ptr, type, member) hlist_entry_safe(ptr, type, member)

/**
 * iterate over list of given type
 * @pos: the type * to use as a loop cursor
 * @head: the head for the list
 * @member: the name of the ka_hlist_node within the struct
 */
#define ka_hlist_for_each_entry(pos, head, member) hlist_for_each_entry(pos, head, member)

/**
 * iterate over a hlist continuing after current point
 * @pos: the type * to use as a loop cursor
 * @member: the name of the ka_hlist_node within the struct
 */
#define ka_hlist_for_each_entry_continue(pos, member) hlist_for_each_entry_continue(pos, member)

/**
 * iterate over a hlist continuing from current point
 * @pos: the type * to use as a loop cursor
 * @member: the name of the ka_hlist_node within the struct
 */
#define ka_hlist_for_each_entry_from(pos, member) hlist_for_each_entry_from(pos, member)

/**
 * iterate over list of given type safe against removal of list entry
 * @pos: the type * to use as a loop cursor
 * @n: a &ka_hlist_node_t to use as temporary storage
 * @head: the head for the list
 * @member: the name of the ka_hlist_node within the struct
 */
#define ka_hlist_for_each_entry_safe(pos, n, head, member) hlist_for_each_entry_safe(pos, n, head, member)

typedef struct rcu_head ka_rcu_head_t;

#define KA_INIT_LIST_HEAD_RCU(list) INIT_LIST_HEAD_RCU(list)
#define ka_list_add_tail_rcu(entry, head) list_add_tail_rcu(entry, head)
#define ka_list_del_rcu(entry) list_del_rcu(entry)
#define ka_list_for_each_entry_rcu list_for_each_entry_rcu

#endif
