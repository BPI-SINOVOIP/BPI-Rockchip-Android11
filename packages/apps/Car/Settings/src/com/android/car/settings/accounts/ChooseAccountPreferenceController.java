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

package com.android.car.settings.accounts;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Handler;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.collection.ArrayMap;
import androidx.preference.PreferenceGroup;

import com.android.car.settings.common.ActivityResultCallback;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceController;
import com.android.car.ui.preference.CarUiPreference;
import com.android.settingslib.accounts.AuthenticatorHelper;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Controller for showing the user the list of accounts they can add.
 *
 * <p>Largely derived from {@link com.android.settings.accounts.ChooseAccountActivity}
 */
public class ChooseAccountPreferenceController extends
        PreferenceController<PreferenceGroup> implements ActivityResultCallback {
    @VisibleForTesting
    static final int ADD_ACCOUNT_REQUEST_CODE = 100;

    private AccountTypesHelper mAccountTypesHelper;
    private ArrayMap<String, AuthenticatorDescriptionPreference> mPreferences = new ArrayMap<>();
    private boolean mIsStarted = false;
    private boolean mHasPendingBack = false;

    public ChooseAccountPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
        mAccountTypesHelper = new AccountTypesHelper(context);
        mAccountTypesHelper.setOnChangeListener(this::forceUpdateAccountsCategory);
    }

    /** Sets the authorities that the user has. */
    public void setAuthorities(List<String> authorities) {
        mAccountTypesHelper.setAuthorities(authorities);
    }

    /** Sets the filter for accounts that should be shown. */
    public void setAccountTypesFilter(Set<String> accountTypesFilter) {
        mAccountTypesHelper.setAccountTypesFilter(accountTypesFilter);
    }

    /** Sets the filter for accounts that should NOT be shown. */
    protected void setAccountTypesExclusionFilter(Set<String> accountTypesExclusionFilterFilter) {
        mAccountTypesHelper.setAccountTypesExclusionFilter(accountTypesExclusionFilterFilter);
    }

    @Override
    protected Class<PreferenceGroup> getPreferenceType() {
        return PreferenceGroup.class;
    }

    @Override
    protected void updateState(PreferenceGroup preferenceGroup) {
        mAccountTypesHelper.forceUpdate();
    }

    /**
     * Registers the account update callback.
     */
    @Override
    protected void onStartInternal() {
        mIsStarted = true;
        mAccountTypesHelper.listenToAccountUpdates();

        if (mHasPendingBack) {
            mHasPendingBack = false;

            // Post the fragment navigation because FragmentManager may still be executing
            // transactions during onStart.
            new Handler().post(() -> getFragmentController().goBack());
        }
    }

    /**
     * Unregisters the account update callback.
     */
    @Override
    protected void onStopInternal() {
        mAccountTypesHelper.stopListeningToAccountUpdates();
        mIsStarted = false;
    }

    /** Forces a refresh of the authenticator description preferences. */
    private void forceUpdateAccountsCategory() {
        Set<String> preferencesToRemove = new HashSet<>(mPreferences.keySet());
        List<AuthenticatorDescriptionPreference> preferences =
                getAuthenticatorDescriptionPreferences(preferencesToRemove);
        // Add all preferences that aren't already shown
        for (int i = 0; i < preferences.size(); i++) {
            AuthenticatorDescriptionPreference preference = preferences.get(i);
            preference.setOrder(i);
            String key = preference.getKey();
            getPreference().addPreference(preference);
            mPreferences.put(key, preference);
        }

        // Remove all preferences that should no longer be shown
        for (String key : preferencesToRemove) {
            getPreference().removePreference(mPreferences.get(key));
            mPreferences.remove(key);
        }
    }

    /**
     * Returns a list of preferences corresponding to the account types the user can add.
     *
     * <p> Derived from
     * {@link com.android.settings.accounts.ChooseAccountActivity#onAuthDescriptionsUpdated}
     *
     * @param preferencesToRemove the current preferences shown; will contain the preferences that
     *                            need to be removed from the screen after method execution
     */
    private List<AuthenticatorDescriptionPreference> getAuthenticatorDescriptionPreferences(
            Set<String> preferencesToRemove) {
        ArrayList<AuthenticatorDescriptionPreference> authenticatorDescriptionPreferences =
                new ArrayList<>();
        Set<String> authorizedAccountTypes = mAccountTypesHelper.getAuthorizedAccountTypes();
        // Create list of account providers to show on page.
        for (String accountType : authorizedAccountTypes) {
            CharSequence label = mAccountTypesHelper.getLabelForType(accountType);
            Drawable icon = mAccountTypesHelper.getDrawableForType(accountType);

            // Add a preference for the provider to the list and remove it from preferencesToRemove.
            AuthenticatorDescriptionPreference preference = mPreferences.getOrDefault(accountType,
                    new AuthenticatorDescriptionPreference(getContext(), accountType, label, icon));
            preference.setOnPreferenceClickListener(
                    pref -> {
                        Intent intent = AddAccountActivity.createAddAccountActivityIntent(
                                getContext(), preference.getAccountType());
                        getFragmentController().startActivityForResult(intent,
                                ADD_ACCOUNT_REQUEST_CODE, /* callback= */ this);
                        return true;
                    });
            authenticatorDescriptionPreferences.add(preference);
            preferencesToRemove.remove(accountType);
        }

        Collections.sort(authenticatorDescriptionPreferences, Comparator.comparing(
                (AuthenticatorDescriptionPreference a) -> a.getTitle().toString()));

        return authenticatorDescriptionPreferences;
    }

    /** Used for testing to trigger an account update. */
    @VisibleForTesting
    AuthenticatorHelper getAuthenticatorHelper() {
        return mAccountTypesHelper.getAuthenticatorHelper();
    }

    @Override
    public void processActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        if (requestCode == ADD_ACCOUNT_REQUEST_CODE) {
            if (mIsStarted) {
                getFragmentController().goBack();
            } else {
                mHasPendingBack = true;
            }
        }
    }

    /** Handles adding accounts. */
    interface AddAccountListener {
        /** Handles adding an account. */
        void addAccount(String accountType);
    }

    private static class AuthenticatorDescriptionPreference extends CarUiPreference {
        private final String mType;

        AuthenticatorDescriptionPreference(Context context, String accountType, CharSequence label,
                Drawable icon) {
            super(context);
            mType = accountType;

            setKey(accountType);
            setTitle(label);
            setIcon(icon);
            setShowChevron(false);
        }

        private String getAccountType() {
            return mType;
        }
    }
}
