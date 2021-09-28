/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.server.wifi.scanner;

import static com.android.server.wifi.ScanTestUtil.bandIs;
import static com.android.server.wifi.ScanTestUtil.channelsAre;
import static com.android.server.wifi.ScanTestUtil.channelsToSpec;
import static com.android.server.wifi.ScanTestUtil.createRequest;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.net.wifi.WifiScanner;

import androidx.test.filters.SmallTest;

import com.android.server.wifi.WifiBaseTest;
import com.android.server.wifi.WifiNative;

import org.junit.Before;
import org.junit.Test;
import org.junit.experimental.runners.Enclosed;
import org.junit.runner.RunWith;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;

/**
 * Unit tests for {@link com.android.server.wifi.scanner.KnownBandsChannelHelper}.
 */
@RunWith(Enclosed.class) // WARNING: tests cannot be declared in the outer class
public class KnownBandsChannelHelperTest {

    private static final int[] CHANNELS_24_GHZ = new int[]{2412, 2450};
    private static final int[] CHANNELS_5_GHZ = new int[]{5160, 5175};
    private static final int[] CHANNELS_DFS = new int[]{5600, 5650, 5660};
    private static final int[] CHANNELS_DFS_OTHER = new int[]{5600, 5650, 5660, 5680};
    private static final int[] CHANNELS_6_GHZ = new int[]{5945, 5985};

    /**
     * Unit tests for
     * {@link com.android.server.wifi.scanner.KnownBandsChannelHelper.estimateScanDuration}.
     */
    @SmallTest
    public static class EstimateScanDurationTest extends WifiBaseTest {
        KnownBandsChannelHelper mChannelHelper;

        /**
         * Called before each test
         * Create a channel helper
         */
        @Before
        public void setUp() throws Exception {
            mChannelHelper = new PresetKnownBandsChannelHelper(
                    CHANNELS_24_GHZ,
                    CHANNELS_5_GHZ,
                    CHANNELS_DFS,
                    CHANNELS_6_GHZ);
        }

        /**
         * check a settings object with a few channels
         */
        @Test
        public void fewChannels() {
            WifiScanner.ScanSettings testSettings = createRequest(channelsToSpec(2412, 2450, 5200),
                    10000, 0, 20, WifiScanner.REPORT_EVENT_AFTER_EACH_SCAN);

            assertEquals(ChannelHelper.SCAN_PERIOD_PER_CHANNEL_MS * 3,
                    mChannelHelper.estimateScanDuration(testSettings));
        }

        /**
         * check a settings object with a band
         */
        @Test
        public void band() {
            WifiScanner.ScanSettings testSettings = createRequest(WifiScanner.WIFI_BAND_24_GHZ,
                    10000, 0, 20, WifiScanner.REPORT_EVENT_AFTER_EACH_SCAN);

            assertEquals(ChannelHelper.SCAN_PERIOD_PER_CHANNEL_MS * CHANNELS_24_GHZ.length,
                    mChannelHelper.estimateScanDuration(testSettings));
        }
    }

    /**
     * Unit tests for
     * {@link com.android.server.wifi.scanner.KnownBandsChannelHelper.getAvailableScanChannels}.
     */
    @SmallTest
    public static class GetAvailableScanChannelsTest extends WifiBaseTest {
        KnownBandsChannelHelper mChannelHelper;

        /**
         * Called before each test
         * Create a channel helper
         */
        @Before
        public void setUp() throws Exception {
            mChannelHelper = new PresetKnownBandsChannelHelper(
                    CHANNELS_24_GHZ,
                    CHANNELS_5_GHZ,
                    CHANNELS_DFS,
                    CHANNELS_6_GHZ);
        }

        private void testBand(int[] expectedChannels, int band) {
            WifiScanner.ChannelSpec[][] channels =
                    mChannelHelper.getAvailableScanChannels(band);
            int len = 0;
            for (int i = 0; i < channels.length; ++i) {
                len += channels[i].length;
            }
            assertEquals("num channels", expectedChannels.length, len);
            int index = 0;
            for (int i = 0; i < channels.length; ++i) {
                for (int j = 0; j < channels[i].length; ++j) {
                    assertEquals("channels[" + index + "].frequency",
                            expectedChannels[index++], channels[i][j].frequency);
                }
            }
        }

