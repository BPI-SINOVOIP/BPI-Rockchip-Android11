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

package android.dropboxmanager.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.DropBoxManager;
import android.os.SystemClock;
import android.platform.test.annotations.AppModeFull;
import android.provider.Settings;
import android.support.test.uiautomator.UiDevice;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;
import java.text.MessageFormat;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import com.android.compatibility.common.util.AmUtils;
import com.android.compatibility.common.util.SystemUtil;
import com.android.internal.annotations.GuardedBy;

/**
 * Tests DropBox entry management
 */
@LargeTest
@RunWith(AndroidJUnit4.class)
public class DropBoxTests {
    private static final String ENABLED_TAG = "DropBoxTestsEnabledTag";
    private static final String LOW_PRIORITY_TAG = "DropBoxTestsLowPriorityTag";
    private static final String ANOTHER_LOW_PRIORITY_TAG = "AnotherDropBoxTestsLowPriorityTag";
    private static final long BROADCAST_RATE_LIMIT = 1000L;
    private static final long BROADCAST_DELAY_ALLOWED_ERROR = 200L;

    private static final String SET_RATE_LIMIT_SHELL_COMMAND = "cmd dropbox set-rate-limit {0}";
    private static final String ADD_LOW_PRIORITY_SHELL_COMMAND =
            "cmd dropbox add-low-priority {0}";
    private static final String RESTORE_DEFAULTS_SHELL_COMMAND = "cmd dropbox restore-defaults";

    private Context mContext;
    private DropBoxManager mDropBoxManager;

    private CountDownLatch mEnabledTagLatch = new CountDownLatch(0);
    private CountDownLatch mLowPriorityTagLatch = new CountDownLatch(0);
    private CountDownLatch mAnotherLowPriorityTagLatch = new CountDownLatch(0);

    private ArrayList<DropBoxEntryAddedData> mEnabledBuffer;
    private ArrayList<DropBoxEntryAddedData> mLowPriorityBuffer;
    private ArrayList<DropBoxEntryAddedData> mAnotherLowPriorityBuffer;

    public static class DropBoxEntryAddedData {
        String tag;
        long time;
        int droppedCount;
        long received;
    }

