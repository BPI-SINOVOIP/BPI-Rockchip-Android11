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

package com.android.car.developeroptions.password;

import static com.android.car.developeroptions.Utils.SETTINGS_PACKAGE_NAME;

import android.annotation.Nullable;
import android.app.Activity;
import android.app.KeyguardManager;
import android.app.admin.DevicePolicyManager;
import android.content.Intent;
import android.content.IntentSender;
import android.os.Bundle;
import android.os.UserManager;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import com.android.internal.widget.LockPatternUtils;
import com.android.car.developeroptions.SetupWizardUtils;
import com.android.car.developeroptions.Utils;

import com.google.android.setupcompat.util.WizardManagerHelper;

public final class ChooseLockSettingsHelper {

    public static final String EXTRA_KEY_TYPE = "type";
    public static final String EXTRA_KEY_PASSWORD = "password";
    public static final String EXTRA_KEY_RETURN_CREDENTIALS = "return_credentials";
    public static final String EXTRA_KEY_HAS_CHALLENGE = "has_challenge";
    public static final String EXTRA_KEY_CHALLENGE = "challenge";
    public static final String EXTRA_KEY_CHALLENGE_TOKEN = "hw_auth_token";
    public static final String EXTRA_KEY_FOR_FINGERPRINT = "for_fingerprint";
    public static final String EXTRA_KEY_FOR_FACE = "for_face";
    public static final String EXTRA_KEY_FOR_CHANGE_CRED_REQUIRED_FOR_BOOT = "for_cred_req_boot";

    /**
     * Intent extra for passing the requested min password complexity to later steps in the set new
     * screen lock flow.
     */
    public static final String EXTRA_KEY_REQUESTED_MIN_COMPLEXITY = "requested_min_complexity";

    /**
     * Intent extra for passing the label of the calling app to later steps in the set new screen
     * lock flow.
     */
    public static final String EXTRA_KEY_CALLER_APP_NAME = "caller_app_name";

    /**
     * When invoked via {@link ConfirmLockPassword.InternalActivity}, this flag
     * controls if we relax the enforcement of
     * {@link Utils#enforceSameOwner(android.content.Context, int)}.
     */
    public static final String EXTRA_ALLOW_ANY_USER = "allow_any_user";

    @VisibleForTesting LockPatternUtils mLockPatternUtils;
    private Activity mActivity;
    private Fragment mFragment;

    public ChooseLockSettingsHelper(Activity activity) {
        mActivity = activity;
        mLockPatternUtils = new LockPatternUtils(activity);
    }

    public ChooseLockSettingsHelper(Activity activity, Fragment fragment) {
        this(activity);
        mFragment = fragment;
    }

    public LockPatternUtils utils() {
        return mLockPatternUtils;
    }

    /**
     * If a pattern, password or PIN exists, prompt the user before allowing them to change it.
     *
     * @param title title of the confirmation screen; shown in the action bar
     * @return true if one exists and we launched an activity to confirm it
     * @see Activity#onActivityResult(int, int, android.content.Intent)
     */
    public boolean launchConfirmationActivity(int request, CharSequence title) {
        return launchConfirmationActivity(
                request /* request */,
                title /* title */,
                null /* header */,
                null /* description */,
                false /* returnCredentials */,
                false /* external */);
    }

    /**
     * If a pattern, password or PIN exists, prompt the user before allowing them to change it.
     *
     * @param title title of the confirmation screen; shown in the action bar
     * @param returnCredentials if true, put credentials into intent. Note that if this is true,
     *                          this can only be called internally.
     * @return true if one exists and we launched an activity to confirm it
     * @see Activity#onActivityResult(int, int, android.content.Intent)
     */
    public boolean launchConfirmationActivity(int request, CharSequence title, boolean returnCredentials) {
        return launchConfirmationActivity(
                request /* request */,
                title /* title */,
                null /* header */,
                null /* description */,
                returnCredentials /* returnCredentials */,
                false /* external */);
    }

    /**
     * If a pattern, password or PIN exists, prompt the user before allowing them to change it.
     *
     * @param title title of the confirmation screen; shown in the action bar
     * @param returnCredentials if true, put credentials into intent. Note that if this is true,
     *                          this can only be called internally.
     * @param userId The userId for whom the lock should be confirmed.
     * @return true if one exists and we launched an activity to confirm it
     * @see Activity#onActivityResult(int, int, android.content.Intent)
     */
    public boolean launchConfirmationActivity(int request, CharSequence title,
            boolean returnCredentials, int userId) {
        return launchConfirmationActivity(
                request /* request */,
                title /* title */,
                null /* header */,
                null /* description */,
                returnCredentials /* returnCredentials */,
                false /* external */,
                false /* hasChallenge */,
                0 /* challenge */,
                Utils.enforceSameOwner(mActivity, userId) /* userId */);
    }

