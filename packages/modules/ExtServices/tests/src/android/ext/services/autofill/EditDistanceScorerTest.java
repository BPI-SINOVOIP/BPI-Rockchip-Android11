/*
 * Copyright (C) 2018 The Android Open Source Project
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
package android.ext.services.autofill;

import static android.ext.services.autofill.EditDistanceScorer.calculateScore;
import static android.ext.services.autofill.EditDistanceScorer.editDistance;

import static com.google.common.truth.Truth.assertThat;

import android.view.autofill.AutofillValue;

import org.junit.Test;

public class EditDistanceScorerTest {

    @Test
    public void testCalculateScore_nullValue() {
        assertFloat(calculateScore(null, "D'OH!", null), 0);
    }

    @Test
    public void testCalculateScore_nonTextValue() {
        assertFloat(calculateScore(AutofillValue.forToggle(true), "D'OH!", null), 0);
    }

    @Test
    public void testCalculateScore_nullUserData() {
        assertFloat(calculateScore(AutofillValue.forText("D'OH!"), null, null), 0);
    }

    @Test
    public void testCalculateScore_fullMatch() {
        assertFloat(calculateScore(AutofillValue.forText("D'OH!"), "D'OH!", null), 1);
        assertFloat(calculateScore(AutofillValue.forText(""), "", null), 1);
    }

    @Test
    public void testCalculateScore_fullMatchMixedCase() {
        assertFloat(calculateScore(AutofillValue.forText("D'OH!"), "D'oH!", null), 1);
    }

    @Test
    public void testCalculateScore_mismatchDifferentSizes() {
        assertFloat(calculateScore(AutofillValue.forText("X"), "Xy", null), 0.50F);
        assertFloat(calculateScore(AutofillValue.forText("Xy"), "X", null), 0.50F);
        assertFloat(calculateScore(AutofillValue.forText("One"), "MoreThanOne", null), 0.27F);
        assertFloat(calculateScore(AutofillValue.forText("MoreThanOne"), "One", null), 0.27F);
        assertFloat(calculateScore(AutofillValue.forText("1600 Amphitheatre Parkway"),
                "1600 Amphitheatre Pkwy", null), 0.88F);
        assertFloat(calculateScore(AutofillValue.forText("1600 Amphitheatre Pkwy"),
                "1600 Amphitheatre Parkway", null), 0.88F);
    }

    @Test
    public void testCalculateScore_partialMatch() {
        assertFloat(calculateScore(AutofillValue.forText("Dude"), "Dxxx", null), 0.25F);
        assertFloat(calculateScore(AutofillValue.forText("Dude"), "DUxx", null), 0.50F);
        assertFloat(calculateScore(AutofillValue.forText("Dude"), "DUDx", null), 0.75F);
        assertFloat(calculateScore(AutofillValue.forText("Dxxx"), "Dude", null), 0.25F);
        assertFloat(calculateScore(AutofillValue.forText("DUxx"), "Dude", null), 0.50F);
        assertFloat(calculateScore(AutofillValue.forText("DUDx"), "Dude", null), 0.75F);
    }

    @Test
    public void testEditDistance_maxDistance() {
        assertFloat(editDistance("testing", "b", 4), Integer.MAX_VALUE);
    }

    public static void assertFloat(float actualValue, float expectedValue) {
        assertThat(actualValue).isWithin(0.01F).of(expectedValue);
    }
}
