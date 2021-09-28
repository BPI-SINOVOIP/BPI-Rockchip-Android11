/*
 * Copyright 2017 The Android Open Source Project
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

import android.media.MediaDescription;
import android.media.browse.MediaBrowser.MediaItem;
import android.media.session.PlaybackState;
import android.os.Handler;
import android.os.HandlerThread;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;
import java.util.List;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class BrowserPlayerWrapperTest {

    @Captor ArgumentCaptor<MediaBrowser.ConnectionCallback> mBrowserConnCb;
    @Captor ArgumentCaptor<MediaBrowser.SubscriptionCallback> mSubscriptionCb;
    @Captor ArgumentCaptor<MediaController.Callback> mControllerCb;
    @Captor ArgumentCaptor<Handler> mTimeoutHandler;
    @Captor ArgumentCaptor<List<ListItem>> mWrapperBrowseCb;
    @Mock MediaBrowser mMockBrowser;
    @Mock BrowsedPlayerWrapper.ConnectionCallback mConnCb;
    @Mock BrowsedPlayerWrapper.BrowseCallback mBrowseCb;
    private HandlerThread mThread;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Set up Looper thread for the timeout handler
        mThread = new HandlerThread("MediaPlayerWrapperTestThread");
        mThread.start();

        when(mMockBrowser.getRoot()).thenReturn("root_folder");

        MediaBrowserFactory.inject(mMockBrowser);
    }

    @Test
    public void testWrap() {
        BrowsedPlayerWrapper wrapper =
                BrowsedPlayerWrapper.wrap(null, mThread.getLooper(), "test", "test");
        wrapper.connect(mConnCb);
        verify(mMockBrowser).testInit(any(), any(), mBrowserConnCb.capture(), any());
        verify(mMockBrowser).connect();

        MediaBrowser.ConnectionCallback browserConnCb = mBrowserConnCb.getValue();
        browserConnCb.onConnected();

        verify(mConnCb).run(eq(BrowsedPlayerWrapper.STATUS_SUCCESS), eq(wrapper));
        verify(mMockBrowser).disconnect();
    }

    @Test
    public void testConnect_Successful() {
        BrowsedPlayerWrapper wrapper =
                BrowsedPlayerWrapper.wrap(null, mThread.getLooper(), "test", "test");
        wrapper.connect(mConnCb);
        verify(mMockBrowser).testInit(any(), any(), mBrowserConnCb.capture(), any());
        MediaBrowser.ConnectionCallback browserConnCb = mBrowserConnCb.getValue();

        verify(mMockBrowser, times(1)).connect();
        browserConnCb.onConnected();
        verify(mConnCb).run(eq(BrowsedPlayerWrapper.STATUS_SUCCESS), eq(wrapper));
        verify(mMockBrowser, times(1)).disconnect();
    }

    @Test
    public void testConnect_Suspended() {
        BrowsedPlayerWrapper wrapper =
                BrowsedPlayerWrapper.wrap(null, mThread.getLooper(), "test", "test");
        wrapper.connect(mConnCb);
        verify(mMockBrowser).testInit(any(), any(), mBrowserConnCb.capture(), any());
        MediaBrowser.ConnectionCallback browserConnCb = mBrowserConnCb.getValue();

        verify(mMockBrowser, times(1)).connect();
        browserConnCb.onConnectionSuspended();
        verify(mConnCb).run(eq(BrowsedPlayerWrapper.STATUS_CONN_ERROR), eq(wrapper));
        // Twice because our mConnCb is wrapped when using the plain connect() call and disconnect
        // is called for us when the callback is invoked in addition to error handling calling
        // disconnect.
        verify(mMockBrowser, times(2)).disconnect();
    }

    @Test
    public void testConnect_Failed() {
        BrowsedPlayerWrapper wrapper =
                BrowsedPlayerWrapper.wrap(null, mThread.getLooper(), "test", "test");
        wrapper.connect(mConnCb);
        verify(mMockBrowser).testInit(any(), any(), mBrowserConnCb.capture(), any());
        MediaBrowser.ConnectionCallback browserConnCb = mBrowserConnCb.getValue();

        verify(mMockBrowser, times(1)).connect();
        browserConnCb.onConnectionFailed();
        verify(mConnCb).run(eq(BrowsedPlayerWrapper.STATUS_CONN_ERROR), eq(wrapper));
        verify(mMockBrowser, times(1)).disconnect();
    }

    @Test
    public void testEmptyRoot() {
        BrowsedPlayerWrapper wrapper =
                BrowsedPlayerWrapper.wrap(null, mThread.getLooper(), "test", "test");

        doReturn("").when(mMockBrowser).getRoot();

        wrapper.connect(mConnCb);
        verify(mMockBrowser).testInit(any(), any(), mBrowserConnCb.capture(), any());
        MediaBrowser.ConnectionCallback browserConnCb = mBrowserConnCb.getValue();

        verify(mMockBrowser, times(1)).connect();

        browserConnCb.onConnected();
        verify(mConnCb).run(eq(BrowsedPlayerWrapper.STATUS_CONN_ERROR), eq(wrapper));
        verify(mMockBrowser, times(1)).disconnect();
    }

    @Test
    public void testDisconnect() {
        BrowsedPlayerWrapper wrapper =
                BrowsedPlayerWrapper.wrap(null, mThread.getLooper(), "test", "test");
        wrapper.connect(mConnCb);
        verify(mMockBrowser).testInit(any(), any(), mBrowserConnCb.capture(), any());
        MediaBrowser.ConnectionCallback browserConnCb = mBrowserConnCb.getValue();
        browserConnCb.onConnected();
        verify(mMockBrowser).disconnect();
    }

    @Test
    public void testGetRootId() {
        BrowsedPlayerWrapper wrapper =
                BrowsedPlayerWrapper.wrap(null, mThread.getLooper(), "test", "test");
        wrapper.connect(mConnCb);
        verify(mMockBrowser).testInit(any(), any(), mBrowserConnCb.capture(), any());
        MediaBrowser.ConnectionCallback browserConnCb = mBrowserConnCb.getValue();
        browserConnCb.onConnected();

        Assert.assertEquals("root_folder", wrapper.getRootId());
        verify(mMockBrowser).disconnect();
    }

    @Test
    public void testPlayItem() {
        BrowsedPlayerWrapper wrapper =
                BrowsedPlayerWrapper.wrap(null, mThread.getLooper(), "test", "test");
        verify(mMockBrowser).testInit(any(), any(), mBrowserConnCb.capture(), any());
        MediaBrowser.ConnectionCallback browserConnCb = mBrowserConnCb.getValue();

        wrapper.playItem("test_item");
        verify(mMockBrowser, times(1)).connect();

        MediaController mockController = mock(MediaController.class);
        MediaController.TransportControls mockTransport =
                mock(MediaController.TransportControls.class);
        when(mockController.getTransportControls()).thenReturn(mockTransport);
        MediaControllerFactory.inject(mockController);

        browserConnCb.onConnected();
        verify(mockTransport).playFromMediaId(eq("test_item"), eq(null));

        // Do not immediately disconnect. Non-foreground playback serves will likely fail
        verify(mMockBrowser, times(0)).disconnect();

        verify(mockController).registerCallback(mControllerCb.capture(), any());
        MediaController.Callback controllerCb = mControllerCb.getValue();
        PlaybackState.Builder builder = new PlaybackState.Builder();

        // Do not disconnect on an event that isn't "playing"
        builder.setState(PlaybackState.STATE_PAUSED, 0, 1);
        controllerCb.onPlaybackStateChanged(builder.build());
        verify(mMockBrowser, times(0)).disconnect();

        // Once we're told we're playing, make sure we disconnect
        builder.setState(PlaybackState.STATE_PLAYING, 0, 1);
        controllerCb.onPlaybackStateChanged(builder.build());
        verify(mMockBrowser, times(1)).disconnect();
    }

    @Test
    public void testPlayItem_Timeout() {
        BrowsedPlayerWrapper wrapper =
                BrowsedPlayerWrapper.wrap(null, mThread.getLooper(), "test", "test");
        verify(mMockBrowser).testInit(any(), any(), mBrowserConnCb.capture(), any());
        MediaBrowser.ConnectionCallback browserConnCb = mBrowserConnCb.getValue();

        wrapper.playItem("test_item");
        verify(mMockBrowser, times(1)).connect();

        MediaController mockController = mock(MediaController.class);
        MediaController.TransportControls mockTransport =
                mock(MediaController.TransportControls.class);
        when(mockController.getTransportControls()).thenReturn(mockTransport);
        MediaControllerFactory.inject(mockController);

        browserConnCb.onConnected();
        verify(mockTransport).playFromMediaId(eq("test_item"), eq(null));

        verify(mockController).registerCallback(any(), mTimeoutHandler.capture());
        Handler timeoutHandler = mTimeoutHandler.getValue();

        timeoutHandler.sendEmptyMessage(BrowsedPlayerWrapper.TimeoutHandler.MSG_TIMEOUT);

        verify(mMockBrowser, timeout(2000).times(1)).disconnect();
    }

    @Test
    public void testGetFolderItems() {
        BrowsedPlayerWrapper wrapper =
                BrowsedPlayerWrapper.wrap(null, mThread.getLooper(), "test", "test");
        verify(mMockBrowser).testInit(any(), any(), mBrowserConnCb.capture(), any());
        MediaBrowser.ConnectionCallback browserConnCb = mBrowserConnCb.getValue();

        wrapper.getFolderItems("test_folder", mBrowseCb);


        browserConnCb.onConnected();
        verify(mMockBrowser).subscribe(any(), mSubscriptionCb.capture());
        MediaBrowser.SubscriptionCallback subscriptionCb = mSubscriptionCb.getValue();

        ArrayList<MediaItem> items = new ArrayList<MediaItem>();
        MediaDescription.Builder bob = new MediaDescription.Builder();
        bob.setTitle("test_song1");
        bob.setMediaId("ts1");
        items.add(new MediaItem(bob.build(), 0));
        bob.setTitle("test_song2");
        bob.setMediaId("ts2");
        items.add(new MediaItem(bob.build(), 0));

        subscriptionCb.onChildrenLoaded("test_folder", items);
        verify(mMockBrowser).unsubscribe(eq("test_folder"));
        verify(mBrowseCb).run(eq(BrowsedPlayerWrapper.STATUS_SUCCESS), eq("test_folder"),
                mWrapperBrowseCb.capture());

        List<ListItem> item_list = mWrapperBrowseCb.getValue();
        for (int i = 0; i < item_list.size(); i++) {
            Assert.assertFalse(item_list.get(i).isFolder);
            Assert.assertEquals(item_list.get(i).song, Util.toMetadata(items.get(i)));
        }

        verify(mMockBrowser).disconnect();
    }
}
