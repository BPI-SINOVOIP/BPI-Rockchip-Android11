/*
 * Copyright (C) 2020 Google LLC.
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

package android.media.cts;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;
import static org.testng.Assert.expectThrows;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioDeviceInfo;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.HwAudioSource;
import android.media.MediaRecorder;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/*
 * Tests SystemUsage behavior in non-system app without the MODIFY_AUDIO_ROUTING permission
 */
@NonMediaMainlineTest
@RunWith(AndroidJUnit4.class)
public class AudioSystemUsageTest {
    private static final AudioAttributes SYSTEM_USAGE_AUDIO_ATTRIBUTES =
            new AudioAttributes.Builder()
                    .setSystemUsage(AudioAttributes.USAGE_EMERGENCY)
                    .build();

    private AudioManager mAudioManager;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        mAudioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
    }

    @Test
    public void getSupportedSystemUsages_throwsException() {
        assertThrows(SecurityException.class, () ->
                mAudioManager.getSupportedSystemUsages());
    }

    @Test
    public void setSupportedSystemUsage_throwsException() {
        assertThrows(SecurityException.class, () ->
                mAudioManager.setSupportedSystemUsages(new int[0]));
    }

    @Test
    public void requestAudioFocus_returnsFailedResponse() {
        AudioFocusRequest request = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                .setAudioAttributes(SYSTEM_USAGE_AUDIO_ATTRIBUTES).build();

        int response = mAudioManager.requestAudioFocus(request);
        assertThat(response).isEqualTo(AudioManager.AUDIOFOCUS_REQUEST_FAILED);
    }

    @Test
    public void abandonAudioFocusRequest_returnsFailedResponse() {
        AudioFocusRequest request = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                .setAudioAttributes(SYSTEM_USAGE_AUDIO_ATTRIBUTES).build();

        int response = mAudioManager.abandonAudioFocusRequest(request);
        assertThat(response).isEqualTo(AudioManager.AUDIOFOCUS_REQUEST_FAILED);
    }

    @Test
    public void trackPlayer_throwsException() {
        AudioDeviceInfo testDevice = mAudioManager.getDevices(AudioManager.GET_DEVICES_INPUTS)[0];
        HwAudioSource.Builder builder = new HwAudioSource.Builder()
                .setAudioAttributes(SYSTEM_USAGE_AUDIO_ATTRIBUTES)
                .setAudioDeviceInfo(testDevice);

        // Constructor calls PlayerBase#baseRegisterPlayer which calls AudioService#trackPlayer
        assertThrows(SecurityException.class, builder::build);
    }

    @Test
    public void getOutputForAttr_returnsError() {
        AudioTrack.Builder builder = new AudioTrack.Builder()
                .setAudioAttributes(SYSTEM_USAGE_AUDIO_ATTRIBUTES);

        // Calls AudioPolicyService#getOutputForAttr which returns ERROR for unsupported usage
        UnsupportedOperationException thrown = expectThrows(UnsupportedOperationException.class,
                builder::build);
        assertThat(thrown).hasMessageThat().contains("Cannot create AudioTrack");
    }

    @Test
    public void getInputForAttr_returnsError() {
        AudioAttributes unsupportedInputAudioAttributes = new AudioAttributes.Builder()
                .setSystemUsage(AudioAttributes.USAGE_SAFETY)
                .setCapturePreset(MediaRecorder.AudioSource.MIC)
                .build();
        AudioRecord.Builder builder = new AudioRecord.Builder()
                .setAudioAttributes(unsupportedInputAudioAttributes);

        // Calls AudioPolicyService#getInputForAttr which returns ERROR for unsupported usage
        UnsupportedOperationException thrown = expectThrows(UnsupportedOperationException.class,
                builder::build);
        assertThat(thrown).hasMessageThat().contains("Cannot create AudioRecord");
    }
}
