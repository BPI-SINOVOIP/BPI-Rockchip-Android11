/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of The Linux Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _WIFIHAL_LIST_H_
#define _WIFIHAL_LIST_H_

struct list_head {
  struct list_head *next, *prev;
};

void INITIALISE_LIST(struct list_head *list);
void list_add(struct list_head *latest, struct list_head *prev,
                  struct list_head *next);
void add_to_list(struct list_head *latest, struct list_head *head);
void list_add_tail(struct list_head *latest, struct list_head *head);
void list_del(struct list_head *prev, struct list_head *next);
void del_from_list(struct list_head *record);
void replace_in_list(struct list_head *old, struct list_head *latest);

#define list_for_each(ref, head) \
        for (ref = (head)->next; ref->next, ref != (head); ref = ref->next)

#define offset_of(type, member) ((char *)&((type *)0)->member)

#define container_of(ptr, type, member) ({ \
        const typeof(((type *)0)->member) *__mptr = (ptr); \
        (type *)((char *)__mptr - offset_of(type, member)); })

#define list_entry(ptr, type, member) \
        container_of(ptr, type, member)

#define list_for_each_entry(ref, head, member) \
        for (ref = list_entry((head)->next, typeof(*ref), member); \
             ref->member.next, &ref->member != (head); \
             ref = list_entry(ref->member.next, typeof(*ref), member))

#define list_for_each_entry_safe(pos, n, head, member) \
        for (pos = list_entry((head)->next, typeof(*pos), member), \
             n = list_entry(pos->member.next, typeof(*pos), member); \
             &pos->member != (head); \
             pos = n, n = list_entry(n->member.next, typeof(*n), member))

#define list_for_each_safe(pos, n, head) \
        for (pos = (head)->next, n = pos->next; pos != (head); \
             pos = n, n = pos->next)

#endif
