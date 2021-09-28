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
package android.ext.services.autofill;

import static android.ext.services.autofill.CreditCardMatcher.OPTIONAL_ARG_SUFFIX_LENGTH;
import static android.ext.services.autofill.CreditCardMatcher.REQUIRED_ARG_MAX_CREDIT_CARD_LENGTH;
import static android.ext.services.autofill.CreditCardMatcher.REQUIRED_ARG_MIN_CREDIT_CARD_LENGTH;
import static android.ext.services.autofill.CreditCardMatcher.calculateScore;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.os.Bundle;
import android.view.autofill.AutofillValue;

import org.junit.Test;

public class CreditCardMatcherTest {
    @Test
    public void testCalculateScore_ForCreditCard() {
        final Bundle forCreditCard = forCreditCardBundle();
        assertFloat(calculateScore(AutofillValue.forText("5678"), "1234123412345678",
                forCreditCard), 1F);
        assertFloat(calculateScore(AutofillValue.forText("5678"), "12341234125678",
                forCreditCard), 1F);
        assertFloat(calculateScore(AutofillValue.forText("1234"), "1234123412341234",
                forCreditCard), 1F);
        assertFloat(calculateScore(AutofillValue.forText("1234"), "1234123412345678",
                forCreditCard), 0F);
        assertFloat(calculateScore(AutofillValue.forText("1234"), "12341234",
                forCreditCard), 0F);
        assertFloat(calculateScore(AutofillValue.forText("1234"), "12341234123412341234",
                forCreditCard), 0F);
    }

    @Test
    public void testCalculateScore_BadBundle() {
        final Bundle bundle = new Bundle();

        // Both length args are negative
        bundle.putInt(REQUIRED_ARG_MIN_CREDIT_CARD_LENGTH, -1);
        bundle.putInt(REQUIRED_ARG_MAX_CREDIT_CARD_LENGTH, -1);
        assertThrows(IllegalArgumentException.class, () -> calculateScore(
                AutofillValue.forText("TEST"), "TEST", bundle));

        // min_length is negative
        bundle.putInt(REQUIRED_ARG_MIN_CREDIT_CARD_LENGTH, -1);
        bundle.putInt(REQUIRED_ARG_MAX_CREDIT_CARD_LENGTH, 1);
        assertThrows(IllegalArgumentException.class, () -> calculateScore(
                AutofillValue.forText("TEST"), "TEST", bundle));

        // max_length is negative
        bundle.putInt(REQUIRED_ARG_MIN_CREDIT_CARD_LENGTH, 1);
        bundle.putInt(REQUIRED_ARG_MAX_CREDIT_CARD_LENGTH, -1);
        assertThrows(IllegalArgumentException.class, () -> calculateScore(
                AutofillValue.forText("TEST"), "TEST", bundle));

        // max is less than min
        bundle.putInt(REQUIRED_ARG_MIN_CREDIT_CARD_LENGTH, 4);
        bundle.putInt(REQUIRED_ARG_MAX_CREDIT_CARD_LENGTH, 3);
        assertThrows(IllegalArgumentException.class, () -> calculateScore(
                AutofillValue.forText("TEST"), "TEST", bundle));

        // suffix argument is invalid
        bundle.putInt(REQUIRED_ARG_MIN_CREDIT_CARD_LENGTH, 13);
        bundle.putInt(REQUIRED_ARG_MAX_CREDIT_CARD_LENGTH, 19);
        bundle.putInt(OPTIONAL_ARG_SUFFIX_LENGTH, -1);
        assertThrows(IllegalArgumentException.class, () -> calculateScore(
                AutofillValue.forText("TEST"), "TEST", bundle));
    }

    private Bundle forCreditCardBundle() {
        final Bundle bundle = new Bundle();
        bundle.putInt(REQUIRED_ARG_MIN_CREDIT_CARD_LENGTH, 13);
        bundle.putInt(REQUIRED_ARG_MAX_CREDIT_CARD_LENGTH, 19);
        bundle.putInt(OPTIONAL_ARG_SUFFIX_LENGTH, 4);
        return bundle;
    }

    public static void assertFloat(float actualValue, float expectedValue) {
        assertThat(actualValue).isWithin(0.01F).of(expectedValue);
    }
}