    /**
     * If a pattern, password or PIN exists, prompt the user before allowing them to change it.
     *
     * @param title title of the confirmation screen; shown in the action bar
     * @param header header of the confirmation screen; shown as large text
     * @param description description of the confirmation screen
     * @param returnCredentials if true, put credentials into intent. Note that if this is true,
     *                          this can only be called internally.
     * @param external specifies whether this activity is launched externally, meaning that it will
     *                 get a dark theme, allow fingerprint authentication and it will forward
     *                 activity result.
     * @return true if one exists and we launched an activity to confirm it
     * @see Activity#onActivityResult(int, int, android.content.Intent)
     */
    boolean launchConfirmationActivity(int request, @Nullable CharSequence title,
            @Nullable CharSequence header, @Nullable CharSequence description,
            boolean returnCredentials, boolean external) {
        return launchConfirmationActivity(
                request /* request */,
                title /* title */,
                header /* header */,
                description /* description */,
                returnCredentials /* returnCredentials */,
                external /* external */,
                false /* hasChallenge */,
                0 /* challenge */,
                Utils.getCredentialOwnerUserId(mActivity) /* userId */);
    }

    /**
     * If a pattern, password or PIN exists, prompt the user before allowing them to change it.
     *
     * @param title title of the confirmation screen; shown in the action bar
     * @param header header of the confirmation screen; shown as large text
     * @param description description of the confirmation screen
     * @param returnCredentials if true, put credentials into intent. Note that if this is true,
     *                          this can only be called internally.
     * @param external specifies whether this activity is launched externally, meaning that it will
     *                 get a dark theme, allow fingerprint authentication and it will forward
     *                 activity result.
     * @param userId The userId for whom the lock should be confirmed.
     * @return true if one exists and we launched an activity to confirm it
     * @see Activity#onActivityResult(int, int, android.content.Intent)
     */
    boolean launchConfirmationActivity(int request, @Nullable CharSequence title,
            @Nullable CharSequence header, @Nullable CharSequence description,
            boolean returnCredentials, boolean external, int userId) {
        return launchConfirmationActivity(
                request /* request */,
                title /* title */,
                header /* header */,
                description /* description */,
                returnCredentials /* returnCredentials */,
                external /* external */,
                false /* hasChallenge */,
                0 /* challenge */,
                Utils.enforceSameOwner(mActivity, userId) /* userId */);
    }

    /**
     * If a pattern, password or PIN exists, prompt the user before allowing them to change it.
     *
     * @param title title of the confirmation screen; shown in the action bar
     * @param header header of the confirmation screen; shown as large text
     * @param description description of the confirmation screen
     * @param challenge a challenge to be verified against the device credential.
     * @return true if one exists and we launched an activity to confirm it
     * @see Activity#onActivityResult(int, int, android.content.Intent)
     */
    public boolean launchConfirmationActivity(int request, @Nullable CharSequence title,
            @Nullable CharSequence header, @Nullable CharSequence description,
            long challenge) {
        return launchConfirmationActivity(
                request /* request */,
                title /* title */,
                header /* header */,
                description /* description */,
                true /* returnCredentials */,
                false /* external */,
                true /* hasChallenge */,
                challenge /* challenge */,
                Utils.getCredentialOwnerUserId(mActivity) /* userId */);
    }

    /**
     * If a pattern, password or PIN exists, prompt the user before allowing them to change it.
     *
     * @param title title of the confirmation screen; shown in the action bar
     * @param header header of the confirmation screen; shown as large text
     * @param description description of the confirmation screen
     * @param challenge a challenge to be verified against the device credential.
     * @param userId The userId for whom the lock should be confirmed.
     * @return true if one exists and we launched an activity to confirm it
     * @see Activity#onActivityResult(int, int, android.content.Intent)
     */
    public boolean launchConfirmationActivity(int request, @Nullable CharSequence title,
            @Nullable CharSequence header, @Nullable CharSequence description,
            long challenge, int userId) {
        return launchConfirmationActivity(
                request /* request */,
                title /* title */,
                header /* header */,
                description /* description */,
                true /* returnCredentials */,
                false /* external */,
                true /* hasChallenge */,
                challenge /* challenge */,
                Utils.enforceSameOwner(mActivity, userId) /* userId */);
    }

