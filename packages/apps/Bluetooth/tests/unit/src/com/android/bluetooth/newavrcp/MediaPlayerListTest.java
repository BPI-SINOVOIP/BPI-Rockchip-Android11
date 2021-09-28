/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.bluetooth.avrcp;

import static org.mockito.Mockito.*;

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.media.AudioManager;
import android.media.session.MediaSessionManager;
import android.media.session.PlaybackState;
import android.os.Looper;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class MediaPlayerListTest {
    private MediaPlayerList mMediaPlayerList;

    private @Captor ArgumentCaptor<AudioManager.AudioPlaybackCallback> mAudioCb;
    private @Captor ArgumentCaptor<MediaPlayerWrapper.Callback> mPlayerWrapperCb;
    private @Captor ArgumentCaptor<MediaData> mMediaUpdateData;
    private @Mock Context mMockContext;
    private @Mock AvrcpTargetService.ListCallback mAvrcpCallback;
    private @Mock MediaController mMockController;
    private @Mock MediaPlayerWrapper mMockPlayerWrapper;

    private final String mFlagDexmarker = System.getProperty("dexmaker.share_classloader", "false");
    private MediaPlayerWrapper.Callback mActivePlayerCallback;

    @Before
    public void setUp() throws Exception {
        if (!mFlagDexmarker.equals("true")) {
            System.setProperty("dexmaker.share_classloader", "true");
        }

        if (Looper.myLooper() == null) {
            Looper.prepare();
        }
        Assert.assertNotNull(Looper.myLooper());

        MockitoAnnotations.initMocks(this);

        AudioManager mockAudioManager = mock(AudioManager.class);
        when(mMockContext.getSystemService(Context.AUDIO_SERVICE)).thenReturn(mockAudioManager);

        MediaSessionManager mMediaSessionManager =
                (MediaSessionManager) InstrumentationRegistry.getTargetContext()
                .getSystemService(Context.MEDIA_SESSION_SERVICE);
        PackageManager mockPackageManager = mock(PackageManager.class);
        when(mMockContext.getSystemService(Context.MEDIA_SESSION_SERVICE))
            .thenReturn(mMediaSessionManager);

        mMediaPlayerList =
            new MediaPlayerList(Looper.myLooper(), InstrumentationRegistry.getTargetContext());

        when(mMockContext.registerReceiver(any(), any())).thenReturn(null);
        when(mMockContext.getApplicationContext()).thenReturn(mMockContext);
        when(mMockContext.getPackageManager()).thenReturn(mockPackageManager);
        List<ResolveInfo> playerList = new ArrayList<ResolveInfo>();
        when(mockPackageManager.queryIntentServices(any(), anyInt())).thenReturn(null);

        Method method = BrowsablePlayerConnector.class.getDeclaredMethod("setInstanceForTesting",
                BrowsablePlayerConnector.class);
        BrowsablePlayerConnector mockConnector = mock(BrowsablePlayerConnector.class);
        method.setAccessible(true);
        method.invoke(null, mockConnector);
        mMediaPlayerList.init(mAvrcpCallback);

        MediaControllerFactory.inject(mMockController);
        MediaPlayerWrapperFactory.inject(mMockPlayerWrapper);

        doReturn("testPlayer").when(mMockController).getPackageName();
        when(mMockPlayerWrapper.isMetadataSynced()).thenReturn(false);
        mMediaPlayerList.setActivePlayer(mMediaPlayerList.addMediaPlayer(mMockController));

        verify(mMockPlayerWrapper).registerCallback(mPlayerWrapperCb.capture());
        mActivePlayerCallback = mPlayerWrapperCb.getValue();
    }

    @After
    public void tearDown() throws Exception {
        Method method = BrowsablePlayerConnector.class.getDeclaredMethod("setInstanceForTesting",
                BrowsablePlayerConnector.class);
        method.setAccessible(true);
        method.invoke(null, (BrowsablePlayerConnector) null);

        MediaControllerFactory.inject(null);
        MediaPlayerWrapperFactory.inject(null);
        mMediaPlayerList.cleanup();
        if (!mFlagDexmarker.equals("true")) {
            System.setProperty("dexmaker.share_classloader", mFlagDexmarker);
        }
    }

    private MediaData prepareMediaData(int playbackState) {
        PlaybackState.Builder builder = new PlaybackState.Builder();
        builder.setState(playbackState, 0, 1);
        ArrayList<Metadata> list = new ArrayList<Metadata>();
        list.add(Util.empty_data());
        MediaData newData = new MediaData(
                Util.empty_data(),
                builder.build(),
                list);

        return newData;
    }

    @Test
    public void testUpdateMeidaDataForAudioPlaybackWhenAcitvePlayNotPlaying() {
        // Verify update media data with playing state
        doReturn(prepareMediaData(PlaybackState.STATE_PAUSED))
            .when(mMockPlayerWrapper).getCurrentMediaData();
        mMediaPlayerList.injectAudioPlaybacActive(true);
        verify(mAvrcpCallback).run(mMediaUpdateData.capture());
        MediaData data = mMediaUpdateData.getValue();
        Assert.assertEquals(data.state.getState(), PlaybackState.STATE_PLAYING);

        // verify update media data with current media player media data
        MediaData currentMediaData = prepareMediaData(PlaybackState.STATE_PAUSED);
        doReturn(currentMediaData).when(mMockPlayerWrapper).getCurrentMediaData();
        mMediaPlayerList.injectAudioPlaybacActive(false);
        verify(mAvrcpCallback, times(2)).run(mMediaUpdateData.capture());
        data = mMediaUpdateData.getValue();
        Assert.assertEquals(data.metadata, currentMediaData.metadata);
        Assert.assertEquals(data.state.toString(), currentMediaData.state.toString());
        Assert.assertEquals(data.queue, currentMediaData.queue);
    }

    @Test
    public void testUpdateMediaDataForActivePlayerWhenAudioPlaybackIsNotActive() {
        MediaData currMediaData = prepareMediaData(PlaybackState.STATE_PLAYING);
        mActivePlayerCallback.mediaUpdatedCallback(currMediaData);
        verify(mAvrcpCallback).run(currMediaData);

        currMediaData = prepareMediaData(PlaybackState.STATE_PAUSED);
        mActivePlayerCallback.mediaUpdatedCallback(currMediaData);
        verify(mAvrcpCallback).run(currMediaData);
    }

    @Test
    public void testNotUdpateMediaDataForAudioPlaybackWhenActivePlayerIsPlaying() {
        // Verify not update media data for Audio Playback when active player is playing
        doReturn(prepareMediaData(PlaybackState.STATE_PLAYING))
            .when(mMockPlayerWrapper).getCurrentMediaData();
        mMediaPlayerList.injectAudioPlaybacActive(true);
        mMediaPlayerList.injectAudioPlaybacActive(false);
        verify(mAvrcpCallback, never()).run(any());
    }

    @Test
    public void testNotUdpateMediaDataForActivePlayerWhenAudioPlaybackIsActive() {
        doReturn(prepareMediaData(PlaybackState.STATE_PLAYING))
            .when(mMockPlayerWrapper).getCurrentMediaData();
        mMediaPlayerList.injectAudioPlaybacActive(true);
        verify(mAvrcpCallback, never()).run(any());

        // Verify not update active player media data when audio playback is active
        mActivePlayerCallback.mediaUpdatedCallback(prepareMediaData(PlaybackState.STATE_PAUSED));
        verify(mAvrcpCallback, never()).run(any());
    }

}
