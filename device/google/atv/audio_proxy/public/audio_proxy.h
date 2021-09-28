// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef DEVICE_GOOGLE_ATV_AUDIO_PROXY_PUBLIC_AUDIO_PROXY_H_
#define DEVICE_GOOGLE_ATV_AUDIO_PROXY_PUBLIC_AUDIO_PROXY_H_

#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// audio proxy allows the application to implement an audio HAL. It contains two
// components, a client library and a service.
// The client library is defined by this header file. Applications should
// integrate this library to provide audio HAL components. Currently it's only
// IStreamOut.
// The service implements IDevicesFactory and IDevice. It will register itself
// to audio server and forward function calls to client.

// Most of the struct/functions just converts the HIDL definitions into C
// definitions.

// The following enum and typedef are subset of those defined in
// hardware/interfaces/audio/common/$VERSION/types.hal, or
// hardware/interfaces/audio/$VERSION/types.hal.
// The selected subsets are those commonly supported by a normal audio HAL. The
// library won't check the validation of these enums. In other words, Audio
// server can still pass value not defined here to the application.

// AudioFormat
enum {
  AUDIO_PROXY_FORMAT_INVALID = 0xFFFFFFFFu,
  AUDIO_PROXY_FORMAT_PCM_16_BIT = 0x1u,
  AUDIO_PROXY_FORMAT_PCM_8_BIT = 0x2u,
  AUDIO_PROXY_FORMAT_PCM_FLOAT = 0x5u,
};
typedef uint32_t audio_proxy_format_t;

// AudioChannelMask
enum {
  AUDIO_PROXY_CHANNEL_INVALID = 0xC0000000u,
  AUDIO_PROXY_CHANNEL_OUT_MONO = 0x1u,
  AUDIO_PROXY_CHANNEL_OUT_STEREO = 0x3u,
};
typedef uint32_t audio_proxy_channel_mask_t;

// AudioDrain
enum {
  AUDIO_PROXY_DRAIN_ALL,
  AUDIO_PROXY_DRAIN_EARLY_NOTIFY,
};
typedef int32_t audio_proxy_drain_type_t;

// AudioOutputFlag
enum {
  AUDIO_PROXY_OUTPUT_FLAG_NONE = 0x0,
  AUDIO_PROXY_OUTPUT_FLAG_DIRECT = 0x1,
};
typedef int32_t audio_proxy_output_flags_t;

// AudioConfig
typedef struct {
  uint32_t sample_rate;
  audio_proxy_channel_mask_t channel_mask;
  audio_proxy_format_t format;
  uint32_t frame_count;

  // Points to extra fields defined in the future versions.
  void* extension;
} audio_proxy_config_t;

// Util structure for key value pair.
typedef struct {
  const char* key;
  const char* val;
} audio_proxy_key_val_t;

typedef void (*audio_proxy_get_parameters_callback_t)(
    void*, const audio_proxy_key_val_t*);

// The following struct/functions mirror those definitions in audio HAL. They
// should have the same requirement as audio HAL interfaces, unless specified.

// IStreamOut.
struct audio_proxy_stream_out {
  size_t (*get_buffer_size)(const struct audio_proxy_stream_out* stream);
  uint64_t (*get_frame_count)(const struct audio_proxy_stream_out* stream);

  // Gets all the sample rate supported by the stream. The list is terminated
  // by 0. The returned list should have the same life cycle of |stream|.
  const uint32_t* (*get_supported_sample_rates)(
      const struct audio_proxy_stream_out* stream, audio_proxy_format_t format);
  uint32_t (*get_sample_rate)(const struct audio_proxy_stream_out* stream);

  // optional.
  int (*set_sample_rate)(struct audio_proxy_stream_out* stream, uint32_t rate);

  // Gets all the channel mask supported by the stream. The list is terminated
  // by AUDIO_PROXY_CHANNEL_INVALID. The returned list should have the same life
  // cycle of |stream|.
  const audio_proxy_channel_mask_t* (*get_supported_channel_masks)(
      const struct audio_proxy_stream_out* stream, audio_proxy_format_t format);
  audio_proxy_channel_mask_t (*get_channel_mask)(
      const struct audio_proxy_stream_out* stream);

