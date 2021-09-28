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

package com.android.server.wifi;

import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.lessThan;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;

import androidx.test.filters.SmallTest;

import com.android.server.wifi.WifiCandidates.Candidate;
import com.android.server.wifi.WifiCandidates.CandidateScorer;
import com.android.server.wifi.WifiCandidates.ScoredCandidate;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;
import org.junit.runners.Parameterized.Parameters;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for implementations of
 * {@link com.android.server.wifi.WifiCandidates.CandidateScorer}.
 *
 * Runs tests that any reasonable CandidateScorer should pass.
 * Individual scorers may have additional tests of their own.
 */
@SmallTest
@RunWith(Parameterized.class)
public class CandidateScorerTest extends WifiBaseTest {

    @Parameters(name = "{index}: {0}")
    public static List<Object[]> listOfObjectArraysBecauseJUnitMadeUs() {
        ScoringParams sp;
        ArrayList<Object[]> ans = new ArrayList<>();

        sp = new ScoringParams();
        ans.add(new Object[]{
                "Compatibility Scorer",
                CompatibilityScorer.COMPATIBILITY_SCORER_DEFAULT_EXPID,
                new CompatibilityScorer(sp),
                sp});

        sp = new ScoringParams();
        ans.add(new Object[]{
                "Score Card Based Scorer",
                ScoreCardBasedScorer.SCORE_CARD_BASED_SCORER_DEFAULT_EXPID,
                new ScoreCardBasedScorer(sp),
                sp});

        sp = new ScoringParams();
        ans.add(new Object[]{
                "Bubble Function Scorer",
                BubbleFunScorer.BUBBLE_FUN_SCORER_DEFAULT_EXPID,
                new BubbleFunScorer(sp),
                sp});

        sp = new ScoringParams();
        ans.add(new Object[]{
                "Throughput Scorer",
                ThroughputScorer.THROUGHPUT_SCORER_DEFAULT_EXPID,
                new ThroughputScorer(sp),
                sp});

        return ans;
    }

    @Parameter(0)
    public String mTitleForUseInGeneratedParameterNames;

    @Parameter(1)
    public int mExpectedExpId;

    @Parameter(2)
    public CandidateScorer mCandidateScorer;

    @Parameter(3)
    public ScoringParams mScoringParams;

    private static final double TOL = 1e-6; // for assertEquals(double, double, tolerance)

    private ConcreteCandidate mCandidate1;
    private ConcreteCandidate mCandidate2;

    /**
     * Sets up for unit test
     */
    @Before
    public void setUp() throws Exception {
        mScoringParams.update("");
        mCandidate1 = new ConcreteCandidate().setNominatorId(0)
                .setScanRssi(-50).setFrequency(5180);
        mCandidate2 = new ConcreteCandidate().setNominatorId(0)
                .setScanRssi(-50).setFrequency(5180);
    }

    /**
     * Test that the expected expid is computed with the built-in defaults.
     */
    @Test
    public void testExpid() throws Exception {
        String identifier = mCandidateScorer.getIdentifier();
        assertEquals(identifier,
                mExpectedExpId,
                WifiNetworkSelector.experimentIdFromIdentifier(identifier));
    }

    /**
     * Utility function to build and evaluate a candidate.
     */
    private double evaluate(ConcreteCandidate candidate) {
        ArrayList<Candidate> candidates = new ArrayList<>(1);
        candidates.add(candidate);
        ScoredCandidate choice = mCandidateScorer.scoreCandidates(candidates);
        return Math.max(-999999999.0, choice.value);
    }

    /**
     * Evaluating equal inputs should give the same result.
     */
    @Test
    public void testEqualInputsShouldGiveTheSameResult() throws Exception {
        assertEquals(evaluate(mCandidate1), evaluate(mCandidate2), TOL);
    }

