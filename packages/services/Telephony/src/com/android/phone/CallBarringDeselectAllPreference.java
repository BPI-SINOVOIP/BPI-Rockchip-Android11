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

package com.android.phone;

import android.content.Context;
import android.os.Bundle;
import android.util.AttributeSet;
import android.view.View;
import android.widget.EditText;

import com.android.phone.settings.fdn.EditPinPreference;

/**
 * This preference represents the status of disable all barring option.
 */
public class CallBarringDeselectAllPreference extends EditPinPreference {
    private static final String LOG_TAG = "CallBarringDeselectAllPreference";
    private static final boolean DBG = (PhoneGlobals.DBG_LEVEL >= 2);

    /**
     * CallBarringDeselectAllPreference constructor.
     *
     * @param context The context of view.
     * @param attrs The attributes of the XML tag that is inflating EditTextPreference.
     */
    public CallBarringDeselectAllPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void showDialog(Bundle state) {
        setDialogMessage(getContext().getString(R.string.messageCallBarring));
        super.showDialog(state);
    }

    @Override
    protected void onBindDialogView(View view) {
        super.onBindDialogView(view);

        final EditText editText = (EditText) view.findViewById(android.R.id.edit);
        if (editText != null) {
            // Hide the input-text-line if the password is not shown.
            editText.setVisibility(View.VISIBLE);
        }
    }
}
