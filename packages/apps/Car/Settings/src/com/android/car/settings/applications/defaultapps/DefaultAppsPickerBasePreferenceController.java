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

package com.android.car.settings.applications.defaultapps;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.os.UserHandle;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.preference.TwoStatePreference;

import com.android.car.settings.R;
import com.android.car.settings.common.ConfirmationDialogFragment;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.GroupSelectionPreferenceController;
import com.android.car.settings.common.Logger;
import com.android.car.ui.preference.CarUiRadioButtonPreference;
import com.android.settingslib.applications.DefaultAppInfo;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Defines the shared logic in picking a default application. */
public abstract class DefaultAppsPickerBasePreferenceController extends
        GroupSelectionPreferenceController {

    private static final Logger LOG = new Logger(DefaultAppsPickerBasePreferenceController.class);
    private static final String DIALOG_KEY_ARG = "key_arg";
    protected static final String NONE_PREFERENCE_KEY = "";

    private final Map<String, DefaultAppInfo> mDefaultAppInfoMap = new HashMap<>();
    private final ConfirmationDialogFragment.ConfirmListener mConfirmListener = arguments -> {
        setCurrentDefault(arguments.getString(DIALOG_KEY_ARG));
        notifyCheckedKeyChanged();
    };

    public DefaultAppsPickerBasePreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
    }

    @Override
    protected void onCreateInternal() {
        ConfirmationDialogFragment.resetListeners(
                (ConfirmationDialogFragment) getFragmentController().findDialogByTag(
                        ConfirmationDialogFragment.TAG),
                mConfirmListener,
                /* rejectListener= */ null,
                /* neutralListener= */ null);
    }

    @Override
    @NonNull
    protected List<TwoStatePreference> getGroupPreferences() {
        List<TwoStatePreference> entries = new ArrayList<>();
        if (includeNonePreference()) {
            entries.add(createNoneOption());
        }

        List<DefaultAppInfo> currentCandidates = getCandidates();
        if (currentCandidates != null) {
            for (DefaultAppInfo info : currentCandidates) {
                mDefaultAppInfoMap.put(info.getKey(), info);
                entries.add(createOption(info));
            }
        } else {
            LOG.i("no candidate provided");
        }

        return entries;
    }

    @Override
    protected final boolean handleGroupItemSelected(TwoStatePreference preference) {
        String selectedKey = preference.getKey();
        if (TextUtils.equals(selectedKey, getCurrentCheckedKey())) {
            return false;
        }

        CharSequence message = getConfirmationMessage(mDefaultAppInfoMap.get(selectedKey));
        if (!TextUtils.isEmpty(message)) {
            ConfirmationDialogFragment dialogFragment =
                    new ConfirmationDialogFragment.Builder(getContext())
                            .setMessage(message.toString())
                            .setPositiveButton(android.R.string.ok, mConfirmListener)
                            .setNegativeButton(android.R.string.cancel, /* rejectListener= */ null)
                            .addArgumentString(DIALOG_KEY_ARG, selectedKey)
                            .build();
            getFragmentController().showDialog(dialogFragment, ConfirmationDialogFragment.TAG);
            return false;
        }

        setCurrentDefault(selectedKey);
        return true;
    }

    @Override
    protected final String getCurrentCheckedKey() {
        return getCurrentDefaultKey();
    }

    protected TwoStatePreference createOption(DefaultAppInfo info) {
        CarUiRadioButtonPreference preference = new CarUiRadioButtonPreference(getContext());
        preference.setKey(info.getKey());
        preference.setTitle(info.loadLabel());
        preference.setIcon(DefaultAppUtils.getSafeIcon(info.loadIcon(),
                getContext().getResources().getInteger(R.integer.default_app_safe_icon_size)));
        preference.setEnabled(info.enabled);
        return preference;
    }

    /** Gets all of the candidates that should be considered when choosing a default application. */
    @NonNull
    protected abstract List<DefaultAppInfo> getCandidates();

    /** Gets the key of the currently selected candidate. */
    protected abstract String getCurrentDefaultKey();

    /**
     * Sets the key of the currently selected candidate. The implementation of this method should
     * modify the value returned by {@link #getCurrentDefaultKey()}}.
     *
     * @param key represents the key from {@link DefaultAppInfo} which should mark the default
     *            application.
     */
    protected abstract void setCurrentDefault(String key);

    /**
     * Defines the warning dialog message to be shown when a default app is selected.
     */
    protected CharSequence getConfirmationMessage(DefaultAppInfo info) {
        return null;
    }

    /** Gets the current process user id. */
    protected int getCurrentProcessUserId() {
        return UserHandle.myUserId();
    }

    /**
     * Determines whether the list of default apps should include "none". Implementation classes can
     * override this value to {@code false} in order to remove the "none" preference.
     */
    protected boolean includeNonePreference() {
        return true;
    }

    private CarUiRadioButtonPreference createNoneOption() {
        CarUiRadioButtonPreference preference = new CarUiRadioButtonPreference(getContext());
        preference.setKey(NONE_PREFERENCE_KEY);
        preference.setTitle(R.string.app_list_preference_none);
        preference.setIcon(R.drawable.ic_remove_circle);
        return preference;
    }
}
