/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.car.settings.sound;

import static android.media.AudioAttributes.USAGE_ALARM;
import static android.media.AudioAttributes.USAGE_MEDIA;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.media.AudioAttributes.AttributeUsage;
import android.media.Ringtone;
import android.net.Uri;
import android.provider.Settings.System;

import com.android.car.settings.R;
import com.android.car.settings.testutils.ShadowRingtoneManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowRingtoneManager.class})
public class VolumeSettingsRingtoneManagerTest {

    private static final int TEST_GROUP_ID = 1;
    private static final int TEST_USAGE_ID = USAGE_MEDIA;

    private Context mContext;
    private VolumeSettingsRingtoneManager mRingtoneManager;
    @Mock
    private Ringtone mRingtone;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowRingtoneManager.setRingtone(mRingtone);
        mContext = spy(RuntimeEnvironment.application);
        mRingtoneManager = new VolumeSettingsRingtoneManager(mContext);
    }

    @After
    public void tearDown() {
        ShadowRingtoneManager.reset();
        when(mRingtone.isPlaying()).thenReturn(false);
    }

    @Test
    public void getRingtoneUri_defaultsToDefaultRingtoneUriIfConfigIsEmpty() {
        setResources(new int[] {}, new String[] {});
        mRingtoneManager = new VolumeSettingsRingtoneManager(mContext);
        Uri ringtoneUri = mRingtoneManager.getRingtoneUri(USAGE_ALARM);

        assertThat(ringtoneUri).isEqualTo(System.DEFAULT_RINGTONE_URI);
    }

    @Test
    public void getRingtoneUri_defaultsToDefaultRingtoneUriIfUsageNotInConfig() {
        setResources(
                new int[] {USAGE_MEDIA},
                new String[] {"content://settings/system/notification_sound"}
        );
        mRingtoneManager = new VolumeSettingsRingtoneManager(mContext);

        Uri ringtoneUri = mRingtoneManager.getRingtoneUri(USAGE_ALARM);

        assertThat(ringtoneUri).isEqualTo(System.DEFAULT_RINGTONE_URI);
    }

    @Test
    public void getRingtoneUri_usesConfigValuesForUsageRingtoneMapping() {
        @AttributeUsage int usage = USAGE_MEDIA;
        String ringtoneValue = "content://settings/system/notification_sound";
        setResources(new int[] {usage}, new String[] {ringtoneValue});
        mRingtoneManager = new VolumeSettingsRingtoneManager(mContext);

        Uri ringtoneUri = mRingtoneManager.getRingtoneUri(usage);

        assertThat(ringtoneUri).isEqualTo(Uri.parse(ringtoneValue));
    }

    @Test
    public void testPlayAudioFeedback_play_playUntilTimeout() {
        mRingtoneManager.playAudioFeedback(TEST_GROUP_ID, TEST_USAGE_ID);
        verify(mRingtone).play();
        when(mRingtone.isPlaying()).thenReturn(true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mRingtone).stop();
    }

    @Test
    public void testPlayAudioFeedback_play_stoppedBeforeTimeout() {
        mRingtoneManager.playAudioFeedback(TEST_GROUP_ID, TEST_USAGE_ID);
        verify(mRingtone).play();
        when(mRingtone.isPlaying()).thenReturn(false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mRingtone, never()).stop();
    }

    @Test
    public void testStopCurrentRingtone_stop() {
        mRingtoneManager.playAudioFeedback(TEST_GROUP_ID, TEST_USAGE_ID);
        mRingtoneManager.stopCurrentRingtone();
        verify(mRingtone).stop();
    }

    @Test
    public void testStopCurrentRingtone_noCurrentRingtone() {
        mRingtoneManager.stopCurrentRingtone();
        verify(mRingtone, never()).stop();
    }

    private void setResources(int[] ringtoneAudioAttributeUsages, String[] ringtoneUris) {
        Resources resources = spy(mContext.getResources());
        when(mContext.getResources()).thenReturn(resources);
        when(resources.getIntArray(R.array.config_ringtone_audio_attribute_usages_map_key))
            .thenReturn(ringtoneAudioAttributeUsages);
        when(resources.getStringArray(R.array.config_ringtone_uri_map_value))
            .thenReturn(ringtoneUris);
    }
}
