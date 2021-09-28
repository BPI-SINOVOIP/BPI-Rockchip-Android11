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

package com.android.car.audio;

import static android.media.AudioAttributes.USAGE_GAME;
import static android.media.AudioAttributes.USAGE_MEDIA;
import static android.media.AudioAttributes.USAGE_UNKNOWN;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.media.AudioAttributes.AttributeUsage;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.audio.CarAudioContext.AudioContext;

import com.google.common.primitives.Ints;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.HashSet;
import java.util.Set;

@RunWith(AndroidJUnit4.class)
public class CarAudioContextTest {
    private static final int INVALID_USAGE = -5;
    private static final int INVALID_CONTEXT = -5;

    @Test
    public void getContextForUsage_forValidUsage_returnsContext() {
        assertThat(CarAudioContext.getContextForUsage(USAGE_MEDIA))
                .isEqualTo(CarAudioContext.MUSIC);
    }

    @Test
    public void getContextForUsage_withInvalidUsage_returnsInvalidContext() {
        assertThat(CarAudioContext.getContextForUsage(INVALID_USAGE)).isEqualTo(
                CarAudioContext.INVALID);
    }

    @Test
    public void getUsagesForContext_withValidContext_returnsUsages() {
        int[] usages = CarAudioContext.getUsagesForContext(CarAudioContext.MUSIC);
        assertThat(usages).asList().containsExactly(USAGE_UNKNOWN, USAGE_GAME, USAGE_MEDIA);
    }

    @Test
    public void getUsagesForContext_withInvalidContext_throws() {
        assertThrows(IllegalArgumentException.class, () -> {
            CarAudioContext.getUsagesForContext(INVALID_CONTEXT);
        });
    }

    @Test
    public void getUsagesForContext_returnsUniqueValuesForAllContexts() {
        Set<Integer> allUsages = new HashSet<>();
        for (@AudioContext int audioContext : CarAudioContext.CONTEXTS) {
            @AttributeUsage int[] usages = CarAudioContext.getUsagesForContext(audioContext);
            assertThat(allUsages.addAll(Ints.asList(usages))).isTrue();
        }
    }
}
