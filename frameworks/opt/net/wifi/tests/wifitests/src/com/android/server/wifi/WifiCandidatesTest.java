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

package com.android.server.wifi;

import static com.android.server.wifi.util.NativeUtil.removeEnclosingQuotes;

import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

import android.content.Context;
import android.net.MacAddress;
import android.net.util.MacAddressUtils;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiConfiguration;

import androidx.test.filters.SmallTest;

import com.android.wifi.resources.R;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/**
 * Unit tests for {@link com.android.server.wifi.WifiCandidates}.
 */
@SmallTest
public class WifiCandidatesTest extends WifiBaseTest {

    @Mock ScanDetail mScanDetail1;
    @Mock ScanDetail mScanDetail2;
    @Mock WifiScoreCard mWifiScoreCard;
    @Mock WifiScoreCard.PerBssid mPerBssid;
    @Mock Context mContext;

    ScanResult mScanResult1;
    ScanResult mScanResult2;

    WifiConfiguration mConfig1;
    WifiConfiguration mConfig2;

    WifiCandidates mWifiCandidates;
    MockResources mResources;

    /**
     * Sets up for unit test
     */
    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mWifiCandidates = new WifiCandidates(mWifiScoreCard, mContext);
        mConfig1 = WifiConfigurationTestUtil.createOpenNetwork();

        mScanResult1 = new ScanResult();
        mScanResult1.SSID = removeEnclosingQuotes(mConfig1.SSID);
        mScanResult1.capabilities = "[ESS]";
        mScanResult1.BSSID = "00:00:00:00:00:01";

        mConfig2 = WifiConfigurationTestUtil.createEphemeralNetwork();
        mScanResult2 = new ScanResult();
        mScanResult2.SSID = removeEnclosingQuotes(mConfig2.SSID);
        mScanResult2.capabilities = "[ESS]";

