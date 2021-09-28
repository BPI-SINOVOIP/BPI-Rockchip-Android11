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

package com.android.car.media.common.playback;

import static com.android.car.arch.common.LiveDataFunctions.dataOf;
import static com.android.car.media.common.MediaTestUtils.newFakeMediaSource;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.robolectric.RuntimeEnvironment.application;

import android.support.v4.media.MediaBrowserCompat;
import android.support.v4.media.MediaDescriptionCompat;
import android.support.v4.media.MediaMetadataCompat;
import android.support.v4.media.session.MediaControllerCompat;
import android.support.v4.media.session.MediaSessionCompat;
import android.support.v4.media.session.PlaybackStateCompat;

import androidx.annotation.NonNull;
import androidx.lifecycle.MutableLiveData;

import com.android.car.arch.common.testing.CaptureObserver;
import com.android.car.arch.common.testing.InstantTaskExecutorRule;
import com.android.car.arch.common.testing.TestLifecycleOwner;
import com.android.car.media.common.MediaItemMetadata;
import com.android.car.media.common.source.MediaBrowserConnector.BrowsingState;
import com.android.car.media.common.source.MediaBrowserConnector.ConnectionStatus;
import com.android.car.media.common.source.MediaSource;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RobolectricTestRunner;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

@RunWith(RobolectricTestRunner.class)
public class PlaybackViewModelTest {

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public final InstantTaskExecutorRule mTaskExecutorRule = new InstantTaskExecutorRule();
    @Rule
    public final TestLifecycleOwner mLifecycleOwner = new TestLifecycleOwner();

    private final MediaSource mMediaSource = newFakeMediaSource("test", "test");

    @Mock
    public MediaBrowserCompat mMediaBrowser;
    @Mock
    public MediaControllerCompat mMediaController;
    @Mock
    public MediaMetadataCompat mMediaMetadata;
    @Mock
    public MediaDescriptionCompat mMediaDescriptionCompat;
    @Mock
    public PlaybackStateCompat mPlaybackState;
    @Captor
    private ArgumentCaptor<MediaControllerCompat.Callback> mCapturedCallback;

    private PlaybackViewModel mPlaybackViewModel;

    private MutableLiveData<BrowsingState> mBrowsingStateLD;

    private Map<MediaBrowserCompat, MediaControllerCompat> mBrowserToController = new HashMap<>();

    @Before
    public void setUp() {
        mBrowserToController.put(mMediaBrowser, mMediaController);
        doNothing().when(mMediaController).registerCallback(mCapturedCallback.capture());
        mBrowsingStateLD = dataOf(
                new BrowsingState(mMediaSource, mMediaBrowser, ConnectionStatus.CONNECTED));
        mPlaybackViewModel = new PlaybackViewModel(application, mBrowsingStateLD,
                browser -> mBrowserToController.get(browser));
    }

    @Test
    public void testGetMetadata() {
        when(mMediaMetadata.getDescription()).thenReturn(mMediaDescriptionCompat);
        CaptureObserver<MediaItemMetadata> observer = new CaptureObserver<>();
        mPlaybackViewModel.getMetadata().observe(mLifecycleOwner, observer);
        observer.reset();

        assertThat(mCapturedCallback.getValue()).isNotNull();
        mCapturedCallback.getValue().onMetadataChanged(mMediaMetadata);

        assertThat(observer.hasBeenNotified()).isTrue();
        MediaItemMetadata observedValue = observer.getObservedValue();
        assertThat(observedValue).isNotNull();
        assertThat(observedValue).isEqualTo(new MediaItemMetadata(mMediaMetadata));
    }

    @Test
    public void testGetPlaybackState() {
        CaptureObserver<PlaybackViewModel.PlaybackStateWrapper> observer = new CaptureObserver<>();
        mPlaybackViewModel.getPlaybackStateWrapper().observe(mLifecycleOwner, observer);
        observer.reset();

        assertThat(mCapturedCallback.getValue()).isNotNull();
        mCapturedCallback.getValue().onMetadataChanged(mMediaMetadata);
        mCapturedCallback.getValue().onPlaybackStateChanged(mPlaybackState);

        assertThat(observer.hasBeenNotified()).isTrue();
        assertThat(observer.getObservedValue().getStateCompat()).isEqualTo(mPlaybackState);
    }

    @Test
    public void testGetSanitizedQueue() {
        String title = "title";
        int queueId = 1;
        MediaSessionCompat.QueueItem queueItem = createQueueItem(title, queueId);
        List<MediaSessionCompat.QueueItem> queue = Collections.singletonList(queueItem);
        CaptureObserver<List<MediaItemMetadata>> observer = new CaptureObserver<>();
        mPlaybackViewModel.getQueue().observe(mLifecycleOwner, observer);
        observer.reset();

        mCapturedCallback.getValue().onQueueChanged(queue);

        assertThat(observer.hasBeenNotified()).isTrue();
        List<MediaItemMetadata> observedValue = observer.getObservedValue();
        assertThat(observedValue).isNotNull();
        assertThat(observedValue).hasSize(1);
        MediaItemMetadata observedItem = observedValue.get(0);
        assertThat(observedItem).isNotNull();
        assertThat(observedItem.getTitle()).isEqualTo(title);
        assertThat(observedItem.getQueueId()).isEqualTo(queueId);
    }

