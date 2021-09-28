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

#include <hardware/hardware.h>
#include <hardware/vibrator.h>
#include <log/log.h>
#include <utils/Trace.h>

#include <cinttypes>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof((x)) / sizeof((x)[0]))
#endif

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

static constexpr uint32_t BASE_CONTINUOUS_EFFECT_OFFSET = 32768;

static constexpr uint32_t WAVEFORM_EFFECT_0_20_LEVEL = 0;
static constexpr uint32_t WAVEFORM_EFFECT_1_00_LEVEL = 4;
static constexpr uint32_t WAVEFORM_EFFECT_LEVEL_MINIMUM = 4;

static constexpr uint32_t WAVEFORM_DOUBLE_CLICK_SILENCE_MS = 100;

static constexpr uint32_t WAVEFORM_LONG_VIBRATION_EFFECT_INDEX = 0;
static constexpr uint32_t WAVEFORM_LONG_VIBRATION_THRESHOLD_MS = 50;
static constexpr uint32_t WAVEFORM_SHORT_VIBRATION_EFFECT_INDEX = 3 + BASE_CONTINUOUS_EFFECT_OFFSET;

static constexpr uint32_t WAVEFORM_CLICK_INDEX = 2;
static constexpr uint32_t WAVEFORM_QUICK_RISE_INDEX = 6;
static constexpr uint32_t WAVEFORM_SLOW_RISE_INDEX = 7;
static constexpr uint32_t WAVEFORM_QUICK_FALL_INDEX = 8;
static constexpr uint32_t WAVEFORM_LIGHT_TICK_INDEX = 9;

static constexpr uint32_t WAVEFORM_TRIGGER_QUEUE_INDEX = 65534;

static constexpr uint32_t VOLTAGE_GLOBAL_SCALE_LEVEL = 5;
static constexpr uint8_t VOLTAGE_SCALE_MAX = 100;

static constexpr int8_t MAX_COLD_START_LATENCY_MS = 6;  // I2C Transaction + DSP Return-From-Standby
static constexpr int8_t MAX_PAUSE_TIMING_ERROR_MS = 1;  // ALERT Irq Handling
static constexpr uint32_t MAX_TIME_MS = UINT32_MAX;

static constexpr float AMP_ATTENUATE_STEP_SIZE = 0.125f;
static constexpr float EFFECT_FREQUENCY_KHZ = 48.0f;

static constexpr auto ASYNC_COMPLETION_TIMEOUT = std::chrono::milliseconds(100);

static constexpr int32_t COMPOSE_DELAY_MAX_MS = 10000;
static constexpr int32_t COMPOSE_SIZE_MAX = 127;

static uint8_t amplitudeToScale(float amplitude, float maximum) {
    return std::round((-20 * std::log10(amplitude / static_cast<float>(maximum))) /
                      (AMP_ATTENUATE_STEP_SIZE));
}

enum class AlwaysOnId : uint32_t {
    GPIO_RISE,
    GPIO_FALL,
};

Vibrator::Vibrator(std::unique_ptr<HwApi> hwapi, std::unique_ptr<HwCal> hwcal)
    : mHwApi(std::move(hwapi)), mHwCal(std::move(hwcal)), mAsyncHandle(std::async([] {})) {
    uint32_t caldata;
    uint32_t effectCount;
    std::array<uint32_t, 6> volLevels;

    if (!mHwApi->setState(true)) {
        ALOGE("Failed to set state (%d): %s", errno, strerror(errno));
    }

    if (mHwCal->getF0(&caldata)) {
        mHwApi->setF0(caldata);
    }
    if (mHwCal->getRedc(&caldata)) {
        mHwApi->setRedc(caldata);
    }
    if (mHwCal->getQ(&caldata)) {
        mHwApi->setQ(caldata);
    }

    mHwCal->getVolLevels(&volLevels);
    /*
     * Given voltage levels for two intensities, assuming a linear function,
     * solve for 'f(0)' in 'v = f(i) = a + b * i' (i.e 'v0 - (v1 - v0) / ((i1 - i0) / i0)').
     */
    mEffectVolMin = std::max(std::lround(volLevels[WAVEFORM_EFFECT_0_20_LEVEL] -
                                         (volLevels[WAVEFORM_EFFECT_1_00_LEVEL] -
                                          volLevels[WAVEFORM_EFFECT_0_20_LEVEL]) /
                                                 4.0f),
                             static_cast<long>(WAVEFORM_EFFECT_LEVEL_MINIMUM));
    mEffectVolMax = volLevels[WAVEFORM_EFFECT_1_00_LEVEL];
    mGlobalVolMax = volLevels[VOLTAGE_GLOBAL_SCALE_LEVEL];

    mHwApi->getEffectCount(&effectCount);
    mEffectDurations.resize(effectCount);
    for (size_t effectIndex = 0; effectIndex < effectCount; effectIndex++) {
        mHwApi->setEffectIndex(effectIndex);
        uint32_t effectDuration;
        if (mHwApi->getEffectDuration(&effectDuration)) {
            mEffectDurations[effectIndex] = std::ceil(effectDuration / EFFECT_FREQUENCY_KHZ);
        }
    }
}

