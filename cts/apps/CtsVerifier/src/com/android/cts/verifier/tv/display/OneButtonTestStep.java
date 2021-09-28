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

package com.android.cts.verifier.tv.display;

import android.view.View;

import androidx.annotation.StringRes;

import com.android.cts.verifier.tv.TestStepBase;
import com.android.cts.verifier.tv.TvAppVerifierActivity;

/** Test step containing instruction to the user and a button. */
public abstract class OneButtonTestStep extends TestStepBase {

    protected View mButtonView;

    @StringRes private int mButtonStringId;

    @StringRes private int mStepNameStringId;

    /**
     * Constructs a test step containing instruction to the user and a button.
     *
     * @param context The test activity which this test step is part of.
     * @param instructionText The text of the test instruction visible to the user.
     * @param stepNameStringId Id of a string resource containing human readable name of this step
     *     to be used in logs.
     * @param buttonStringId Id of a string resource containing the text of the button.
     */
    public OneButtonTestStep(
            TvAppVerifierActivity context,
            @StringRes int stepNameStringId,
            String instructionText,
            @StringRes int buttonStringId) {
        super(context, instructionText);
        mStepNameStringId = stepNameStringId;
        mButtonStringId = buttonStringId;
    }

    @Override
    public void createUiElements() {
        super.createUiElements();
        mButtonView =
                mContext.createButtonItem(
                        mButtonStringId,
                        (View view) -> {
                            String stepName = mContext.getString(mStepNameStringId);
                            appendInfoDetails("Running test step %s...", stepName);
                            onButtonClickRunTest();
                        });
    }

    @Override
    public void enableInteractivity() {
        TvAppVerifierActivity.setButtonEnabled(mButtonView, true);
    }

    @Override
    public void disableInteractivity() {
        TvAppVerifierActivity.setButtonEnabled(mButtonView, false);
    }

    /** Test logic to be executed when the button is pressed. */
    protected abstract void onButtonClickRunTest();
}
