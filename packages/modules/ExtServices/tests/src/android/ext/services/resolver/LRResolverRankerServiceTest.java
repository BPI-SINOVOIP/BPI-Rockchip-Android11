/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.ext.services.resolver;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.when;
import static org.testng.Assert.assertThrows;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.UserManager;
import android.service.resolver.ResolverTarget;
import android.test.ServiceTestCase;
import android.util.ArrayMap;

import com.google.common.collect.Range;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import java.util.Arrays;
import java.util.List;

public class LRResolverRankerServiceTest extends ServiceTestCase<LRResolverRankerService> {
    private static final String PARAM_SHARED_PREF_NAME = "resolver_ranker_params";
    private static final String BIAS_PREF_KEY = "bias";
    private static final String VERSION_PREF_KEY = "version";
    private static final String LAUNCH_SCORE = "launch";
    private static final String TIME_SPENT_SCORE = "timeSpent";
    private static final String RECENCY_SCORE = "recency";
    private static final String CHOOSER_SCORE = "chooser";

    @Mock private Context mContext;
    @Mock private SharedPreferences mSharedPreferences;
    @Mock private SharedPreferences.Editor mEditor;
    @Mock private UserManager mUserManager;

    public LRResolverRankerServiceTest() {
        super(LRResolverRankerService.class);
    }

    @Before
    public void setUp() throws Exception {
        super.setUp();
        MockitoAnnotations.initMocks(this);
        mContext = Mockito.spy(new ContextWrapper(getSystemContext()));
        setContext(mContext);

        Intent intent = new Intent(getContext(), LRResolverRankerService.class);
        startService(intent);
    }

    @Test
    public void testInitModelUnderUserLocked() {
        setUserLockedStatus(true);
        createSharedPreferences(/* hasSharedPreferences */ true, /* version */ 1);

        final LRResolverRankerService service = getService();
        service.onBind(new Intent("test"));

        assertThat(service.mFeatureWeights).isNull();
    }

    @Test
    public void testInitModelWhileNoSharedPreferences() {
        setUserLockedStatus(false);
        createSharedPreferences(/* hasSharedPreferences */ false, /* version */ 0);

        final LRResolverRankerService service = getService();
        service.onBind(new Intent("test"));

        final ArrayMap<String, Float> featureWeights = service.mFeatureWeights;
        assertThat(featureWeights.get(LAUNCH_SCORE)).isEqualTo(2.5543f);
        assertThat(featureWeights.get(TIME_SPENT_SCORE)).isEqualTo(2.8412f);
        assertThat(featureWeights.get(RECENCY_SCORE)).isEqualTo(0.269f);
        assertThat(featureWeights.get(CHOOSER_SCORE)).isEqualTo(4.2222f);
    }

    @Test
    public void testInitModelWhileVersionInSharedPreferencesSmallerThanCurrent() {
        setUserLockedStatus(false);
        createSharedPreferences(/* hasSharedPreferences */ true, /* version */ 0);

        final LRResolverRankerService service = getService();
        service.onBind(new Intent("test"));

        final ArrayMap<String, Float> featureWeights = service.mFeatureWeights;
        assertThat(featureWeights.get(LAUNCH_SCORE)).isEqualTo(2.5543f);
        assertThat(featureWeights.get(TIME_SPENT_SCORE)).isEqualTo(2.8412f);
        assertThat(featureWeights.get(RECENCY_SCORE)).isEqualTo(0.269f);
        assertThat(featureWeights.get(CHOOSER_SCORE)).isEqualTo(4.2222f);
    }

    @Test
    public void testInitModelWhileVersionInSharedPreferencesNotSmallerThanCurrent() {
        setUserLockedStatus(false);
        createSharedPreferences(/* hasSharedPreferences */ true, /* version */ 1);

        final LRResolverRankerService service = getService();
        service.onBind(new Intent("test"));

        final ArrayMap<String, Float> featureWeights = service.mFeatureWeights;
        assertThat(featureWeights.get(LAUNCH_SCORE)).isEqualTo(0.1f);
        assertThat(featureWeights.get(TIME_SPENT_SCORE)).isEqualTo(0.2f);
        assertThat(featureWeights.get(RECENCY_SCORE)).isEqualTo(0.3f);
        assertThat(featureWeights.get(CHOOSER_SCORE)).isEqualTo(0.4f);
    }

    @Test
    public void testOnPredictSharingProbabilitiesWhileServiceIsNotReady() {
        setUserLockedStatus(true);
        createSharedPreferences(/* hasSharedPreferences */ true, /* version */ 1);
        final LRResolverRankerService service = getService();
        service.onBind(new Intent("test"));
        final List<ResolverTarget> targets =
                Arrays.asList(
                        makeNewResolverTarget(/* recency */ 0.1f, /* timeSpent */ 0.2f,
                                /* launch */ 0.3f, /* chooser */ 0.4f, /* selectProb */ -1.0f),
                        makeNewResolverTarget(/* recency */ 0.4f, /* timeSpent */ 0.3f,
                                /* launch */ 0.2f, /* chooser */ 0.1f, /* selectProb */ -1.0f));

        assertThrows(IllegalStateException.class,
                () -> service.onPredictSharingProbabilities(targets));
    }

