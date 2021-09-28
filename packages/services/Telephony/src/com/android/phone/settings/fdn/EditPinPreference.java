/*
 * Copyright (C) 2008 The Android Open Source Project
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

package com.android.phone.settings.fdn;

import android.app.AlertDialog;
import android.content.Context;
import android.preference.EditTextPreference;
import android.text.InputType;
import android.util.AttributeSet;
import android.view.View;

/**
 * Class similar to the com.android.settings.EditPinPreference
 * class, with a couple of modifications, including a different layout
 * for the dialog.
 */
public class EditPinPreference extends EditTextPreference {

    private boolean shouldHideButtons;

    /**
     * Interface definition for a callback to be invoked when the PIN is entered.
     */
    public interface OnPinEnteredListener {
        /**
         * Called when the dialog of {@link #EditPinPreference} is dismissed.
         *
         * @param preference the specified {@link #EditPinPreference}
         * @param positiveResult Whether the positive button was clicked (true), or
         *                       the negative button was clicked or the dialog was canceled (false).
         */
        void onPinEntered(EditPinPreference preference, boolean positiveResult);
    }

    private OnPinEnteredListener mPinListener;

    public void setOnPinEnteredListener(OnPinEnteredListener listener) {
        mPinListener = listener;
    }

    public EditPinPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public EditPinPreference(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void onBindDialogView(View view) {
        super.onBindDialogView(view);

        getEditText().setInputType(InputType.TYPE_CLASS_NUMBER
                | InputType.TYPE_NUMBER_VARIATION_PASSWORD);
        // If the layout does not contain an edittext, hide the buttons.
        shouldHideButtons = (view.findViewById(android.R.id.edit) == null);
    }

    @Override
    protected void onPrepareDialogBuilder(AlertDialog.Builder builder) {
        super.onPrepareDialogBuilder(builder);

        // hide the buttons if we need to.
        if (shouldHideButtons) {
            builder.setPositiveButton(null, this);
            builder.setNegativeButton(null, this);
        }
    }

    @Override
    protected void onDialogClosed(boolean positiveResult) {
        super.onDialogClosed(positiveResult);
        if (mPinListener != null) {
            mPinListener.onPinEntered(this, positiveResult);
        }
    }

    /**
     * Externally visible method to bring up the dialog to
     * for multi-step / multi-dialog requests (like changing
     * the SIM pin).
     */
    public void showPinDialog() {
        showDialog(null);
    }
}
