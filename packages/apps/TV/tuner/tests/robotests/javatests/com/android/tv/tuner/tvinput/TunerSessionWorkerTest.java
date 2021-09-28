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
package com.android.tv.tuner.tvinput;

import static com.android.tv.common.customization.CustomizationManager.TRICKPLAY_MODE_ENABLED;

import static com.google.common.truth.Truth.assertThat;

import android.app.Application;
import android.content.Context;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.view.accessibility.CaptioningManager;

import com.android.tv.common.CommonConstants;
import com.android.tv.common.CommonPreferences;
import com.android.tv.common.compat.TvInputConstantCompat;
import com.android.tv.common.customization.CustomizationManager;
import com.android.tv.common.flags.impl.DefaultLegacyFlags;
import com.android.tv.common.flags.impl.DefaultTunerFlags;
import com.android.tv.testing.TestSingletonApp;
import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.tuner.cc.CaptionTrackRenderer;
import com.android.tv.tuner.exoplayer.MpegTsPlayer;
import com.android.tv.tuner.source.TsDataSourceManager;
import com.android.tv.tuner.source.TunerTsStreamerManager;
import com.android.tv.tuner.testing.TvTunerRobolectricTestRunner;
import com.android.tv.tuner.tvinput.datamanager.ChannelDataManager;
import com.android.tv.tuner.tvinput.TunerSessionOverlay;

import com.google.android.exoplayer.audio.AudioCapabilities;

import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mockito;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowContextImpl;

import java.lang.reflect.Field;

import javax.inject.Provider;

/** Tests for {@link TunerSessionWorker}. */
@RunWith(TvTunerRobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK, application = TestSingletonApp.class)
public class TunerSessionWorkerTest {

    private TunerSessionWorker tunerSessionWorker;
    private int mSignalStrength = TvInputConstantCompat.SIGNAL_STRENGTH_UNKNOWN;
    private MpegTsPlayer mPlayer = Mockito.mock(MpegTsPlayer.class);
    private Handler mHandler;
    private DefaultLegacyFlags mLegacyFlags;
    private DefaultTunerFlags mTunerFlags;

    @Before
    public void setUp() throws NoSuchFieldException, IllegalAccessException {
        Application context = RuntimeEnvironment.application;
        CaptioningManager captioningManager = Mockito.mock(CaptioningManager.class);
        mLegacyFlags = DefaultLegacyFlags.DEFAULT;
        mTunerFlags = new DefaultTunerFlags();

        // TODO (b/65160115)
        Field field = CustomizationManager.class.getDeclaredField("sCustomizationPackage");
        field.setAccessible(true);
        field.set(null, CommonConstants.BASE_PACKAGE + ".tuner");
        field = CustomizationManager.class.getDeclaredField("sTrickplayMode");
        field.setAccessible(true);
        field.set(null, TRICKPLAY_MODE_ENABLED);

        ShadowContextImpl shadowContext = Shadow.extract(context.getBaseContext());
        shadowContext.setSystemService(Context.CAPTIONING_SERVICE, captioningManager);

        CommonPreferences.initialize(context);
        ChannelDataManager channelDataManager = new ChannelDataManager(context, "testInput");

        mHandler = new Handler(Looper.getMainLooper(), null);
        Provider<TunerTsStreamerManager> tsStreamerManagerProvider =
                () -> new TunerTsStreamerManager(null);
        TsDataSourceManager.Factory tsdm =
                new TsDataSourceManager.Factory(tsStreamerManagerProvider);
        TunerSessionOverlay.Factory tunerSessionOverlayFactory =
                context1 ->
                        new TunerSessionOverlay(
                                context1,
                                captionLayout ->
                                        new CaptionTrackRenderer(
                                                captionLayout,
                                                context2 -> null,
                                                mTunerFlags));

        new TunerSession(
                context,
                channelDataManager,
                session -> {},
                recordingSession -> Uri.parse("recordingUri"),
                (context1, channelDataManager1, tunerSession1, tunerSessionOverlay) -> {
                    tunerSessionWorker =
                            new TunerSessionWorker(
                                    context1,
                                    channelDataManager1,
                                    tunerSession1,
                                    tunerSessionOverlay,
                                    mHandler,
                                    mLegacyFlags,
                                    (context2, bufferManager, bufferListener) -> null,
                                    tsdm) {
                                @Override
                                protected void notifySignal(int signal) {
                                    mSignalStrength = signal;
                                }

                                @Override
                                protected MpegTsPlayer createPlayer(
                                        AudioCapabilities capabilities) {
                                    return mPlayer;
                                }
                            };
                    return tunerSessionWorker;
                },
                tunerSessionOverlayFactory);
    }