        doReturn(mScanResult1).when(mScanDetail1).getScanResult();
        doReturn(mScanResult2).when(mScanDetail2).getScanResult();
        doReturn(mPerBssid).when(mWifiScoreCard).lookupBssid(any(), any());
        doReturn(50).when(mPerBssid).estimatePercentInternetAvailability();
        MockResources mResources = new MockResources();
        mResources.setBoolean(R.bool.config_wifiSaeUpgradeEnabled, true);
        doReturn(mResources).when(mContext).getResources();
    }

    /**
     * Test for absence of null pointer exceptions
     */
    @Test
    public void testDontDieFromNulls() throws Exception {
        mWifiCandidates.add(null, mConfig1, 1, 0.0, false, 100);
        mWifiCandidates.add(mScanDetail1, null, 2, 0.0, false, 100);
        doReturn(null).when(mScanDetail2).getScanResult();
        mWifiCandidates.add(mScanDetail2, mConfig2, 3, 1.0, true, 100);
        assertFalse(mWifiCandidates.remove(null));

        assertEquals(0, mWifiCandidates.size());
    }

    /**
     * Add just one thing
     */
    @Test
    public void testAddJustOne() throws Exception {
        assertTrue(mWifiCandidates.add(mScanDetail1, mConfig1, 2, 0.0, false, 100));

        assertEquals(1, mWifiCandidates.size());
        assertEquals(0, mWifiCandidates.getFaultCount());
        assertNull(mWifiCandidates.getLastFault());
        verify(mPerBssid).setNetworkConfigId(eq(mConfig1.networkId));
    }

    /**
     * Test retrieving the list of candidates.
     */
    @Test
    public void testGetCandidates() {
        assertTrue(mWifiCandidates.add(mScanDetail1, mConfig1, 2, 0.0, false, 100));
        assertNotNull(mWifiCandidates.getCandidates());
        assertEquals(1, mWifiCandidates.getCandidates().size());
    }

    /**
     * Make sure we catch SSID mismatch due to quoting error
     */
    @Test
    public void testQuotingBotch() throws Exception {
        // Unfortunately ScanResult.SSID is not quoted; make sure we catch that
        mScanResult1.SSID = mConfig1.SSID;
        mWifiCandidates.add(mScanDetail1, mConfig1, 2, 0.0, true, 100);

        // Should not have added this one
        assertEquals(0, mWifiCandidates.size());
        // The failure should have been recorded
        assertEquals(1, mWifiCandidates.getFaultCount());
        // The record of the failure should contain the culprit
        String blah = mWifiCandidates.getLastFault().toString();
        assertTrue(blah, blah.contains(mConfig1.SSID));

        // Now check that we can clear the faults
        mWifiCandidates.clearFaults();

        assertEquals(0, mWifiCandidates.getFaultCount());
        assertNull(mWifiCandidates.getLastFault());
    }

    /**
     * Test Key equals and hashCode methods
     */
    @Test
    public void testKeyEquivalence() throws Exception {
        ScanResultMatchInfo matchInfo1 = ScanResultMatchInfo.fromWifiConfiguration(mConfig1);
        ScanResultMatchInfo matchInfo1Prime = ScanResultMatchInfo.fromWifiConfiguration(mConfig1);
        ScanResultMatchInfo matchInfo2 = ScanResultMatchInfo.fromWifiConfiguration(mConfig2);
        assertFalse(matchInfo1 == matchInfo1Prime); // Checking assumption
        MacAddress mac1 = MacAddressUtils.createRandomUnicastAddress();
        MacAddress mac2 = MacAddressUtils.createRandomUnicastAddress();
        assertNotEquals(mac1, mac2); // really tiny probability of failing here

        WifiCandidates.Key key1 = new WifiCandidates.Key(matchInfo1, mac1, 1);

        assertFalse(key1.equals(null));
        assertFalse(key1.equals((Integer) 0));
        // Same inputs should give equal results
        assertEquals(key1, new WifiCandidates.Key(matchInfo1, mac1, 1));
        // Equal inputs should give equal results
        assertEquals(key1, new WifiCandidates.Key(matchInfo1Prime, mac1, 1));
        // Hash codes of equal things should be equal
        assertEquals(key1.hashCode(), key1.hashCode());
        assertEquals(key1.hashCode(), new WifiCandidates.Key(matchInfo1, mac1, 1).hashCode());
        assertEquals(key1.hashCode(), new WifiCandidates.Key(matchInfo1Prime, mac1, 1).hashCode());

        // Unequal inputs should give unequal results
        assertFalse(key1.equals(new WifiCandidates.Key(matchInfo2, mac1, 1)));
        assertFalse(key1.equals(new WifiCandidates.Key(matchInfo1, mac2, 1)));
        assertFalse(key1.equals(new WifiCandidates.Key(matchInfo1, mac1, 2)));
    }

    /**
     * Test toString method
     */
    @Test
    public void testCandidateToString() throws Exception {
        doReturn(57).when(mPerBssid).estimatePercentInternetAvailability();
        mWifiCandidates.add(mScanDetail1, mConfig1, 2, 0.0015001, false, 100);
        WifiCandidates.Candidate c = mWifiCandidates.getGroupedCandidates()
                .iterator().next().iterator().next();
        String s = c.toString();
        assertTrue(s, s.contains(" nominator = 2, "));
        assertTrue(s, s.contains(" config = " + mConfig1.networkId + ", "));
        assertTrue(s, s.contains(" lastSelectionWeight = 0.002, ")); // should be rounded
        assertTrue(s, s.contains(" pInternet = 57, "));
        for (String x : s.split(",")) {
            if (x.startsWith("Candidate {")) x = x.substring("Candidate {".length());
            if (x.endsWith(" }")) x = x.substring(0, x.length() - 2);
            String diagnose = s + " !! " + x;
            assertTrue(diagnose, x.startsWith(" ")); // space between items
            assertFalse(diagnose, x.contains("  ")); // no double spaces
            if (x.contains("=")) {
                // Only one equals sign, if there is one
                assertTrue(diagnose, x.indexOf("=") == x.lastIndexOf("="));
                assertTrue(diagnose, x.matches(" [A-Za-z]+ = [^ ]+"));
            } else {
                assertTrue(diagnose, x.matches(" [a-z]+"));
            }
        }
    }

    /**
     * Test that picky mode works
     */
    @Test
    public void testPickyMode() throws Exception {
        // Set picky mode, make sure that it returns the object itself (so that
        // method chaining may be used).
        assertTrue(mWifiCandidates == mWifiCandidates.setPicky(true));
        try {
            mScanResult1.SSID = mConfig1.SSID; // As in testQuotingBotch()
            mWifiCandidates.add(mScanDetail1, mConfig1, 2, 0.0, false, 100);
            fail("Exception not raised in picky mode");
        } catch (IllegalArgumentException e) {
            assertEquals(1, mWifiCandidates.getFaultCount());
            assertEquals(e, mWifiCandidates.getLastFault());
        }
    }

    /**
     * Try cases where we don't overwrite existing candidates
     */
    @Test
    public void testNoOverwriteCases() throws Exception {
        // Setup is to add the first candidate
        mWifiCandidates.add(mScanDetail1, mConfig1, 2, 0.0, false, 100);
        assertEquals(1, mWifiCandidates.size());

        // Later nominator. Should not add.
        assertFalse(mWifiCandidates.add(mScanDetail1, mConfig1, 5, 0.0, false, 100));
        assertFalse(mWifiCandidates.add(mScanDetail1, mConfig1, 5, 0.0, false, 100));
        assertEquals(0, mWifiCandidates.getFaultCount()); // Still no faults
        // After all that, only one candidate should be there.
        assertEquals(1, mWifiCandidates.size());
    }

    /**
     * Try cases where we do overwrite existing candidates
     */
    @Test
    public void testOverwriteCases() throws Exception {
        // Setup is to add the first candidate
        mWifiCandidates.add(mScanDetail1, mConfig1, 2, 0.0, false, 100);
        assertEquals(1, mWifiCandidates.size());

        // Same nominator, should replace.
        assertTrue(mWifiCandidates.add(mScanDetail1, mConfig1, 2, 0.0, false, 100));
        assertEquals(0, mWifiCandidates.getFaultCount()); // No fault
        // Nominator out of order. Should replace.
        assertTrue(mWifiCandidates.add(mScanDetail1, mConfig1, 1, 0.0, false, 100));
        assertEquals(0, mWifiCandidates.getFaultCount());  // But not considered a fault
        // After all that, only one candidate should be there.
        assertEquals(1, mWifiCandidates.size());
    }

    /**
     * BSSID validation
     */
    @Test
    public void testBssidValidation() throws Exception {
        // Null BSSID.
        mScanResult1.BSSID = null;
        mWifiCandidates.add(mScanDetail1, mConfig1, 2, 0.0, false, 100);
        assertTrue("Expecting NPE, got " + mWifiCandidates.getLastFault(),
                mWifiCandidates.getLastFault() instanceof NullPointerException);
        // Malformed BSSID
        mScanResult1.BSSID = "NotaBssid!";
        mWifiCandidates.add(mScanDetail1, mConfig1, 2, 0.0, false, 100);
        assertTrue("Expecting IAE, got " + mWifiCandidates.getLastFault(),
                mWifiCandidates.getLastFault() instanceof IllegalArgumentException);
        assertEquals(0, mWifiCandidates.size());
    }

    /**
    * Add candidate BSSIDs in the same network, then remove them
    */
    @Test
    public void testTwoBssids() throws Exception {
        // Make a duplicate of the first config
        mConfig2 = new WifiConfiguration(mConfig1);
        // Make a second scan result, same network, different BSSID.
        mScanResult2.SSID = mScanResult1.SSID;
        mScanResult2.BSSID = mScanResult1.BSSID.replace('1', '2');
        // Add both
        mWifiCandidates.add(mScanDetail1, mConfig1, 2, 0.0, false, 100);
        mWifiCandidates.add(mScanDetail2, mConfig2, 2, 0.0, false, 100);
        // We expect them both to be there
        assertEquals(2, mWifiCandidates.size());
        // But just one group
        assertEquals(1, mWifiCandidates.getGroupedCandidates().size());
        // Now remove them one at a time
        WifiCandidates.Candidate c1, c2;
        c1 = mWifiCandidates.getGroupedCandidates().iterator().next().iterator().next();
        assertTrue(mWifiCandidates.remove(c1));
        assertEquals(1, mWifiCandidates.size());
        assertEquals(1, mWifiCandidates.getGroupedCandidates().size());
        // Should not be able to remove the one that isn't there
        assertFalse(mWifiCandidates.remove(c1));
        // Remove the other one, too
        c2 = mWifiCandidates.getGroupedCandidates().iterator().next().iterator().next();
        assertTrue(mWifiCandidates.remove(c2));
        assertFalse(mWifiCandidates.remove(c2));
        assertEquals(0, mWifiCandidates.size());
        assertEquals(0, mWifiCandidates.getGroupedCandidates().size());
    }

    /**
     * Test replacing a candidate with a higher scoring one
     */
    @Test
    public void testReplace() throws Exception {
        // Make a duplicate of the first config
        mConfig2 = new WifiConfiguration(mConfig1);
        // And the scan result
        mScanResult2.SSID = mScanResult1.SSID;
        mScanResult2.BSSID = mScanResult1.BSSID;
        // Try adding them both, in a known order
        assertTrue(mWifiCandidates.add(mScanDetail2, mConfig2, 2, 0.0, false, 100));
        assertTrue(mWifiCandidates.add(mScanDetail1, mConfig1, 2, 0.0, false, 90));
        // Only one should survive
        assertEquals(1, mWifiCandidates.size());
        assertEquals(0, mWifiCandidates.getFaultCount());
        // Make sure we kept the second one
        WifiCandidates.Candidate c;
        c = mWifiCandidates.getGroupedCandidates().iterator().next().iterator().next();
        assertEquals(90, c.getPredictedThroughputMbps());
    }

    /**
     * Tests passpiont network from same provider(FQDN) can have multiple candidates with different
     * scanDetails.
     */
    @Test
    public void testMultiplePasspointCandidatesWithSameFQDN() {
        // Create a Passpoint WifiConfig
        WifiConfiguration config1 = WifiConfigurationTestUtil.createPasspointNetwork();
        mScanResult2.BSSID = mScanResult1.BSSID.replace('1', '2');
        // Add candidates with different scanDetail for same passpoint WifiConfig.
        assertTrue(mWifiCandidates.add(mScanDetail1, config1, 2, 0.0, false, 100));
        assertTrue(mWifiCandidates.add(mScanDetail2, config1, 2, 0.0, false, 100));
        // Both should survive and no faults.
        assertEquals(2, mWifiCandidates.size());
        assertEquals(0, mWifiCandidates.getFaultCount());
    }

    /**
     * Verify CarrierOrPrivileged bit is remembered.
     */
    @Test
    public void testAddCarrierOrPrivilegedCandidate() {
        WifiCandidates.Key key = mWifiCandidates
                .keyFromScanDetailAndConfig(mScanDetail1, mConfig1);
        WifiCandidates.Candidate candidate;
        // Make sure the CarrierOrPrivileged false is remembered
        assertTrue(mWifiCandidates.add(key, mConfig1, 0, -50, 2412, 0.0, false, false, 100));
        candidate = mWifiCandidates.getCandidates().get(0);
        assertFalse(candidate.isCarrierOrPrivileged());
        mWifiCandidates.remove(candidate);
        // Make sure the CarrierOrPrivileged true is remembered
        assertTrue(mWifiCandidates.add(key, mConfig1, 0, -50, 2412, 0.0, false, true, 100));
        candidate = mWifiCandidates.getCandidates().get(0);
        assertTrue(candidate.isCarrierOrPrivileged());
        mWifiCandidates.remove(candidate);
    }
}
