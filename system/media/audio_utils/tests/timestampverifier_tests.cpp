/*
 * Copyright 2018 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "audio_utils_timestampverifier_tests"

#include <audio_utils/TimestampVerifier.h>

#include <stdio.h>
#include <gtest/gtest.h>

// Ensure that all TimestampVerifier mutators are really constexpr and free from
// nasty system calls (in case called from a SCHED_FIFO thread).
static constexpr auto makeVerifier(
        size_t N, uint32_t sampleRate, size_t errors, size_t discontinuities) {
    android::TimestampVerifier<int64_t, int64_t> tv;

    int64_t f = 0;
    int64_t t = 0;
    for (size_t i = 0; i < N; ++i) {
        tv.add(f, t, sampleRate);
        f += sampleRate;
        t += (int64_t)1e9;
    }
    for (size_t i = 0; i < discontinuities; ++i) {
        tv.discontinuity();
    }
    for (size_t i = 0; i < errors; ++i) {
        tv.error();
    }
    return tv;
}

TEST(TimestampVerifier, sanity)
{
    constexpr android::TimestampVerifier<int64_t, int64_t> tv;

    // The timestamp verifier must be embeddable in a memcpy structure just like pod.
    // We use is_trivially_copyable and is_trivially_destructible for this test.
    static_assert(std::is_trivially_copyable<decltype(tv)>::value,
        "TimestampVerifier must be trivially copyable");
    static_assert(std::is_trivially_destructible<decltype(tv)>::value,
        "TimestampVerifier must be trivially destructible");

    constexpr android::audio_utils::Statistics<double> s = tv.getJitterMs();

    EXPECT_EQ(std::numeric_limits<double>::infinity(), s.getMin());
    EXPECT_EQ(-std::numeric_limits<double>::infinity(), s.getMax());

    constexpr int64_t frames[] { 0, 48000 };
    constexpr int64_t timeNs[] { 0, 1000000000 };
    constexpr android::TimestampVerifier<int64_t, int64_t> tv2(frames, timeNs, 48000);
    EXPECT_EQ(0., tv2.getJitterMs().getMax());
    EXPECT_EQ(0., tv2.getJitterMs().getMin());
    EXPECT_EQ(0., tv2.getJitterMs().getMean());
    EXPECT_EQ(1, tv2.getJitterMs().getN());

    // We should get a perfect straight line estimate as there is no noise.
    double a, b, r2;
    tv2.estimateSampleRate(a, b, r2);
    EXPECT_EQ(0., a);
    EXPECT_EQ(48000., b);
    EXPECT_NEAR(1., r2, std::numeric_limits<double>::epsilon());

    constexpr android::TimestampVerifier<int64_t, int64_t> tv3 =
            makeVerifier(8 /* N */, 48000 /* sampleRate */, 10 /* errors */, 10 /* disc */);
    EXPECT_EQ(8, tv3.getN());
    EXPECT_EQ(10, tv3.getErrors());
    EXPECT_EQ(1, tv3.getDiscontinuities());  // consecutive discontinuities read as 1.
    EXPECT_EQ(0., tv3.getJitterMs().getMax());
    EXPECT_EQ(0., tv3.getJitterMs().getMin());
    EXPECT_EQ(0., tv3.getJitterMs().getMean());

    constexpr auto first = tv3.getFirstTimestamp();
    constexpr auto last = tv3.getLastTimestamp();

    EXPECT_EQ(0, first.mFrames);
    EXPECT_EQ(0, first.mTimeNs);
    EXPECT_EQ(48000 * (8 - 1), last.mFrames);
    EXPECT_EQ((int64_t)1e9 * (8 - 1), last.mTimeNs);
    EXPECT_EQ((uint32_t)48000, tv3.getSampleRate());
    EXPECT_EQ(0, tv3.getColds());

    tv3.estimateSampleRate(a, b, r2);
    EXPECT_EQ(0., a);
    EXPECT_EQ(48000., b);
    EXPECT_NEAR(1., r2, std::numeric_limits<double>::epsilon());
}

TEST(TimestampVerifier, discontinuity_zero)
{
    android::TimestampVerifier<int64_t, int64_t> tv;
    tv.setDiscontinuityMode(tv.DISCONTINUITY_MODE_ZERO);

    // Add timestamps advancing at normal rate over 2 seconds
    tv.add(0, 0, 48000);
    tv.add(48000, 1000000000, 48000);
    tv.add(96000, 2000000000, 48000);

    // Raise (mode zero) discontinuity at "3 seconds"
    tv.discontinuity();
    // Add timestamp where frame count has reset to zero (and not advancing)
    tv.add(0, 3000000000, 48000);

    // The last corrected timestamp after discontinuity (mode zero) should be zeroed
    EXPECT_EQ(0., tv.getLastCorrectedTimestamp().mFrames);
    EXPECT_EQ(3000000000., tv.getLastCorrectedTimestamp().mTimeNs);

    // Add timestamp where frame count has not advanced from zero, but time has advanced 100 ms more
    tv.add(0, 3100000000, 48000);

    // The last corrected frame should be the raw timestamp if not advancing at normal rate
    EXPECT_EQ(0., tv.getLastCorrectedTimestamp().mFrames);
    EXPECT_EQ(3100000000., tv.getLastCorrectedTimestamp().mTimeNs);

    // Add imperfect normal advancing timestamps
    tv.add(48000*0.9, 4100000000*1.1, 48000);
    tv.add(96000*1.1, 5100000000*0.9, 48000);

    // Last corrected timestamp frame count should not be raw (or zero) as timestamps are now
    // advancing at a (imperfect) normal rate (but the time should, as implementation uses frame
    // rather than time correction).
    EXPECT_NE(0, tv.getLastCorrectedTimestamp().mFrames);
    EXPECT_NE(96000*1.1, tv.getLastCorrectedTimestamp().mFrames);
    EXPECT_EQ(5100000000*0.9, tv.getLastCorrectedTimestamp().mTimeNs);
}
