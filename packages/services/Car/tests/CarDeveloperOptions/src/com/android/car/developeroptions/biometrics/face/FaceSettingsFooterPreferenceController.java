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

package com.android.car.developeroptions.biometrics.face;

import android.content.Context;
import android.content.Intent;

import androidx.preference.Preference;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.core.BasePreferenceController;
import com.android.car.developeroptions.utils.AnnotationSpan;
import com.android.settingslib.HelpUtils;
import com.android.settingslib.widget.FooterPreference;

/**
 * Footer for face settings showing the help text and help link.
 */
public class FaceSettingsFooterPreferenceController extends BasePreferenceController {

    private static final String ANNOTATION_URL = "url";

    public FaceSettingsFooterPreferenceController(Context context, String preferenceKey) {
        super(context, preferenceKey);
    }

    public FaceSettingsFooterPreferenceController(Context context) {
        this(context, FooterPreference.KEY_FOOTER);
    }

    @Override
    public int getAvailabilityStatus() {
        return AVAILABLE;
    }

    @Override
    public void updateState(Preference preference) {
        super.updateState(preference);

        final Intent helpIntent = HelpUtils.getHelpIntent(
                mContext, mContext.getString(R.string.help_url_face), getClass().getName());
        final AnnotationSpan.LinkInfo linkInfo =
                new AnnotationSpan.LinkInfo(mContext, ANNOTATION_URL, helpIntent);
        preference.setTitle(AnnotationSpan.linkify(
                mContext.getText(R.string.security_settings_face_settings_footer), linkInfo));
    }
}
