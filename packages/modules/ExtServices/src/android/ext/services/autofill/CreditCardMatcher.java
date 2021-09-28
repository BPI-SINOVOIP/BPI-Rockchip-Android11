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

import android.os.Bundle;
import android.view.autofill.AutofillValue;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

final class CreditCardMatcher {

    /**
     * Required arg for {@link #calculateScore} that provides the min and max lengths for
     * credit card number.
     *
     * <p>must be non-negative and less than or equal to
     * {@link #REQUIRED_ARG_MAX_CREDIT_CARD_LENGTH}.</p>
     *
     * <p>Must supply an int.</p>
     */
    public static final String REQUIRED_ARG_MIN_CREDIT_CARD_LENGTH = "REQUIRED_ARG_MIN_CC_LENGTH";

    /**
     * Required arg for {@link #calculateScore} that provides the max length for
     * credit card number.
     *
     * <p>must be non-negative and greater or equal to
     * {@link #REQUIRED_ARG_MIN_CREDIT_CARD_LENGTH}.</p>
     *
     * <p>Must supply an int.</p>
     */
    public static final String REQUIRED_ARG_MAX_CREDIT_CARD_LENGTH = "REQUIRED_ARG_MAX_CC_LENGTH";

    /**
     * Optional arg for {@link #calculateScore} that provides the suffix length e.g. last N digits
     * of the credit card number.
     *
     * <p>If argument is not provided, the suffix length defaults to 4.</p>
     *
     * <p>Must supply an int.</p>
     */
    public static final String OPTIONAL_ARG_SUFFIX_LENGTH = "OPTIONAL_ARG_SUFFIX_LENGTH";

    /**
     * Gets the field classification score of a credit card number string and a string representing
     * the last four digits of the credit card.
     *
     * @return {@code 1.0} if the last four digits of the credit card matches with the given four
     * digits, and the length of the credit card number is within the given requirements,
     * {@code 0.0} otherwise.
     *
     * @param actualValue credit card number.
     * @param userDataValue four digit string.
     */
    @VisibleForTesting
    static float calculateScore(@Nullable AutofillValue actualValue,
            @Nullable String userDataValue, @Nullable Bundle args) {
        if (actualValue == null || !actualValue.isText() || userDataValue == null || args == null) {
            return 0;
        }
        final String actualValueText = actualValue.getTextValue().toString();

        final int minCreditCardLength = args.getInt(REQUIRED_ARG_MIN_CREDIT_CARD_LENGTH, -1);
        final int maxCreditCardLength = args.getInt(REQUIRED_ARG_MAX_CREDIT_CARD_LENGTH, -1);
        if (minCreditCardLength < 0 || maxCreditCardLength < minCreditCardLength) {
            throw new IllegalArgumentException("bad length arguments");
        }

        final int actualValueLength = actualValueText.length();
        final int userDataLength = userDataValue.length();
        final int suffixLength = args.getInt(OPTIONAL_ARG_SUFFIX_LENGTH, 4);
        if (suffixLength <= 0) {
            throw new IllegalArgumentException("bad suffix length argument");
        }

        // Satisfies length checks.
        if (actualValueLength != suffixLength || userDataLength < minCreditCardLength
                || userDataLength > maxCreditCardLength || userDataLength < actualValueLength) {
            return 0;
        }
        final String userDataLastN = userDataValue.substring(
                userDataLength - suffixLength);

        // Last 4 digits are equal.
        return actualValueText.equalsIgnoreCase(userDataLastN) ? 1 : 0;
    }
}