    /**
     * Prefer 5 GHz over 2.4 GHz in non-fringe conditions, similar rssi.
     */
    @Test
    public void testPrefer5GhzOver2GhzInNonFringeConditionsSimilarRssi() throws Exception {
        assertThat(evaluate(mCandidate1.setFrequency(5180).setScanRssi(-44)),
                greaterThan(evaluate(mCandidate2.setFrequency(2432).setScanRssi(-44))));
    }

    /**
     * Prefer higher rssi.
     */
    @Test
    public void testPreferHigherRssi() throws Exception {
        assertThat(evaluate(mCandidate1.setScanRssi(-70)),
                greaterThan(evaluate(mCandidate2.setScanRssi(-71))));
    }

    /**
     * Prefer a secure network over an open one.
     */
    @Test
    public void testPreferASecureNetworkOverAnOpenOne() throws Exception {
        assertThat(evaluate(mCandidate1),
                greaterThan(evaluate(mCandidate2.setOpenNetwork(true))));
    }

    /**
     * Prefer the current network, even if rssi difference is significant.
     */
    @Test
    public void testPreferTheCurrentNetworkEvenIfRssiDifferenceIsSignificant() throws Exception {
        assertThat(evaluate(mCandidate1.setScanRssi(-76).setCurrentNetwork(true)
                                    .setPredictedThroughputMbps(433)),
                greaterThan(evaluate(mCandidate2.setScanRssi(-69)
                                    .setPredictedThroughputMbps(433))));
    }

    /**
     * Prefer the current network, even if throughput difference is significant.
     */
    @Test
    public void testPreferTheCurrentNetworkEvenIfTputDifferenceIsSignificant() throws Exception {
        assertThat(evaluate(mCandidate1.setScanRssi(-57)
                                    .setCurrentNetwork(true)
                                    .setPredictedThroughputMbps(433)),
                greaterThan(evaluate(mCandidate2.setScanRssi(-57)
                                    .setPredictedThroughputMbps(560))));
    }

    /**
     * Prefer to switch when current network has low throughput and no internet (unexpected)
     */
    @Test
    public void testSwitchifCurrentNetworkNoInternetUnexpectedAndLowThroughput() throws Exception {
        if (mExpectedExpId != ThroughputScorer.THROUGHPUT_SCORER_DEFAULT_EXPID) return;
        assertThat(evaluate(mCandidate1.setScanRssi(-57)
                        .setCurrentNetwork(true)
                        .setPredictedThroughputMbps(433)
                        .setNoInternetAccess(true)
                        .setNoInternetAccessExpected(false)),
                lessThan(evaluate(mCandidate2.setScanRssi(-57)
                        .setPredictedThroughputMbps(560))));
    }

    /**
     * Prefer current network when current network has low throughput and no internet (but expected)
     */
    @Test
    public void testSwitchifCurrentNetworkHasNoInternetExceptedAndLowThroughput() throws Exception {
        if (mExpectedExpId != ThroughputScorer.THROUGHPUT_SCORER_DEFAULT_EXPID) return;
        assertThat(evaluate(mCandidate1.setScanRssi(-57)
                        .setCurrentNetwork(true)
                        .setPredictedThroughputMbps(433)
                        .setNoInternetAccess(true)
                        .setNoInternetAccessExpected(true)),
                greaterThan(evaluate(mCandidate2.setScanRssi(-57)
                        .setPredictedThroughputMbps(560))));
    }

    /**
     * Prefer to switch with a larger rssi difference.
     */
    @Test
    public void testSwitchWithLargerDifference() throws Exception {
        assertThat(evaluate(mCandidate1.setScanRssi(-80)
                                       .setCurrentNetwork(true)),
                lessThan(evaluate(mCandidate2.setScanRssi(-60))));
    }

    /**
     * Stay on recently selected network.
     */
    @Test
    public void testStayOnRecentlySelected() throws Exception {
        assertThat(evaluate(mCandidate1.setScanRssi(-80)
                                       .setCurrentNetwork(true)
                                       .setLastSelectionWeight(0.25)),
                greaterThan(evaluate(mCandidate2.setScanRssi(-60))));
    }

