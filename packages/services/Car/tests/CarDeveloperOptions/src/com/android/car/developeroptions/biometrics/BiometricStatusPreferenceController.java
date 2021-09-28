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

package com.android.car.developeroptions.biometrics;

import static com.android.car.developeroptions.Utils.SETTINGS_PACKAGE_NAME;

import android.content.Context;
import android.content.Intent;
import android.os.UserHandle;
import android.os.UserManager;

import androidx.preference.Preference;

import com.android.internal.widget.LockPatternUtils;
import com.android.car.developeroptions.Utils;
import com.android.car.developeroptions.core.BasePreferenceController;
import com.android.car.developeroptions.overlay.FeatureFactory;

public abstract class BiometricStatusPreferenceController extends BasePreferenceController {

    protected final UserManager mUm;
    protected final LockPatternUtils mLockPatternUtils;

    private final int mUserId = UserHandle.myUserId();
    protected final int mProfileChallengeUserId;

    /**
     * @return true if the manager is not null and the hardware is detected.
     */
    protected abstract boolean isDeviceSupported();

    /**
     * @return true if the user has enrolled biometrics of the subclassed type.
     */
    protected abstract boolean hasEnrolledBiometrics();

    /**
     * @return the summary text if biometrics are enrolled.
     */
    protected abstract String getSummaryTextEnrolled();

    /**
     * @return the summary text if no biometrics are enrolled.
     */
    protected abstract String getSummaryTextNoneEnrolled();

    /**
     * @return the class name for the settings page.
     */
    protected abstract String getSettingsClassName();

    /**
     * @return the class name for entry to enrollment.
     */
    protected abstract String getEnrollClassName();

    public BiometricStatusPreferenceController(Context context, String key) {
        super(context, key);
        mUm = (UserManager) context.getSystemService(Context.USER_SERVICE);
        mLockPatternUtils = FeatureFactory.getFactory(context)
                .getSecurityFeatureProvider()
                .getLockPatternUtils(context);
        mProfileChallengeUserId = Utils.getManagedProfileId(mUm, mUserId);
    }

    @Override
    public int getAvailabilityStatus() {
        if (!isDeviceSupported()) {
            return UNSUPPORTED_ON_DEVICE;
        }
        if (isUserSupported()) {
            return AVAILABLE;
        } else {
            return DISABLED_FOR_USER;
        }
    }

    @Override
    public void updateState(Preference preference) {
        if (!isAvailable()) {
            if (preference != null) {
                preference.setVisible(false);
            }
            return;
        } else {
            preference.setVisible(true);
        }
        final int userId = getUserId();
        final String clazz;
        if (hasEnrolledBiometrics()) {
            preference.setSummary(getSummaryTextEnrolled());
            clazz = getSettingsClassName();
        } else {
            preference.setSummary(getSummaryTextNoneEnrolled());
            clazz = getEnrollClassName();
        }
        preference.setOnPreferenceClickListener(target -> {
            final Context context = target.getContext();
            final UserManager userManager = UserManager.get(context);
            if (Utils.startQuietModeDialogIfNecessary(context, userManager,
                    userId)) {
                return false;
            }
            Intent intent = new Intent();
            intent.setClassName(SETTINGS_PACKAGE_NAME, clazz);
            intent.putExtra(Intent.EXTRA_USER_ID, userId);
            context.startActivity(intent);
            return true;
        });
    }

    protected int getUserId() {
        return mUserId;
    }

    protected boolean isUserSupported() {
        return true;
    }
}
