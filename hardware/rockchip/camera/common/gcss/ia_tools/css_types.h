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

#ifndef __CSS_TYPES_H__
#define __CSS_TYPES_H__

#include <stdint.h>
#include <stdbool.h>

/*!
 * \brief Error codes.
 * \ingroup ia_tools
*/
typedef int32_t css_err_t;
#define css_err_none     0         /*!< No errors*/
#define css_err_general  (-(1 << 1))  /*!< General error*/
#define css_err_nomemory (-(1 << 2))  /*!< Out of memory*/
#define css_err_data     (-(1 << 3))  /*!< Corrupted data*/
#define css_err_internal (-(1 << 4))  /*!< Error in code*/
#define css_err_argument (-(1 << 5))  /*!< Invalid argument for a function*/
#define css_err_noentry  (-(1 << 6))  /*!< No such entry/entity/file */
#define css_err_timeout  (-(1 << 7))  /*!< Time out*/
#define css_err_end      (-(1 << 8))  /*!< End of values*/
#define css_err_full     (-(1 << 9))  /*!< Exchange full */
#define css_err_again    (-(1 << 10)) /*!< Operation requires additional call */
#define css_err_nimpl    (-(1 << 11)) /*!< Not implemented */

#endif /* _CSS_TYPES_H_ */
