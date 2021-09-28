/*
 * Copyright (C) 2012-2017 Intel Corporation
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

// System dependencies
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dlfcn.h>

#include <LogHelperAndroid.h>
#include <cutils/properties.h>
#include "LogHelper.h"

#ifdef RKCAMERA_REDEFINE_LOG
/*===========================================================================
 * DESCRIPTION: mm camera debug interface
 *
 *==========================================================================*/
pthread_mutex_t dbg_log_mutex;

static int         cam_soft_assert     = 0;
static FILE       *cam_log_fd          = NULL;
static const char *cam_log_filename    = "/data/misc/camera/cam_dbg_log_hal.txt";

#undef LOG_TAG
#define LOG_TAG "RkCamera"
#define CDBG_MAX_STR_LEN 1024
#define CDBG_MAX_LINE_LENGTH 256

/* current trace loggin permissions
   * {NONE, ERR, WARN, HIGH, DEBUG, LOW, INFO} */
int g_cam_log[CAM_LAST_MODULE][CAM_GLBL_DBG_LOW + 1] = {
    {0, 1, 1, 1, 0, 0, 0}, /* CAM_NO_MODULE     */
    {0, 1, 1, 1, 0, 0, 0}, /* CAM_HAL_MODULE    */
    {0, 1, 1, 1, 0, 0, 0}, /* CAM_JPEG_MODULE   */
};

/* string representation for logging level */
static const char *cam_dbg_level_to_str[] = {
     "",        /* CAM_GLBL_DBG_NONE  */
     "<ERROR>", /* CAM_GLBL_DBG_ERR   */
     "<WARN>", /* CAM_GLBL_DBG_WARN  */
     "<INFO>"  /* CAM_GLBL_DBG_INFO  */
     "<DBG>", /* CAM_GLBL_DBG_DEBUG */
     "<HIGH>", /* CAM_GLBL_DBG_HIGH  */
     "<LOW>", /* CAM_GLBL_DBG_LOW   */
};

/* current trace logging configuration */
typedef struct {
   cam_global_debug_level_t  level;
   int                       initialized;
   const char               *name;
   const char               *prop;
} module_debug_t;

static module_debug_t cam_loginfo[(int)CAM_LAST_MODULE] = {
  {CAM_GLBL_DBG_ERR, 1,
      "",         "persist.vendor.camera.global.debug"     }, /* CAM_NO_MODULE     */
  {CAM_GLBL_DBG_ERR, 1,
      "<HAL>", "persist.vendor.camera.hal.debug"        }, /* CAM_HAL_MODULE    */
  {CAM_GLBL_DBG_ERR, 1,
      "<JPEG>", "persist.vendor.camera.mmstill.logs"     }, /* CAM_JPEG_MODULE   */
};

/** cam_get_dbg_level
 *
 *    @module: module name
 *    @level:  module debug logging level
 *
 *  Maps debug log string to value.
 *
 *  Return: logging level
 **/
__unused
static cam_global_debug_level_t cam_get_dbg_level(const char *module,
  char *pValue) {

  cam_global_debug_level_t rc = CAM_GLBL_DBG_NONE;

  if (!strcmp(pValue, "none")) {
    rc = CAM_GLBL_DBG_NONE;
  } else if (!strcmp(pValue, "warn")) {
    rc = CAM_GLBL_DBG_WARN;
  } else if (!strcmp(pValue, "debug")) {
    rc = CAM_GLBL_DBG_DEBUG;
  } else if (!strcmp(pValue, "error")) {
    rc = CAM_GLBL_DBG_ERR;
  } else if (!strcmp(pValue, "low")) {
    rc = CAM_GLBL_DBG_LOW;
  } else if (!strcmp(pValue, "high")) {
    rc = CAM_GLBL_DBG_HIGH;
  } else if (!strcmp(pValue, "info")) {
    rc = CAM_GLBL_DBG_INFO;
  } else {
    ALOGE("Invalid %s debug log level %s\n", module, pValue);
  }

  ALOGD("%s debug log level: %s\n", module, cam_dbg_level_to_str[rc]);

  return rc;
}

/** cam_vsnprintf
 *    @pdst:   destination buffer pointer
 *    @size:   size of destination b uffer
 *    @pfmt:   string format
 *    @argptr: variabkle length argument list
 *
 *  Processes variable length argument list to a formatted string.
 *
 *  Return: n/a
 **/
static void cam_vsnprintf(char* pdst, unsigned int size,
                          const char* pfmt, va_list argptr) {
  int num_chars_written = 0;

  pdst[0] = '\0';
  num_chars_written = vsnprintf(pdst, size, pfmt, argptr);

  if ((num_chars_written >= (int)size) && (size > 0)) {
     /* Message length exceeds the buffer limit size */
     num_chars_written = size - 1;
     pdst[size - 1] = '\0';
  }
}

/** rk_camera_debug_log
 *    @module: origin or log message
 *    @level:  logging level
 *    @fmt:    log message formatting string
 *    @...:    variable argument list
 *
 *  Generig logger method.
 *
 *  Return: N/A
 **/
