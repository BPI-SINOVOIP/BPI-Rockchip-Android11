/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.tv.data;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.filters.SmallTest;

import com.android.tv.data.WatchedHistoryManager.WatchedRecord;
import com.android.tv.testing.constants.ConfigConstants;

import com.google.common.util.concurrent.MoreExecutors;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.concurrent.TimeUnit;

/** Test for {@link WatchedHistoryManagerTest}. */
@SmallTest
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class WatchedHistoryManagerTest {
    // Wait time for expected success.
    private static final int MAX_HISTORY_SIZE = 100;

    private WatchedHistoryManager mWatchedHistoryManager;
    private TestWatchedHistoryManagerListener mListener;

    @Before
    public void setUp() {
        mWatchedHistoryManager =
                new WatchedHistoryManager(
                        RuntimeEnvironment.application,
                        MAX_HISTORY_SIZE,
                        MoreExecutors.directExecutor());
        mListener = new TestWatchedHistoryManagerListener();
        mWatchedHistoryManager.setListener(mListener);
    }

    private void startAndWaitForComplete() {
        mWatchedHistoryManager.start();
        assertThat(mListener.mLoadFinished).isTrue();
    }

    @Test
    public void testIsLoaded() {
        startAndWaitForComplete();
        assertThat(mWatchedHistoryManager.isLoaded()).isTrue();
    }

    @Test
    public void testLogChannelViewStop() {
        startAndWaitForComplete();
        long fakeId = 100000000;
        long time = System.currentTimeMillis();
        long duration = TimeUnit.MINUTES.toMillis(10);
        ChannelImpl channel = new ChannelImpl.Builder().setId(fakeId).build();
        mWatchedHistoryManager.logChannelViewStop(channel, time, duration);

        WatchedRecord record = mWatchedHistoryManager.getRecord(0);
        WatchedRecord recordFromSharedPreferences =
                mWatchedHistoryManager.getRecordFromSharedPreferences(0);
        assertThat(fakeId).isEqualTo(record.channelId);
        assertThat(time - duration).isEqualTo(record.watchedStartTime);
        assertThat(duration).isEqualTo(record.duration);
        assertThat(recordFromSharedPreferences).isEqualTo(record);
    }

    @Test
    public void testCircularHistoryQueue() {
        startAndWaitForComplete();
        final long startChannelId = 100000000;
        long time = System.currentTimeMillis();
        long duration = TimeUnit.MINUTES.toMillis(10);

        int size = MAX_HISTORY_SIZE * 2;
        for (int i = 0; i < size; ++i) {
            ChannelImpl channel = new ChannelImpl.Builder().setId(startChannelId + i).build();
            mWatchedHistoryManager.logChannelViewStop(channel, time + duration * i, duration);
        }
        for (int i = 0; i < MAX_HISTORY_SIZE; ++i) {
            WatchedRecord record = mWatchedHistoryManager.getRecord(i);
            WatchedRecord recordFromSharedPreferences =
                    mWatchedHistoryManager.getRecordFromSharedPreferences(i);
            assertThat(recordFromSharedPreferences).isEqualTo(record);
            assertThat(startChannelId + size - 1 - i).isEqualTo(record.channelId);
        }
        // Since the WatchedHistory is a circular queue, the value for 0 and maxHistorySize
        // are same.
        assertThat(mWatchedHistoryManager.getRecordFromSharedPreferences(MAX_HISTORY_SIZE))
                .isEqualTo(mWatchedHistoryManager.getRecordFromSharedPreferences(0));
    }

    @Test
    public void testWatchedRecordEquals() {
        assertThat(new WatchedRecord(1, 2, 3).equals(new WatchedRecord(1, 2, 3))).isTrue();
        assertThat(new WatchedRecord(1, 2, 3).equals(new WatchedRecord(1, 2, 4))).isFalse();
        assertThat(new WatchedRecord(1, 2, 3).equals(new WatchedRecord(1, 4, 3))).isFalse();
        assertThat(new WatchedRecord(1, 2, 3).equals(new WatchedRecord(4, 2, 3))).isFalse();
    }

    @Test
    public void testEncodeDecodeWatchedRecord() {
        long fakeId = 100000000;
        long time = System.currentTimeMillis();
        long duration = TimeUnit.MINUTES.toMillis(10);
        WatchedRecord record = new WatchedRecord(fakeId, time, duration);
        WatchedRecord sameRecord =
                mWatchedHistoryManager.decode(mWatchedHistoryManager.encode(record));
        assertThat(sameRecord).isEqualTo(record);
    }

    private static final class TestWatchedHistoryManagerListener
            implements WatchedHistoryManager.Listener {
        boolean mLoadFinished;

        @Override
        public void onLoadFinished() {
            mLoadFinished = true;
        }

        @Override
        public void onNewRecordAdded(WatchedRecord watchedRecord) {}
    }
}