    /**
     * Above saturation, don't switch from current even with a large rssi difference.
     */
    @Test
    public void testAboveSaturationDoNotSwitchAwayEvenWithALargeRssiDifference() throws Exception {
        int currentRssi = (mExpectedExpId == ThroughputScorer.THROUGHPUT_SCORER_DEFAULT_EXPID)
                ? mScoringParams.getSufficientRssi(mCandidate1.getFrequency()) :
                mScoringParams.getGoodRssi(mCandidate1.getFrequency());
        int unbelievablyGoodRssi = -1;
        assertThat(evaluate(mCandidate1.setScanRssi(currentRssi).setCurrentNetwork(true)),
                greaterThan(evaluate(mCandidate2.setScanRssi(unbelievablyGoodRssi))));
    }


    /**
     * Prefer high throughput network.
     */
    @Test
    public void testPreferHighThroughputNetwork() throws Exception {
        if (mExpectedExpId == ThroughputScorer.THROUGHPUT_SCORER_DEFAULT_EXPID) {
            assertThat(evaluate(mCandidate1.setScanRssi(-74)
                            .setPredictedThroughputMbps(100)),
                    greaterThan(evaluate(mCandidate2.setScanRssi(-74)
                            .setPredictedThroughputMbps(50))));
        }
    }

    /**
     * Prefer saved over suggestion.
     */
    @Test
    public void testPreferSavedOverSuggestion() throws Exception {
        if (mExpectedExpId != ThroughputScorer.THROUGHPUT_SCORER_DEFAULT_EXPID) return;
        assertThat(evaluate(mCandidate1.setScanRssi(-77).setEphemeral(false)),
                greaterThan(evaluate(mCandidate2.setScanRssi(-40)
                                                .setEphemeral(true)
                                                .setPredictedThroughputMbps(1000))));
    }

    /**
     * Prefer metered saved over unmetered suggestion.
     */
    @Test
    public void testPreferMeteredSavedOverUnmeteredSuggestion() throws Exception {
        if (mExpectedExpId != ThroughputScorer.THROUGHPUT_SCORER_DEFAULT_EXPID) return;
        assertThat(evaluate(mCandidate1.setScanRssi(-77).setEphemeral(false).setMetered(false)),
                greaterThan(evaluate(mCandidate2.setScanRssi(-40)
                                                .setEphemeral(true)
                                                .setMetered(true)
                                                .setPredictedThroughputMbps(1000))));
    }

    /**
     * Prefer trusted metered suggestion over privileged untrusted.
     */
    @Test
    public void testPreferTrustedOverUntrusted() throws Exception {
        if (mExpectedExpId != ThroughputScorer.THROUGHPUT_SCORER_DEFAULT_EXPID) return;
        assertThat(evaluate(mCandidate1.setScanRssi(-77).setEphemeral(true).setMetered(true)),
                greaterThan(evaluate(mCandidate2.setScanRssi(-40)
                                                .setEphemeral(true)
                                                .setPredictedThroughputMbps(1000)
                                                .setTrusted(false)
                                                .setCarrierOrPrivileged(true))));
    }

    /**
     * Prefer carrier untrusted over other untrusted.
     */
    @Test
    public void testPreferCarrierUntrustedOverOtherUntrusted() throws Exception {
        if (mExpectedExpId != ThroughputScorer.THROUGHPUT_SCORER_DEFAULT_EXPID) return;
        assertThat(evaluate(mCandidate1.setScanRssi(-77)
                                       .setEphemeral(true)
                                       .setMetered(true)
                                       .setCarrierOrPrivileged(true)),
                greaterThan(evaluate(mCandidate2.setScanRssi(-40)
                                                .setPredictedThroughputMbps(1000)
                                                .setTrusted(false))));
    }

}
