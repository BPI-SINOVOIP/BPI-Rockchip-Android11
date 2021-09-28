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

package com.android.car.audio;

import static com.android.car.audio.CarAudioContext.AudioContext;
import static com.android.car.audio.FocusInteraction.INTERACTION_CONCURRENT;
import static com.android.car.audio.FocusInteraction.INTERACTION_EXCLUSIVE;
import static com.android.car.audio.FocusInteraction.INTERACTION_REJECT;
import static com.android.dx.mockito.inline.extended.ExtendedMockito.doReturn;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.testng.Assert.assertThrows;

import android.content.ContentResolver;
import android.media.AudioManager;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public class FocusInteractionTest {
    private static final int UNDEFINED_CONTEXT_VALUE = -10;
    private static final int TEST_USER_ID = 10;

    @Mock
    private CarAudioSettings mMockCarAudioSettings;
    @Mock
    private ContentResolver mMockContentResolver;
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final List<FocusEntry> mLosers = new ArrayList<>();

    private FocusInteraction mFocusInteraction;

    @Before
    public void setUp() {
        doReturn(mMockContentResolver).when(mMockCarAudioSettings).getContentResolver();
        mFocusInteraction = new FocusInteraction(mMockCarAudioSettings);
    }

    @Test
    public void getInteractionMatrix_returnsNByNMatrix() {
        int n = CarAudioContext.CONTEXTS.length + 1; // One extra for CarAudioContext.INVALID

        int[][] interactionMatrix = mFocusInteraction.getInteractionMatrix();

        assertThat(interactionMatrix.length).isEqualTo(n);
        for (int i = 0; i < n; i++) {
            assertWithMessage("Row %s is not of length %s", i, n)
                    .that(interactionMatrix[i].length).isEqualTo(n);
        }
    }

    @Test
    public void getInteractionMatrix_hasValidInteractionValues() {
        List<Integer> supportedInteractions = Arrays.asList(INTERACTION_REJECT,
                INTERACTION_EXCLUSIVE, INTERACTION_CONCURRENT);

        int[][] interactionMatrix = mFocusInteraction.getInteractionMatrix();

        for (int i = 0; i < interactionMatrix.length; i++) {
            for (int j = 0; j < interactionMatrix[i].length; j++) {
                assertWithMessage("Row %s column %s has unexpected value %s", i, j,
                        interactionMatrix[i][j]).that(
                        interactionMatrix[i][j]).isIn(supportedInteractions);
            }
        }
    }

    @Test
    public void evaluateResult_forRejectPair_returnsFailed() {
        FocusEntry focusEntry = newMockFocusEntryWithContext(CarAudioContext.INVALID);

        int result = mFocusInteraction.evaluateRequest(CarAudioContext.INVALID, focusEntry, mLosers,
                false, false);

        assertThat(result).isEqualTo(AudioManager.AUDIOFOCUS_REQUEST_FAILED);
    }

    @Test
    public void evaluateResult_forCallAndNavigation_withNavigationNotRejected_returnsConcurrent() {
        doReturn(false)
                .when(mMockCarAudioSettings)
                .isRejectNavigationOnCallEnabledInSettings(TEST_USER_ID);

        mFocusInteraction.setUserIdForSettings(TEST_USER_ID);
        FocusEntry focusEntry = newMockFocusEntryWithContext(CarAudioContext.CALL);

        int result = mFocusInteraction.evaluateRequest(CarAudioContext.NAVIGATION, focusEntry,
                mLosers, false, false);

        assertThat(result).isEqualTo(AudioManager.AUDIOFOCUS_REQUEST_GRANTED);
    }

    @Test
    public void evaluateResult_forCallAndNavigation_withNavigationRejected_returnsConcurrent() {
        doReturn(true)
                .when(mMockCarAudioSettings)
                .isRejectNavigationOnCallEnabledInSettings(TEST_USER_ID);
        mFocusInteraction.setUserIdForSettings(TEST_USER_ID);
        FocusEntry focusEntry = newMockFocusEntryWithContext(CarAudioContext.CALL);

        int result = mFocusInteraction.evaluateRequest(CarAudioContext.NAVIGATION, focusEntry,
                mLosers, false, false);

        assertThat(result).isEqualTo(AudioManager.AUDIOFOCUS_REQUEST_FAILED);
    }

    @Test
    public void evaluateResult_forRejectPair_doesNotAddToLosers() {
        FocusEntry focusEntry = newMockFocusEntryWithContext(CarAudioContext.INVALID);

        mFocusInteraction
                .evaluateRequest(CarAudioContext.INVALID, focusEntry, mLosers, false,
                        false);

        assertThat(mLosers).isEmpty();
    }

    @Test
    public void evaluateRequest_forExclusivePair_returnsGranted() {
        FocusEntry focusEntry = newMockFocusEntryWithContext(CarAudioContext.MUSIC);

        int result = mFocusInteraction.evaluateRequest(CarAudioContext.MUSIC, focusEntry, mLosers,
                false, false);

        assertThat(result).isEqualTo(AudioManager.AUDIOFOCUS_REQUEST_GRANTED);
    }

    @Test
    public void evaluateRequest_forExclusivePair_addsEntryToLosers() {
        FocusEntry focusEntry = newMockFocusEntryWithContext(CarAudioContext.MUSIC);

        mFocusInteraction
                .evaluateRequest(CarAudioContext.MUSIC, focusEntry, mLosers, false,
                        false);

        assertThat(mLosers).containsExactly(focusEntry);
    }

    @Test
    public void evaluateResult_forConcurrentPair_returnsGranted() {
        FocusEntry focusEntry = newMockFocusEntryWithContext(CarAudioContext.NAVIGATION);

        int result = mFocusInteraction.evaluateRequest(CarAudioContext.MUSIC, focusEntry, mLosers,
                false, false);

        assertThat(result).isEqualTo(AudioManager.AUDIOFOCUS_REQUEST_GRANTED);
    }

    @Test
    public void evaluateResult_forConcurrentPair_andNoDucking_addsToLosers() {
        FocusEntry focusEntry =
                newMockFocusEntryWithDuckingBehavior(false, false);

        mFocusInteraction
                .evaluateRequest(CarAudioContext.MUSIC, focusEntry, mLosers, false, false);

        assertThat(mLosers).containsExactly(focusEntry);
    }

    @Test
    public void evaluateResult_forConcurrentPair_andWantsPauseInsteadOfDucking_addsToLosers() {
        FocusEntry focusEntry =
                newMockFocusEntryWithDuckingBehavior(true, false);

        mFocusInteraction
                .evaluateRequest(CarAudioContext.MUSIC, focusEntry, mLosers, true, false);

        assertThat(mLosers).containsExactly(focusEntry);
    }

    @Test
    public void evaluateResult_forConcurrentPair_andReceivesDuckEvents_addsToLosers() {
        FocusEntry focusEntry =
                newMockFocusEntryWithDuckingBehavior(false, true);

        mFocusInteraction
                .evaluateRequest(CarAudioContext.MUSIC, focusEntry, mLosers, true, false);

        assertThat(mLosers).containsExactly(focusEntry);
    }

    @Test
    public void evaluateResult_forConcurrentPair_andDucking_doesAddsToLosers() {
        FocusEntry focusEntry =
                newMockFocusEntryWithDuckingBehavior(false, true);

        mFocusInteraction
                .evaluateRequest(CarAudioContext.MUSIC, focusEntry, mLosers, true,
                        false);

        assertThat(mLosers).containsExactly(focusEntry);
    }

    @Test
    public void evaluateResult_forUndefinedContext_throws() {
        FocusEntry focusEntry = newMockFocusEntryWithContext(CarAudioContext.NAVIGATION);

        assertThrows(IllegalArgumentException.class,
                () -> mFocusInteraction.evaluateRequest(UNDEFINED_CONTEXT_VALUE, focusEntry,
                        mLosers, false, false));
    }

    @Test
    public void evaluateResult_forUndefinedFocusHolderContext_throws() {
        FocusEntry focusEntry = newMockFocusEntryWithContext(UNDEFINED_CONTEXT_VALUE);

        assertThrows(IllegalArgumentException.class,
                () -> mFocusInteraction.evaluateRequest(CarAudioContext.MUSIC, focusEntry,
                        mLosers, false, false));
    }

    @Test
    public void evaluateRequest_forExclusivePair_withDelayedFocus_returnsGranted() {
        FocusEntry focusEntry = newMockFocusEntryWithContext(CarAudioContext.MUSIC);

        int result = mFocusInteraction.evaluateRequest(CarAudioContext.MUSIC, focusEntry, mLosers,
                false, true);

        assertThat(result).isEqualTo(AudioManager.AUDIOFOCUS_REQUEST_GRANTED);
    }

    @Test
    public void evaluateRequest_forRejectPair_withDelayedFocus_returnsDelayed() {
        FocusEntry focusEntry = newMockFocusEntryWithContext(CarAudioContext.CALL);

        int result = mFocusInteraction.evaluateRequest(CarAudioContext.MUSIC, focusEntry, mLosers,
                false, true);

        assertThat(result).isEqualTo(AudioManager.AUDIOFOCUS_REQUEST_DELAYED);
    }

    @Test
    public void evaluateRequest_forRejectPair_withoutDelayedFocus_returnsReject() {
        FocusEntry focusEntry = newMockFocusEntryWithContext(CarAudioContext.CALL);

        int result = mFocusInteraction.evaluateRequest(CarAudioContext.MUSIC, focusEntry, mLosers,
                false, false);

        assertThat(result).isEqualTo(AudioManager.AUDIOFOCUS_REQUEST_FAILED);
    }

    private FocusEntry newMockFocusEntryWithContext(@AudioContext int audioContext) {
        FocusEntry focusEntry = mock(FocusEntry.class);
        when(focusEntry.getAudioContext()).thenReturn(audioContext);
        return focusEntry;
    }

    private FocusEntry newMockFocusEntryWithDuckingBehavior(boolean pauseInsteadOfDucking,
            boolean receivesDuckingEvents) {
        FocusEntry focusEntry = newMockFocusEntryWithContext(CarAudioContext.NAVIGATION);
        when(focusEntry.wantsPauseInsteadOfDucking()).thenReturn(pauseInsteadOfDucking);
        when(focusEntry.receivesDuckEvents()).thenReturn(receivesDuckingEvents);
        return focusEntry;
    }
}
