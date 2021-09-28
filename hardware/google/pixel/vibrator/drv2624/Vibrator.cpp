/*
 * Copyright (C) 2017 The Android Open Source Project
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


#include "Vibrator.h"
#include "utils.h"

#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <hardware/vibrator.h>
#include <log/log.h>
#include <utils/Trace.h>

#include <cinttypes>
#include <cmath>
#include <fstream>
#include <iostream>

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

static constexpr int8_t MAX_RTP_INPUT = 127;
static constexpr int8_t MIN_RTP_INPUT = 0;

static constexpr char RTP_MODE[] = "rtp";
static constexpr char WAVEFORM_MODE[] = "waveform";

// Use effect #1 in the waveform library for CLICK effect
static constexpr char WAVEFORM_CLICK_EFFECT_SEQ[] = "1 0";

// Use effect #2 in the waveform library for TICK effect
static constexpr char WAVEFORM_TICK_EFFECT_SEQ[] = "2 0";

// Use effect #3 in the waveform library for DOUBLE_CLICK effect
static constexpr char WAVEFORM_DOUBLE_CLICK_EFFECT_SEQ[] = "3 0";

// Use effect #4 in the waveform library for HEAVY_CLICK effect
static constexpr char WAVEFORM_HEAVY_CLICK_EFFECT_SEQ[] = "4 0";

static std::uint32_t freqPeriodFormula(std::uint32_t in) {
    return 1000000000 / (24615 * in);
}

using utils::toUnderlying;

Vibrator::Vibrator(std::unique_ptr<HwApi> hwapi, std::unique_ptr<HwCal> hwcal)
    : mHwApi(std::move(hwapi)), mHwCal(std::move(hwcal)) {
    std::string autocal;
    uint32_t lraPeriod;
    bool dynamicConfig;

    if (!mHwApi->setState(true)) {
        ALOGE("Failed to set state (%d): %s", errno, strerror(errno));
    }

    if (mHwCal->getAutocal(&autocal)) {
        mHwApi->setAutocal(autocal);
    }
    mHwCal->getLraPeriod(&lraPeriod);

    mHwCal->getCloseLoopThreshold(&mCloseLoopThreshold);
    mHwCal->getDynamicConfig(&dynamicConfig);

    if (dynamicConfig) {
        uint32_t longFreqencyShift;
        uint32_t shortVoltageMax, longVoltageMax;

        mHwCal->getLongFrequencyShift(&longFreqencyShift);
        mHwCal->getShortVoltageMax(&shortVoltageMax);
        mHwCal->getLongVoltageMax(&longVoltageMax);

        mEffectConfig.reset(new VibrationConfig({
                .shape = WaveShape::SINE,
                .odClamp = shortVoltageMax,
                .olLraPeriod = lraPeriod,
        }));
        mSteadyConfig.reset(new VibrationConfig({
                .shape = WaveShape::SQUARE,
                .odClamp = longVoltageMax,
                // 1. Change long lra period to frequency
                // 2. Get frequency': subtract the frequency shift from the frequency
                // 3. Get final long lra period after put the frequency' to formula
                .olLraPeriod = freqPeriodFormula(freqPeriodFormula(lraPeriod) - longFreqencyShift),
        }));
    } else {
        mHwApi->setOlLraPeriod(lraPeriod);
    }

    mHwCal->getClickDuration(&mClickDuration);
    mHwCal->getTickDuration(&mTickDuration);
    mHwCal->getDoubleClickDuration(&mDoubleClickDuration);
    mHwCal->getHeavyClickDuration(&mHeavyClickDuration);

    // This enables effect #1 from the waveform library to be triggered by SLPI
    // while the AP is in suspend mode
    if (!mHwApi->setLpTriggerEffect(1)) {
        ALOGW("Failed to set LP trigger mode (%d): %s", errno, strerror(errno));
    }
}

ndk::ScopedAStatus Vibrator::getCapabilities(int32_t *_aidl_return) {
    ATRACE_NAME("Vibrator::getCapabilities");
    int32_t ret = 0;
    if (mHwApi->hasRtpInput()) {
        ret |= IVibrator::CAP_AMPLITUDE_CONTROL;
    }
    *_aidl_return = ret;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::on(uint32_t timeoutMs, const char mode[],
                                const std::unique_ptr<VibrationConfig> &config) {
    LoopControl loopMode = LoopControl::OPEN;

    // Open-loop mode is used for short click for over-drive
    // Close-loop mode is used for long notification for stability
    if (mode == RTP_MODE && timeoutMs > mCloseLoopThreshold) {
        loopMode = LoopControl::CLOSE;
    }

    mHwApi->setCtrlLoop(toUnderlying(loopMode));
    if (!mHwApi->setDuration(timeoutMs)) {
        ALOGE("Failed to set duration (%d): %s", errno, strerror(errno));
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    mHwApi->setMode(mode);
    if (config != nullptr) {
        mHwApi->setLraWaveShape(toUnderlying(config->shape));
        mHwApi->setOdClamp(config->odClamp);
        mHwApi->setOlLraPeriod(config->olLraPeriod);
    }

    if (!mHwApi->setActivate(1)) {
        ALOGE("Failed to activate (%d): %s", errno, strerror(errno));
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::on(int32_t timeoutMs,
                                const std::shared_ptr<IVibratorCallback> &callback) {
    ATRACE_NAME("Vibrator::on");
    if (callback) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    return on(timeoutMs, RTP_MODE, mSteadyConfig);
}

ndk::ScopedAStatus Vibrator::off() {
    ATRACE_NAME("Vibrator::off");
    if (!mHwApi->setActivate(0)) {
        ALOGE("Failed to turn vibrator off (%d): %s", errno, strerror(errno));
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setAmplitude(float amplitude) {
    ATRACE_NAME("Vibrator::setAmplitude");
    if (amplitude <= 0.0f || amplitude > 1.0f) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    int32_t rtp_input = std::round(amplitude * (MAX_RTP_INPUT - MIN_RTP_INPUT) + MIN_RTP_INPUT);

    if (!mHwApi->setRtpInput(rtp_input)) {
        ALOGE("Failed to set amplitude (%d): %s", errno, strerror(errno));
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setExternalControl(bool enabled) {
    ATRACE_NAME("Vibrator::setExternalControl");
    ALOGE("Not support in DRV2624 solution, %d", enabled);
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

binder_status_t Vibrator::dump(int fd, const char **args, uint32_t numArgs) {
    if (fd < 0) {
        ALOGE("Called debug() with invalid fd.");
        return STATUS_OK;
    }

    (void)args;
    (void)numArgs;

    dprintf(fd, "AIDL:\n");

    dprintf(fd, "  Close Loop Thresh: %" PRIu32 "\n", mCloseLoopThreshold);
    if (mSteadyConfig) {
        dprintf(fd, "  Steady Shape: %" PRIu32 "\n", mSteadyConfig->shape);
        dprintf(fd, "  Steady OD Clamp: %" PRIu32 "\n", mSteadyConfig->odClamp);
        dprintf(fd, "  Steady OL LRA Period: %" PRIu32 "\n", mSteadyConfig->olLraPeriod);
    }
    if (mEffectConfig) {
        dprintf(fd, "  Effect Shape: %" PRIu32 "\n", mEffectConfig->shape);
        dprintf(fd, "  Effect OD Clamp: %" PRIu32 "\n", mEffectConfig->odClamp);
        dprintf(fd, "  Effect OL LRA Period: %" PRIu32 "\n", mEffectConfig->olLraPeriod);
    }
    dprintf(fd, "  Click Duration: %" PRIu32 "\n", mClickDuration);
    dprintf(fd, "  Tick Duration: %" PRIu32 "\n", mTickDuration);
    dprintf(fd, "  Double Click Duration: %" PRIu32 "\n", mDoubleClickDuration);
    dprintf(fd, "  Heavy Click Duration: %" PRIu32 "\n", mHeavyClickDuration);

    dprintf(fd, "\n");

    mHwApi->debug(fd);

    dprintf(fd, "\n");

    mHwCal->debug(fd);

    fsync(fd);
    return STATUS_OK;
}

ndk::ScopedAStatus Vibrator::getSupportedEffects(std::vector<Effect> *_aidl_return) {
    *_aidl_return = {Effect::TEXTURE_TICK, Effect::TICK, Effect::CLICK, Effect::HEAVY_CLICK,
                     Effect::DOUBLE_CLICK};
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::perform(Effect effect, EffectStrength strength,
                                     const std::shared_ptr<IVibratorCallback> &callback,
                                     int32_t *_aidl_return) {
    ATRACE_NAME("Vibrator::perform");
    ndk::ScopedAStatus status;

    if (callback) {
        status = ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    } else {
        status = performEffect(effect, strength, _aidl_return);
    }

    return status;
}

static ndk::ScopedAStatus convertEffectStrength(EffectStrength strength, uint8_t *outScale) {
    uint8_t scale;

    switch (strength) {
        case EffectStrength::LIGHT:
            scale = 2;  // 50%
            break;
        case EffectStrength::MEDIUM:
        case EffectStrength::STRONG:
            scale = 0;  // 100%
            break;
        default:
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    *outScale = scale;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::performEffect(Effect effect, EffectStrength strength,
                                           int32_t *outTimeMs) {
    ndk::ScopedAStatus status;
    uint32_t timeMS = 0;
    uint8_t scale;

    switch (effect) {
        case Effect::TEXTURE_TICK:
            mHwApi->setSequencer(WAVEFORM_TICK_EFFECT_SEQ);
            timeMS = mTickDuration;
            break;
        case Effect::CLICK:
            mHwApi->setSequencer(WAVEFORM_CLICK_EFFECT_SEQ);
            timeMS = mClickDuration;
            break;
        case Effect::DOUBLE_CLICK:
            mHwApi->setSequencer(WAVEFORM_DOUBLE_CLICK_EFFECT_SEQ);
            timeMS = mDoubleClickDuration;
            break;
        case Effect::TICK:
            mHwApi->setSequencer(WAVEFORM_TICK_EFFECT_SEQ);
            timeMS = mTickDuration;
            break;
        case Effect::HEAVY_CLICK:
            mHwApi->setSequencer(WAVEFORM_HEAVY_CLICK_EFFECT_SEQ);
            timeMS = mHeavyClickDuration;
            break;
        default:
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    status = convertEffectStrength(strength, &scale);
    if (!status.isOk()) {
        return status;
    }

    mHwApi->setScale(scale);
    status = on(timeMS, WAVEFORM_MODE, mEffectConfig);
    if (!status.isOk()) {
        return status;
    }

    *outTimeMs = timeMS;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getSupportedAlwaysOnEffects(std::vector<Effect> * /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::alwaysOnEnable(int32_t /*id*/, Effect /*effect*/,
                                            EffectStrength /*strength*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::alwaysOnDisable(int32_t /*id*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getCompositionDelayMax(int32_t * /*maxDelayMs*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getCompositionSizeMax(int32_t * /*maxSize*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getSupportedPrimitives(
        std::vector<CompositePrimitive> * /*supported*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getPrimitiveDuration(CompositePrimitive /*primitive*/,
                                                  int32_t * /*durationMs*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::compose(const std::vector<CompositeEffect> & /*composite*/,
                                     const std::shared_ptr<IVibratorCallback> & /*callback*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

}  // namespace vibrator
}  // namespace hardware
}  // namespace android
}  // namespace aidl
