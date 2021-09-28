/*
 * Copyright (C) 2015 The Android Open Source Project
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
package com.android.tv.data.api;

import static com.google.common.truth.Truth.assertThat;

import com.android.tv.data.ProgramImpl;
import com.android.tv.testing.constants.ConfigConstants;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Tests for {@link Program}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class ProgramTest {

    private static ProgramImpl createProgramWithStartEndTimes(
            long startTimeUtcMillis, long endTimeUtcMillis) {
        return new ProgramImpl.Builder()
                .setStartTimeUtcMillis(startTimeUtcMillis)
                .setEndTimeUtcMillis(endTimeUtcMillis)
                .build();
    }

    private static ProgramImpl createProgramWithChannelId(long channelId) {
        return new ProgramImpl.Builder().setChannelId(channelId).build();
    }

    private final Program start10end20 = createProgramWithStartEndTimes(10, 20);
    private final Program channel1 = createProgramWithChannelId(1);

    @Test
    public void sameChannel_nullAlwaysFalse() {
        assertThat(Program.sameChannel(null, null)).isFalse();
        assertThat(Program.sameChannel(channel1, null)).isFalse();
        assertThat(Program.sameChannel(null, channel1)).isFalse();
    }

    @Test
    public void sameChannel_true() {
        assertThat(Program.sameChannel(channel1, channel1)).isTrue();
        assertThat(Program.sameChannel(channel1, createProgramWithChannelId(1))).isTrue();
    }

    @Test
    public void sameChannel_false() {
        assertThat(Program.sameChannel(channel1, createProgramWithChannelId(2))).isFalse();
    }

    @Test
    public void isOverLapping_nullAlwaysFalse() {
        assertThat(Program.isOverlapping(null, null)).isFalse();
        assertThat(Program.isOverlapping(start10end20, null)).isFalse();
        assertThat(Program.isOverlapping(null, start10end20)).isFalse();
    }

    @Test
    public void isOverLapping_same() {
        assertThat(Program.isOverlapping(start10end20, start10end20)).isTrue();
    }

    @Test
    public void isOverLapping_endBefore() {
        assertThat(Program.isOverlapping(start10end20, createProgramWithStartEndTimes(1, 9)))
                .isFalse();
    }

    @Test
    public void isOverLapping_endAtStart() {
        assertThat(Program.isOverlapping(start10end20, createProgramWithStartEndTimes(1, 10)))
                .isFalse();
    }

    @Test
    public void isOverLapping_endDuring() {
        assertThat(Program.isOverlapping(start10end20, createProgramWithStartEndTimes(1, 11)))
                .isTrue();
    }

    @Test
    public void isOverLapping_startAfter() {
        assertThat(Program.isOverlapping(start10end20, createProgramWithStartEndTimes(21, 30)))
                .isFalse();
    }

    @Test
    public void isOverLapping_beginAtEnd() {
        assertThat(Program.isOverlapping(start10end20, createProgramWithStartEndTimes(20, 30)))
                .isFalse();
    }

    @Test
    public void isOverLapping_beginBeforeEnd() {
        assertThat(Program.isOverlapping(start10end20, createProgramWithStartEndTimes(19, 30)))
                .isTrue();
    }

    @Test
    public void isOverLapping_inside() {
        assertThat(Program.isOverlapping(start10end20, createProgramWithStartEndTimes(11, 19)))
                .isTrue();
    }
}