        /**
         * test the 2.4GHz band
         */
        @Test
        public void channels24Ghz() {
            testBand(CHANNELS_24_GHZ, WifiScanner.WIFI_BAND_24_GHZ);
        }

        /**
         * test the 5GHz band
         */
        @Test
        public void channels5Ghz() {
            testBand(CHANNELS_5_GHZ, WifiScanner.WIFI_BAND_5_GHZ);
        }

        /**
         * test the 5GHz DFS band
         */
        @Test
        public void channelsDfs() {
            testBand(CHANNELS_DFS, WifiScanner.WIFI_BAND_5_GHZ_DFS_ONLY);
        }

        /**
         * test the 2.4GHz and 5GHz band
         */
        @Test
        public void channels24GhzAnd5Ghz() {
            int[] expectedChannels = new int[CHANNELS_24_GHZ.length + CHANNELS_5_GHZ.length];
            System.arraycopy(CHANNELS_24_GHZ, 0, expectedChannels, 0, CHANNELS_24_GHZ.length);
            System.arraycopy(CHANNELS_5_GHZ, 0, expectedChannels, CHANNELS_24_GHZ.length,
                    CHANNELS_5_GHZ.length);
            testBand(expectedChannels, WifiScanner.WIFI_BAND_BOTH);
        }

        /**
         * test all bands
         */
        @Test
        public void channelsAll() {
            int[] expectedChannels =
                    new int[CHANNELS_24_GHZ.length + CHANNELS_5_GHZ.length + CHANNELS_DFS.length];
            System.arraycopy(CHANNELS_24_GHZ, 0, expectedChannels, 0, CHANNELS_24_GHZ.length);
            System.arraycopy(CHANNELS_5_GHZ, 0, expectedChannels, CHANNELS_24_GHZ.length,
                    CHANNELS_5_GHZ.length);
            System.arraycopy(CHANNELS_DFS, 0, expectedChannels,
                    CHANNELS_24_GHZ.length + CHANNELS_5_GHZ.length,
                    CHANNELS_DFS.length);
            testBand(expectedChannels, WifiScanner.WIFI_BAND_BOTH_WITH_DFS);
        }
    }

    /**
     * Unit tests for
     * {@link com.android.server.wifi.scanner.KnownBandsChannelHelper.settingsContainChannel}.
     */
    @SmallTest
    public static class SettingsContainChannelTest extends WifiBaseTest {
        KnownBandsChannelHelper mChannelHelper;

        /**
         * Called before each test
         * Create a channel helper
         */
        @Before
        public void setUp() throws Exception {
            mChannelHelper = new PresetKnownBandsChannelHelper(
                    CHANNELS_24_GHZ,
                    CHANNELS_5_GHZ,
                    CHANNELS_DFS,
                    CHANNELS_6_GHZ);
        }

        /**
         * check a settings object with no channels
         */
        @Test
        public void emptySettings() {
            WifiScanner.ScanSettings testSettings = createRequest(channelsToSpec(),
                    10000, 0, 20, WifiScanner.REPORT_EVENT_AFTER_EACH_SCAN);

            assertFalse(mChannelHelper.settingsContainChannel(testSettings, 2412));
            assertFalse(mChannelHelper.settingsContainChannel(testSettings, 5160));
            assertFalse(mChannelHelper.settingsContainChannel(testSettings, 5650));
        }

        /**
         * check a settings object with some channels
         */
        @Test
        public void settingsWithChannels() {
            WifiScanner.ScanSettings testSettings = createRequest(channelsToSpec(2412, 5650),
                    10000, 0, 20, WifiScanner.REPORT_EVENT_AFTER_EACH_SCAN);

            assertTrue(mChannelHelper.settingsContainChannel(testSettings, 2412));
            assertFalse(mChannelHelper.settingsContainChannel(testSettings, 5160));
            assertTrue(mChannelHelper.settingsContainChannel(testSettings, 5650));
        }

