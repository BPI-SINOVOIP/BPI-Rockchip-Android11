/*
 * Copyright (C) 2019 The Android Open Source Project
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

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;
import static org.testng.Assert.expectThrows;

import android.car.media.CarAudioManager;
import android.hardware.automotive.audiocontrol.V1_0.ContextNumber;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnitRunner;

@RunWith(MockitoJUnitRunner.class)
public class CarAudioZoneTest {
    private static final String MUSIC_ADDRESS = "bus0_music";
    private static final String NAV_ADDRESS = "bus1_nav";

    @Mock
    private CarVolumeGroup mMockMusicGroup;
    @Mock
    private CarVolumeGroup mMockNavGroup;
    private CarAudioZone mTestAudioZone =
            new CarAudioZone(CarAudioManager.PRIMARY_AUDIO_ZONE, "Primary zone");
    @Before
    public void setUp() {
        when(mMockMusicGroup.getAddressForContext(ContextNumber.MUSIC)).thenReturn(MUSIC_ADDRESS);
        when(mMockMusicGroup.getContexts()).thenReturn(new int[]{ContextNumber.MUSIC});

        when(mMockNavGroup.getAddressForContext(ContextNumber.NAVIGATION)).thenReturn(NAV_ADDRESS);
        when(mMockNavGroup.getContexts()).thenReturn(new int[]{ContextNumber.NAVIGATION});
    }

    @Test
    public void getAddressForContext_returnsExpectedDeviceAddress() {
        mTestAudioZone.addVolumeGroup(mMockMusicGroup);
        mTestAudioZone.addVolumeGroup(mMockNavGroup);

        String musicAddress = mTestAudioZone.getAddressForContext(ContextNumber.MUSIC);
        assertThat(musicAddress).isEqualTo(MUSIC_ADDRESS);

        String navAddress = mTestAudioZone.getAddressForContext(ContextNumber.NAVIGATION);
        assertThat(navAddress).matches(NAV_ADDRESS);
    }

    @Test
    public void getAddressForContext_throwsOnInvalidContext() {
        IllegalArgumentException thrown =
                expectThrows(IllegalArgumentException.class,
                        () -> mTestAudioZone.getAddressForContext(ContextNumber.INVALID));

        assertThat(thrown).hasMessageThat().contains("audioContext 0 is invalid");
    }

    @Test
    public void getAddressForContext_throwsOnNonExistentContext() {
        IllegalStateException thrown =
                expectThrows(IllegalStateException.class,
                        () -> mTestAudioZone.getAddressForContext(ContextNumber.MUSIC));

        assertThat(thrown).hasMessageThat().contains("Could not find output device in zone");
    }
}
