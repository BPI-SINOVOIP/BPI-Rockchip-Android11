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

#include <stdlib.h>
#include "list.h"

void INITIALISE_LIST(struct list_head *list)
{
  list->next = list;
  list->prev = list;
}

void list_add(struct list_head *latest,
                struct list_head *prev, struct list_head *next)
{
  next->prev = latest;
  latest->next = next;
  latest->prev = prev;
  prev->next = latest;
}

void add_to_list(struct list_head *latest, struct list_head *head)
{
  list_add(latest, head, head->next);
}

void list_add_tail(struct list_head *latest, struct list_head *head)
{
  list_add(latest, head->prev, head);
}

void list_del(struct list_head *prev, struct list_head *next)
{
  next->prev = prev;
  prev->next = next;
}

void del_from_list(struct list_head *record)
{
  list_del(record->prev, record->next);
  record->next = NULL;
  record->prev = NULL;
}

void replace_in_list(struct list_head *old, struct list_head *latest)
{
  latest->next = old->next;
  latest->next->prev = latest;
  latest->prev = old->prev;
  latest->prev->next = latest;
}