void rk_camera_debug_log(const cam_modules_t module,
                   const cam_global_debug_level_t level,
                   const char *tag, const char *fmt, ...) {
  char    str_buffer[CDBG_MAX_STR_LEN];
  va_list args;

  va_start(args, fmt);
  cam_vsnprintf(str_buffer, CDBG_MAX_STR_LEN, fmt, args);
  va_end(args);

  switch (level) {
  case CAM_GLBL_DBG_WARN:
    ALOGW("%s %s: %s", cam_loginfo[module].name,
          tag, str_buffer);
    break;
  case CAM_GLBL_DBG_ERR:
    ALOGE("%s %s: %s", cam_loginfo[module].name,
          tag, str_buffer);
    break;
  case CAM_GLBL_DBG_INFO:
    ALOGI("%s %s: %s", cam_loginfo[module].name,
          tag, str_buffer);
    break;
  case CAM_GLBL_DBG_HIGH:
  case CAM_GLBL_DBG_DEBUG:
  case CAM_GLBL_DBG_LOW:
  default:
    ALOGD("%s %s: %s", cam_loginfo[module].name,
          tag, str_buffer);
  }


  if (cam_log_fd != NULL) {
    char new_str_buffer[CDBG_MAX_STR_LEN];
    pthread_mutex_lock(&dbg_log_mutex);

    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);

    struct tm *now;
    now = gmtime((time_t *)&tv.tv_sec);
    snprintf(new_str_buffer, CDBG_MAX_STR_LEN, "%2d %02d:%02d:%02d.%03ld %d:%d Camera%s%s:%s",
              now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec, tv.tv_usec, getpid(),gettid(),
              cam_dbg_level_to_str[level], cam_loginfo[module].name,
              str_buffer);

    fprintf(cam_log_fd, "%s", new_str_buffer);
    pthread_mutex_unlock(&dbg_log_mutex);
  }

}

 /** rk_camera_set_dbg_log_properties
 *
 *  Set global and module log level properties.
 *
 *  Return: N/A
 **/
void rk_camera_set_dbg_log_properties(void) {
  int          i;
  unsigned int j;
  char         property_value[PROPERTY_VALUE_MAX] = {0};
  char         default_value[PROPERTY_VALUE_MAX]  = {0};

  /* set global and individual module logging levels */
  pthread_mutex_lock(&dbg_log_mutex);
  for (i = CAM_NO_MODULE; i < CAM_LAST_MODULE; i++) {
    cam_global_debug_level_t log_level;
    snprintf(default_value, PROPERTY_VALUE_MAX, "%d", (int)cam_loginfo[i].level);
    property_get(cam_loginfo[i].prop, property_value, default_value);
    log_level = (cam_global_debug_level_t)atoi(property_value);

    /* fix KW warnings */
    if (log_level > CAM_GLBL_DBG_LOW) {
       log_level = CAM_GLBL_DBG_LOW;
    }

    cam_loginfo[i].level = log_level;

    /* The logging macros will produce a log message when logging level for
     * a module is less or equal to the level specified in the property for
     * the module, or less or equal the level specified by the global logging
     * property. Currently we don't allow INFO logging to be turned off */
    for (j = CAM_GLBL_DBG_ERR; j <= CAM_GLBL_DBG_LOW; j++) {
      g_cam_log[i][j] = (cam_loginfo[CAM_NO_MODULE].level != CAM_GLBL_DBG_NONE)     &&
                        (cam_loginfo[i].level             != CAM_GLBL_DBG_NONE)     &&
                        ((j                                <= cam_loginfo[i].level) ||
                         (j                                <= cam_loginfo[CAM_NO_MODULE].level));
    }
  }
  pthread_mutex_unlock(&dbg_log_mutex);
}

/** rk_camera_debug_open
 *
 * Open log file if it is enabled
 *
 *  Return: N/A
 **/
void rk_camera_debug_open(void) {
  char property_value[PROPERTY_VALUE_MAX] = {0};

  pthread_mutex_init(&dbg_log_mutex, 0);
  rk_camera_set_dbg_log_properties();

  /* configure asserts */
  property_get("persist.vendor.camera.debug.assert", property_value, "0");
  cam_soft_assert = atoi(property_value);

  android::camera2::LogHelper::setDebugLevel();
  /* open default log file according to property setting */
  if (cam_log_fd == NULL) {
    property_get("persist.vendor.camera.debug.logfile", property_value, "0");
    if (atoi(property_value)) {
      /* we always put the current process id at end of log file name */
      char pid_str[255] = {0};
      char new_log_file_name[1024] = {0};

      snprintf(pid_str, 255, "_%d", getpid());
      strlcpy(new_log_file_name, cam_log_filename, sizeof(new_log_file_name));
      strlcat(new_log_file_name, pid_str, sizeof(new_log_file_name));

      cam_log_fd = fopen(new_log_file_name, "a");
      if (cam_log_fd == NULL) {
        ALOGE("Failed to create debug log file %s\n",
            new_log_file_name);
      } else {
        ALOGD("Debug log file %s open\n", new_log_file_name);
      }
    } else {
      property_set("persist.vendor.camera.debug.logfile", "0");
      ALOGD("Debug log file is not enabled");
      return;
    }
  }
}

/** cam_debug_close
 *
 *  Release logging resources.
 *
 *  Return: N/A
 **/
void rk_camera_debug_close(void) {

  if (cam_log_fd != NULL) {
    fclose(cam_log_fd);
    cam_log_fd = NULL;
  }

  pthread_mutex_destroy(&dbg_log_mutex);
}
#endif