        /**
         * check a settings object with a band specified
         */
        @Test
        public void settingsWithBand() {
            WifiScanner.ScanSettings testSettings = createRequest(WifiScanner.WIFI_BAND_24_GHZ,
                    10000, 0, 20, WifiScanner.REPORT_EVENT_AFTER_EACH_SCAN);

            assertTrue(mChannelHelper.settingsContainChannel(testSettings, 2412));
            assertTrue(mChannelHelper.settingsContainChannel(testSettings, 2450));
            assertFalse(mChannelHelper.settingsContainChannel(testSettings, 5160));
            assertFalse(mChannelHelper.settingsContainChannel(testSettings, 5650));
        }

        /**
         * check a settings object with multiple bands specified
         */
        @Test
        public void settingsWithMultiBand() {
            WifiScanner.ScanSettings testSettings = createRequest(WifiScanner.WIFI_BAND_BOTH,
                    10000, 0, 20, WifiScanner.REPORT_EVENT_AFTER_EACH_SCAN);

            assertTrue(mChannelHelper.settingsContainChannel(testSettings, 2412));
            assertTrue(mChannelHelper.settingsContainChannel(testSettings, 2450));
            assertTrue(mChannelHelper.settingsContainChannel(testSettings, 5160));
            assertFalse(mChannelHelper.settingsContainChannel(testSettings, 5650));
        }
    }

    /**
     * Unit tests for
     * {@link com.android.server.wifi.scanner.KnownBandsChannelHelper#equals(ChannelHelper)}.
     */
    @SmallTest
    public static class EqualsTest extends WifiBaseTest {
        /**
         * Creates 2 channel helper instances which are equal.
         */
        @Test
        public void channelHelpersAreSatisfiedBySame() {
            KnownBandsChannelHelper channelHelper0 = new PresetKnownBandsChannelHelper(
                    CHANNELS_24_GHZ,
                    CHANNELS_5_GHZ,
                    CHANNELS_DFS,
                    CHANNELS_6_GHZ);
            KnownBandsChannelHelper channelHelper1 = new PresetKnownBandsChannelHelper(
                    CHANNELS_24_GHZ,
                    CHANNELS_5_GHZ,
                    CHANNELS_DFS,
                    CHANNELS_6_GHZ);
            assertTrue(channelHelper0.satisfies(channelHelper1));
        }

        /**
         * Creates 2 channel helper instances which are equal.
         */
        @Test
        public void channelHelpersAreNotSatisfiedByDifferent() {
            KnownBandsChannelHelper channelHelper0 = new PresetKnownBandsChannelHelper(
                    CHANNELS_24_GHZ,
                    CHANNELS_5_GHZ,
                    CHANNELS_DFS,
                    CHANNELS_6_GHZ);
            KnownBandsChannelHelper channelHelper1 = new PresetKnownBandsChannelHelper(
                    CHANNELS_24_GHZ,
                    CHANNELS_5_GHZ,
                    CHANNELS_DFS_OTHER,
                    CHANNELS_6_GHZ);
            assertFalse(channelHelper0.satisfies(channelHelper1));
        }
    }

    /**
     * Unit tests for
     * {@link com.android.server.wifi.scanner.KnownBandsChannelHelper.KnownBandsChannelCollection}.
     */
    @SmallTest
    public static class KnownBandsChannelCollectionTest extends WifiBaseTest {
        ChannelHelper.ChannelCollection mChannelCollection;

        /**
         * Called before each test
         * Create a collection to use for each test
         */
        @Before
        public void setUp() throws Exception {
            KnownBandsChannelHelper channelHelper = new PresetKnownBandsChannelHelper(
                    CHANNELS_24_GHZ,
                    CHANNELS_5_GHZ,
                    CHANNELS_DFS,
                    CHANNELS_6_GHZ);
            mChannelCollection = channelHelper.createChannelCollection();
        }

        /**
         * Create an empty collection
         */
        @Test
        public void empty() {
            WifiNative.BucketSettings bucketSettings = new WifiNative.BucketSettings();
            mChannelCollection.fillBucketSettings(bucketSettings, Integer.MAX_VALUE);
            assertThat(bucketSettings, channelsAre());

            assertEquals(Collections.<Integer>emptySet(),
                    mChannelCollection.getScanFreqs());

            assertTrue(mChannelCollection.isEmpty());
            assertFalse(mChannelCollection.containsChannel(2412));
            assertFalse(mChannelCollection.containsChannel(5160));
            assertFalse(mChannelCollection.isAllChannels());
        }