    /**
     * If a pattern, password or PIN exists, prompt the user before allowing them to change it.
     *
     * @param title title of the confirmation screen; shown in the action bar
     * @param header header of the confirmation screen; shown as large text
     * @param description description of the confirmation screen
     * @param external specifies whether this activity is launched externally, meaning that it will
     *                 get a dark theme, allow fingerprint authentication and it will forward
     *                 activity result.
     * @param challenge a challenge to be verified against the device credential.
     * @param userId The userId for whom the lock should be confirmed.
     * @return true if one exists and we launched an activity to confirm it
     * @see Activity#onActivityResult(int, int, android.content.Intent)
     */
    public boolean launchConfirmationActivityWithExternalAndChallenge(int request,
            @Nullable CharSequence title, @Nullable CharSequence header,
            @Nullable CharSequence description, boolean external, long challenge, int userId) {
        return launchConfirmationActivity(
                request /* request */,
                title /* title */,
                header /* header */,
                description /* description */,
                false /* returnCredentials */,
                external /* external */,
                true /* hasChallenge */,
                challenge /* challenge */,
                Utils.enforceSameOwner(mActivity, userId) /* userId */);
    }

    /**
     * Variant that allows you to prompt for credentials of any user, including
     * those which aren't associated with the current user. As an example, this
     * is useful when unlocking the storage for secondary users.
     */
    public boolean launchConfirmationActivityForAnyUser(int request,
            @Nullable CharSequence title, @Nullable CharSequence header,
            @Nullable CharSequence description, int userId) {
        final Bundle extras = new Bundle();
        extras.putBoolean(EXTRA_ALLOW_ANY_USER, true);
        return launchConfirmationActivity(
                request /* request */,
                title /* title */,
                header /* header */,
                description /* description */,
                false /* returnCredentials */,
                false /* external */,
                true /* hasChallenge */,
                0 /* challenge */,
                userId /* userId */,
                extras /* extras */);
    }

    private boolean launchConfirmationActivity(int request, @Nullable CharSequence title,
            @Nullable CharSequence header, @Nullable CharSequence description,
            boolean returnCredentials, boolean external, boolean hasChallenge,
            long challenge, int userId) {
        return launchConfirmationActivity(
                request /* request */,
                title /* title */,
                header /* header */,
                description /* description */,
                returnCredentials /* returnCredentials */,
                external /* external */,
                hasChallenge /* hasChallenge */,
                challenge /* challenge */,
                userId /* userId */,
                null /* alternateButton */,
                null /* extras */);
    }

    private boolean launchConfirmationActivity(int request, @Nullable CharSequence title,
            @Nullable CharSequence header, @Nullable CharSequence description,
            boolean returnCredentials, boolean external, boolean hasChallenge,
            long challenge, int userId, Bundle extras) {
        return launchConfirmationActivity(
                request /* request */,
                title /* title */,
                header /* header */,
                description /* description */,
                returnCredentials /* returnCredentials */,
                external /* external */,
                hasChallenge /* hasChallenge */,
                challenge /* challenge */,
                userId /* userId */,
                null /* alternateButton */,
                extras /* extras */);
    }

    public boolean launchFrpConfirmationActivity(int request, @Nullable CharSequence header,
            @Nullable CharSequence description, @Nullable CharSequence alternateButton) {
        return launchConfirmationActivity(
                request /* request */,
                null /* title */,
                header /* header */,
                description /* description */,
                false /* returnCredentials */,
                true /* external */,
                false /* hasChallenge */,
                0 /* challenge */,
                LockPatternUtils.USER_FRP /* userId */,
                alternateButton /* alternateButton */,
                null /* extras */);
    }

    private boolean launchConfirmationActivity(int request, @Nullable CharSequence title,
            @Nullable CharSequence header, @Nullable CharSequence description,
            boolean returnCredentials, boolean external, boolean hasChallenge,
            long challenge, int userId, @Nullable CharSequence alternateButton, Bundle extras) {
        final int effectiveUserId = UserManager.get(mActivity).getCredentialOwnerProfile(userId);
        boolean launched = false;

        switch (mLockPatternUtils.getKeyguardStoredPasswordQuality(effectiveUserId)) {
            case DevicePolicyManager.PASSWORD_QUALITY_SOMETHING:
                launched = launchConfirmationActivity(request, title, header, description,
                        returnCredentials || hasChallenge
                                ? ConfirmLockPattern.InternalActivity.class
                                : ConfirmLockPattern.class, returnCredentials, external,
                                hasChallenge, challenge, userId, alternateButton, extras);
                break;
            case DevicePolicyManager.PASSWORD_QUALITY_NUMERIC:
            case DevicePolicyManager.PASSWORD_QUALITY_NUMERIC_COMPLEX:
            case DevicePolicyManager.PASSWORD_QUALITY_ALPHABETIC:
            case DevicePolicyManager.PASSWORD_QUALITY_ALPHANUMERIC:
            case DevicePolicyManager.PASSWORD_QUALITY_COMPLEX:
            case DevicePolicyManager.PASSWORD_QUALITY_MANAGED:
                launched = launchConfirmationActivity(request, title, header, description,
                        returnCredentials || hasChallenge
                                ? ConfirmLockPassword.InternalActivity.class
                                : ConfirmLockPassword.class, returnCredentials, external,
                                hasChallenge, challenge, userId, alternateButton, extras);
                break;
        }
        return launched;
    }

