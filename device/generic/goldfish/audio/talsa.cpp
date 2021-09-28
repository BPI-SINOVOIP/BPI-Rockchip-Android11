/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <log/log.h>
#include "talsa.h"
#include "debug.h"

namespace android {
namespace hardware {
namespace audio {
namespace V6_0 {
namespace implementation {
namespace talsa {

void PcmDeleter::operator()(pcm_t *x) const {
    LOG_ALWAYS_FATAL_IF(pcm_close(x) != 0);
};

void MixerDeleter::operator()(struct mixer *x) const {
    mixer_close(x);
}

std::unique_ptr<pcm_t, PcmDeleter> pcmOpen(const unsigned int dev,
                                           const unsigned int card,
                                           const unsigned int nChannels,
                                           const size_t sampleRateHz,
                                           const size_t frameCount,
                                           const bool isOut) {
    struct pcm_config pcm_config;
    memset(&pcm_config, 0, sizeof(pcm_config));

    pcm_config.channels = nChannels;
    pcm_config.rate = sampleRateHz;
    pcm_config.period_size = frameCount;     // Approx frames between interrupts
    pcm_config.period_count = 4;             // Approx interrupts per buffer
    pcm_config.format = PCM_FORMAT_S16_LE;
    pcm_config.start_threshold = 0;
    pcm_config.stop_threshold = isOut ? 0 : INT_MAX;

    PcmPtr pcm =
        PcmPtr(::pcm_open(dev, card,
                          (isOut ? PCM_OUT : PCM_IN) | PCM_MONOTONIC,
                           &pcm_config));
    if (::pcm_is_ready(pcm.get())) {
        return pcm;
    } else {
        ALOGE("%s:%d pcm_open failed for nChannels=%u sampleRateHz=%zu "
              "frameCount=%zu isOut=%d with %s", __func__, __LINE__,
              nChannels, sampleRateHz, frameCount, isOut,
              pcm_get_error(pcm.get()));
        return FAILURE(nullptr);
    }
}

MixerPtr mixerOpen(unsigned int card) {
    return MixerPtr(::mixer_open(card));
}

void mixerSetValueAll(mixer_ctl_t *ctl, int value) {
    const unsigned int n = mixer_ctl_get_num_values(ctl);
    for (unsigned int i = 0; i < n; i++) {
        mixer_ctl_set_value(ctl, i, value);
    }
}

void mixerSetPercentAll(mixer_ctl_t *ctl, int percent) {
    const unsigned int n = mixer_ctl_get_num_values(ctl);
    for (unsigned int i = 0; i < n; i++) {
        mixer_ctl_set_percent(ctl, i, percent);
    }
}


}  // namespace talsa
}  // namespace implementation
}  // namespace V6_0
}  // namespace audio
}  // namespace hardware
}  // namespace android