ndk::ScopedAStatus Vibrator::getCapabilities(int32_t *_aidl_return) {
    ATRACE_NAME("Vibrator::getCapabilities");
    int32_t ret = IVibrator::CAP_ON_CALLBACK | IVibrator::CAP_PERFORM_CALLBACK |
                  IVibrator::CAP_COMPOSE_EFFECTS | IVibrator::CAP_ALWAYS_ON_CONTROL;
    if (mHwApi->hasEffectScale()) {
        ret |= IVibrator::CAP_AMPLITUDE_CONTROL;
    }
    if (mHwApi->hasAspEnable()) {
        ret |= IVibrator::CAP_EXTERNAL_CONTROL;
    }
    *_aidl_return = ret;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::off() {
    ATRACE_NAME("Vibrator::off");
    setGlobalAmplitude(false);
    if (!mHwApi->setActivate(0)) {
        ALOGE("Failed to turn vibrator off (%d): %s", errno, strerror(errno));
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::on(int32_t timeoutMs,
                                const std::shared_ptr<IVibratorCallback> &callback) {
    ATRACE_NAME("Vibrator::on");
    const uint32_t index = timeoutMs < WAVEFORM_LONG_VIBRATION_THRESHOLD_MS
                                   ? WAVEFORM_SHORT_VIBRATION_EFFECT_INDEX
                                   : WAVEFORM_LONG_VIBRATION_EFFECT_INDEX;
    if (MAX_COLD_START_LATENCY_MS <= UINT32_MAX - timeoutMs) {
        timeoutMs += MAX_COLD_START_LATENCY_MS;
    }
    setGlobalAmplitude(true);
    return on(timeoutMs, index, callback);
}

ndk::ScopedAStatus Vibrator::perform(Effect effect, EffectStrength strength,
                                     const std::shared_ptr<IVibratorCallback> &callback,
                                     int32_t *_aidl_return) {
    ATRACE_NAME("Vibrator::perform");
    return performEffect(effect, strength, callback, _aidl_return);
}

ndk::ScopedAStatus Vibrator::getSupportedEffects(std::vector<Effect> *_aidl_return) {
    *_aidl_return = {Effect::TEXTURE_TICK, Effect::TICK, Effect::CLICK, Effect::HEAVY_CLICK,
                     Effect::DOUBLE_CLICK};
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setAmplitude(float amplitude) {
    ATRACE_NAME("Vibrator::setAmplitude");
    if (amplitude <= 0.0f || amplitude > 1.0f) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    if (!isUnderExternalControl()) {
        return setEffectAmplitude(amplitude, 1.0);
    } else {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
}

ndk::ScopedAStatus Vibrator::setExternalControl(bool enabled) {
    ATRACE_NAME("Vibrator::setExternalControl");
    setGlobalAmplitude(enabled);

    if (!mHwApi->setAspEnable(enabled)) {
        ALOGE("Failed to set external control (%d): %s", errno, strerror(errno));
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getCompositionDelayMax(int32_t *maxDelayMs) {
    ATRACE_NAME("Vibrator::getCompositionDelayMax");
    *maxDelayMs = COMPOSE_DELAY_MAX_MS;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getCompositionSizeMax(int32_t *maxSize) {
    ATRACE_NAME("Vibrator::getCompositionSizeMax");
    *maxSize = COMPOSE_SIZE_MAX;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getSupportedPrimitives(std::vector<CompositePrimitive> *supported) {
    *supported = {
            CompositePrimitive::NOOP,       CompositePrimitive::CLICK,
            CompositePrimitive::QUICK_RISE, CompositePrimitive::SLOW_RISE,
            CompositePrimitive::QUICK_FALL, CompositePrimitive::LIGHT_TICK,
    };
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getPrimitiveDuration(CompositePrimitive primitive,
                                                  int32_t *durationMs) {
    ndk::ScopedAStatus status;
    uint32_t effectIndex;

    if (primitive != CompositePrimitive::NOOP) {
        status = getPrimitiveDetails(primitive, &effectIndex);
        if (!status.isOk()) {
            return status;
        }

        *durationMs = mEffectDurations[effectIndex];
    } else {
        *durationMs = 0;
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::compose(const std::vector<CompositeEffect> &composite,
                                     const std::shared_ptr<IVibratorCallback> &callback) {
    ATRACE_NAME("Vibrator::compose");
    std::ostringstream effectBuilder;
    std::string effectQueue;

    if (composite.size() > COMPOSE_SIZE_MAX) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    for (auto &e : composite) {
        if (e.scale < 0.0f || e.scale > 1.0f) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }

        if (e.delayMs) {
            if (e.delayMs > COMPOSE_DELAY_MAX_MS) {
                return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
            }
            effectBuilder << e.delayMs << ",";
        }
        if (e.primitive != CompositePrimitive::NOOP) {
            ndk::ScopedAStatus status;
            uint32_t effectIndex;

            status = getPrimitiveDetails(e.primitive, &effectIndex);
            if (!status.isOk()) {
                return status;
            }

            effectBuilder << effectIndex << "." << intensityToVolLevel(e.scale) << ",";
        }
    }

    if (effectBuilder.tellp() == 0) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    effectBuilder << 0;

    effectQueue = effectBuilder.str();

    return performEffect(0 /*ignored*/, 0 /*ignored*/, &effectQueue, callback);
}

ndk::ScopedAStatus Vibrator::on(uint32_t timeoutMs, uint32_t effectIndex,
                                const std::shared_ptr<IVibratorCallback> &callback) {
    if (mAsyncHandle.wait_for(ASYNC_COMPLETION_TIMEOUT) != std::future_status::ready) {
        ALOGE("Previous vibration pending.");
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    mHwApi->setEffectIndex(effectIndex);
    mHwApi->setDuration(timeoutMs);
    mHwApi->setActivate(1);

    mAsyncHandle = std::async(&Vibrator::waitForComplete, this, callback);

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setEffectAmplitude(float amplitude, float maximum) {
    int32_t scale = amplitudeToScale(amplitude, maximum);

    if (!mHwApi->setEffectScale(scale)) {
        ALOGE("Failed to set effect amplitude (%d): %s", errno, strerror(errno));
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setGlobalAmplitude(bool set) {
    uint8_t amplitude = set ? mGlobalVolMax : VOLTAGE_SCALE_MAX;
    int32_t scale = amplitudeToScale(amplitude, VOLTAGE_SCALE_MAX);

    if (!mHwApi->setGlobalScale(scale)) {
        ALOGE("Failed to set global amplitude (%d): %s", errno, strerror(errno));
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getSupportedAlwaysOnEffects(std::vector<Effect> *_aidl_return) {
    *_aidl_return = {Effect::TEXTURE_TICK, Effect::TICK, Effect::CLICK, Effect::HEAVY_CLICK};
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::alwaysOnEnable(int32_t id, Effect effect, EffectStrength strength) {
    ndk::ScopedAStatus status;
    uint32_t effectIndex;
    uint32_t timeMs;
    uint32_t volLevel;
    uint32_t scale;

    status = getSimpleDetails(effect, strength, &effectIndex, &timeMs, &volLevel);
    if (!status.isOk()) {
        return status;
    }

    scale = amplitudeToScale(volLevel, VOLTAGE_SCALE_MAX);

    switch (static_cast<AlwaysOnId>(id)) {
        case AlwaysOnId::GPIO_RISE:
            mHwApi->setGpioRiseIndex(effectIndex);
            mHwApi->setGpioRiseScale(scale);
            return ndk::ScopedAStatus::ok();
        case AlwaysOnId::GPIO_FALL:
            mHwApi->setGpioFallIndex(effectIndex);
            mHwApi->setGpioFallScale(scale);
            return ndk::ScopedAStatus::ok();
    }

    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus Vibrator::alwaysOnDisable(int32_t id) {
    switch (static_cast<AlwaysOnId>(id)) {
        case AlwaysOnId::GPIO_RISE:
            mHwApi->setGpioRiseIndex(0);
            return ndk::ScopedAStatus::ok();
        case AlwaysOnId::GPIO_FALL:
            mHwApi->setGpioFallIndex(0);
            return ndk::ScopedAStatus::ok();
    }

    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

bool Vibrator::isUnderExternalControl() {
    bool isAspEnabled;
    mHwApi->getAspEnable(&isAspEnabled);
    return isAspEnabled;
}

binder_status_t Vibrator::dump(int fd, const char **args, uint32_t numArgs) {
    if (fd < 0) {
        ALOGE("Called debug() with invalid fd.");
        return STATUS_OK;
    }

    (void)args;
    (void)numArgs;

    dprintf(fd, "AIDL:\n");

    dprintf(fd, "  Voltage Levels:\n");
    dprintf(fd, "    Effect Min: %" PRIu32 "\n", mEffectVolMin);
    dprintf(fd, "    Effect Max: %" PRIu32 "\n", mEffectVolMax);
    dprintf(fd, "    Global Max: %" PRIu32 "\n", mGlobalVolMax);

    dprintf(fd, "  Effect Durations:");
    for (auto d : mEffectDurations) {
        dprintf(fd, " %" PRIu32, d);
    }
    dprintf(fd, "\n");

    dprintf(fd, "\n");

    mHwApi->debug(fd);

    dprintf(fd, "\n");

    mHwCal->debug(fd);

    fsync(fd);
    return STATUS_OK;
}

ndk::ScopedAStatus Vibrator::getSimpleDetails(Effect effect, EffectStrength strength,
                                              uint32_t *outEffectIndex, uint32_t *outTimeMs,
                                              uint32_t *outVolLevel) {
    uint32_t effectIndex;
    uint32_t timeMs;
    float intensity;
    uint32_t volLevel;

    switch (strength) {
        case EffectStrength::LIGHT:
            intensity = 0.5f;
            break;
        case EffectStrength::MEDIUM:
            intensity = 0.7f;
            break;
        case EffectStrength::STRONG:
            intensity = 1.0f;
            break;
        default:
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    switch (effect) {
        case Effect::TEXTURE_TICK:
            effectIndex = WAVEFORM_LIGHT_TICK_INDEX;
            intensity *= 0.5f;
            break;
        case Effect::TICK:
            effectIndex = WAVEFORM_CLICK_INDEX;
            intensity *= 0.5f;
            break;
        case Effect::CLICK:
            effectIndex = WAVEFORM_CLICK_INDEX;
            intensity *= 0.7f;
            break;
        case Effect::HEAVY_CLICK:
            effectIndex = WAVEFORM_CLICK_INDEX;
            intensity *= 1.0f;
            break;
        default:
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    volLevel = intensityToVolLevel(intensity);
    timeMs = mEffectDurations[effectIndex] + MAX_COLD_START_LATENCY_MS;

    *outEffectIndex = effectIndex;
    *outTimeMs = timeMs;
    *outVolLevel = volLevel;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getCompoundDetails(Effect effect, EffectStrength strength,
                                                uint32_t *outTimeMs, uint32_t * /*outVolLevel*/,
                                                std::string *outEffectQueue) {
    ndk::ScopedAStatus status;
    uint32_t timeMs;
    std::ostringstream effectBuilder;
    uint32_t thisEffectIndex;
    uint32_t thisTimeMs;
    uint32_t thisVolLevel;

    switch (effect) {
        case Effect::DOUBLE_CLICK:
            timeMs = 0;

            status = getSimpleDetails(Effect::CLICK, strength, &thisEffectIndex, &thisTimeMs,
                                      &thisVolLevel);
            if (!status.isOk()) {
                return status;
            }
            effectBuilder << thisEffectIndex << "." << thisVolLevel;
            timeMs += thisTimeMs;

            effectBuilder << ",";

            effectBuilder << WAVEFORM_DOUBLE_CLICK_SILENCE_MS;
            timeMs += WAVEFORM_DOUBLE_CLICK_SILENCE_MS + MAX_PAUSE_TIMING_ERROR_MS;

            effectBuilder << ",";

            status = getSimpleDetails(Effect::HEAVY_CLICK, strength, &thisEffectIndex, &thisTimeMs,
                                      &thisVolLevel);
            if (!status.isOk()) {
                return status;
            }
            effectBuilder << thisEffectIndex << "." << thisVolLevel;
            timeMs += thisTimeMs;

            break;
        default:
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    *outTimeMs = timeMs;
    *outEffectQueue = effectBuilder.str();

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getPrimitiveDetails(CompositePrimitive primitive,
                                                 uint32_t *outEffectIndex) {
    uint32_t effectIndex;

    switch (primitive) {
        case CompositePrimitive::NOOP:
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        case CompositePrimitive::CLICK:
            effectIndex = WAVEFORM_CLICK_INDEX;
            break;
        case CompositePrimitive::QUICK_RISE:
            effectIndex = WAVEFORM_QUICK_RISE_INDEX;
            break;
        case CompositePrimitive::SLOW_RISE:
            effectIndex = WAVEFORM_SLOW_RISE_INDEX;
            break;
        case CompositePrimitive::QUICK_FALL:
            effectIndex = WAVEFORM_QUICK_FALL_INDEX;
            break;
        case CompositePrimitive::LIGHT_TICK:
            effectIndex = WAVEFORM_LIGHT_TICK_INDEX;
            break;
        default:
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    *outEffectIndex = effectIndex;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setEffectQueue(const std::string &effectQueue) {
    if (!mHwApi->setEffectQueue(effectQueue)) {
        ALOGE("Failed to write \"%s\" to effect queue (%d): %s", effectQueue.c_str(), errno,
              strerror(errno));
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::performEffect(Effect effect, EffectStrength strength,
                                           const std::shared_ptr<IVibratorCallback> &callback,
                                           int32_t *outTimeMs) {
    ndk::ScopedAStatus status;
    uint32_t effectIndex;
    uint32_t timeMs = 0;
    uint32_t volLevel;
    std::string effectQueue;

    switch (effect) {
        case Effect::TEXTURE_TICK:
            // fall-through
        case Effect::TICK:
            // fall-through
        case Effect::CLICK:
            // fall-through
        case Effect::HEAVY_CLICK:
            status = getSimpleDetails(effect, strength, &effectIndex, &timeMs, &volLevel);
            break;
        case Effect::DOUBLE_CLICK:
            status = getCompoundDetails(effect, strength, &timeMs, &volLevel, &effectQueue);
            break;
        default:
            status = ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
            break;
    }
    if (!status.isOk()) {
        goto exit;
    }

    status = performEffect(effectIndex, volLevel, &effectQueue, callback);

exit:

    *outTimeMs = timeMs;
    return status;
}

ndk::ScopedAStatus Vibrator::performEffect(uint32_t effectIndex, uint32_t volLevel,
                                           const std::string *effectQueue,
                                           const std::shared_ptr<IVibratorCallback> &callback) {
    if (effectQueue && !effectQueue->empty()) {
        ndk::ScopedAStatus status = setEffectQueue(*effectQueue);
        if (!status.isOk()) {
            return status;
        }
        setEffectAmplitude(VOLTAGE_SCALE_MAX, VOLTAGE_SCALE_MAX);
        effectIndex = WAVEFORM_TRIGGER_QUEUE_INDEX;
    } else {
        setEffectAmplitude(volLevel, VOLTAGE_SCALE_MAX);
    }

    return on(MAX_TIME_MS, effectIndex, callback);
}

void Vibrator::waitForComplete(std::shared_ptr<IVibratorCallback> &&callback) {
    mHwApi->pollVibeState(false);
    mHwApi->setActivate(false);

    if (callback) {
        auto ret = callback->onComplete();
        if (!ret.isOk()) {
            ALOGE("Failed completion callback: %d", ret.getExceptionCode());
        }
    }
}

uint32_t Vibrator::intensityToVolLevel(float intensity) {
    return std::lround(intensity * (mEffectVolMax - mEffectVolMin)) + mEffectVolMin;
}

}  // namespace vibrator
}  // namespace hardware
}  // namespace android
}  // namespace aidl
