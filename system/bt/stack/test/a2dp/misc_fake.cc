/*
 * Copyright 2020 The Android Open Source Project
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

#include "service/common/bluetooth/a2dp_codec_config.h"
#include "stack/include/a2dp_vendor_ldac.h"

bluetooth::A2dpCodecConfig* bta_av_get_a2dp_current_codec(void) {
  return nullptr;
}

int A2DP_VendorGetTrackSampleRateLdac(const uint8_t* p_codec_info) { return 0; }
int A2DP_VendorGetTrackBitsPerSampleLdac(const uint8_t* p_codec_info) {
  return 0;
}
int A2DP_VendorGetChannelModeCodeLdac(const uint8_t* p_codec_info) { return 0; }
