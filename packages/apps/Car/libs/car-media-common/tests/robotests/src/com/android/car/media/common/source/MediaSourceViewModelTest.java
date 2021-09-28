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

package com.android.car.media.common.source;

import static android.car.media.CarMediaManager.MEDIA_SOURCE_MODE_PLAYBACK;

import static com.android.car.apps.common.util.CarAppsDebugUtils.idHash;
import static com.android.car.media.common.MediaTestUtils.newFakeMediaSource;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;
import static org.robolectric.RuntimeEnvironment.application;

import android.app.Application;
import android.car.Car;
import android.car.media.CarMediaManager;
import android.content.ComponentName;
import android.support.v4.media.MediaBrowserCompat;
import android.util.Log;

import androidx.annotation.NonNull;

import com.android.car.arch.common.testing.CaptureObserver;
import com.android.car.arch.common.testing.InstantTaskExecutorRule;
import com.android.car.arch.common.testing.TestLifecycleOwner;
import com.android.car.media.common.source.MediaBrowserConnector.BrowsingState;
import com.android.car.media.common.source.MediaBrowserConnector.ConnectionStatus;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RobolectricTestRunner;


@RunWith(RobolectricTestRunner.class)
public class MediaSourceViewModelTest {

    private static final String TAG = "MediaSourceVMTest";

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public final InstantTaskExecutorRule mTaskExecutorRule = new InstantTaskExecutorRule();
    @Rule
    public final TestLifecycleOwner mLifecycleOwner = new TestLifecycleOwner();

    @Mock
    public MediaBrowserCompat mMediaBrowser;

    @Mock
    public Car mCar;
    @Mock
    public CarMediaManager mCarMediaManager;

    private MediaSourceViewModel mViewModel;

    private MediaSource mRequestedSource;
    private MediaSource mMediaSource;

    @Before
    public void setUp() {
        mRequestedSource = null;
        mMediaSource = null;
    }

    private void initializeViewModel() {
        mViewModel = new MediaSourceViewModel(application, MEDIA_SOURCE_MODE_PLAYBACK,
                new MediaSourceViewModel.InputFactory() {
            @Override
            public MediaBrowserConnector createMediaBrowserConnector(
                    @NonNull Application application,
                    @NonNull MediaBrowserConnector.Callback connectedBrowserCallback) {
                return new MediaBrowserConnector(application, connectedBrowserCallback) {
                    @Override
                    protected MediaBrowserCompat createMediaBrowser(MediaSource mediaSource,
                            MediaBrowserCompat.ConnectionCallback callback) {
                        mRequestedSource = mediaSource;
                        MediaBrowserCompat bro = super.createMediaBrowser(mediaSource, callback);
                        Log.i(TAG, "createMediaBrowser: " + idHash(bro) + " for: " + mediaSource);
                        return bro;
                    }
                };
            }

            @Override
            public Car getCarApi() {
                return mCar;
            }

            @Override
            public CarMediaManager getCarMediaManager(Car carApi) {
                return mCarMediaManager;
            }

            @Override
            public MediaSource getMediaSource(ComponentName componentName) {
                return mMediaSource;
            }
        });
    }

    @Test
    public void testGetSelectedMediaSource_none() {
        initializeViewModel();
        CaptureObserver<MediaSource> observer = new CaptureObserver<>();

        mViewModel.getPrimaryMediaSource().observe(mLifecycleOwner, observer);

        assertThat(observer.getObservedValue()).isNull();
    }

    @Test
    public void testGetMediaController_connectedBrowser() {
        CaptureObserver<BrowsingState> observer = new CaptureObserver<>();
        mMediaSource = newFakeMediaSource("test", "test");
        when(mMediaBrowser.isConnected()).thenReturn(true);
        initializeViewModel();

        mViewModel.getBrowserCallback().onBrowserConnectionChanged(
                new BrowsingState(mMediaSource, mMediaBrowser, ConnectionStatus.CONNECTED));
        mViewModel.getBrowsingState().observe(mLifecycleOwner, observer);

        BrowsingState browsingState = observer.getObservedValue();
        assertThat(browsingState.mBrowser).isSameAs(mMediaBrowser);
        assertThat(browsingState.mConnectionStatus).isEqualTo(ConnectionStatus.CONNECTED);
        assertThat(mRequestedSource).isEqualTo(mMediaSource);
    }

    @Test
    public void testGetMediaController_noActiveSession_notConnected() {
        CaptureObserver<BrowsingState> observer = new CaptureObserver<>();
        mMediaSource = newFakeMediaSource("test", "test");
        when(mMediaBrowser.isConnected()).thenReturn(false);
        initializeViewModel();

        mViewModel.getBrowserCallback().onBrowserConnectionChanged(
                new BrowsingState(mMediaSource, mMediaBrowser, ConnectionStatus.REJECTED));
        mViewModel.getBrowsingState().observe(mLifecycleOwner, observer);

        BrowsingState browsingState = observer.getObservedValue();
        assertThat(browsingState.mBrowser).isSameAs(mMediaBrowser);
        assertThat(browsingState.mConnectionStatus).isEqualTo(ConnectionStatus.REJECTED);
        assertThat(mRequestedSource).isEqualTo(mMediaSource);
    }
}