  // optional.
  int (*set_channel_mask)(struct audio_proxy_stream_out* stream,
                          audio_proxy_channel_mask_t mask);

  // Gets all the audio formats supported by the stream. The list is terminated
  // by AUDIO_PROXY_FORMAT_INVALID. The returned list should have the same life
  // cycle of |stream|.
  const audio_proxy_format_t* (*get_supported_formats)(
      const struct audio_proxy_stream_out* stream);
  audio_proxy_format_t (*get_format)(
      const struct audio_proxy_stream_out* stream);

  // optional.
  int (*set_format)(struct audio_proxy_stream_out* stream,
                    audio_proxy_format_t format);

  uint32_t (*get_latency)(const struct audio_proxy_stream_out* stream);

  int (*standby)(struct audio_proxy_stream_out* stream);

  int (*pause)(struct audio_proxy_stream_out* stream);
  int (*resume)(struct audio_proxy_stream_out* stream);

  // optional.
  int (*drain)(struct audio_proxy_stream_out* stream,
               audio_proxy_drain_type_t type);

  int (*flush)(struct audio_proxy_stream_out* stream);

  // Writes |buffer| into |stream|. This is called on an internal thread of this
  // library.
  ssize_t (*write)(struct audio_proxy_stream_out* self, const void* buffer,
                   size_t bytes);

  // optional.
  int (*get_render_position)(const struct audio_proxy_stream_out* stream,
                             uint32_t* dsp_frames);

  // optional.
  int (*get_next_write_timestamp)(const struct audio_proxy_stream_out* stream,
                                  int64_t* timestamp);

  int (*get_presentation_position)(const struct audio_proxy_stream_out* stream,
                                   uint64_t* frames,
                                   struct timespec* timestamp);

  // opional.
  int (*set_volume)(struct audio_proxy_stream_out* stream, float left,
                    float right);

  // Sets parameters on |stream|. Both |context| and |param| are terminated
  // by key_val_t whose key is null. They are only valid during the function
  // call.
  int (*set_parameters)(struct audio_proxy_stream_out* stream,
                        const audio_proxy_key_val_t* context,
                        const audio_proxy_key_val_t* param);

  // Gets parameters from |stream|.
  // |context| is key val pairs array terminated by null key
  // audio_proxy_key_val_t. |keys| is C string array, terminated by nullptr.
  // |on_result| is the callback to deliver the result. It must be called before
  // this function returns, with |obj| as the first argument, and the list of
  // caller owned list of key value pairs as the second argument.
  // |obj| opaque object. Implementation should not touch it.
  void (*get_parameters)(const struct audio_proxy_stream_out* stream,
                         const audio_proxy_key_val_t* context,
                         const char** keys,
                         audio_proxy_get_parameters_callback_t on_result,
                         void* obj);

  // optional.
  int (*dump)(const struct audio_proxy_stream_out* stream, int fd);

  // Pointer to the next version structure, for compatibility.
  void* extension;
};

typedef struct audio_proxy_stream_out audio_proxy_stream_out_t;

// Represents an audio HAL bus device.
struct audio_proxy_device {
  // Returns the unique address of this device.
  const char* (*get_address)(struct audio_proxy_device* device);

  // Similar to IDevice::openOutputStream.
  int (*open_output_stream)(struct audio_proxy_device* device,
                            audio_proxy_output_flags_t flags,
                            audio_proxy_config_t* config,
                            audio_proxy_stream_out_t** stream_out);

  // Close |stream|. No more methods will be called on |stream| after this.
  void (*close_output_stream)(struct audio_proxy_device* device,
                              struct audio_proxy_stream_out* stream);

  // Points to next version's struct. Implementation should set this field to
  // null if next version struct is not available.
  // This allows library to work with applications integrated with older version
  // header.
  void* extension;
};

typedef struct audio_proxy_device audio_proxy_device_t;

// Provides |device| to the library. It returns 0 on success. This function is
// supposed to be called once per process.
// The service behind this library will register a new audio HAL to the audio
// server, on the first call to the service.
int audio_proxy_register_device(audio_proxy_device_t* device);

#ifdef __cplusplus
}
#endif

#endif  // DEVICE_GOOGLE_ATV_AUDIO_PROXY_PUBLIC_AUDIO_PROXY_H_