        /**
         * Add something to a collection and then clear it and make sure nothing is in it
         */
        @Test
        public void clear() {
            mChannelCollection.addBand(WifiScanner.WIFI_BAND_24_GHZ);
            mChannelCollection.clear();

            WifiNative.BucketSettings bucketSettings = new WifiNative.BucketSettings();
            mChannelCollection.fillBucketSettings(bucketSettings, Integer.MAX_VALUE);
            assertThat(bucketSettings, channelsAre());

            assertEquals(Collections.<Integer>emptySet(),
                    mChannelCollection.getScanFreqs());

            assertTrue(mChannelCollection.isEmpty());
            assertFalse(mChannelCollection.containsChannel(2412));
            assertFalse(mChannelCollection.containsChannel(5160));
            assertFalse(mChannelCollection.isAllChannels());
        }

        /**
         * Add a single band to the collection
         */
        @Test
        public void addBand() {
            mChannelCollection.addBand(WifiScanner.WIFI_BAND_24_GHZ);

            WifiNative.BucketSettings bucketSettings = new WifiNative.BucketSettings();
            mChannelCollection.fillBucketSettings(bucketSettings, Integer.MAX_VALUE);
            assertThat(bucketSettings, bandIs(WifiScanner.WIFI_BAND_24_GHZ));

            assertEquals(new HashSet<Integer>(Arrays.asList(2412, 2450)),
                    mChannelCollection.getScanFreqs());

            assertFalse(mChannelCollection.isEmpty());
            assertTrue(mChannelCollection.containsChannel(2412));
            assertFalse(mChannelCollection.containsChannel(5160));
            assertFalse(mChannelCollection.isAllChannels());
        }

        /**
         * Add a single channel to the collection
         */
        @Test
        public void addChannel_single() {
            mChannelCollection.addChannel(2412);

            WifiNative.BucketSettings bucketSettings = new WifiNative.BucketSettings();
            mChannelCollection.fillBucketSettings(bucketSettings, Integer.MAX_VALUE);
            assertThat(bucketSettings, channelsAre(2412));

            assertEquals(new HashSet<Integer>(Arrays.asList(2412)),
                    mChannelCollection.getScanFreqs());

            assertFalse(mChannelCollection.isEmpty());
            assertTrue(mChannelCollection.containsChannel(2412));
            assertFalse(mChannelCollection.containsChannel(5160));
            assertFalse(mChannelCollection.isAllChannels());
        }

        /**
         * Add a multiple channels to the collection
         */
        @Test
        public void addChannel_multiple() {
            mChannelCollection.addChannel(2412);
            mChannelCollection.addChannel(2450);

            WifiNative.BucketSettings bucketSettings = new WifiNative.BucketSettings();
            mChannelCollection.fillBucketSettings(bucketSettings, Integer.MAX_VALUE);
            assertThat(bucketSettings, channelsAre(2412, 2450));

            assertEquals(new HashSet<Integer>(Arrays.asList(2412, 2450)),
                    mChannelCollection.getScanFreqs());

            assertFalse(mChannelCollection.isEmpty());
            assertTrue(mChannelCollection.containsChannel(2412));
            assertFalse(mChannelCollection.containsChannel(5160));
            assertFalse(mChannelCollection.isAllChannels());
        }

        /**
         * Add a band and channel that is on that band
         */
        @Test
        public void addChannel_and_addBand_sameBand() {
            mChannelCollection.addBand(WifiScanner.WIFI_BAND_24_GHZ);
            mChannelCollection.addChannel(2412);

            WifiNative.BucketSettings bucketSettings = new WifiNative.BucketSettings();
            mChannelCollection.fillBucketSettings(bucketSettings, Integer.MAX_VALUE);
            assertThat(bucketSettings, bandIs(WifiScanner.WIFI_BAND_24_GHZ));

            assertEquals(new HashSet<Integer>(Arrays.asList(2412, 2450)),
                    mChannelCollection.getScanFreqs());

            assertFalse(mChannelCollection.isEmpty());
            assertTrue(mChannelCollection.containsChannel(2412));
            assertFalse(mChannelCollection.containsChannel(5160));
            assertFalse(mChannelCollection.isAllChannels());
        }