    @Test
    public void doSelectTrack_mPlayerIsNull() {
        Message msg = new Message();
        msg.what = TunerSessionWorker.MSG_SELECT_TRACK;
        assertThat(tunerSessionWorker.handleMessage(msg)).isFalse();
    }

    @Test
    public void doCheckSignalStrength_mPlayerIsNull() {
        Message msg = new Message();
        msg.what = TunerSessionWorker.MSG_CHECK_SIGNAL_STRENGTH;
        assertThat(tunerSessionWorker.handleMessage(msg)).isFalse();
    }

    @Test
    public void handleSignal_isNotUsed() {
        assertThat(tunerSessionWorker.handleSignal(TvInputConstantCompat.SIGNAL_STRENGTH_NOT_USED))
                .isTrue();
        assertThat(mSignalStrength).isEqualTo(TvInputConstantCompat.SIGNAL_STRENGTH_NOT_USED);
    }

    @Test
    public void handleSignal_isError() {
        assertThat(tunerSessionWorker.handleSignal(TvInputConstantCompat.SIGNAL_STRENGTH_ERROR))
                .isTrue();
        assertThat(mSignalStrength).isEqualTo(TvInputConstantCompat.SIGNAL_STRENGTH_ERROR);
    }

    @Test
    public void handleSignal_isUnknown() {
        assertThat(tunerSessionWorker.handleSignal(TvInputConstantCompat.SIGNAL_STRENGTH_UNKNOWN))
                .isTrue();
        assertThat(mSignalStrength).isEqualTo(TvInputConstantCompat.SIGNAL_STRENGTH_UNKNOWN);
    }

    @Test
    public void handleSignal_isNotifySignal() {
        assertThat(tunerSessionWorker.handleSignal(100)).isTrue();
        assertThat(mSignalStrength).isEqualTo(100);
    }

    @Test
    public void preparePlayback_playerIsNotReady() {
        Mockito.when(
                        mPlayer.prepare(
                                Mockito.eq(RuntimeEnvironment.application),
                                ArgumentMatchers.any(),
                                ArgumentMatchers.anyBoolean(),
                                ArgumentMatchers.any()))
                .thenReturn(false);
        tunerSessionWorker.preparePlayback();
        assertThat(mHandler.hasMessages(TunerSessionWorker.MSG_TUNE)).isFalse();
        assertThat(mHandler.hasMessages(TunerSessionWorker.MSG_RETRY_PLAYBACK)).isTrue();
        assertThat(mHandler.hasMessages(TunerSessionWorker.MSG_CHECK_SIGNAL_STRENGTH)).isFalse();
        assertThat(mHandler.hasMessages(TunerSessionWorker.MSG_CHECK_SIGNAL)).isFalse();
    }

    @Test
    @Ignore
    public void preparePlayback_playerIsReady() {
        Mockito.when(
                        mPlayer.prepare(
                                RuntimeEnvironment.application,
                                ArgumentMatchers.any(),
                                ArgumentMatchers.anyBoolean(),
                                ArgumentMatchers.any()))
                .thenReturn(true);
        tunerSessionWorker.preparePlayback();
        assertThat(mHandler.hasMessages(TunerSessionWorker.MSG_RETRY_PLAYBACK)).isFalse();
        assertThat(mHandler.hasMessages(TunerSessionWorker.MSG_CHECK_SIGNAL_STRENGTH)).isTrue();
        assertThat(mHandler.hasMessages(TunerSessionWorker.MSG_CHECK_SIGNAL)).isTrue();
    }
}
