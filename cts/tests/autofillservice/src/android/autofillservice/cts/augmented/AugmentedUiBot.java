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

import static android.autofillservice.cts.augmented.AugmentedHelper.getContentDescriptionForUi;
import static android.autofillservice.cts.augmented.AugmentedTimeouts.AUGMENTED_FILL_TIMEOUT;
import static android.autofillservice.cts.augmented.AugmentedTimeouts.AUGMENTED_UI_NOT_SHOWN_NAPTIME_MS;

import static com.google.common.truth.Truth.assertWithMessage;

import android.autofillservice.cts.R;
import android.autofillservice.cts.UiBot;
import android.support.test.uiautomator.UiObject2;
import android.view.autofill.AutofillId;

import androidx.annotation.NonNull;

import com.google.common.base.Preconditions;

import java.util.Objects;

/**
 * Helper for UI-related needs.
 */
public final class AugmentedUiBot {

    private final UiBot mUiBot;
    private boolean mOkToCallAssertUiGone;

    public AugmentedUiBot(@NonNull UiBot uiBot) {
        mUiBot = uiBot;
    }

    /**
     * Asserts the augmented autofill UI was never shown.
     *
     * <p>This method is slower than {@link #assertUiGone()} and should only be called in the
     * cases where the dataset picker was not previous shown.
     */
    public void assertUiNeverShown() throws Exception {
        mUiBot.assertNeverShownByRelativeId("augmented autofil UI", R.id.augmentedAutofillUi,
                AUGMENTED_UI_NOT_SHOWN_NAPTIME_MS);
    }

    /**
     * Asserts the augmented autofill UI was shown.
     *
     * @param focusedId where it should have been shown
     * @param expectedText the expected text in the UI
     */
    public UiObject2 assertUiShown(@NonNull AutofillId focusedId,
            @NonNull String expectedText) throws Exception {
        Objects.requireNonNull(focusedId);
        Objects.requireNonNull(expectedText);

        final UiObject2 ui = mUiBot.assertShownByRelativeId(R.id.augmentedAutofillUi);

        assertWithMessage("Wrong text on UI").that(ui.getText()).isEqualTo(expectedText);

        final String expectedContentDescription = getContentDescriptionForUi(focusedId);
        assertWithMessage("Wrong content description on UI")
                .that(ui.getContentDescription()).isEqualTo(expectedContentDescription);

        mOkToCallAssertUiGone = true;

        return ui;
    }

    /**
     * Asserts the augmented autofill UI is gone AFTER it was previously shown.
     *
     * @throws IllegalStateException if this method is called without calling
     * {@link #assertUiShown(AutofillId, String)} before.
     */
    public void assertUiGone() {
        Preconditions.checkState(mOkToCallAssertUiGone, "must call assertUiShown() first");
        mUiBot.assertGoneByRelativeId(R.id.augmentedAutofillUi, AUGMENTED_FILL_TIMEOUT);
    }
}
