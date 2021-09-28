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

package com.android.car.settings.accounts;

import android.accounts.AccountManager;
import android.accounts.AuthenticatorDescription;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.drawable.Drawable;
import android.os.UserHandle;

import androidx.annotation.VisibleForTesting;

import com.android.settingslib.accounts.AuthenticatorHelper;

import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;

/**
 * Utility that maintains a set of authorized account types.
 */
public class AccountTypesHelper {
    /** Callback invoked when the set of authorized account types changes. */
    public interface OnChangeListener {
        /** Called when the set of authorized account types changes. */
        void onAuthorizedAccountTypesChanged();
    }

    private final Context mContext;
    private UserHandle mUserHandle;
    private AuthenticatorHelper mAuthenticatorHelper;
    private List<String> mAuthorities;
    private Set<String> mAccountTypesFilter;
    private Set<String> mAccountTypesExclusionFilter;
    private Set<String> mAuthorizedAccountTypes;
    private OnChangeListener mListener;

    public AccountTypesHelper(Context context) {
        mContext = context;

        // Default to hardcoded Bluetooth account type.
        mAccountTypesExclusionFilter = new HashSet<>();
        mAccountTypesExclusionFilter.add("com.android.bluetooth.pbapsink");
        setAccountTypesExclusionFilter(mAccountTypesExclusionFilter);

        mUserHandle = UserHandle.of(UserHandle.myUserId());
        mAuthenticatorHelper = new AuthenticatorHelper(mContext, mUserHandle,
                userHandle -> {
                    // Only force a refresh if accounts have changed for the current user.
                    if (userHandle.equals(mUserHandle)) {
                        updateAuthorizedAccountTypes(false /* isForced */);
                    }
                });
    }

    /** Sets the authorities that the user has. */
    public void setAuthorities(List<String> authorities) {
        mAuthorities = authorities;
    }

    /** Sets the filter for accounts that should be shown. */
    public void setAccountTypesFilter(Set<String> accountTypesFilter) {
        mAccountTypesFilter = accountTypesFilter;
    }

    /** Sets the filter for accounts that should NOT be shown. */
    protected void setAccountTypesExclusionFilter(Set<String> accountTypesExclusionFilter) {
        mAccountTypesExclusionFilter = accountTypesExclusionFilter;
    }

    /** Sets the callback invoked when the set of authorized account types changes. */
    public void setOnChangeListener(OnChangeListener listener) {
        mListener = listener;
    }

    /**
     * Updates the set of authorized account types.
     *
     * <p>Derived from
     * {@link com.android.settings.accounts.ChooseAccountActivity#onAuthDescriptionsUpdated}
     */
    private void updateAuthorizedAccountTypes(boolean isForced) {
        AccountManager accountManager = AccountManager.get(mContext);
        AuthenticatorDescription[] authenticatorDescriptions =
                accountManager.getAuthenticatorTypesAsUser(mUserHandle.getIdentifier());

        Set<String> authorizedAccountTypes = new HashSet<>();
        for (AuthenticatorDescription authenticatorDescription : authenticatorDescriptions) {
            String accountType = authenticatorDescription.type;

            List<String> accountAuthorities =
                    mAuthenticatorHelper.getAuthoritiesForAccountType(accountType);

            // If there are specific authorities required, we need to check whether they are
            // included in the account type.
            boolean authorized =
                    (mAuthorities == null || mAuthorities.isEmpty() || accountAuthorities == null
                            || !Collections.disjoint(accountAuthorities, mAuthorities));

            // If there is an account type filter, make sure this account type is included.
            authorized = authorized && (mAccountTypesFilter == null
                    || mAccountTypesFilter.contains(accountType));

            // Check if the account type is in the exclusion list.
            authorized = authorized && (mAccountTypesExclusionFilter == null
                    || !mAccountTypesExclusionFilter.contains(accountType));

            if (authorized) {
                authorizedAccountTypes.add(accountType);
            }
        }

        if (isForced || !Objects.equals(mAuthorizedAccountTypes, authorizedAccountTypes)) {
            mAuthorizedAccountTypes = authorizedAccountTypes;
            if (mListener != null) {
                mListener.onAuthorizedAccountTypesChanged();
            }
        }
    }

    /** Returns the set of authorized account types, initializing the set first if necessary. */
    public Set<String> getAuthorizedAccountTypes() {
        if (mAuthorizedAccountTypes == null) {
            updateAuthorizedAccountTypes(false /* isForced */);
        }
        return mAuthorizedAccountTypes;
    }

    /** Forces an update of the authorized account types. */
    public void forceUpdate() {
        updateAuthorizedAccountTypes(true /* isForced */);
    }

    /** Starts listening for account updates. */
    public void listenToAccountUpdates() {
        mAuthenticatorHelper.listenToAccountUpdates();
    }

    /** Stops listening for account updates. */
    public void stopListeningToAccountUpdates() {
        mAuthenticatorHelper.stopListeningToAccountUpdates();
    }

    /**
     * Gets the label associated with a particular account type. Returns {@code null} if none found.
     *
     * @param accountType the type of account
     */
    public CharSequence getLabelForType(String accountType) {
        return mAuthenticatorHelper.getLabelForType(mContext, accountType);
    }

    /**
     * Gets an icon associated with a particular account type. Returns a default icon if none found.
     *
     * @param accountType the type of account
     * @return a drawable for the icon or a default icon returned by
     *     {@link PackageManager#getDefaultActivityIcon} if one cannot be found.
     */
    public Drawable getDrawableForType(String accountType) {
        return mAuthenticatorHelper.getDrawableForType(mContext, accountType);
    }

    /** Used for testing to trigger an account update. */
    @VisibleForTesting
    AuthenticatorHelper getAuthenticatorHelper() {
        return mAuthenticatorHelper;
    }
}