        /**
         * Add a band and channel that is not that band
         */
        @Test
        public void addChannel_and_addBand_withDifferentBandChannel() {
            mChannelCollection.addBand(WifiScanner.WIFI_BAND_24_GHZ);
            mChannelCollection.addChannel(5160);

            WifiNative.BucketSettings bucketSettings = new WifiNative.BucketSettings();
            mChannelCollection.fillBucketSettings(bucketSettings, Integer.MAX_VALUE);
            assertThat(bucketSettings, channelsAre(2412, 2450, 5160));

            assertEquals(new HashSet<Integer>(Arrays.asList(2412, 2450, 5160)),
                    mChannelCollection.getScanFreqs());

            assertFalse(mChannelCollection.isEmpty());
            assertTrue(mChannelCollection.containsChannel(2412));
            assertTrue(mChannelCollection.containsChannel(5160));
            assertFalse(mChannelCollection.isAllChannels());
        }

        /**
         * Add a band that should contain all channels
         */
        @Test
        public void addChannel_and_addBand_all() {
            mChannelCollection.addBand(WifiScanner.WIFI_BAND_24_5_WITH_DFS_6_GHZ);
            mChannelCollection.addChannel(5160);

            WifiNative.BucketSettings bucketSettings = new WifiNative.BucketSettings();
            mChannelCollection.fillBucketSettings(bucketSettings, Integer.MAX_VALUE);
            assertThat(bucketSettings, bandIs(WifiScanner.WIFI_BAND_24_5_WITH_DFS_6_GHZ));

            assertNull(mChannelCollection.getScanFreqs());

            assertFalse(mChannelCollection.isEmpty());
            assertTrue(mChannelCollection.containsChannel(2412));
            assertTrue(mChannelCollection.containsChannel(5160));
            assertTrue(mChannelCollection.containsChannel(5600));
            assertTrue(mChannelCollection.isAllChannels());
        }

        /**
         * Add enough channels on a single band that the max channels is exceeded
         */
        @Test
        public void addChannel_exceedMaxChannels() {
            mChannelCollection.addChannel(5600);
            mChannelCollection.addChannel(5650);
            mChannelCollection.addChannel(5660);

            WifiNative.BucketSettings bucketSettings = new WifiNative.BucketSettings();
            mChannelCollection.fillBucketSettings(bucketSettings, 2);
            assertThat(bucketSettings, bandIs(WifiScanner.WIFI_BAND_5_GHZ_DFS_ONLY));
            assertFalse(mChannelCollection.isAllChannels());
        }

        /**
         * Add enough channels across multiple bands that the max channels is exceeded
         */
        @Test
        public void addChannel_exceedMaxChannelsOnMultipleBands() {
            mChannelCollection.addChannel(2412);
            mChannelCollection.addChannel(2450);
            mChannelCollection.addChannel(5160);

            WifiNative.BucketSettings bucketSettings = new WifiNative.BucketSettings();
            mChannelCollection.fillBucketSettings(bucketSettings, 2);
            assertThat(bucketSettings, bandIs(WifiScanner.WIFI_BAND_BOTH));
            assertFalse(mChannelCollection.isAllChannels());
        }


        /**
         * Add enough channels across all bands that the max channels is exceeded
         */
        @Test
        public void addChannel_addAllAvailableChannels() {
            mChannelCollection.addChannel(2412);
            mChannelCollection.addChannel(2450);
            mChannelCollection.addChannel(5160);
            mChannelCollection.addChannel(5175);
            mChannelCollection.addChannel(5600);
            mChannelCollection.addChannel(5650);
            mChannelCollection.addChannel(5660);
            mChannelCollection.addChannel(5945);
            mChannelCollection.addChannel(5985);

            WifiNative.BucketSettings bucketSettings = new WifiNative.BucketSettings();
            mChannelCollection.fillBucketSettings(bucketSettings, Integer.MAX_VALUE);
            assertThat(bucketSettings,
                    channelsAre(2412, 2450, 5160, 5175, 5600, 5650, 5660, 5945, 5985));
            assertTrue(mChannelCollection.isAllChannels());
        }
    }
}
