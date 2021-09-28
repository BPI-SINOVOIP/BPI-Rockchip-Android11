/*
 * Copyright © 2015 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#ifndef IGT_LIST_H
#define IGT_LIST_H

#include <stdbool.h>
#include <stddef.h>

/*
 * This list data structure is a verbatim copy from wayland-util.h from the
 * Wayland project; except that wl_ prefix has been removed.
 */

struct igt_list {
	struct igt_list *prev;
	struct igt_list *next;
};

#define __IGT_INIT_LIST(name) { &(name), &(name) }
#define IGT_LIST(name) struct igt_list name = __IGT_INIT_LIST(name)

static inline void igt_list_init(struct igt_list *list)
{
	list->prev = list;
	list->next = list;
}

static inline void __igt_list_add(struct igt_list *list,
				  struct igt_list *prev,
				  struct igt_list *next)
{
	next->prev = list;
	list->next = next;
	list->prev = prev;
	prev->next = list;
}

static inline void igt_list_add(struct igt_list *elm, struct igt_list *list)
{
	__igt_list_add(elm, list, list->next);
}

static inline void igt_list_add_tail(struct igt_list *elm,
				     struct igt_list *list)
{
	__igt_list_add(elm, list->prev, list);
}

static inline void __igt_list_del(struct igt_list *prev, struct igt_list *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void igt_list_del(struct igt_list *elm)
{
	__igt_list_del(elm->prev, elm->next);
}

static inline void igt_list_move(struct igt_list *elm, struct igt_list *list)
{
	igt_list_del(elm);
	igt_list_add(elm, list);
}

static inline void igt_list_move_tail(struct igt_list *elm,
				      struct igt_list *list)
{
	igt_list_del(elm);
	igt_list_add_tail(elm, list);
}

static inline bool igt_list_empty(const struct igt_list *list)
{
	return list->next == list;
}

#define container_of(ptr, sample, member)				\
	(typeof(sample))((char *)(ptr) - offsetof(typeof(*sample), member))

#define igt_list_first_entry(head, pos, member) \
	container_of((head)->next, (pos), member)
#define igt_list_last_entry(head, pos, member) \
	container_of((head)->prev, (pos), member)

#define igt_list_next_entry(pos, member) \
	container_of((pos)->member.next, (pos), member)
#define igt_list_prev_entry(pos, member) \
	container_of((pos)->member.prev, (pos), member)

#define igt_list_for_each(pos, head, member)				\
	for (pos = igt_list_first_entry(head, pos, member);		\
	     &pos->member != (head);					\
	     pos = igt_list_next_entry(pos, member))

#define igt_list_for_each_reverse(pos, head, member)			\
	for (pos = igt_list_last_entry(head, pos, member);		\
	     &pos->member != (head);					\
	     pos = igt_list_prev_entry(pos, member))

#define igt_list_for_each_safe(pos, tmp, head, member)			\
	for (pos = igt_list_first_entry(head, pos, member),		\
	     tmp = igt_list_next_entry(pos, member);			\
	     &pos->member != (head);					\
	     pos = tmp, tmp = igt_list_next_entry(pos, member))

#endif /* IGT_LIST_H */
