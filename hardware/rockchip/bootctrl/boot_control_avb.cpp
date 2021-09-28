/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <errno.h>
#include <string.h>

#include <cutils/properties.h>
#include <hardware/boot_control.h>
#include <hardware/hardware.h>

#include <libavb_ab/libavb_ab.h>
#include <libavb_user/libavb_user.h>

#include <android-base/logging.h>
#include <bootloader_message/bootloader_message.h>
#include <android/hardware/boot/1.1/IBootControl.h>

#include "rk_boot_control.h"

#define LOG_TAG "Rockchip bootctrl"

#include <log/log.h>


using android::bootable::BootControl;

struct boot_control_private_t {
  // The base struct needs to be first in the list.
  boot_control_module_t base;

  BootControl impl;
};


static void module_init(boot_control_module_t* module) {
  auto& impl = reinterpret_cast<boot_control_private_t*>(module)->impl;

  setenv("ANDROID_LOG_TAGS", "*:v", 1);
  LOG(INFO) << "rk BootControl module_init ";
  impl.Init();
}

static unsigned int module_getNumberSlots(boot_control_module_t* module) {
  auto& impl = reinterpret_cast<boot_control_private_t*>(module)->impl;

  setenv("ANDROID_LOG_TAGS", "*:v", 1);
  LOG(INFO) << "rk BootControl module_getNumberSlots ";
  return impl.GetNumberSlots();;
}

static unsigned int module_getCurrentSlot(boot_control_module_t* module) {
  auto& impl = reinterpret_cast<boot_control_private_t*>(module)->impl;

  LOG(INFO) << "rk BootControl module_getCurrentSlot ";
  return impl.GetCurrentSlot();
}

static int module_markBootSuccessful(boot_control_module_t* module) {
  auto& impl = reinterpret_cast<boot_control_private_t*>(module)->impl;

  setenv("ANDROID_LOG_TAGS", "*:v", 1);
  LOG(INFO) << "rk BootControl module_markBootSuccessful ";
  return impl.MarkBootSuccessful();
}

static int module_setActiveBootSlot(boot_control_module_t* module,
                                    unsigned int slot) {
  auto& impl = reinterpret_cast<boot_control_private_t*>(module)->impl;

  LOG(INFO) << "rk BootControl module_setActiveBootSlot ";
  return impl.SetActiveBootSlot(slot);
}

static int module_setSlotAsUnbootable(struct boot_control_module* module,
                                      unsigned int slot) {
  auto& impl = reinterpret_cast<boot_control_private_t*>(module)->impl;

  LOG(INFO) << "rk BootControl module_setSlotAsUnbootable ";
  return impl.SetSlotAsUnbootable(slot);
}

static int module_isSlotBootable(struct boot_control_module* module,
                                 unsigned int slot) {
  auto& impl = reinterpret_cast<boot_control_private_t*>(module)->impl;

  LOG(INFO) << "rk BootControl module_isSlotBootable ";
  return impl.IsSlotBootable(slot);
}

static int module_isSlotMarkedSuccessful(struct boot_control_module* module,
                                         unsigned int slot) {
  auto& impl = reinterpret_cast<boot_control_private_t*>(module)->impl;

  LOG(INFO) << "rk BootControl module_isSlotMarkedSuccessful ";
  return impl.IsSlotMarkedSuccessful(slot);
}

static const char* module_getSuffix(boot_control_module_t* module,
                                    unsigned int slot) {
  auto& impl = reinterpret_cast<boot_control_private_t*>(module)->impl;

  LOG(INFO) << "rk BootControl module_getSuffix ";
  return impl.GetSuffix(slot);
}

static struct hw_module_methods_t module_methods = {
    .open = NULL,
};

boot_control_private_t HAL_MODULE_INFO_SYM = {
  .base =
      {
          .common =
              {
                  .tag = HARDWARE_MODULE_TAG,
                  .module_api_version = BOOT_CONTROL_MODULE_API_VERSION_0_1,
                  .hal_api_version = HARDWARE_HAL_API_VERSION,
                  .id = BOOT_CONTROL_HARDWARE_MODULE_ID,
                  .name = "AVB implementation of boot_control HAL",
                  .author = "The Android Open Source Project",
                  .methods = &module_methods,
              },
          .init = module_init,
          .getNumberSlots = module_getNumberSlots,
          .getCurrentSlot = module_getCurrentSlot,
          .markBootSuccessful = module_markBootSuccessful,
          .setActiveBootSlot = module_setActiveBootSlot,
          .setSlotAsUnbootable = module_setSlotAsUnbootable,
          .isSlotBootable = module_isSlotBootable,
          .getSuffix = module_getSuffix,
          .isSlotMarkedSuccessful = module_isSlotMarkedSuccessful,
      },
};