    @Test
    public void testOnPredictSharingProbabilitiesAfterUserUnlock() {
        setUserLockedStatus(true);
        createSharedPreferences(/* hasSharedPreferences */ true, /* version */ 1);
        final LRResolverRankerService service = getService();
        service.onBind(new Intent("test"));
        setUserLockedStatus(false);
        final List<ResolverTarget> targets =
                Arrays.asList(
                        makeNewResolverTarget(/* recency */ 0.1f, /* timeSpent */ 0.2f,
                                /* launch */ 0.3f, /* chooser */ 0.4f, /* selectProb */ -1.0f),
                        makeNewResolverTarget(/* recency */ 0.4f, /* timeSpent */ 0.3f,
                                /* launch */ 0.2f, /* chooser */ 0.1f, /* selectProb */ -1.0f));

        service.onPredictSharingProbabilities(targets);

        assertThat(targets.get(0).getSelectProbability()).isIn(Range.closed(0f, 1.0f));
        assertThat(targets.get(1).getSelectProbability()).isIn(Range.closed(0f, 1.0f));
    }

    @Test
    public void testOnPredictSharingProbabilities() {
        setUserLockedStatus(false);
        createSharedPreferences(/* hasSharedPreferences */ false, /* version */ 0);
        final LRResolverRankerService service = getService();
        service.onBind(new Intent("test"));
        final List<ResolverTarget> targets =
                Arrays.asList(
                        makeNewResolverTarget(/* recency */ 0.1f, /* timeSpent */ 0.2f,
                                /* launch */ 0.3f, /* chooser */ 0.4f, /* selectProb */ -1.0f),
                        makeNewResolverTarget(/* recency */ 0.4f, /* timeSpent */ 0.3f,
                                /* launch */ 0.2f, /* chooser */ 0.1f, /* selectProb */ -1.0f));

        service.onPredictSharingProbabilities(targets);

        assertThat(targets.get(0).getSelectProbability()).isIn(Range.closed(0f, 1.0f));
        assertThat(targets.get(1).getSelectProbability()).isIn(Range.closed(0f, 1.0f));
    }

    @Test
    public void testOnTrainRankingModelWhileServiceIsNotReady() {
        setUserLockedStatus(true);
        createSharedPreferences(/* hasSharedPreferences */ true, /* version */ 1);
        final LRResolverRankerService service = getService();
        service.onBind(new Intent("test"));
        final List<ResolverTarget> targets =
                Arrays.asList(
                        makeNewResolverTarget(/* recency */ 0.1f, /* timeSpent */ 0.1f,
                                /* launch */ 0.1f, /* chooser */ 0.1f, /* selectProb */ 0.1f),
                        makeNewResolverTarget(/* recency */ 0.2f, /* timeSpent */ 0.2f,
                                /* launch */ 0.2f, /* chooser */ 0.2f, /* selectProb */ 0.2f),
                        makeNewResolverTarget(/* recency */ 0.3f, /* timeSpent */ 0.3f,
                                /* launch */ 0.3f, /* chooser */ 0.3f, /* selectProb */ 0.3f));

        assertThrows(IllegalStateException.class,
                () -> service.onTrainRankingModel(targets, /* selectedPosition */ 0));
    }

    @Test
    public void testOnTrainRankingModelAfterUserUnlock() {
        setUserLockedStatus(true);
        createSharedPreferences(/* hasSharedPreferences */ true, /* version */ 1);
        final LRResolverRankerService service = getService();
        service.onBind(new Intent("test"));
        setUserLockedStatus(false);
        final List<ResolverTarget> targets =
                Arrays.asList(
                        makeNewResolverTarget(/* recency */ 0.1f, /* timeSpent */ 0.1f,
                                /* launch */ 0.1f, /* chooser */ 0.1f, /* selectProb */ 0.1f),
                        makeNewResolverTarget(/* recency */ 0.2f, /* timeSpent */ 0.2f,
                                /* launch */ 0.2f, /* chooser */ 0.2f, /* selectProb */ 0.2f),
                        makeNewResolverTarget(/* recency */ 0.3f, /* timeSpent */ 0.3f,
                                /* launch */ 0.3f, /* chooser */ 0.3f, /* selectProb */ 0.3f));

        service.onTrainRankingModel(targets, /* selectedPosition */ 0);

        final ArrayMap<String, Float> featureWeights = service.mFeatureWeights;
        assertThat(featureWeights.get(LAUNCH_SCORE)).isNotEqualTo(0.1f);
        assertThat(featureWeights.get(TIME_SPENT_SCORE)).isNotEqualTo(0.2f);
        assertThat(featureWeights.get(RECENCY_SCORE)).isNotEqualTo(0.3f);
        assertThat(featureWeights.get(CHOOSER_SCORE)).isNotEqualTo(0.4f);
    }

