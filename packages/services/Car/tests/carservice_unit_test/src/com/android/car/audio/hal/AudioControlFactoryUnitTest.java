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

package audio.hal;

import static com.android.dx.mockito.inline.extended.ExtendedMockito.doReturn;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.car.test.mocks.AbstractExtendedMockitoTestCase;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.audio.hal.AudioControlFactory;
import com.android.car.audio.hal.AudioControlWrapper;
import com.android.car.audio.hal.AudioControlWrapperV1;
import com.android.car.audio.hal.AudioControlWrapperV2;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

@RunWith(AndroidJUnit4.class)
public class AudioControlFactoryUnitTest extends AbstractExtendedMockitoTestCase {
    private static final String TAG = AudioControlFactoryUnitTest.class.getSimpleName();

    @Mock
    android.hardware.automotive.audiocontrol.V2_0.IAudioControl mIAudioControlV2;

    @Mock
    android.hardware.automotive.audiocontrol.V1_0.IAudioControl mIAudioControlV1;

    @Override
    protected void onSessionBuilder(CustomMockitoSessionBuilder session) {
        session.spyStatic(AudioControlWrapperV2.class).spyStatic(AudioControlWrapperV1.class);
    }

    @Test
    public void newAudioControl_forAudioControlWrapperV2_returnsInstance() {
        doReturn(null).when(() -> AudioControlWrapperV1.getService());
        doReturn(mIAudioControlV2).when(() -> AudioControlWrapperV2.getService());

        AudioControlWrapper wrapper = AudioControlFactory.newAudioControl();
        assertThat(wrapper).isNotNull();
    }

    @Test
    public void newAudioControl_forAudioControlWrapperV1_returnsInstance() {
        doReturn(mIAudioControlV1).when(() -> AudioControlWrapperV1.getService());
        doReturn(null).when(() -> AudioControlWrapperV2.getService());

        AudioControlWrapper wrapper = AudioControlFactory.newAudioControl();
        assertThat(wrapper).isNotNull();
    }

    @Test
    public void newAudioControl_forNullAudioControlWrappers_fails() {
        doReturn(null).when(() -> AudioControlWrapperV1.getService());
        doReturn(null).when(() -> AudioControlWrapperV2.getService());

        assertThrows(IllegalStateException.class, () -> AudioControlFactory.newAudioControl());
    }
}
