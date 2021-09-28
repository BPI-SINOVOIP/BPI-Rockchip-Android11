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
package com.android.bluetooth.avrcpcontroller;

import static org.mockito.Mockito.*;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothAvrcpController;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.media.AudioManager;
import android.os.Looper;
import android.support.v4.media.session.MediaControllerCompat;
import android.support.v4.media.session.PlaybackStateCompat;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.FlakyTest;
import androidx.test.filters.MediumTest;
import androidx.test.rule.ServiceTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.bluetooth.R;
import com.android.bluetooth.TestUtils;
import com.android.bluetooth.a2dpsink.A2dpSinkService;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.ProfileService;

import org.hamcrest.core.IsInstanceOf;
import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class AvrcpControllerStateMachineTest {
    private static final int ASYNC_CALL_TIMEOUT_MILLIS = 100;
    private static final int CONNECT_TIMEOUT_TEST_MILLIS = 1000;
    private static final int KEY_DOWN = 0;
    private static final int KEY_UP = 1;
    private AvrcpControllerStateMachine mAvrcpStateMachine;
    private BluetoothAdapter mAdapter;
    private Context mTargetContext;
    private BluetoothDevice mTestDevice;
    private ArgumentCaptor<Intent> mIntentArgument = ArgumentCaptor.forClass(Intent.class);
    private byte[] mTestAddress = new byte[]{00, 01, 02, 03, 04, 05};

    @Rule public final ServiceTestRule mAvrcpServiceRule = new ServiceTestRule();
    @Rule public final ServiceTestRule mA2dpServiceRule = new ServiceTestRule();

    @Mock
    private AdapterService mAvrcpAdapterService;

    @Mock
    private AdapterService mA2dpAdapterService;

    @Mock
    private AudioManager mAudioManager;
    @Mock
    private AvrcpControllerService mAvrcpControllerService;
    @Mock
    private A2dpSinkService mA2dpSinkService;

    @Mock
    private Resources mMockResources;


    @Before
    public void setUp() throws Exception {
        mTargetContext = InstrumentationRegistry.getTargetContext();
        Assume.assumeTrue("Ignore test when AVRCP Controller is not enabled",
                mTargetContext.getResources().getBoolean(
                        R.bool.profile_supported_avrcp_controller));
        if (Looper.myLooper() == null) {
            Looper.prepare();
        }
        Assert.assertNotNull(Looper.myLooper());

        // Setup mocks and test assets
        MockitoAnnotations.initMocks(this);
        TestUtils.setAdapterService(mAvrcpAdapterService);
        TestUtils.startService(mAvrcpServiceRule, AvrcpControllerService.class);
        TestUtils.clearAdapterService(mAvrcpAdapterService);
        TestUtils.setAdapterService(mA2dpAdapterService);
        TestUtils.startService(mA2dpServiceRule, A2dpSinkService.class);
        when(mA2dpSinkService.setActiveDeviceNative(any())).thenReturn(true);

        when(mMockResources.getBoolean(R.bool.a2dp_sink_automatically_request_audio_focus))
                .thenReturn(true);
        doReturn(mMockResources).when(mAvrcpControllerService).getResources();
        A2dpSinkService.setA2dpSinkService(mA2dpSinkService);
        doReturn(15).when(mAudioManager).getStreamMaxVolume(anyInt());
        doReturn(8).when(mAudioManager).getStreamVolume(anyInt());
        doReturn(true).when(mAudioManager).isVolumeFixed();
        doReturn(mAudioManager).when(mAvrcpControllerService)
                .getSystemService(Context.AUDIO_SERVICE);

        // This line must be called to make sure relevant objects are initialized properly
        mAdapter = BluetoothAdapter.getDefaultAdapter();
        // Get a device for testing
        mTestDevice = mAdapter.getRemoteDevice(mTestAddress);
        mAvrcpControllerService.start();
        mAvrcpControllerService.sBrowseTree = new BrowseTree(null);
        mAvrcpStateMachine = new AvrcpControllerStateMachine(mTestDevice, mAvrcpControllerService);
        mAvrcpStateMachine.start();
    }

    @After
    public void tearDown() throws Exception {
        if (!mTargetContext.getResources().getBoolean(R.bool.profile_supported_avrcp_controller)) {
            return;
        }

        mAvrcpStateMachine.disconnect();
        TestUtils.waitForLooperToFinishScheduledTask(mAvrcpStateMachine.getHandler().getLooper());
        Assert.assertFalse(mAvrcpStateMachine.isActive());

        TestUtils.clearAdapterService(mA2dpAdapterService);
    }

    /**
     * Test to confirm that the state machine is capable of cycling through the 4
     * connection states, and that upon completion, it cleans up aftwards.
     */
    @Test
    public void testDisconnect() {
        int numBroadcastsSent = setUpConnectedState(true, true);
        StackEvent event =
                StackEvent.connectionStateChanged(false, false);

        mAvrcpStateMachine.disconnect();
        numBroadcastsSent += 2;
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(numBroadcastsSent)).sendBroadcast(
                mIntentArgument.capture(), eq(ProfileService.BLUETOOTH_PERM));
        Assert.assertEquals(mTestDevice, mIntentArgument.getValue().getParcelableExtra(
                BluetoothDevice.EXTRA_DEVICE));
        Assert.assertEquals(BluetoothAvrcpController.ACTION_CONNECTION_STATE_CHANGED,
                mIntentArgument.getValue().getAction());
        Assert.assertEquals(BluetoothProfile.STATE_DISCONNECTED,
                mIntentArgument.getValue().getIntExtra(BluetoothProfile.EXTRA_STATE, -1));
        Assert.assertThat(mAvrcpStateMachine.getCurrentState(),
                IsInstanceOf.instanceOf(AvrcpControllerStateMachine.Disconnected.class));
        Assert.assertEquals(mAvrcpStateMachine.getState(), BluetoothProfile.STATE_DISCONNECTED);
        verify(mAvrcpControllerService).removeStateMachine(eq(mAvrcpStateMachine));
        MediaControllerCompat.TransportControls transportControls =
                BluetoothMediaBrowserService.getTransportControls();
        Assert.assertEquals(PlaybackStateCompat.STATE_ERROR,
                BluetoothMediaBrowserService.getPlaybackState());
    }

    /**
     * Test to confirm that a control only device can be established (no browsing)
     */
    @Test
    public void testControlOnly() {
        int numBroadcastsSent = setUpConnectedState(true, false);
        MediaControllerCompat.TransportControls transportControls =
                BluetoothMediaBrowserService.getTransportControls();
        Assert.assertNotNull(transportControls);
        Assert.assertEquals(PlaybackStateCompat.STATE_NONE,
                BluetoothMediaBrowserService.getPlaybackState());
        StackEvent event =
                StackEvent.connectionStateChanged(false, false);
        mAvrcpStateMachine.disconnect();
        numBroadcastsSent += 2;
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(numBroadcastsSent)).sendBroadcast(
                mIntentArgument.capture(), eq(ProfileService.BLUETOOTH_PERM));
        Assert.assertEquals(mTestDevice, mIntentArgument.getValue().getParcelableExtra(
                BluetoothDevice.EXTRA_DEVICE));
        Assert.assertEquals(BluetoothAvrcpController.ACTION_CONNECTION_STATE_CHANGED,
                mIntentArgument.getValue().getAction());
        Assert.assertEquals(BluetoothProfile.STATE_DISCONNECTED,
                mIntentArgument.getValue().getIntExtra(BluetoothProfile.EXTRA_STATE, -1));
        Assert.assertThat(mAvrcpStateMachine.getCurrentState(),
                IsInstanceOf.instanceOf(AvrcpControllerStateMachine.Disconnected.class));
        Assert.assertEquals(mAvrcpStateMachine.getState(), BluetoothProfile.STATE_DISCONNECTED);
        verify(mAvrcpControllerService).removeStateMachine(eq(mAvrcpStateMachine));
        Assert.assertEquals(PlaybackStateCompat.STATE_ERROR,
                BluetoothMediaBrowserService.getPlaybackState());
    }

    /**
     * Test to confirm that a browsing only device can be established (no control)
     */
    @Test
    @FlakyTest
    public void testBrowsingOnly() {
        Assert.assertEquals(0, mAvrcpControllerService.sBrowseTree.mRootNode.getChildrenCount());
        int numBroadcastsSent = setUpConnectedState(false, true);
        Assert.assertEquals(1, mAvrcpControllerService.sBrowseTree.mRootNode.getChildrenCount());
        Assert.assertEquals(PlaybackStateCompat.STATE_NONE,
                BluetoothMediaBrowserService.getPlaybackState());
        StackEvent event =
                StackEvent.connectionStateChanged(false, false);
        mAvrcpStateMachine.disconnect();
        numBroadcastsSent += 2;
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(numBroadcastsSent)).sendBroadcast(
                mIntentArgument.capture(), eq(ProfileService.BLUETOOTH_PERM));
        Assert.assertEquals(mTestDevice, mIntentArgument.getValue().getParcelableExtra(
                BluetoothDevice.EXTRA_DEVICE));
        Assert.assertEquals(BluetoothAvrcpController.ACTION_CONNECTION_STATE_CHANGED,
                mIntentArgument.getValue().getAction());
        Assert.assertEquals(BluetoothProfile.STATE_DISCONNECTED,
                mIntentArgument.getValue().getIntExtra(BluetoothProfile.EXTRA_STATE, -1));
        Assert.assertThat(mAvrcpStateMachine.getCurrentState(),
                IsInstanceOf.instanceOf(AvrcpControllerStateMachine.Disconnected.class));
        Assert.assertEquals(mAvrcpStateMachine.getState(), BluetoothProfile.STATE_DISCONNECTED);
        verify(mAvrcpControllerService).removeStateMachine(eq(mAvrcpStateMachine));
        MediaControllerCompat.TransportControls transportControls =
                BluetoothMediaBrowserService.getTransportControls();
        Assert.assertEquals(PlaybackStateCompat.STATE_ERROR,
                BluetoothMediaBrowserService.getPlaybackState());
    }

    /**
     * Test to make sure the state machine is tracking the correct device
     */
    @Test
    public void testGetDevice() {
        Assert.assertEquals(mAvrcpStateMachine.getDevice(), mTestDevice);
    }

    /**
     * Test that dumpsys will generate information about connected devices
     */
    @Test
    public void testDump() {
        StringBuilder sb = new StringBuilder();
        mAvrcpStateMachine.dump(sb);
        Assert.assertEquals(sb.toString(),
                "  mDevice: " + mTestDevice.toString()
                + "(null) name=AvrcpControllerStateMachine state=Disconnected\n"
                + "  isActive: false\n");
    }

    /**
     * Test media browser play command
     */
    @Test
    public void testPlay() throws Exception {
        setUpConnectedState(true, true);
        MediaControllerCompat.TransportControls transportControls =
                BluetoothMediaBrowserService.getTransportControls();

        //Play
        transportControls.play();
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_PLAY), eq(KEY_DOWN));
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_PLAY), eq(KEY_UP));
    }

    /**
     * Test media browser pause command
     */
    @Test
    public void testPause() throws Exception {
        setUpConnectedState(true, true);
        MediaControllerCompat.TransportControls transportControls =
                BluetoothMediaBrowserService.getTransportControls();

        //Pause
        transportControls.pause();
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_PAUSE), eq(KEY_DOWN));
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_PAUSE), eq(KEY_UP));
    }

    /**
     * Test media browser stop command
     */
    @Test
    public void testStop() throws Exception {
        setUpConnectedState(true, true);
        MediaControllerCompat.TransportControls transportControls =
                BluetoothMediaBrowserService.getTransportControls();

        //Stop
        transportControls.stop();
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_STOP), eq(KEY_DOWN));
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_STOP), eq(KEY_UP));
    }

    /**
     * Test media browser next command
     */
    @Test
    public void testNext() throws Exception {
        setUpConnectedState(true, true);
        MediaControllerCompat.TransportControls transportControls =
                BluetoothMediaBrowserService.getTransportControls();

        //Next
        transportControls.skipToNext();
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_FORWARD),
                eq(KEY_DOWN));
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_FORWARD), eq(KEY_UP));
    }

    /**
     * Test media browser previous command
     */
    @Test
    public void testPrevious() throws Exception {
        setUpConnectedState(true, true);
        MediaControllerCompat.TransportControls transportControls =
                BluetoothMediaBrowserService.getTransportControls();

        //Previous
        transportControls.skipToPrevious();
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_BACKWARD),
                eq(KEY_DOWN));
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_BACKWARD), eq(KEY_UP));
    }

    /**
     * Test media browser fast forward command
     */
    @Test
    @FlakyTest
    public void testFastForward() throws Exception {
        setUpConnectedState(true, true);
        MediaControllerCompat.TransportControls transportControls =
                BluetoothMediaBrowserService.getTransportControls();

        //FastForward
        transportControls.fastForward();
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_FF), eq(KEY_DOWN));
        //Finish FastForwarding
        transportControls.play();
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_FF), eq(KEY_UP));
    }

    /**
     * Test media browser rewind command
     */
    @Test
    public void testRewind() throws Exception {
        setUpConnectedState(true, true);
        MediaControllerCompat.TransportControls transportControls =
                BluetoothMediaBrowserService.getTransportControls();

        //Rewind
        transportControls.rewind();
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_REWIND), eq(KEY_DOWN));
        //Finish Rewinding
        transportControls.play();
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_REWIND), eq(KEY_UP));
    }

    /**
     * Test media browser skip to queue item
     */
    @Test
    public void testSkipToQueueInvalid() throws Exception {
        byte scope = 1;
        int minSize = 0;
        int maxSize = 255;
        setUpConnectedState(true, true);
        MediaControllerCompat.TransportControls transportControls =
                BluetoothMediaBrowserService.getTransportControls();

        //Play an invalid item below start
        transportControls.skipToQueueItem(minSize - 1);
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(0)).playItemNative(
                eq(mTestAddress), eq(scope), anyLong(), anyInt());

        //Play an invalid item beyond end
        transportControls.skipToQueueItem(maxSize + 1);
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(0)).playItemNative(
                eq(mTestAddress), eq(scope), anyLong(), anyInt());
    }

    /**
     * Test media browser shuffle command
     */
    @Test
    public void testShuffle() throws Exception {
        byte[] shuffleSetting = new byte[]{3};
        byte[] shuffleMode = new byte[]{2};

        setUpConnectedState(true, true);
        MediaControllerCompat.TransportControls transportControls =
                BluetoothMediaBrowserService.getTransportControls();

        //Shuffle
        transportControls.setShuffleMode(1);
        verify(mAvrcpControllerService, timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1))
                .setPlayerApplicationSettingValuesNative(
                eq(mTestAddress), eq((byte) 1), eq(shuffleSetting), eq(shuffleMode));
    }

    /**
     * Test media browser repeat command
     */
    @Test
    public void testRepeat() throws Exception {
        byte[] repeatSetting = new byte[]{2};
        byte[] repeatMode = new byte[]{3};

        setUpConnectedState(true, true);
        MediaControllerCompat.TransportControls transportControls =
                BluetoothMediaBrowserService.getTransportControls();

        //Shuffle
        transportControls.setRepeatMode(2);
        verify(mAvrcpControllerService, timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1))
                .setPlayerApplicationSettingValuesNative(
                eq(mTestAddress), eq((byte) 1), eq(repeatSetting), eq(repeatMode));
    }

    /**
     * Test media browsing
     * Verify that a browse tree is created with the proper root
     * Verify that a player can be fetched and added to the browse tree
     * Verify that the contents of a player are fetched upon request
     */
    @Test
    @FlakyTest
    public void testBrowsingCommands() {
        setUpConnectedState(true, true);
        final String rootName = "__ROOT__";
        final String playerName = "Player 1";

        //Get the root of the device
        BrowseTree.BrowseNode results = mAvrcpStateMachine.findNode(rootName);
        Assert.assertEquals(rootName + mTestDevice.toString(), results.getID());

        //Request fetch the list of players
        BrowseTree.BrowseNode playerNodes = mAvrcpStateMachine.findNode(results.getID());
        mAvrcpStateMachine.requestContents(results);
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).getPlayerListNative(eq(mTestAddress),
                eq(0), eq(19));

        //Provide back a player object
        byte[] playerFeatures =
                new byte[]{0, 0, 0, 0, 0, (byte) 0xb7, 0x01, 0x0c, 0x0a, 0, 0, 0, 0, 0, 0, 0};
        AvrcpPlayer playerOne = new AvrcpPlayer(mTestDevice, 1, playerName, playerFeatures, 1, 1);
        List<AvrcpPlayer> testPlayers = new ArrayList<>();
        testPlayers.add(playerOne);
        mAvrcpStateMachine.sendMessage(AvrcpControllerStateMachine.MESSAGE_PROCESS_GET_PLAYER_ITEMS,
                testPlayers);

        //Verify that the player object is available.
        mAvrcpStateMachine.requestContents(results);
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).getPlayerListNative(eq(mTestAddress),
                eq(1), eq(0));
        mAvrcpStateMachine.sendMessage(
                AvrcpControllerStateMachine.MESSAGE_PROCESS_GET_FOLDER_ITEMS_OUT_OF_RANGE);
        playerNodes = mAvrcpStateMachine.findNode(results.getID());
        Assert.assertEquals(true, results.isCached());
        Assert.assertEquals("MediaItem{mFlags=1, mDescription=" + playerName + ", null, null}",
                results.getChildren().get(0).getMediaItem().toString());

        //Fetch contents of that player object
        BrowseTree.BrowseNode playerOneNode = mAvrcpStateMachine.findNode(
                results.getChildren().get(0).getID());
        mAvrcpStateMachine.requestContents(playerOneNode);
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).setBrowsedPlayerNative(
                eq(mTestAddress), eq(1));
        mAvrcpStateMachine.sendMessage(AvrcpControllerStateMachine.MESSAGE_PROCESS_FOLDER_PATH, 5);
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).getFolderListNative(eq(mTestAddress),
                eq(0), eq(4));
    }

    /**
     * Make an AvrcpItem suitable for being included in the Now Playing list for the test device
     */
    private AvrcpItem makeNowPlayingItem(long uid, String name) {
        AvrcpItem.Builder aib = new AvrcpItem.Builder();
        aib.setDevice(mTestDevice);
        aib.setItemType(AvrcpItem.TYPE_MEDIA);
        aib.setType(AvrcpItem.MEDIA_AUDIO);
        aib.setTitle(name);
        aib.setUid(uid);
        aib.setUuid(UUID.randomUUID().toString());
        aib.setPlayable(true);
        return aib.build();
    }

    /**
     * Get the current Now Playing list for the test device
     */
    private List<AvrcpItem> getNowPlayingList() {
        BrowseTree.BrowseNode nowPlaying = mAvrcpStateMachine.findNode("NOW_PLAYING");
        List<AvrcpItem> nowPlayingList = new ArrayList<AvrcpItem>();
        for (BrowseTree.BrowseNode child : nowPlaying.getChildren()) {
            nowPlayingList.add(child.mItem);
        }
        return nowPlayingList;
    }

    /**
     * Set the current Now Playing list for the test device
     */
    private void setNowPlayingList(List<AvrcpItem> nowPlayingList) {
        BrowseTree.BrowseNode nowPlaying = mAvrcpStateMachine.findNode("NOW_PLAYING");
        mAvrcpStateMachine.requestContents(nowPlaying);
        mAvrcpStateMachine.sendMessage(
                AvrcpControllerStateMachine.MESSAGE_PROCESS_GET_FOLDER_ITEMS, nowPlayingList);
        mAvrcpStateMachine.sendMessage(
                AvrcpControllerStateMachine.MESSAGE_PROCESS_GET_FOLDER_ITEMS_OUT_OF_RANGE);

        // Wait for the now playing list to be propagated
        TestUtils.waitForLooperToFinishScheduledTask(mAvrcpStateMachine.getHandler().getLooper());

        // Make sure its set by re grabbing the node and checking its contents are cached
        nowPlaying = mAvrcpStateMachine.findNode("NOW_PLAYING");
        Assert.assertTrue(nowPlaying.isCached());
        assertNowPlayingList(nowPlayingList);
    }

    /**
     * Assert that the Now Playing list is a particular value
     */
    private void assertNowPlayingList(List<AvrcpItem> expected) {
        List<AvrcpItem> current = getNowPlayingList();
        Assert.assertEquals(expected.size(), current.size());
        for (int i = 0; i < expected.size(); i++) {
            Assert.assertEquals(expected.get(i), current.get(i));
        }
    }

    /**
     * Test addressed media player changing to a player we know about
     * Verify when the addressed media player changes browsing data updates
     */
    @Test
    public void testPlayerChanged() {
        setUpConnectedState(true, true);
        final String rootName = "__ROOT__";

        //Get the root of the device
        BrowseTree.BrowseNode results = mAvrcpStateMachine.findNode(rootName);
        Assert.assertEquals(rootName + mTestDevice.toString(), results.getID());

        //Request fetch the list of players
        BrowseTree.BrowseNode playerNodes = mAvrcpStateMachine.findNode(results.getID());
        mAvrcpStateMachine.requestContents(results);
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).getPlayerListNative(eq(mTestAddress),
                eq(0), eq(19));

        //Provide back two player objects, IDs 1 and 2
        byte[] playerFeatures =
                new byte[]{0, 0, 0, 0, 0, (byte) 0xb7, 0x01, 0x0c, 0x0a, 0, 0, 0, 0, 0, 0, 0};
        AvrcpPlayer playerOne = new AvrcpPlayer(mTestDevice, 1, "Player 1", playerFeatures, 1, 1);
        AvrcpPlayer playerTwo = new AvrcpPlayer(mTestDevice, 2, "Player 2", playerFeatures, 1, 1);
        List<AvrcpPlayer> testPlayers = new ArrayList<>();
        testPlayers.add(playerOne);
        testPlayers.add(playerTwo);
        mAvrcpStateMachine.sendMessage(AvrcpControllerStateMachine.MESSAGE_PROCESS_GET_PLAYER_ITEMS,
                testPlayers);

        //Set something arbitrary for the current Now Playing list
        List<AvrcpItem> nowPlayingList = new ArrayList<AvrcpItem>();
        nowPlayingList.add(makeNowPlayingItem(1, "Song 1"));
        nowPlayingList.add(makeNowPlayingItem(2, "Song 2"));
        setNowPlayingList(nowPlayingList);

        //Change players and verify that BT attempts to update the results
        mAvrcpStateMachine.sendMessage(
                AvrcpControllerStateMachine.MESSAGE_PROCESS_ADDRESSED_PLAYER_CHANGED, 2);
        TestUtils.waitForLooperToFinishScheduledTask(mAvrcpStateMachine.getHandler().getLooper());

        //Make sure the Now Playing list is now cleared
        assertNowPlayingList(new ArrayList<AvrcpItem>());

        //Verify that a player change to a player with Now Playing support causes a refresh. This
        //should be called twice, once to give data and once to ensure we're out of elements
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(2)).getNowPlayingListNative(
                eq(mTestAddress), eq(0), eq(19));
    }

    /**
     * Test addressed media player change to a player we don't know about
     * Verify when the addressed media player changes browsing data updates
     * Verify that the contents of a player are fetched upon request
     */
    @Test
    public void testPlayerChangedToUnknownPlayer() {
        setUpConnectedState(true, true);
        final String rootName = "__ROOT__";

        //Get the root of the device
        BrowseTree.BrowseNode rootNode = mAvrcpStateMachine.findNode(rootName);
        Assert.assertEquals(rootName + mTestDevice.toString(), rootNode.getID());

        //Request fetch the list of players
        BrowseTree.BrowseNode playerNodes = mAvrcpStateMachine.findNode(rootNode.getID());
        mAvrcpStateMachine.requestContents(rootNode);
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).getPlayerListNative(eq(mTestAddress),
                eq(0), eq(19));

        //Provide back a player object
        byte[] playerFeatures =
                new byte[]{0, 0, 0, 0, 0, (byte) 0xb7, 0x01, 0x0c, 0x0a, 0, 0, 0, 0, 0, 0, 0};
        AvrcpPlayer playerOne = new AvrcpPlayer(mTestDevice, 1, "Player 1", playerFeatures, 1, 1);
        List<AvrcpPlayer> testPlayers = new ArrayList<>();
        testPlayers.add(playerOne);
        mAvrcpStateMachine.sendMessage(
                AvrcpControllerStateMachine.MESSAGE_PROCESS_GET_PLAYER_ITEMS, testPlayers);

        //Set something arbitrary for the current Now Playing list
        List<AvrcpItem> nowPlayingList = new ArrayList<AvrcpItem>();
        nowPlayingList.add(makeNowPlayingItem(1, "Song 1"));
        nowPlayingList.add(makeNowPlayingItem(2, "Song 2"));
        setNowPlayingList(nowPlayingList);

        //Change players
        mAvrcpStateMachine.sendMessage(
                AvrcpControllerStateMachine.MESSAGE_PROCESS_ADDRESSED_PLAYER_CHANGED, 4);
        TestUtils.waitForLooperToFinishScheduledTask(mAvrcpStateMachine.getHandler().getLooper());

        //Make sure the Now Playing list is now cleared
        assertNowPlayingList(new ArrayList<AvrcpItem>());

        //Make sure the root node is no longer cached
        rootNode = mAvrcpStateMachine.findNode(rootName);
        Assert.assertFalse(rootNode.isCached());
    }

    /**
     * Test that the Now Playing playlist is updated when it changes.
     */
    @Test
    public void testNowPlaying() {
        setUpConnectedState(true, true);
        mAvrcpStateMachine.nowPlayingContentChanged();
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).getNowPlayingListNative(
                eq(mTestAddress), eq(0), eq(19));
    }

    /**
     * Test that AVRCP events such as playback commands can execute while performing browsing.
     */
    @Test
    public void testPlayWhileBrowsing() {
        setUpConnectedState(true, true);
        final String rootName = "__ROOT__";
        final String playerName = "Player 1";

        //Get the root of the device
        BrowseTree.BrowseNode results = mAvrcpStateMachine.findNode(rootName);
        Assert.assertEquals(rootName + mTestDevice.toString(), results.getID());

        //Request fetch the list of players
        BrowseTree.BrowseNode playerNodes = mAvrcpStateMachine.findNode(results.getID());
        mAvrcpStateMachine.requestContents(results);

        MediaControllerCompat.TransportControls transportControls =
                BluetoothMediaBrowserService.getTransportControls();
        transportControls.play();
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_PLAY), eq(KEY_DOWN));
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_PLAY), eq(KEY_UP));
    }

    /**
     * Test that Absolute Volume Registration is working
     */
    @Test
    public void testRegisterAbsVolumeNotification() {
        setUpConnectedState(true, true);
        mAvrcpStateMachine.sendMessage(
                AvrcpControllerStateMachine.MESSAGE_PROCESS_REGISTER_ABS_VOL_NOTIFICATION);
        verify(mAvrcpControllerService, timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1))
                .sendRegisterAbsVolRspNative(any(), anyByte(), eq(127), anyInt());
    }

    /**
     * Test playback does not request focus when another app is playing music.
     */
    @Test
    public void testPlaybackWhileMusicPlaying() {
        when(mMockResources.getBoolean(R.bool.a2dp_sink_automatically_request_audio_focus))
                .thenReturn(false);
        when(mA2dpSinkService.getFocusState()).thenReturn(AudioManager.AUDIOFOCUS_NONE);
        doReturn(true).when(mAudioManager).isMusicActive();
        setUpConnectedState(true, true);
        mAvrcpStateMachine.sendMessage(
                AvrcpControllerStateMachine.MESSAGE_PROCESS_PLAY_STATUS_CHANGED,
                PlaybackStateCompat.STATE_PLAYING);
        TestUtils.waitForLooperToFinishScheduledTask(mAvrcpStateMachine.getHandler().getLooper());
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_PAUSE), eq(KEY_DOWN));
        verify(mA2dpSinkService, never()).requestAudioFocus(mTestDevice, true);
    }

    /**
     * Test playback requests focus while nothing is playing music.
     */
    @Test
    public void testPlaybackWhileIdle() {
        when(mA2dpSinkService.getFocusState()).thenReturn(AudioManager.AUDIOFOCUS_NONE);
        doReturn(false).when(mAudioManager).isMusicActive();
        setUpConnectedState(true, true);
        mAvrcpStateMachine.sendMessage(
                AvrcpControllerStateMachine.MESSAGE_PROCESS_PLAY_STATUS_CHANGED,
                PlaybackStateCompat.STATE_PLAYING);
        TestUtils.waitForLooperToFinishScheduledTask(mAvrcpStateMachine.getHandler().getLooper());
        verify(mA2dpSinkService).requestAudioFocus(mTestDevice, true);
    }

    /**
     * Test receiving a playback status of playing while we're in an error state
     * as it relates to getting audio focus.
     *
     * Verify we send a pause command and never attempt to request audio focus
     */
    @Test
    public void testPlaybackWhileErrorState() {
        when(mA2dpSinkService.getFocusState()).thenReturn(AudioManager.ERROR);
        setUpConnectedState(true, true);
        mAvrcpStateMachine.sendMessage(
                AvrcpControllerStateMachine.MESSAGE_PROCESS_PLAY_STATUS_CHANGED,
                PlaybackStateCompat.STATE_PLAYING);
        TestUtils.waitForLooperToFinishScheduledTask(mAvrcpStateMachine.getHandler().getLooper());
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_PAUSE), eq(KEY_DOWN));
        verify(mA2dpSinkService, never()).requestAudioFocus(mTestDevice, true);
    }

    /**
     * Test receiving a playback status of playing while we have focus
     *
     * Verify we do not send a pause command and never attempt to request audio focus
     */
    @Test
    public void testPlaybackWhilePlayingState() {
        when(mA2dpSinkService.getFocusState()).thenReturn(AudioManager.AUDIOFOCUS_GAIN);
        setUpConnectedState(true, true);
        Assert.assertTrue(mAvrcpStateMachine.isActive());
        mAvrcpStateMachine.sendMessage(
                AvrcpControllerStateMachine.MESSAGE_PROCESS_PLAY_STATUS_CHANGED,
                PlaybackStateCompat.STATE_PLAYING);
        TestUtils.waitForLooperToFinishScheduledTask(mAvrcpStateMachine.getHandler().getLooper());
        verify(mAvrcpControllerService, never()).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_PAUSE), eq(KEY_DOWN));
        verify(mA2dpSinkService, never()).requestAudioFocus(mTestDevice, true);
    }

    /**
     * Test receiving a playback status of playing from a device that isn't active
     *
     * Verify we do not send a pause command and never attempt to request audio focus
     */
    @Test
    public void testPlaybackWhileNotActiveDevice() {
        byte[] secondTestAddress = new byte[]{00, 01, 02, 03, 04, 06};
        BluetoothDevice secondTestDevice = mAdapter.getRemoteDevice(secondTestAddress);
        AvrcpControllerStateMachine secondAvrcpStateMachine =
                new AvrcpControllerStateMachine(secondTestDevice, mAvrcpControllerService);
        secondAvrcpStateMachine.start();

        setUpConnectedState(true, true);
        secondAvrcpStateMachine.connect(StackEvent.connectionStateChanged(true, true));
        TestUtils.waitForLooperToFinishScheduledTask(secondAvrcpStateMachine.getHandler()
                .getLooper());

        Assert.assertTrue(secondAvrcpStateMachine.isActive());
        Assert.assertFalse(mAvrcpStateMachine.isActive());

        mAvrcpStateMachine.sendMessage(
                AvrcpControllerStateMachine.MESSAGE_PROCESS_PLAY_STATUS_CHANGED,
                PlaybackStateCompat.STATE_PLAYING);
        TestUtils.waitForLooperToFinishScheduledTask(mAvrcpStateMachine.getHandler().getLooper());
        verify(mAvrcpControllerService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendPassThroughCommandNative(
                eq(mTestAddress), eq(AvrcpControllerService.PASS_THRU_CMD_ID_PAUSE), eq(KEY_DOWN));
        verify(mA2dpSinkService, never()).requestAudioFocus(mTestDevice, true);

        secondAvrcpStateMachine.disconnect();
        TestUtils.waitForLooperToFinishScheduledTask(secondAvrcpStateMachine.getHandler()
                .getLooper());
        Assert.assertFalse(secondAvrcpStateMachine.isActive());
        Assert.assertFalse(mAvrcpStateMachine.isActive());
    }

    /**
     * Test that the correct device becomes active
     *
     * The first connected device is automatically active, additional ones are not.
     * After an explicit play command a device becomes active.
     */
    @Test
    public void testActiveDeviceManagement() {
        // Setup structures and verify initial conditions
        final String rootName = "__ROOT__";
        final String playerName = "Player 1";
        byte[] secondTestAddress = new byte[]{00, 01, 02, 03, 04, 06};
        BluetoothDevice secondTestDevice = mAdapter.getRemoteDevice(secondTestAddress);
        AvrcpControllerStateMachine secondAvrcpStateMachine =
                new AvrcpControllerStateMachine(secondTestDevice, mAvrcpControllerService);
        secondAvrcpStateMachine.start();
        Assert.assertFalse(mAvrcpStateMachine.isActive());

        // Connect device 1 and 2 and verify second one is set as active
        setUpConnectedState(true, true);
        TestUtils.waitForLooperToFinishScheduledTask(mAvrcpStateMachine.getHandler().getLooper());
        Assert.assertTrue(mAvrcpStateMachine.isActive());

        secondAvrcpStateMachine.connect(StackEvent.connectionStateChanged(true, true));
        TestUtils.waitForLooperToFinishScheduledTask(mAvrcpStateMachine.getHandler().getLooper());
        TestUtils.waitForLooperToFinishScheduledTask(secondAvrcpStateMachine.getHandler()
                .getLooper());

        Assert.assertFalse(mAvrcpStateMachine.isActive());
        Assert.assertTrue(secondAvrcpStateMachine.isActive());

        // Request the second device to play an item and verify active device switched
        BrowseTree.BrowseNode results = mAvrcpStateMachine.findNode(rootName);
        Assert.assertEquals(rootName + mTestDevice.toString(), results.getID());
        BrowseTree.BrowseNode playerNodes = mAvrcpStateMachine.findNode(results.getID());
        secondAvrcpStateMachine.playItem(playerNodes);
        TestUtils.waitForLooperToFinishScheduledTask(secondAvrcpStateMachine.getHandler()
                .getLooper());
        Assert.assertFalse(mAvrcpStateMachine.isActive());
        Assert.assertTrue(secondAvrcpStateMachine.isActive());

        secondAvrcpStateMachine.disconnect();
        TestUtils.waitForLooperToFinishScheduledTask(secondAvrcpStateMachine.getHandler()
                .getLooper());
        Assert.assertFalse(secondAvrcpStateMachine.isActive());
        Assert.assertFalse(mAvrcpStateMachine.isActive());
    }

    /**
     * Setup Connected State
     *
     * @return number of times mAvrcpControllerService.sendBroadcastAsUser() has been invoked
     */
    private int setUpConnectedState(boolean control, boolean browsing) {
        // Put test state machine into connected state
        Assert.assertThat(mAvrcpStateMachine.getCurrentState(),
                IsInstanceOf.instanceOf(AvrcpControllerStateMachine.Disconnected.class));

        mAvrcpStateMachine.connect(StackEvent.connectionStateChanged(control, browsing));
        TestUtils.waitForLooperToFinishScheduledTask(mAvrcpStateMachine.getHandler().getLooper());
        verify(mAvrcpControllerService, timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(2)).sendBroadcast(
                mIntentArgument.capture(), eq(ProfileService.BLUETOOTH_PERM));
        Assert.assertThat(mAvrcpStateMachine.getCurrentState(),
                IsInstanceOf.instanceOf(AvrcpControllerStateMachine.Connected.class));
        Assert.assertEquals(mAvrcpStateMachine.getState(), BluetoothProfile.STATE_CONNECTED);

        return BluetoothProfile.STATE_CONNECTED;
    }

}
