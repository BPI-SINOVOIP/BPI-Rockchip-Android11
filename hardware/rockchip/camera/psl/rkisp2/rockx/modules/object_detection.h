/****************************************************************************
*
*    Copyright (c) 2017 - 2019 by Rockchip Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Rockchip Corporation. This is proprietary information owned by
*    Rockchip Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Rockchip Corporation.
*
*****************************************************************************/

#ifndef _ROCKX_OBJECT_DETECTION_H
#define _ROCKX_OBJECT_DETECTION_H

#include "../rockx_type.h"

namespace android {
namespace camera2 {
namespace rkisp2 {

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Object Detection Labels Table (91 Classes)
 *
 *    "???", "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", \n
 *    "trafficlight", "firehydrant", "???", "stopsign", "parkingmeter", "bench", "bird", "cat", "dog", "horse", \n
 *    "sheep", "cow", "elephant", "bear", "zebra", "giraffe", "???", "backpack", "umbrella", "???", \n
 *    "???", "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard", "sportsball", "kite", "baseballbat", \n
 *    "baseballglove", "skateboard", "surfboard", "tennisracket", "bottle", "???", "wineglass", "cup", "fork", "knife", \n
 *    "spoon", "bowl", "banana", "apple", "sandwich", "orange", "broccoli", "carrot", "hotdog", "pizza", \n
 *    "donut", "cake", "chair", "couch", "pottedplant", "bed", "???", "diningtable", "???", "???", \n
 *    "toilet", "???", "tv", "laptop", "mouse", "remote", "keyboard", "cellphone", "microwave", "oven", \n
 *    "toaster", "sink", "refrigerator", "???", "book", "clock", "vase", "scissors", "teddybear", "hairdrier", \n
 *    "toothbrush" \n
 *
 */
extern const char* const OBJECT_DETECTION_LABELS_91[91];

/**
 * @brief Object Detection Result(include Face, CarPlate, Head, Object, etc...)
 */
typedef struct rockx_object_t {
    int id;                 ///< Track id
    int cls_idx;            ///< Class index
    rockx_rect_t box;       ///< Object Region
    float score;            ///< Object confidence score
} rockx_object_t;

/**
 * @brief rockx_object_color_attribute_category
 */
typedef enum {
    COAT_COLOR = 0,     ///< coat color
    TROUSERS_COLOR,     ///< trousers color
    CAR_COLOR           ///< car color
} rockx_object_color_attribute_category;

/**
 * @brief rockx_object_color_attribute
 */
typedef enum{
    BLACK = 0,
    WHITE,
    RED,
    GRAY,
    PURPLE,
    YELLOW,
    BLUE,
    GREEN
}rockx_object_color_attribute;

/**
 * @brief rockx_color_attribute_t
 */
typedef struct rockx_color_attribute_t {
    rockx_object_color_attribute_category category;
    int result_idx;
    float attribute_score;
} rockx_color_attribute_t;

/**
 * @brief rockx_color_attribute_array_t
 */
typedef struct rockx_color_attribute_array_t {
    int count;                  ///< Array Count(0 <= count < 20)
    rockx_color_attribute_t color_attribute[20];
} rockx_color_attribute_array_t;

/**
 * @brief Object Array Result
 */
typedef struct rockx_object_array_t {
    int count;                  ///< Array Count(0 <= count < 128)
    rockx_object_t object[128]; ///< Objects
} rockx_object_array_t;

/**
 * Object Detection (91 Class)
 * @param handle [in] Handle of a created ROCKX_MODULE_OBJECT_DETECTION module(created by @ref rockx_create)
 * @param in_img [in] Input image
 * @param object_array [out] Detection Result
 * @param callback [in] Async callback function pointer
 * @return @ref rockx_ret_t
 */
rockx_ret_t rockx_object_detect(rockx_handle_t handle, rockx_image_t *in_img, rockx_object_array_t *object_array,
                              rockx_async_callback *callback);

/**
 * Head Detection
 * @param handle [in] Handle of a created ROCKX_MODULE_HEAD_DETECTION module(created by @ref rockx_create)
 * @param in_img [in] Input image
 * @param object_array [out] Detection Result
 * @param callback [in] Async callback function pointer
 * @return @ref rockx_ret_t
 */
rockx_ret_t rockx_head_detect(rockx_handle_t handle, rockx_image_t *in_img, rockx_object_array_t *object_array,
                              rockx_async_callback *callback);


/**
 * Person Detection
 * @param handle [in] Handle of a created ROCKX_MODULE_BODY_DETECTION module(created by @ref rockx_create)
 * @param in_img [in] Input image
 * @param object_array [out] Detection Result
 * @param callback [in] Async callback function pointer
 * @return @ref rockx_ret_t
 */
rockx_ret_t rockx_person_detect(rockx_handle_t handle, rockx_image_t *in_img, rockx_object_array_t *object_array,
                              rockx_async_callback *callback);

/**
 * Person Detection (input RGB and IR image)
 * @param handle [in] Handle of a created ROCKX_MODULE_BODY_DETECTION module(created by @ref rockx_create)
 * @param in_img [in] Input image
 * @param mode [in] 0 for RGB, 1 for IR
 * @param object_array [out] Detection Result
 * @param callback [in] Async callback function pointer
 * @return @ref rockx_ret_t
 */
rockx_ret_t rockx_person_detect2(rockx_handle_t handle, rockx_image_t *in_img, int mode, rockx_object_array_t *object_array,
                              rockx_async_callback *callback);

/**
 * IPC Object Detection
 * @param handle [in] Handle of a created ROCKX_MODULE_OBJECT_DETECTION_IPC module(created by @ref rockx_create), model input width=640, height=384
 * @param in_img [in] Input image
 * @param object_array [out] Detection Result, cls_idx 0: person, 1: face£¬2: car
 * @param callback [in] Async callback function pointer
 * @return @ref rockx_ret_t
 */
rockx_ret_t rockx_object_detect_ipc(rockx_handle_t handle, rockx_image_t *in_img, rockx_object_array_t *object_array,
                              rockx_async_callback *callback);

/**
 * Object Attribute analysize
 * @param handle [in] Handle of a created ROCKX_MODULE_OBJECT_ATTRIBUTE module(created by @ref rockx_create)
 * @param in_img [in] Input image
 * @param object_array [out] Detection Result, cls_idx 0: person, 1: face
 * @param callback [in] Async callback function pointer
 * @return @ref rockx_ret_t
 */

rockx_ret_t rockx_object_attribute_color(rockx_handle_t handle, rockx_image_t *in_img, rockx_object_t *in_object, rockx_color_attribute_array_t *color_attribute_array);

#ifdef __cplusplus
} //extern "C"
#endif

} /* namespace rkisp2 */
} /* namespace camera2 */
} /* namespace android */

#endif // _ROCKX_OBJECT_DETECTION_H