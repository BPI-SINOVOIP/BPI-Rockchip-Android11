/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "draw_fn.h"

#include <jni.h>
#include <private/hwui/WebViewFunctor.h>
#include <utils/Log.h>

#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))
#define COMPILE_ASSERT(expr, err) \
__unused static const char (err)[(expr) ? 1 : -1] = "";

namespace android {
namespace {

struct SupportData {
  void* const data;
  AwDrawFnFunctorCallbacks callbacks;
};

void onSync(int functor, void* data,
            const uirenderer::WebViewSyncData& syncData) {
  AwDrawFn_OnSyncParams params = {
      .version = kAwDrawFnVersion,
      .apply_force_dark = syncData.applyForceDark,
  };
  SupportData* support = static_cast<SupportData*>(data);
  support->callbacks.on_sync(functor, support->data, &params);
}

void onContextDestroyed(int functor, void* data) {
  SupportData* support = static_cast<SupportData*>(data);
  support->callbacks.on_context_destroyed(functor, support->data);
}

void onDestroyed(int functor, void* data) {
  SupportData* support = static_cast<SupportData*>(data);
  support->callbacks.on_destroyed(functor, support->data);
  delete support;
}

void draw_gl(int functor, void* data,
             const uirenderer::DrawGlInfo& draw_gl_params) {
  float gabcdef[7];
  draw_gl_params.color_space_ptr->transferFn(gabcdef);
  AwDrawFn_DrawGLParams params = {
      .version = kAwDrawFnVersion,
      .clip_left = draw_gl_params.clipLeft,
      .clip_top = draw_gl_params.clipTop,
      .clip_right = draw_gl_params.clipRight,
      .clip_bottom = draw_gl_params.clipBottom,
      .width = draw_gl_params.width,
      .height = draw_gl_params.height,
      .deprecated_0 = false,
      .transfer_function_g = gabcdef[0],
      .transfer_function_a = gabcdef[1],
      .transfer_function_b = gabcdef[2],
      .transfer_function_c = gabcdef[3],
      .transfer_function_d = gabcdef[4],
      .transfer_function_e = gabcdef[5],
      .transfer_function_f = gabcdef[6],
  };
  COMPILE_ASSERT(NELEM(params.transform) == NELEM(draw_gl_params.transform),
                 mismatched_transform_matrix_sizes);
  for (int i = 0; i < NELEM(params.transform); ++i) {
    params.transform[i] = draw_gl_params.transform[i];
  }
  COMPILE_ASSERT(sizeof(params.color_space_toXYZD50) == sizeof(skcms_Matrix3x3),
                 gamut_transform_size_mismatch);
  draw_gl_params.color_space_ptr->toXYZD50(
      reinterpret_cast<skcms_Matrix3x3*>(&params.color_space_toXYZD50));

  SupportData* support = static_cast<SupportData*>(data);
  support->callbacks.draw_gl(functor, support->data, &params);
}

void initializeVk(int functor, void* data,
                  const uirenderer::VkFunctorInitParams& init_vk_params) {
  SupportData* support = static_cast<SupportData*>(data);
  VkPhysicalDeviceFeatures2 device_features_2;
  if (init_vk_params.device_features_2)
    device_features_2 = *init_vk_params.device_features_2;

  AwDrawFn_InitVkParams params{
      .version = kAwDrawFnVersion,
      .instance = init_vk_params.instance,
      .physical_device = init_vk_params.physical_device,
      .device = init_vk_params.device,
      .queue = init_vk_params.queue,
      .graphics_queue_index = init_vk_params.graphics_queue_index,
      .api_version = init_vk_params.api_version,
      .enabled_instance_extension_names =
          init_vk_params.enabled_instance_extension_names,
      .enabled_instance_extension_names_length =
          init_vk_params.enabled_instance_extension_names_length,
      .enabled_device_extension_names =
          init_vk_params.enabled_device_extension_names,
      .enabled_device_extension_names_length =
          init_vk_params.enabled_device_extension_names_length,
      .device_features = nullptr,
      .device_features_2 =
          init_vk_params.device_features_2 ? &device_features_2 : nullptr,
  };
  support->callbacks.init_vk(functor, support->data, &params);
}

void drawVk(int functor, void* data, const uirenderer::VkFunctorDrawParams& draw_vk_params) {
  SupportData* support = static_cast<SupportData*>(data);
  float gabcdef[7];
  draw_vk_params.color_space_ptr->transferFn(gabcdef);
  AwDrawFn_DrawVkParams params{
      .version = kAwDrawFnVersion,
      .width = draw_vk_params.width,
      .height = draw_vk_params.height,
      .deprecated_0 = false,
      .secondary_command_buffer = draw_vk_params.secondary_command_buffer,
      .color_attachment_index = draw_vk_params.color_attachment_index,
      .compatible_render_pass = draw_vk_params.compatible_render_pass,
      .format = draw_vk_params.format,
      .transfer_function_g = gabcdef[0],
      .transfer_function_a = gabcdef[1],
      .transfer_function_b = gabcdef[2],
      .transfer_function_c = gabcdef[3],
      .transfer_function_d = gabcdef[4],
      .transfer_function_e = gabcdef[5],
      .transfer_function_f = gabcdef[6],
      .clip_left = draw_vk_params.clip_left,
      .clip_top = draw_vk_params.clip_top,
      .clip_right = draw_vk_params.clip_right,
      .clip_bottom = draw_vk_params.clip_bottom,
  };
  COMPILE_ASSERT(sizeof(params.color_space_toXYZD50) == sizeof(skcms_Matrix3x3),
                 gamut_transform_size_mismatch);
  draw_vk_params.color_space_ptr->toXYZD50(
      reinterpret_cast<skcms_Matrix3x3*>(&params.color_space_toXYZD50));
  COMPILE_ASSERT(NELEM(params.transform) == NELEM(draw_vk_params.transform),
                 mismatched_transform_matrix_sizes);
  for (int i = 0; i < NELEM(params.transform); ++i) {
    params.transform[i] = draw_vk_params.transform[i];
  }
  support->callbacks.draw_vk(functor, support->data, &params);
}

void postDrawVk(int functor, void* data) {
  SupportData* support = static_cast<SupportData*>(data);
  AwDrawFn_PostDrawVkParams params{.version = kAwDrawFnVersion};
  support->callbacks.post_draw_vk(functor, support->data, &params);
}

int CreateFunctor(void* data, AwDrawFnFunctorCallbacks* functor_callbacks) {
  static bool callbacks_initialized = false;
  static uirenderer::WebViewFunctorCallbacks webview_functor_callbacks = {
      .onSync = &onSync,
      .onContextDestroyed = &onContextDestroyed,
      .onDestroyed = &onDestroyed,
  };
  if (!callbacks_initialized) {
    switch (uirenderer::WebViewFunctor_queryPlatformRenderMode()) {
      case uirenderer::RenderMode::OpenGL_ES:
        webview_functor_callbacks.gles.draw = &draw_gl;
        break;
      case uirenderer::RenderMode::Vulkan:
        webview_functor_callbacks.vk.initialize = &initializeVk;
        webview_functor_callbacks.vk.draw = &drawVk;
        webview_functor_callbacks.vk.postDraw = &postDrawVk;
        break;
    }
    callbacks_initialized = true;
  }
  SupportData* support = new SupportData{
      .data = data,
      .callbacks = *functor_callbacks,
  };
  int functor = uirenderer::WebViewFunctor_create(
      support, webview_functor_callbacks,
      uirenderer::WebViewFunctor_queryPlatformRenderMode());
  if (functor <= 0) delete support;
  return functor;
}

void ReleaseFunctor(int functor) {
  uirenderer::WebViewFunctor_release(functor);
}

AwDrawFnRenderMode QueryRenderMode(void) {
  switch (uirenderer::WebViewFunctor_queryPlatformRenderMode()) {
    case uirenderer::RenderMode::OpenGL_ES:
      return AW_DRAW_FN_RENDER_MODE_OPENGL_ES;
    case uirenderer::RenderMode::Vulkan:
      return AW_DRAW_FN_RENDER_MODE_VULKAN;
  }
}

jlong GetDrawFnFunctionTable() {
  static AwDrawFnFunctionTable function_table = {
    .version = kAwDrawFnVersion,
    .query_render_mode = &QueryRenderMode,
    .create_functor = &CreateFunctor,
    .release_functor = &ReleaseFunctor,
  };
  return reinterpret_cast<intptr_t>(&function_table);
}

const char kClassName[] = "com/android/webview/chromium/DrawFunctor";
const JNINativeMethod kJniMethods[] = {
    {"nativeGetFunctionTable", "()J",
     reinterpret_cast<void*>(GetDrawFnFunctionTable)},
};

}  // namespace

void RegisterDrawFunctor(JNIEnv* env) {
  jclass clazz = env->FindClass(kClassName);
  LOG_ALWAYS_FATAL_IF(!clazz, "Unable to find class '%s'", kClassName);

  int res = env->RegisterNatives(clazz, kJniMethods, NELEM(kJniMethods));
  LOG_ALWAYS_FATAL_IF(res < 0, "register native methods failed: res=%d", res);
}

}  // namespace android
