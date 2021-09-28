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

package com.android.car.notification;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.media.Ringtone;
import android.net.Uri;
import android.provider.Settings;

import androidx.annotation.NonNull;
import androidx.test.core.app.ApplicationProvider;

import com.android.car.notification.R;
import com.android.car.notification.testutils.ShadowRingtoneManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowAudioManager;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowMediaPlayer;
import org.robolectric.shadows.ShadowMediaPlayer.State;
import org.robolectric.shadows.util.DataSource;

import java.io.IOException;


@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowRingtoneManager.class})
public class BeeperTest implements ShadowMediaPlayer.CreateListener {

    private static String PACKAGE_1 = "package_1";
    private static String PACKAGE_2 = "package_2";
    private static final int MEDIA_DURATION = 5000;

    private Beeper mBeeper;

    @Mock
    private Ringtone mRingtone;

    private Uri mTestUri;
    private Uri mTestUri2;
    private DataSource mTestDataSource;

    private AudioManager mAudioManager;

    private MediaPlayer mMediaPlayer;
    private ShadowMediaPlayer mShadowMediaPlayer;

    private final Context mApplicationContext = ApplicationProvider.getApplicationContext();

    @Before
    public void setup() throws IOException {
        MockitoAnnotations.initMocks(this);
        mTestUri = Uri.parse("android.resource://" + mApplicationContext.getPackageName() + "/" + R.raw.one2six);
        mTestUri2 = Uri.parse(
                "android.resource://" + mApplicationContext.getPackageName() + "/" + R.raw.one2six);

        mTestDataSource = DataSource.toDataSource(mApplicationContext, mTestUri);
        DataSource test2DataSource = DataSource.toDataSource(mApplicationContext, mTestUri2);
        DataSource defaultDataSource =
                DataSource.toDataSource(mApplicationContext, Settings.System.DEFAULT_NOTIFICATION_URI);

        ShadowRingtoneManager.setRingtone(mRingtone);

        AudioManager manager = (AudioManager) mApplicationContext.getSystemService(Context.AUDIO_SERVICE);
        this.mAudioManager = manager;
        ShadowMediaPlayer.setCreateListener(this);

        ShadowMediaPlayer.MediaInfo info =
                new ShadowMediaPlayer.MediaInfo(MEDIA_DURATION, -1 /* prepare delay */);
        ShadowMediaPlayer.addMediaInfo(mTestDataSource, info);
        ShadowMediaPlayer.addMediaInfo(test2DataSource, info);
        ShadowMediaPlayer.addMediaInfo(defaultDataSource, info);

        mBeeper = new Beeper(mApplicationContext);
    }

    @Override // ShadowMediaPlayer.CreateListener
    public void onCreate(MediaPlayer mediaPlayer, ShadowMediaPlayer shadowMediaPlayer) {
        this.mMediaPlayer = mediaPlayer;
        this.mShadowMediaPlayer = shadowMediaPlayer;
    }

    @Test
    public void testBeeperPlaysProvidedSound() {
        doBeep(PACKAGE_1, mTestUri);

        assertThat(mShadowMediaPlayer.getSourceUri()).isEqualTo(mTestUri);
    }


    @Test
    public void testBeepStopsPreviousBeep() {
        doBeep(PACKAGE_1, mTestUri);
        ShadowMediaPlayer previousShadow = mShadowMediaPlayer;

        doBeep(PACKAGE_2, mTestUri2);
        assertThat(previousShadow.getState()).isEqualTo(State.END);
    }

    @Test
    public void testHandlesMediaPlayerLifetime() {
        doBeep(PACKAGE_1, mTestUri);

        assertStateIs(State.PREPARING);
        mShadowMediaPlayer.invokePreparedListener();
        assertStateIs(State.STARTED);
        ShadowLooper.idleMainLooper(MEDIA_DURATION + 100);
        assertStateIs(State.END);
    }


    @Test
    public void testDoesNotPlayWhenAudioFocusDenied() {
        doBeep(PACKAGE_1, mTestUri);

        shadowOf(mAudioManager).setNextFocusRequestResponse(AudioManager.AUDIOFOCUS_REQUEST_FAILED);
        mShadowMediaPlayer.invokePreparedListener();
        assertStateIs(State.END);
    }

    @Test
    public void testHandlesErrorsInSetDataSource_ioException() {
        ShadowMediaPlayer.addException(mTestDataSource, new IOException());
        doBeep(PACKAGE_1, mTestUri);
        assertPlayedViaRingtoneManager(mTestUri);
    }

    @Test
    public void testHandlesErrorsInSetDataSource_securityException() {
        ShadowMediaPlayer.addException(mTestDataSource, new SecurityException());
        doBeep(PACKAGE_1, mTestUri);
        assertPlayedViaRingtoneManager(mTestUri);
    }

    @Test
    public void testHandlesErrorsInSetDataSource_illegalArgumentException() {
        ShadowMediaPlayer.addException(mTestDataSource, new IllegalArgumentException());
        doBeep(PACKAGE_1, mTestUri);
        assertPlayedViaRingtoneManager(mTestUri);
    }

    @Test
    public void testHandlesErrorsWhenPlaying() {
        doBeep(PACKAGE_1, mTestUri);
        mShadowMediaPlayer.invokePreparedListener();
        assertStateIs(State.STARTED);
        ShadowAudioManager.AudioFocusRequest request = shadowOf(mAudioManager).getLastAudioFocusRequest();

        ShadowLooper.idleMainLooper(100);
        mShadowMediaPlayer.invokeErrorListener(
                MediaPlayer.MEDIA_ERROR_UNKNOWN, MediaPlayer.MEDIA_ERROR_IO);
        assertPlayedViaRingtoneManager(mTestUri);
    }

    @Test
    public void testHandlesRingtoneManagerNullResult() {
        ShadowRingtoneManager.setRingtone(null);
        ShadowMediaPlayer.addException(mTestDataSource, new IOException());
        doBeep(PACKAGE_1, mTestUri);
        assertStateIs(State.END);
    }

    @Test
    public void testHandlesPreparedAfterStopped() {
        doBeep(PACKAGE_1, mTestUri);
        mBeeper.stopBeeping();
        mShadowMediaPlayer.invokePreparedListener();
        assertThat(mMediaPlayer.isPlaying()).isFalse();
        // Make sure this didn't go out via RingtoneManager either.
        assertThat(ShadowRingtoneManager.getRingtoneUri()).isNull();
    }

    @Test
    public void testStopAlsoStopsRingtoneManager() {
        ShadowMediaPlayer.addException(mTestDataSource, new IOException());
        doBeep(PACKAGE_1, mTestUri);
        assertPlayedViaRingtoneManager(mTestUri);

        mBeeper.stopBeeping();
        verify(mRingtone).stop();
    }

    private void doBeep(String packageName, @NonNull Uri uri) {
        mBeeper.beep(packageName, uri);
        mShadowMediaPlayer.setInvalidStateBehavior(ShadowMediaPlayer.InvalidStateBehavior.ASSERT);
    }

    private void assertStateIs(State desiredState) {
        assertThat(mShadowMediaPlayer.getState()).isEqualTo(desiredState);
    }

    private void assertPlayedViaRingtoneManager(Uri expectedUri) {
        assertStateIs(State.END);
        assertThat(ShadowRingtoneManager.getRingtoneUri()).isEqualTo(expectedUri);
        verify(mRingtone).setStreamType(AudioManager.STREAM_MUSIC);
        verify(mRingtone).play();
    }
}
