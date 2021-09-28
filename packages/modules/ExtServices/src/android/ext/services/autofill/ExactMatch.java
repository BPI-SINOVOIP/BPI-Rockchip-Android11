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

import android.os.Bundle;
import android.view.autofill.AutofillValue;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

final class ExactMatch {

    /**
     * Arg for {@link #calculateScore} that enforces only matching the last N values.
     *
     * <p>Must supply an int N.</p>
     */
    public static final String MATCH_SUFFIX = "MATCH_SUFFIX";

    /**
     * Gets the field classification score of 2 values based on whether they are an exact match
     *
     * @return {@code 1.0} if the two values are an exact match, {@code 0.0} otherwise.
     */
    @VisibleForTesting
    static float calculateScore(@Nullable AutofillValue actualValue,
            @Nullable String userDataValue, @Nullable Bundle args) {
        if (actualValue == null || !actualValue.isText() || userDataValue == null) return 0;

        final String actualValueText = actualValue.getTextValue().toString();

        if (args == null) {
            return actualValueText.equalsIgnoreCase(userDataValue) ? 1 : 0;
        }

        final int suffixLength = args.getInt(MATCH_SUFFIX, -1);

        if (suffixLength < 0) {
            throw new IllegalArgumentException("suffix argument is invalid");
        }

        final String actualValueSuffix;
        if (suffixLength < actualValueText.length()) {
            actualValueSuffix = actualValueText.substring(actualValueText.length()
                    - suffixLength);
        } else {
            actualValueSuffix = actualValueText;
        }

        final String userDataValueSuffix;
        if (suffixLength < userDataValue.length()) {
            userDataValueSuffix = userDataValue.substring(userDataValue.length()
                    - suffixLength);
        } else {
            userDataValueSuffix = userDataValue;
        }

        return (actualValueSuffix.equalsIgnoreCase(userDataValueSuffix)) ? 1 : 0;
    }
}