    @Test
    public void testGetHasQueue_null() {
        CaptureObserver<Boolean> observer = new CaptureObserver<>();
        mPlaybackViewModel.hasQueue().observe(mLifecycleOwner, observer);
        mCapturedCallback.getValue().onQueueChanged(
                Collections.singletonList(createQueueItem("title", 1)));
        observer.reset();

        mCapturedCallback.getValue().onQueueChanged(null);

        assertThat(observer.hasBeenNotified()).isTrue();
        assertThat(observer.getObservedValue()).isEqualTo(false);
    }

    @Test
    public void testGetHasQueue_empty() {
        List<MediaSessionCompat.QueueItem> queue = Collections.emptyList();
        CaptureObserver<Boolean> observer = new CaptureObserver<>();
        mPlaybackViewModel.hasQueue().observe(mLifecycleOwner, observer);
        mCapturedCallback.getValue().onQueueChanged(
                Collections.singletonList(createQueueItem("title", 1)));
        observer.reset();

        mCapturedCallback.getValue().onQueueChanged(queue);

        assertThat(observer.hasBeenNotified()).isTrue();
        assertThat(observer.getObservedValue()).isEqualTo(false);
    }

    @Test
    public void testGetHasQueue_true() {
        List<MediaSessionCompat.QueueItem> queue =
                Arrays.asList(createQueueItem("title1", 1), createQueueItem("title2", 2));
        CaptureObserver<Boolean> observer = new CaptureObserver<>();
        mPlaybackViewModel.hasQueue().observe(mLifecycleOwner, observer);
        observer.reset();

        mCapturedCallback.getValue().onQueueChanged(queue);

        assertThat(observer.hasBeenNotified()).isTrue();
        assertThat(observer.getObservedValue()).isEqualTo(true);
    }

    @Test
    public void testChangeMediaSource_consistentController() {
        deliverValuesToCallbacks(mCapturedCallback, mMediaMetadata, mPlaybackState);

        // Create new MediaBrowser, new MediaController and associated callback captor
        MediaBrowserCompat newMediaBrowser = mock(MediaBrowserCompat.class);
        MediaControllerCompat newController = mock(MediaControllerCompat.class);
        mBrowserToController.put(newMediaBrowser, newController);

        ArgumentCaptor<MediaControllerCompat.Callback> newCallbackCaptor =
                ArgumentCaptor.forClass(MediaControllerCompat.Callback.class);
        doNothing().when(newController).registerCallback(newCallbackCaptor.capture());

        // Wire up new data for new MediaController
        MediaMetadataCompat newMetadata = mock(MediaMetadataCompat.class);
        PlaybackStateCompat newPlaybackState = mock(PlaybackStateCompat.class);

        // Ensure that all values are coming from the correct MediaController.
        mPlaybackViewModel.getMetadata().observe(mLifecycleOwner, mediaItemMetadata -> {
            if (mPlaybackViewModel.getMediaMetadata() == newMetadata) {
                assertThat(mPlaybackViewModel.getMediaController()).isSameAs(newController);
            }
            if (mPlaybackViewModel.getMediaMetadata() == mMediaMetadata) {
                assertThat(mPlaybackViewModel.getMediaController()).isSameAs(mMediaController);
            }
        });

        mPlaybackViewModel.getPlaybackStateWrapper().observe(mLifecycleOwner, state -> {
            if (state == null) return;

            if (state.getStateCompat() == newPlaybackState) {
                assertThat(mPlaybackViewModel.getMediaController()).isSameAs(newController);
            }
            if (state.getStateCompat() == mPlaybackState) {
                assertThat(mPlaybackViewModel.getMediaController()).isSameAs(mMediaController);
            }
        });

        mBrowsingStateLD.setValue(
                new BrowsingState(mMediaSource, newMediaBrowser, ConnectionStatus.CONNECTED));
        deliverValuesToCallbacks(newCallbackCaptor, newMetadata, newPlaybackState);
    }

    private void deliverValuesToCallbacks(
            ArgumentCaptor<MediaControllerCompat.Callback> callbackCaptor,
            MediaMetadataCompat metadata,
            PlaybackStateCompat playbackState) {
        for (MediaControllerCompat.Callback callback : callbackCaptor.getAllValues()) {
            callback.onMetadataChanged(metadata);
            callback.onPlaybackStateChanged(playbackState);
        }
    }

    @NonNull
    private MediaSessionCompat.QueueItem createQueueItem(String title, int queueId) {
        MediaDescriptionCompat description =
                new MediaDescriptionCompat.Builder().setTitle(title).build();
        return new MediaSessionCompat.QueueItem(description, queueId);
    }
}
