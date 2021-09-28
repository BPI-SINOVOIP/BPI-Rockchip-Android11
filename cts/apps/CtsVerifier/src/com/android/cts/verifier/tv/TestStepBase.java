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
 * limitations under the License
 */

package com.android.cts.verifier.tv;

import android.view.View;
import android.widget.TextView;

import com.android.cts.verifier.R;

import com.google.common.truth.FailureStrategy;
import com.google.common.truth.StandardSubjectBuilder;

/** Encapsulates the logic of a test step, which displays human instructions. */
public abstract class TestStepBase {
    protected final TvAppVerifierActivity mContext;

    private boolean mHasPassed;
    private Runnable mOnDoneListener;
    private String mFailureDetails;
    private StandardSubjectBuilder mAsserter;
    private View mInstructionView;
    private String mInstructionText;

    /**
     * Constructs a test step containing instruction to the user and a button.
     *
     * @param context The test activity which this test step is part of.
     * @param instructionText The text of the test instruction visible to the user.
     */
    public TestStepBase(TvAppVerifierActivity context, String instructionText) {
        this.mContext = context;

        FailureStrategy failureStrategy =
                assertionError -> {
                    appendFailureDetails(assertionError.getMessage());
                    mHasPassed = false;
                };
        mAsserter = StandardSubjectBuilder.forCustomFailureStrategy(failureStrategy);
        mHasPassed = true;
        mInstructionText = instructionText;
    }

    public boolean hasPassed() {
        return mHasPassed;
    }

    /** Creates the View for this test step in the context {@link TvAppVerifierActivity}. */
    public void createUiElements() {
        mInstructionView = mContext.createAutoItem(mInstructionText);
    }

    /** Enables interactivity for this test step - for example, it enables buttons. */
    public abstract void enableInteractivity();

    /** Disables interactivity for this test step - for example, it disables buttons. */
    public abstract void disableInteractivity();

    public void setOnDoneListener(Runnable listener) {
        mOnDoneListener = listener;
    }

    public String getFailureDetails() {
        return mFailureDetails;
    }

    protected void done() {
        TvAppVerifierActivity.setPassState(mInstructionView, mHasPassed);
        if (mOnDoneListener != null) {
            mOnDoneListener.run();
        }
    }

    protected StandardSubjectBuilder getAsserter() {
        return mAsserter;
    }

    protected void appendInfoDetails(String infoFormat, Object... args) {
        String info = String.format(infoFormat, args);
        String details = String.format("Info: %s", info);
        appendDetails(details);
    }

    protected void appendFailureDetails(String failure) {
        String details = String.format("Failure: %s", failure);
        appendDetails(details);

        appendMessageToView(mInstructionView, details);
    }

    protected void appendDetails(String details) {
        if (mFailureDetails == null) {
            mFailureDetails = new String();
        }
        mFailureDetails += details + "\n";
    }

    private static void appendMessageToView(View item, String message) {
        TextView instructions = item.findViewById(R.id.instructions);
        instructions.setText(instructions.getText() + "\n" + message);
    }
}