    private final BroadcastReceiver mDropBoxEntryAddedReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            final DropBoxEntryAddedData data = new DropBoxEntryAddedData();
            data.tag = intent.getStringExtra(DropBoxManager.EXTRA_TAG);
            data.time = intent.getLongExtra(DropBoxManager.EXTRA_TIME, 0);
            data.droppedCount = intent.getIntExtra(DropBoxManager.EXTRA_DROPPED_COUNT, 0);
            data.received = SystemClock.elapsedRealtime();
            if (ENABLED_TAG.equals(data.tag)) {
                mEnabledBuffer.add(data);
                mEnabledTagLatch.countDown();
            } else if (LOW_PRIORITY_TAG.equals(data.tag)) {
                mLowPriorityBuffer.add(data);
                mLowPriorityTagLatch.countDown();
            } else if (ANOTHER_LOW_PRIORITY_TAG.equals(data.tag)) {
                mAnotherLowPriorityBuffer.add(data);
                mAnotherLowPriorityTagLatch.countDown();
            }
        }
    };

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mDropBoxManager = mContext.getSystemService(DropBoxManager.class);

        AmUtils.waitForBroadcastIdle();

        final IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(DropBoxManager.ACTION_DROPBOX_ENTRY_ADDED);
        mContext.registerReceiver(mDropBoxEntryAddedReceiver, intentFilter);

        setTagLowPriority(LOW_PRIORITY_TAG);
        setTagLowPriority(ANOTHER_LOW_PRIORITY_TAG);

        setBroadcastRateLimitSetting(BROADCAST_RATE_LIMIT);
    }

    @After
    public void tearDown() throws Exception {
        mContext.unregisterReceiver(mDropBoxEntryAddedReceiver);

        // Restore dropbox defaults
        restoreDropboxDefaults();
    }

    private void sendExcessiveDropBoxEntries(String tag, int count, long delayPerEntry)
            throws Exception {
        int i = 0;
        mDropBoxManager.addText(tag, String.valueOf(i++));
        for (; i < count; i++) {
            Thread.sleep(delayPerEntry);
            mDropBoxManager.addText(tag, String.valueOf(i));
        }
    }

    /**
     * A single DropBox entry for a low priority tag should have their
     * ACTION_DROPBOX_ENTRY_ADDED broadcasts delayed
     */
    @Test
    public void testLowPrioritySingleEntry() throws Exception {
        final int nLowPriorityEntries = 1;

        mLowPriorityTagLatch = new CountDownLatch(nLowPriorityEntries);
        mLowPriorityBuffer = new ArrayList(nLowPriorityEntries);

        final long startTime = SystemClock.elapsedRealtime();
        mDropBoxManager.addText(LOW_PRIORITY_TAG, "test");

        assertTrue(mLowPriorityTagLatch.await(BROADCAST_RATE_LIMIT * 3 / 2,
                TimeUnit.MILLISECONDS));
        final long endTime = SystemClock.elapsedRealtime();

        assertEqualsWithinDelta("Broadcast not received at expected time", BROADCAST_RATE_LIMIT,
                endTime - startTime, BROADCAST_DELAY_ALLOWED_ERROR);

        assertEquals("A single broadcast should be sent for a single low priority dropbox entry",
                1, mLowPriorityBuffer.size());
        DropBoxEntryAddedData data = mLowPriorityBuffer.get(0);
        assertEquals("Dropped broadcast count should be 0",
                0, data.droppedCount);
    }

    /**
     * Many contemporary DropBox entries for a low priority tag should have their
     * ACTION_DROPBOX_ENTRY_ADDED broadcasts collapsed into one broadcast
     */
    @Test
    public void testLowPriorityRapidEntryLimiting() throws Exception {
        final int nLowPriorityEntries = 10;

        mLowPriorityTagLatch = new CountDownLatch(1);
        mLowPriorityBuffer = new ArrayList(nLowPriorityEntries * 2);

        // add several low priority entries in quick sucession
        final long startTime = SystemClock.elapsedRealtime();
        sendExcessiveDropBoxEntries(LOW_PRIORITY_TAG, nLowPriorityEntries, 0);
        assertTrue(mLowPriorityTagLatch.await(BROADCAST_RATE_LIMIT * 3 / 2,
                TimeUnit.MILLISECONDS));
        final long endTime = SystemClock.elapsedRealtime();

        assertEqualsWithinDelta("Broadcast not received at expected time", BROADCAST_RATE_LIMIT,
                endTime - startTime, BROADCAST_DELAY_ALLOWED_ERROR);

        assertEquals("Many low priority dropbox entries within the rate limit period should " +
                     "result in 1 broadcast", 1, mLowPriorityBuffer.size());
        DropBoxEntryAddedData data = mLowPriorityBuffer.get(0);
        assertEquals("All but one of the low priority broadcasts should have been dropped",
                nLowPriorityEntries - 1, data.droppedCount);
    }

    /**
     * Many DropBox entries for a low priority tag should have their
     * ACTION_DROPBOX_ENTRY_ADDED broadcasts collapsed into a few broadcast
     */
    @Test
    public void testLowPrioritySustainedRapidEntryLimiting() throws Exception {
        final int nLowPriorityEntries = 10;

        mLowPriorityTagLatch = new CountDownLatch(2);
        mLowPriorityBuffer = new ArrayList(nLowPriorityEntries * 2);

        // add several low priority entries across the rate limit period
        final long startTime = SystemClock.elapsedRealtime();
        sendExcessiveDropBoxEntries(LOW_PRIORITY_TAG, nLowPriorityEntries,
                BROADCAST_RATE_LIMIT * 3 / 2 / nLowPriorityEntries);
        assertTrue(mLowPriorityTagLatch.await(BROADCAST_RATE_LIMIT * 5 / 2,
                TimeUnit.MILLISECONDS));
        final long endTime = SystemClock.elapsedRealtime();

        assertEqualsWithinDelta("Broadcast not received at expected time",
                BROADCAST_RATE_LIMIT * 2, endTime - startTime, BROADCAST_DELAY_ALLOWED_ERROR * 2);

        assertEquals("Many low priority dropbox entries across two rate limit periods should " +
                "result in 2 broadcasts", 2, mLowPriorityBuffer.size());
        DropBoxEntryAddedData data = mLowPriorityBuffer.get(0);
        int droppedCount = data.droppedCount;
        data = mLowPriorityBuffer.get(1);
        droppedCount += data.droppedCount;
        assertEquals("All but two of the low priority broadcasts should have been dropped",
                nLowPriorityEntries - 2, droppedCount);
    }

    /**
     * Many contemporary DropBox entries from multiple low priority tag should have their
     * ACTION_DROPBOX_ENTRY_ADDED broadcasts collapsed into seperate broadcasts per tag.
     * Different tags should not interfer with each others' broadcasts
     */
    @Test
    public void testMultipleLowPriorityRateLimiting() throws Exception {
        final int nLowPriorityEntries = 10;
        final int nOtherEntries = 10;

        mLowPriorityTagLatch = new CountDownLatch(1);
        mLowPriorityBuffer = new ArrayList(nLowPriorityEntries * 2);
        mAnotherLowPriorityTagLatch = new CountDownLatch(1);
        mAnotherLowPriorityBuffer = new ArrayList(nOtherEntries * 2);

        final long delayTime = BROADCAST_RATE_LIMIT / 2;

        // add several low priority entries across multiple tags
        final long firstEntryTime = SystemClock.elapsedRealtime();
        sendExcessiveDropBoxEntries(LOW_PRIORITY_TAG, nLowPriorityEntries, 0);
        Thread.sleep(delayTime);
        final long startTime = SystemClock.elapsedRealtime();
        sendExcessiveDropBoxEntries(ANOTHER_LOW_PRIORITY_TAG, nOtherEntries, 0);
        assertTrue(mAnotherLowPriorityTagLatch.await(BROADCAST_RATE_LIMIT * 3 / 2,
                TimeUnit.MILLISECONDS));
        final long endTime = SystemClock.elapsedRealtime();

        assertEqualsWithinDelta("Broadcast not received at expected time", BROADCAST_RATE_LIMIT,
                endTime - startTime, BROADCAST_DELAY_ALLOWED_ERROR);

        assertEquals("Many low priority dropbox entries within the rate limit period should " +
                "result in 1 broadcast for " + LOW_PRIORITY_TAG, 1, mLowPriorityBuffer.size());
        assertEquals("Many low priority dropbox entries within the rate limit period should " +
                "result in 1 broadcastfor " + ANOTHER_LOW_PRIORITY_TAG, 1,
                mAnotherLowPriorityBuffer.size());
        DropBoxEntryAddedData data = mLowPriorityBuffer.get(0);
        DropBoxEntryAddedData anotherData = mAnotherLowPriorityBuffer.get(0);
        assertEquals("All but one of the low priority broadcasts should have been dropped for " +
                        LOW_PRIORITY_TAG, nLowPriorityEntries - 1, data.droppedCount);
        assertEquals("All but one of the low priority broadcasts should have been dropped for " +
                        ANOTHER_LOW_PRIORITY_TAG, nOtherEntries - 1, anotherData.droppedCount);

        final long startTimeDelta = startTime - firstEntryTime;
        final long receivedTimeDelta = anotherData.received - data.received;
        final long errorMargin = receivedTimeDelta - startTimeDelta;

        // Received time delta should be around start time delta (20% margin of error)
        if (errorMargin < -startTimeDelta / 5  || errorMargin > startTimeDelta / 5 ) {
            fail("Multiple low priority entry tags interfered with each others delayed broadcast" +
                    "\nstartTimeDelta = " + String.valueOf(startTimeDelta) +
                    "\nreceivedTimeDelta = " + String.valueOf(receivedTimeDelta));
        }
    }

    /**
     * Broadcasts for regular priority DropBox entries should not be throttled and they should not
     * interfere with the throttling of low priority Dropbox entry broadcasts.
     */
    @Test
    public void testLowPriorityRateLimitingWithEnabledEntries() throws Exception {

        final int nLowPriorityEntries = 10;
        final int nEnabledEntries = 10;

        mLowPriorityTagLatch = new CountDownLatch(1);
        mLowPriorityBuffer = new ArrayList(nLowPriorityEntries * 2);
        mEnabledTagLatch = new CountDownLatch(nEnabledEntries);
        mEnabledBuffer = new ArrayList(nEnabledEntries * 2);

        final long startTimeDelta = BROADCAST_RATE_LIMIT / 2;

        final long startTime = SystemClock.elapsedRealtime();
        // add several low priority and enabled entries
        sendExcessiveDropBoxEntries(LOW_PRIORITY_TAG, nLowPriorityEntries, 0);
        sendExcessiveDropBoxEntries(ENABLED_TAG, nEnabledEntries, 0);
        assertTrue(mLowPriorityTagLatch.await(BROADCAST_RATE_LIMIT * 3 / 2,
                TimeUnit.MILLISECONDS));
        final long endTime = SystemClock.elapsedRealtime();

        assertEqualsWithinDelta("Broadcast not received at expected time", BROADCAST_RATE_LIMIT,
                endTime - startTime, BROADCAST_DELAY_ALLOWED_ERROR);

        assertEquals("Broadcasts for enabled tags should not be limited", nEnabledEntries,
                mEnabledBuffer.size());

        assertEquals("Many low priority dropbox entries within the rate limit period should " +
                "result in 1 broadcast for " + LOW_PRIORITY_TAG, 1, mLowPriorityBuffer.size());
        DropBoxEntryAddedData data = mLowPriorityBuffer.get(0);
        assertEquals("All but one of the low priority broadcasts should have been dropped " +
                LOW_PRIORITY_TAG, nLowPriorityEntries - 1, data.droppedCount);

        for (int i = 0; i < nEnabledEntries; i++) {
            DropBoxEntryAddedData enabledData = mEnabledBuffer.get(i);
            assertEquals("Enabled tag broadcasts should not be dropped", 0,
                    enabledData.droppedCount);
        }
    }

    private void setTagLowPriority(String tag) throws IOException {
        final String putCmd = MessageFormat.format(ADD_LOW_PRIORITY_SHELL_COMMAND, tag);
        SystemUtil.runShellCommand(putCmd);
    }

    private void setBroadcastRateLimitSetting(long period) throws IOException {
        final String putCmd = MessageFormat.format(SET_RATE_LIMIT_SHELL_COMMAND,
                String.valueOf(period));
        SystemUtil.runShellCommand(putCmd);
    }

    private void restoreDropboxDefaults() throws IOException {
        SystemUtil.runShellCommand(RESTORE_DEFAULTS_SHELL_COMMAND);
    }

    private void assertEqualsWithinDelta(String msg, long expected, long actual, long delta) {
        if (expected - actual > delta || actual - expected > delta) {
            assertEquals(msg, expected, actual);
        }
    }
}
