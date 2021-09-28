## 7.9\. Virtual Reality

Android includes APIs and facilities to build "Virtual Reality" (VR)
applications including high quality mobile VR experiences. Device
implementations MUST properly implement these APIs and behaviors,
as detailed in this section.

### 7.9.1\. Virtual Reality Mode

Android includes support for [VR Mode](
https://developer.android.com/reference/android/app/Activity.html#setVrModeEnabled%28boolean, android.content.ComponentName%29),
a feature which handles stereoscopic rendering of notifications and disables
monocular system UI components while a VR application has user focus.

### 7.9.2\. Virtual Reality Mode - High Performance

If device implementations support VR mode, they:

*   [C-1-1] MUST have at least 2 physical cores.
*   [C-1-2] MUST declare the `android.hardware.vr.high_performance` feature.
*   [C-1-3] MUST support sustained performance mode.
*   [C-1-4] MUST support OpenGL ES 3.2.
*   [C-1-5] MUST support `android.hardware.vulkan.level` 0.
*   SHOULD support `android.hardware.vulkan.level` 1 or higher.
*   [C-1-6] MUST implement
    [`EGL_KHR_mutable_render_buffer`](https://www.khronos.org/registry/EGL/extensions/KHR/EGL_KHR_mutable_render_buffer.txt),
    [`EGL_ANDROID_front_buffer_auto_refresh`](https://www.khronos.org/registry/EGL/extensions/ANDROID/EGL_ANDROID_front_buffer_auto_refresh.txt),
    [`EGL_ANDROID_get_native_client_buffer`](https://www.khronos.org/registry/EGL/extensions/ANDROID/EGL_ANDROID_get_native_client_buffer.txt),
    [`EGL_KHR_fence_sync`](https://www.khronos.org/registry/EGL/extensions/KHR/EGL_KHR_fence_sync.txt),
    [`EGL_KHR_wait_sync`](https://www.khronos.org/registry/EGL/extensions/KHR/EGL_KHR_wait_sync.txt),
    [`EGL_IMG_context_priority`](https://www.khronos.org/registry/EGL/extensions/IMG/EGL_IMG_context_priority.txt),
    [`EGL_EXT_protected_content`](https://www.khronos.org/registry/EGL/extensions/EXT/EGL_EXT_protected_content.txt),
    [`EGL_EXT_image_gl_colorspace`](https://www.khronos.org/registry/EGL/extensions/EXT/EGL_EXT_image_gl_colorspace.txt),
    and expose the extensions in the list of available EGL extensions.
*   [C-1-8] MUST implement
    [`GL_EXT_multisampled_render_to_texture2`](https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_multisampled_render_to_texture2.txt),
    [`GL_OVR_multiview`](https://www.khronos.org/registry/OpenGL/extensions/OVR/OVR_multiview.txt),
    [`GL_OVR_multiview2`](https://www.khronos.org/registry/OpenGL/extensions/OVR/OVR_multiview2.txt),
    [`GL_OVR_multiview_multisampled_render_to_texture`](https://www.khronos.org/registry/OpenGL/extensions/OVR/OVR_multiview_multisampled_render_to_texture.txt),
    [`GL_EXT_protected_textures`](https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_protected_textures.txt),
    and expose the extensions in the list of available GL extensions.
*   [C-SR] Are STRONGLY RECOMMENDED to implement
    [`GL_EXT_external_buffer`](https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_external_buffer.txt),
    [`GL_EXT_EGL_image_array`](https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_EGL_image_array.txt),
    and expose the extensions in the list of available GL extensions.
*   [C-SR] Are STRONGLY RECOMMENDED to support Vulkan 1.1.
*   [C-SR] Are STRONGLY RECOMMENDED to implement
    [`VK_ANDROID_external_memory_android_hardware_buffer`](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VK_ANDROID_external_memory_android_hardware_buffer),
    [`VK_GOOGLE_display_timing`](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VK_GOOGLE_display_timing),
    [`VK_KHR_shared_presentable_image`](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VK_KHR_shared_presentable_image),
    and expose it in the list of available Vulkan extensions.
*   [C-SR] Are STRONGLY RECOMMENDED to expose at least one Vulkan queue family where `flags`
    contain both `VK_QUEUE_GRAPHICS_BIT` and `VK_QUEUE_COMPUTE_BIT`,
    and `queueCount` is at least 2.
*   [C-1-7] The GPU and display MUST be able to synchronize access to the shared
    front buffer such that alternating-eye rendering of VR content at 60fps with two
    render contexts will be displayed with no visible tearing artifacts.
*   [C-1-9] MUST implement support for [`AHardwareBuffer`](https://developer.android.com/ndk/reference/hardware__buffer_8h.html)
    flags `AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER`,
    `AHARDWAREBUFFER_USAGE_SENSOR_DIRECT_DATA` and
    `AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT`
    as described in the NDK.
*   [C-1-10] MUST implement support for `AHardwareBuffer`s with any
    combination of the usage flags
    `AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT`,
    `AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE`,
    `AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT`
    for at least the following formats:
    `AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM`,
    `AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM`,
    `AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM`,
    `AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT`.
*   [C-SR] Are STRONGLY RECOMMENDED to support the allocation of `AHardwareBuffer`s
    with more than one layer and flags and formats specified in C-1-10.
*   [C-1-11] MUST support H.264 decoding at least 3840 x 2160 at 30fps,
    compressed to an average of 40Mbps (equivalent to 4 instances of
    1920 x1080 at 30 fps-10 Mbps or 2 instances of 1920 x 1080 at 60 fps-20 Mbps).
*   [C-1-12] MUST support HEVC and VP9, MUST be capable of decoding at least
    1920 x 1080 at 30 fps compressed to an average of 10 Mbps and SHOULD be
    capable of decoding 3840 x 2160 at 30 fps-20 Mbps (equivalent to
    4 instances of 1920 x 1080 at 30 fps-5 Mbps).
*   [C-1-13] MUST support `HardwarePropertiesManager.getDeviceTemperatures` API
    and return accurate values for skin temperature.
*   [C-1-14] MUST have an embedded screen, and its resolution MUST be at least
    1920 x 1080.
*   [C-SR] Are STRONGLY RECOMMENDED to have a display resolution of at least
    2560 x 1440.
*   [C-1-15] The display MUST update at least 60 Hz while in VR Mode.
*   [C-1-17] The display MUST support a low-persistence mode with ≤ 5 milliseconds
    persistence, persistence being defined as the amount of time for
    which a pixel is emitting light.
*   [C-1-18] MUST support Bluetooth 4.2 and Bluetooth LE Data Length Extension
    [section 7.4.3](#7_4_3_bluetooth).
*   [C-1-19] MUST support and properly report
    [Direct Channel Type](https://developer.android.com/reference/android/hardware/Sensor#isDirectChannelTypeSupported%28int%29)
    for all of the following default sensor types:
      * `TYPE_ACCELEROMETER`
      * `TYPE_ACCELEROMETER_UNCALIBRATED`
      * `TYPE_GYROSCOPE`
      * `TYPE_GYROSCOPE_UNCALIBRATED`
      * `TYPE_MAGNETIC_FIELD`
      * `TYPE_MAGNETIC_FIELD_UNCALIBRATED`
*   [C-SR] Are STRONGLY RECOMMENDED to support the
    [`TYPE_HARDWARE_BUFFER`](https://developer.android.com/reference/android/hardware/SensorDirectChannel.html#TYPE_HARDWARE_BUFFER)
    direct channel type for all Direct Channel Types listed above.
*   [C-1-21] MUST meet the gyroscope, accelerometer, and magnetometer related
    requirements for `android.hardware.hifi_sensors`, as specified in
    [section 7.3.9](#7_3_9_high_fidelity_sensors).
*   [C-SR] Are STRONGLY RECOMMENDED to support the
    `android.hardware.sensor.hifi_sensors` feature.
*   [C-1-22] MUST have end-to-end motion to photon latency not higher than
    28 milliseconds.
*   [C-SR] Are STRONGLY RECOMMENDED to have end-to-end motion to photon latency
    not higher than 20 milliseconds.
*   [C-1-23] MUST have first-frame ratio, which is the ratio between the
    brightness of pixels on the first frame after a transition from black to
    white and the brightness of white pixels in steady state, of at least 85%.
*   [C-SR] Are STRONGLY RECOMMENDED to have first-frame ratio of at least 90%.
*   MAY provide an exclusive core to the foreground
    application and MAY support the `Process.getExclusiveCores` API to return
    the numbers of the cpu cores that are exclusive to the top foreground
    application.

If exclusive core is supported, then the core:

*   [C-2-1] MUST not allow any other userspace processes to run on it
(except device drivers used by the application), but MAY allow some kernel
processes to run as necessary.