    private boolean launchConfirmationActivity(int request, CharSequence title, CharSequence header,
            CharSequence message, Class<?> activityClass, boolean returnCredentials,
            boolean external, boolean hasChallenge, long challenge,
            int userId, @Nullable CharSequence alternateButton, Bundle extras) {
        final Intent intent = new Intent();
        intent.putExtra(ConfirmDeviceCredentialBaseFragment.TITLE_TEXT, title);
        intent.putExtra(ConfirmDeviceCredentialBaseFragment.HEADER_TEXT, header);
        intent.putExtra(ConfirmDeviceCredentialBaseFragment.DETAILS_TEXT, message);
        // TODO: Remove dark theme and show_cancel_button options since they are no longer used
        intent.putExtra(ConfirmDeviceCredentialBaseFragment.DARK_THEME, false);
        intent.putExtra(ConfirmDeviceCredentialBaseFragment.SHOW_CANCEL_BUTTON, false);
        intent.putExtra(ConfirmDeviceCredentialBaseFragment.SHOW_WHEN_LOCKED, external);
        intent.putExtra(ConfirmDeviceCredentialBaseFragment.USE_FADE_ANIMATION, external);
        intent.putExtra(ChooseLockSettingsHelper.EXTRA_KEY_RETURN_CREDENTIALS, returnCredentials);
        intent.putExtra(ChooseLockSettingsHelper.EXTRA_KEY_HAS_CHALLENGE, hasChallenge);
        intent.putExtra(ChooseLockSettingsHelper.EXTRA_KEY_CHALLENGE, challenge);
        intent.putExtra(Intent.EXTRA_USER_ID, userId);
        intent.putExtra(KeyguardManager.EXTRA_ALTERNATE_BUTTON_LABEL, alternateButton);
        if (extras != null) {
            intent.putExtras(extras);
        }
        intent.setClassName(SETTINGS_PACKAGE_NAME, activityClass.getName());
        if (external) {
            intent.addFlags(Intent.FLAG_ACTIVITY_FORWARD_RESULT);
            if (mFragment != null) {
                copyOptionalExtras(mFragment.getActivity().getIntent(), intent);
                mFragment.startActivity(intent);
            } else {
                copyOptionalExtras(mActivity.getIntent(), intent);
                mActivity.startActivity(intent);
            }
        } else {
            if (mFragment != null) {
                copyInternalExtras(mFragment.getActivity().getIntent(), intent);
                mFragment.startActivityForResult(intent, request);
            } else {
                copyInternalExtras(mActivity.getIntent(), intent);
                mActivity.startActivityForResult(intent, request);
            }
        }
        return true;
    }

    private void copyOptionalExtras(Intent inIntent, Intent outIntent) {
        IntentSender intentSender = inIntent.getParcelableExtra(Intent.EXTRA_INTENT);
        if (intentSender != null) {
            outIntent.putExtra(Intent.EXTRA_INTENT, intentSender);
        }
        int taskId = inIntent.getIntExtra(Intent.EXTRA_TASK_ID, -1);
        if (taskId != -1) {
            outIntent.putExtra(Intent.EXTRA_TASK_ID, taskId);
        }
        // If we will launch another activity once credentials are confirmed, exclude from recents.
        // This is a workaround to a framework bug where affinity is incorrect for activities
        // that are started from a no display activity, as is ConfirmDeviceCredentialActivity.
        // TODO: Remove once that bug is fixed.
        if (intentSender != null || taskId != -1) {
            outIntent.addFlags(Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);
            outIntent.addFlags(Intent.FLAG_ACTIVITY_NO_HISTORY);
        }
    }

    private void copyInternalExtras(Intent inIntent, Intent outIntent) {
        SetupWizardUtils.copySetupExtras(inIntent, outIntent);
        String theme = inIntent.getStringExtra(WizardManagerHelper.EXTRA_THEME);
        if (theme != null) {
            outIntent.putExtra(WizardManagerHelper.EXTRA_THEME, theme);
        }
    }
}