    @Test
    public void testOnTrainRankingModelWhileSelectedPositionScoreIsNotHighest() {
        setUserLockedStatus(false);
        createSharedPreferences(/* hasSharedPreferences */ true, /* version */ 1);
        final LRResolverRankerService service = getService();
        service.onBind(new Intent("test"));
        final List<ResolverTarget> targets =
                Arrays.asList(
                        makeNewResolverTarget(/* recency */ 0.1f, /* timeSpent */ 0.1f,
                                /* launch */ 0.1f, /* chooser */ 0.1f, /* selectProb */ 0.1f),
                        makeNewResolverTarget(/* recency */ 0.2f, /* timeSpent */ 0.2f,
                                /* launch */ 0.2f, /* chooser */ 0.2f, /* selectProb */ 0.2f),
                        makeNewResolverTarget(/* recency */ 0.3f, /* timeSpent */ 0.3f,
                                /* launch */ 0.3f, /* chooser */ 0.3f, /* selectProb */ 0.3f));

        service.onTrainRankingModel(targets, /* selectedPosition */ 0);

        final ArrayMap<String, Float> featureWeights = service.mFeatureWeights;
        assertThat(featureWeights.get(LAUNCH_SCORE)).isNotEqualTo(0.1f);
        assertThat(featureWeights.get(TIME_SPENT_SCORE)).isNotEqualTo(0.2f);
        assertThat(featureWeights.get(RECENCY_SCORE)).isNotEqualTo(0.3f);
        assertThat(featureWeights.get(CHOOSER_SCORE)).isNotEqualTo(0.4f);
    }

    @Test
    public void testOnTrainRankingModelWhileSelectedPositionScoreIsHighest() {
        setUserLockedStatus(false);
        createSharedPreferences(/* hasSharedPreferences */ true, /* version */ 1);
        final LRResolverRankerService service = getService();
        service.onBind(new Intent("test"));
        final List<ResolverTarget> targets =
                Arrays.asList(
                        makeNewResolverTarget(/* recency */ 0.1f, /* timeSpent */ 0.1f,
                                /* launch */ 0.1f, /* chooser */ 0.1f, /* selectProb */ 0.1f),
                        makeNewResolverTarget(/* recency */ 0.2f, /* timeSpent */ 0.2f,
                                /* launch */ 0.2f, /* chooser */ 0.2f, /* selectProb */ 0.2f),
                        makeNewResolverTarget(/* recency */ 0.3f, /* timeSpent */ 0.3f,
                                /* launch */ 0.3f, /* chooser */ 0.3f, /* selectProb */ 0.3f));

        service.onTrainRankingModel(targets, /* selectedPosition */ 2);

        final ArrayMap<String, Float> featureWeights = service.mFeatureWeights;
        assertThat(featureWeights.get(LAUNCH_SCORE)).isEqualTo(0.1f);
        assertThat(featureWeights.get(TIME_SPENT_SCORE)).isEqualTo(0.2f);
        assertThat(featureWeights.get(RECENCY_SCORE)).isEqualTo(0.3f);
        assertThat(featureWeights.get(CHOOSER_SCORE)).isEqualTo(0.4f);
    }

    private ResolverTarget makeNewResolverTarget(float recency, float timeSpent, float launch,
            float chooser, float selectProb) {
        ResolverTarget target = new ResolverTarget();
        target.setRecencyScore(recency);
        target.setTimeSpentScore(timeSpent);
        target.setLaunchScore(launch);
        target.setChooserScore(chooser);
        target.setSelectProbability(selectProb);
        return target;
    }

    private void createSharedPreferences(boolean hasSharedPreferences, int version) {
        when(mContext.createCredentialProtectedStorageContext()).thenReturn(mContext);
        if (!hasSharedPreferences) {
            when(mContext.getSharedPreferences(/* name */ PARAM_SHARED_PREF_NAME + ".xml",
                    Context.MODE_PRIVATE)).thenReturn(null);
        } else {
            when(mContext.getSharedPreferences(/* name */ PARAM_SHARED_PREF_NAME + ".xml",
                    Context.MODE_PRIVATE)).thenReturn(mSharedPreferences);
            when(mSharedPreferences.getInt(VERSION_PREF_KEY, 0)).thenReturn(version);
            when(mSharedPreferences.getFloat(BIAS_PREF_KEY, 0)).thenReturn(0.0f);
            when(mSharedPreferences.getFloat(LAUNCH_SCORE, 0)).thenReturn(0.1f);
            when(mSharedPreferences.getFloat(TIME_SPENT_SCORE, 0)).thenReturn(0.2f);
            when(mSharedPreferences.getFloat(RECENCY_SCORE, 0)).thenReturn(0.3f);
            when(mSharedPreferences.getFloat(CHOOSER_SCORE, 0)).thenReturn(0.4f);
            when(mSharedPreferences.edit()).thenReturn(mEditor);
            doNothing().when(mEditor).apply();
        }
    }

    private void setUserLockedStatus(boolean locked) {
        when(mContext.getSystemService(eq(Context.USER_SERVICE))).thenReturn(mUserManager);
        when(mUserManager.isUserUnlocked()).thenReturn(!locked);
    }
}
