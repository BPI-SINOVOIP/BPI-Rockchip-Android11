/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.settings.display;

import static com.android.settingslib.RestrictedLockUtils.EnforcedAdmin;

import android.app.Dialog;
import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.DialogInterface;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;

import androidx.appcompat.app.AlertDialog.Builder;

import com.android.settings.R;
import com.android.settings.RestrictedListPreference;
import com.android.settingslib.RestrictedLockUtils;

import java.util.ArrayList;


public class RotationListPreference extends RestrictedListPreference {
    private static final String TAG = "RotationListPreference";
    private EnforcedAdmin mAdmin;
    private final CharSequence[] mInitialEntries;
    private final CharSequence[] mInitialValues;

    public RotationListPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mInitialEntries = getEntries();
        mInitialValues = getEntryValues();
    }

    @Override
    protected void onPrepareDialogBuilder(Builder builder,
            DialogInterface.OnClickListener listener) {
        super.onPrepareDialogBuilder(builder, listener);
        if (mAdmin != null) {
            builder.setView(R.layout.admin_disabled_other_options_footer);
        } else {
            builder.setView(null);
        }
    }

    @Override
    protected void onDialogCreated(Dialog dialog) {
        super.onDialogCreated(dialog);
        dialog.create();
        if (mAdmin != null) {
            View footerView = dialog.findViewById(R.id.admin_disabled_other_options);
            footerView.findViewById(R.id.admin_more_details_link).setOnClickListener(
                    new View.OnClickListener() {
                        @Override
                        public void onClick(View view) {
                            RestrictedLockUtils.sendShowAdminSupportDetailsIntent(
                                    getContext(), mAdmin);
                        }
                    });
        }
    }
}
