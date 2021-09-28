/* * Copyright (C) 2019 The Android Open Source Project *
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

#include "benchmark/benchmark.h"

#include <android-base/file.h>
#include <cutils/fs.h>

#include "Hardware.h"
#include "Vibrator.h"

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

class VibratorBench : public benchmark::Fixture {
  private:
    static constexpr const char *FILE_NAMES[]{
            "device/f0_stored",
            "device/redc_stored",
            "device/q_stored",
            "activate",
            "duration",
            "state",
            "device/cp_trigger_duration",
            "device/cp_trigger_index",
            "device/cp_trigger_queue",
            "device/cp_dig_scale",
            "device/dig_scale",
            "device/asp_enable",
            "device/gpio1_fall_index",
            "device/gpio1_fall_dig_scale",
            "device/gpio1_rise_index",
            "device/gpio1_rise_dig_scale",
            "device/vibe_state",
            "device/num_waves",
    };

  public:
    void SetUp(::benchmark::State & /*state*/) override {
        auto prefix = std::filesystem::path(mFilesDir.path) / "";
        const std::map<const std::string, const std::string> content{
                {"duration", std::to_string((uint32_t)std::rand() ?: 1)},
                {"device/asp_enable", std::to_string(0)},
                {"device/cp_trigger_duration", std::to_string(0)},
                {"device/num_waves", std::to_string(10)},
                {"device/vibe_state", std::to_string(0)},
        };

        setenv("HWAPI_PATH_PREFIX", prefix.c_str(), true);

        for (auto n : FILE_NAMES) {
            const auto it = content.find(n);
            const auto name = std::filesystem::path(n);
            const auto path = std::filesystem::path(mFilesDir.path) / name;

            fs_mkdirs(path.c_str(), S_IRWXU);

            if (it != content.end()) {
                std::ofstream{path} << it->second << std::endl;
            } else {
                symlink("/dev/null", path.c_str());
            }
        }

        mVibrator = ndk::SharedRefBase::make<Vibrator>(std::make_unique<HwApi>(),
                                                       std::make_unique<HwCal>());
    }

    static void DefaultArgs(benchmark::internal::Benchmark *b) { b->Unit(benchmark::kMicrosecond); }

    static void SupportedEffectArgs(benchmark::internal::Benchmark *b) {
        b->ArgNames({"Effect", "Strength"});
        for (Effect effect : ndk::enum_range<Effect>()) {
            for (EffectStrength strength : ndk::enum_range<EffectStrength>()) {
                b->Args({static_cast<long>(effect), static_cast<long>(strength)});
            }
        }
    }

  protected:
    TemporaryDir mFilesDir;
    std::shared_ptr<IVibrator> mVibrator;
};

#define BENCHMARK_WRAPPER(fixt, test, code) \
    BENCHMARK_DEFINE_F(fixt, test)          \
    /* NOLINTNEXTLINE */                    \
    (benchmark::State & state){code} BENCHMARK_REGISTER_F(fixt, test)->Apply(fixt::DefaultArgs)

BENCHMARK_WRAPPER(VibratorBench, on, {
    uint32_t duration = std::rand() ?: 1;

    for (auto _ : state) {
        mVibrator->on(duration, nullptr);
    }
});

BENCHMARK_WRAPPER(VibratorBench, off, {
    for (auto _ : state) {
        mVibrator->off();
    }
});

BENCHMARK_WRAPPER(VibratorBench, setAmplitude, {
    uint8_t amplitude = std::rand() ?: 1;

    for (auto _ : state) {
        mVibrator->setAmplitude(amplitude);
    }
});

BENCHMARK_WRAPPER(VibratorBench, setExternalControl_enable, {
    for (auto _ : state) {
        mVibrator->setExternalControl(true);
    }
});

BENCHMARK_WRAPPER(VibratorBench, setExternalControl_disable, {
    for (auto _ : state) {
        mVibrator->setExternalControl(false);
    }
});

BENCHMARK_WRAPPER(VibratorBench, getCapabilities, {
    int32_t capabilities;

    for (auto _ : state) {
        mVibrator->getCapabilities(&capabilities);
    }
});

BENCHMARK_WRAPPER(VibratorBench, perform, {
    Effect effect = Effect(state.range(0));
    EffectStrength strength = EffectStrength(state.range(1));
    int32_t lengthMs;

    ndk::ScopedAStatus status = mVibrator->perform(effect, strength, nullptr, &lengthMs);

    if (!status.isOk()) {
        return;
    }

    for (auto _ : state) {
        mVibrator->perform(effect, strength, nullptr, &lengthMs);
    }
})->Apply(VibratorBench::SupportedEffectArgs);

}  // namespace vibrator
}  // namespace hardware
}  // namespace android
}  // namespace aidl

BENCHMARK_MAIN();
