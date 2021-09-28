/*
 * Copyright (C) 2013-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _IA_LIST_H_
#define _IA_LIST_H_

#include <stdbool.h>
#include <ia_tools/css_types.h>

/**
 * \ingroup ia_tools
 */
typedef struct ia_list {
    void *data;
    struct ia_list *next;
} ia_list_t;

/**
 * Create a ia_list
 * \ingroup ia_tools
 * \return pointer to the list. nullptr if failed.
 */
ia_list_t *
ia_list_create(void);

/**
 * Destroy a ia_list
 * \ingroup ia_tools
 * \param *list the list
 */
void
ia_list_destroy(ia_list_t *list);

/**
 * Add a new element to a list head. If *list is nullptr, a new ia_list element is
 * allocated and inserted to *list.
 * \ingroup ia_tools
 * \param **list a pointer to the list pointer
 * \param *data the data for the list element
 * \return css_err_none on success
 * Add data to a ia_list
 */
css_err_t
ia_list_prepend(ia_list_t **list, void *data);

/**
 * Add a new element to a list tail. If *list is nullptr, a new ia_list element is
 * allocated and inserted to *list.
 * \ingroup ia_tools
 * \param **list a pointer to the list pointer
 * \param *data the data for the list element
 * \return css_err_none on success
 * Add data to a ia_list
 */
css_err_t
ia_list_append(ia_list_t **list, void *data);

/**
 * Tell whether data is in the list
 * \ingroup ia_tools
 * \param *list handle to the list object
 * \param *data the data pointer
 * \return true if data is in the list, false otherwise
 */
bool
ia_list_contains(const ia_list_t *list, void *data);

/**
 * Remove data if it exists in the list.
 * \ingroup ia_tools
 * \param **list a pointer to the list pointer.
 * \param *data the data to remove from list.
 * \return true if data was removed. False otherwise.
 */
bool
ia_list_remove(ia_list_t **list, void *data);

/**
 * Tell the length of the list.
 * \ingroup ia_tools
 * \param *list a pointer to the list.
 * \return length of the list.
 */
uint32_t
ia_list_length(const ia_list_t *list);

/**
 * Return data at an index of the list.
 * \ingroup ia_tools
 * \param *list a pointer to the list.
 * \param index element index
 * \return pointer to the data at the element at the distance "index" from the
 * given list element
 */
void*
ia_list_data_at(const ia_list_t *list, uint32_t index);

#endif /* _IA_LIST_H_ */
