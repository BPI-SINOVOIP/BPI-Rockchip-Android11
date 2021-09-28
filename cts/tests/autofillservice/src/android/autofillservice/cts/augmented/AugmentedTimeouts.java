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

package android.autofillservice.cts.augmented;

import com.android.compatibility.common.util.Timeout;

/**
 * Timeouts for common tasks.
 */
final class AugmentedTimeouts {

    private static final long ONE_TIMEOUT_TO_RULE_THEN_ALL_MS = 1_000;
    private static final long ONE_NAPTIME_TO_RULE_THEN_ALL_MS = 3_000;

    /**
     * Timeout for expected augmented autofill requests.
     */
    static final Timeout AUGMENTED_FILL_TIMEOUT = new Timeout("AUGMENTED_FILL_TIMEOUT",
            ONE_TIMEOUT_TO_RULE_THEN_ALL_MS, 2F, ONE_TIMEOUT_TO_RULE_THEN_ALL_MS);

    /**
     * Timeout until framework binds / unbinds from service.
     */
    static final Timeout AUGMENTED_CONNECTION_TIMEOUT = new Timeout("AUGMENTED_CONNECTION_TIMEOUT",
            ONE_TIMEOUT_TO_RULE_THEN_ALL_MS, 2F, ONE_TIMEOUT_TO_RULE_THEN_ALL_MS);

    /**
     * Timeout used when the augmented autofill UI not expected to be shown - test will sleep for
     * that amount of time as there is no callback that be received to assert it's not shown.
     */
    static final long AUGMENTED_UI_NOT_SHOWN_NAPTIME_MS = ONE_NAPTIME_TO_RULE_THEN_ALL_MS;

    private AugmentedTimeouts() {
        throw new UnsupportedOperationException("contain static methods only");
    }
}
