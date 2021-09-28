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

package com.android.car.companiondevicesupport.feature.trust.ui;

import static com.android.car.companiondevicesupport.activity.AssociationActivity.ACTION_ASSOCIATION_SETTING;
import static com.android.car.companiondevicesupport.activity.AssociationActivity.ASSOCIATED_DEVICE_DATA_NAME_EXTRA;
import static com.android.car.connecteddevice.util.SafeLog.logd;
import static com.android.car.connecteddevice.util.SafeLog.loge;
import static com.android.car.connecteddevice.util.SafeLog.logw;
import static com.android.car.ui.core.CarUi.requireToolbar;
import static com.android.car.ui.toolbar.Toolbar.State.SUBPAGE;

import android.annotation.Nullable;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.KeyguardManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.UserHandle;
import android.text.Html;
import android.text.Spanned;
import android.widget.Toast;

import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentActivity;
import androidx.lifecycle.ViewModelProviders;

import com.android.car.companiondevicesupport.R;
import com.android.car.companiondevicesupport.api.external.AssociatedDevice;
import com.android.car.companiondevicesupport.api.external.CompanionDevice;
import com.android.car.companiondevicesupport.api.external.IDeviceAssociationCallback;
import com.android.car.companiondevicesupport.api.internal.trust.ITrustedDeviceEnrollmentCallback;
import com.android.car.companiondevicesupport.api.internal.trust.ITrustedDeviceCallback;
import com.android.car.companiondevicesupport.api.internal.trust.ITrustedDeviceManager;
import com.android.car.companiondevicesupport.api.internal.trust.TrustedDevice;
import com.android.car.companiondevicesupport.feature.trust.TrustedDeviceConstants;
import com.android.car.companiondevicesupport.feature.trust.TrustedDeviceManagerService;
import com.android.car.ui.toolbar.ToolbarController;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;


/** Activity for enrolling and viewing trusted devices. */
public class TrustedDeviceActivity extends FragmentActivity {

    private static final String TAG = "TrustedDeviceActivity";

    private static final int ACTIVATE_TOKEN_REQUEST_CODE = 1;

    private static final int CREATE_LOCK_REQUEST_CODE = 2;

    private static final int RETRIEVE_ASSOCIATED_DEVICE_REQUEST_CODE = 3;

    private static final String ACTION_LOCK_SETTINGS = "android.car.settings.SCREEN_LOCK_ACTIVITY";

    private static final String DEVICE_DETAIL_FRAGMENT_TAG = "TrustedDeviceDetailFragmentTag";

    private static final String DEVICE_NOT_CONNECTED_DIALOG_TAG =
            "DeviceNotConnectedDialogFragmentTag";

    private static final String CREATE_PROFILE_LOCK_DIALOG_TAG =
            "CreateProfileLockDialogFragmentTag";

    private static final String UNLOCK_PROFILE_TO_FINISH_DIALOG_TAG =
            "UnlockProfileToFinishDialogFragmentTag";

    private static final String CREATE_PHONE_LOCK_DIALOG_TAG = "CreatePhoneLockDialogFragmentTag";

    private static final String ENROLLMENT_ERROR_DIALOG_TAG = "EnrollmentErrorDialogFragmentTag";

    /** {@code true} if a PIN/Pattern/Password has just been set as a screen lock. */
    private final AtomicBoolean mIsScreenLockNewlyCreated = new AtomicBoolean(false);

    private final AtomicBoolean mIsStartedForEnrollment = new AtomicBoolean(false);

    private final AtomicBoolean mHasPendingCredential = new AtomicBoolean(false);

    /**
     * {@code true} if this activity is relaunched for enrollment and the activity needs to be
     * finished after enrollment has completed.
     */
    private final AtomicBoolean mWasRelaunched = new AtomicBoolean(false);

    private KeyguardManager mKeyguardManager;

    private ITrustedDeviceManager mTrustedDeviceManager;

    private ToolbarController mToolbar;

    private TrustedDeviceViewModel mModel;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.base_activity);
        observeViewModel();
        resumePreviousState(savedInstanceState);
        mToolbar = requireToolbar(this);
        mToolbar.setState(SUBPAGE);
        mToolbar.setTitle(R.string.trusted_device_feature_title);
        mToolbar.getProgressBar().setVisible(true);
        mIsScreenLockNewlyCreated.set(false);
        mIsStartedForEnrollment.set(false);
        mHasPendingCredential.set(false);
        mWasRelaunched.set(false);
        Intent intent = new Intent(this, TrustedDeviceManagerService.class);
        bindServiceAsUser(intent, mServiceConnection, Context.BIND_AUTO_CREATE, UserHandle.SYSTEM);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        switch (requestCode) {
            case ACTIVATE_TOKEN_REQUEST_CODE:
                if (resultCode != RESULT_OK) {
                    loge(TAG, "Lock screen was unsuccessful. Returned result code: " +
                            resultCode + ".");
                    finishEnrollment();
                    return;
                }
                logd(TAG, "Credentials accepted. Waiting for TrustAgent to activate " +
                        "token.");
                break;
            case CREATE_LOCK_REQUEST_CODE:
                if (!isDeviceSecure()) {
                    loge(TAG, "Set up new lock unsuccessful. Returned result code: "
                            + resultCode + ".");
                    mIsScreenLockNewlyCreated.set(false);
                    return;
                }

                if (mHasPendingCredential.get()) {
                    showUnlockProfileDialogFragment();
                }
                break;
            case RETRIEVE_ASSOCIATED_DEVICE_REQUEST_CODE:
                AssociatedDevice device = data.getParcelableExtra(ASSOCIATED_DEVICE_DATA_NAME_EXTRA);
                if (device == null) {
                    loge(TAG, "No valid associated device.");
                    return;
                }
                mModel.setAssociatedDevice(device);
                Intent incomingIntent = getIntent();
                if (isStartedForEnrollment(incomingIntent)) {
                    processEnrollment();
                    return;
                }
                showTrustedDeviceDetailFragment(device);
                break;
            default:
                logw(TAG, "Unrecognized activity result. Request code: " + requestCode
                        + ". Ignoring.");
                break;
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        mWasRelaunched.set(true);
        if (isStartedForEnrollment(intent)) {
            processEnrollment();
        }
    }

    @Override
    protected void onDestroy() {
        try {
            unregisterCallbacks();
        } catch (RemoteException e) {
            loge(TAG, "Error while disconnecting from service.", e);
        }
        unbindService(mServiceConnection);
        super.onDestroy();
    }

    private void resumePreviousState(Bundle saveInstanceState) {
        if (saveInstanceState == null) {
            return;
        }
        CreateProfileLockDialogFragment createProfileLockDialogFragment =
                (CreateProfileLockDialogFragment) getSupportFragmentManager()
                .findFragmentByTag(CREATE_PROFILE_LOCK_DIALOG_TAG);
        if (createProfileLockDialogFragment != null) {
            createProfileLockDialogFragment.setOnConfirmListener((d, w) -> createScreenLock());
        }

        UnlockProfileDialogFragment unlockProfileDialogFragment =
                (UnlockProfileDialogFragment) getSupportFragmentManager()
                .findFragmentByTag(UNLOCK_PROFILE_TO_FINISH_DIALOG_TAG);
        if (unlockProfileDialogFragment != null) {
            unlockProfileDialogFragment.setOnConfirmListener((d, w) -> validateCredentials());
        }
    }

    private void observeViewModel() {
        mModel = ViewModelProviders.of(this).get(TrustedDeviceViewModel.class);
        mModel.getDeviceToDisable().observe(this, trustedDevice -> {
            if (trustedDevice == null) {
                return;
            }
            mModel.setDeviceToDisable(null);
            if (mTrustedDeviceManager == null) {
                loge(TAG, "Failed to remove trusted device. service not connected.");
                return;
            }
            try {
                logd(TAG, "calling removeTrustedDevice");
                mTrustedDeviceManager.removeTrustedDevice(trustedDevice);
            } catch (RemoteException e) {
                loge(TAG, "Failed to remove trusted device.", e);
            }
        });
        mModel.getDeviceToEnable().observe(this, associatedDevice -> {
            if (associatedDevice == null) {
                return;
            }
            mModel.setDeviceToEnable(null);
            attemptInitiatingEnrollment(associatedDevice);
        });
    }

    private boolean hasAssociatedDevice() {
        Intent intent = getIntent();
        String action = intent.getAction();
        if (!TrustedDeviceConstants.INTENT_ACTION_TRUSTED_DEVICE_SETTING.equals(action)) {
            return false;
        }
        AssociatedDevice device = intent.getParcelableExtra(ASSOCIATED_DEVICE_DATA_NAME_EXTRA);
        if (device == null) {
            loge(TAG, "No valid associated device.");
            return false;
        }
        mModel.setAssociatedDevice(device);
        showTrustedDeviceDetailFragment(device);
        return true;
    }

    private void attemptInitiatingEnrollment(AssociatedDevice device) {
        if (!isCompanionDeviceConnected(device.getDeviceId())) {
            DeviceNotConnectedDialogFragment fragment = new DeviceNotConnectedDialogFragment();
            fragment.show(getSupportFragmentManager(), DEVICE_NOT_CONNECTED_DIALOG_TAG);
            return;
        }
        try {
            mTrustedDeviceManager.initiateEnrollment(device.getDeviceId());
        } catch (RemoteException e) {
            loge(TAG, "Failed to initiate enrollment. ", e);
        }
    }

    private boolean isCompanionDeviceConnected(String deviceId) {
        if (mTrustedDeviceManager == null) {
            loge(TAG, "Failed to check connection status for device: " + deviceId +
                    "Service not connected.");
            return false;
        }
        List<CompanionDevice> devices = null;
        try {
            devices = mTrustedDeviceManager.getActiveUserConnectedDevices();
        } catch (RemoteException e) {
            loge(TAG, "Failed to check connection status for device: " + deviceId, e);
            return false;
        }
        if (devices == null || devices.isEmpty()) {
            return false;
        }
        for (CompanionDevice device: devices) {
            if (device.getDeviceId().equals(deviceId)) {
                return true;
            }
        }
        return false;
    }

    private void validateCredentials() {
        logd(TAG, "Validating credentials to activate token.");
        KeyguardManager keyguardManager = getKeyguardManager();
        if (keyguardManager == null) {
            return;
        }
        if (!mIsStartedForEnrollment.get()) {
            mHasPendingCredential.set(true);
            return;
        }
        if (mIsScreenLockNewlyCreated.get()) {
            showUnlockProfileDialogFragment();
            return;
        }
        @SuppressWarnings("deprecation") // Car does not support Biometric lock as of now.
        Intent confirmIntent = keyguardManager.createConfirmDeviceCredentialIntent(
                "PLACEHOLDER PROMPT TITLE", "PLACEHOLDER PROMPT MESSAGE");
        if (confirmIntent == null) {
            loge(TAG, "User either has no lock screen, or a token is already registered.");
            return;
        }
        mHasPendingCredential.set(false);
        mIsStartedForEnrollment.set(false);
        logd(TAG, "Prompting user to validate credentials.");
        startActivityForResult(confirmIntent, ACTIVATE_TOKEN_REQUEST_CODE);
    }

    private void processEnrollment() {
        mIsStartedForEnrollment.set(true);
        if (mHasPendingCredential.get()) {
            validateCredentials();
            return;
        }
        maybePromptToCreatePassword();
    }

    private boolean isStartedForEnrollment(Intent intent) {
        return intent != null && intent.getBooleanExtra(
                TrustedDeviceConstants.INTENT_EXTRA_ENROLL_NEW_TOKEN, false);
    }

    private void finishEnrollment() {
        if (!mWasRelaunched.get()) {
            // If the activity is not relaunched for enrollment, it needs to be finished to make the
            // foreground return to the previous screen.
            finish();
        }
    }

    private void maybePromptToCreatePassword() {
        if (isDeviceSecure()) {
            return;
        }

        CreateProfileLockDialogFragment fragment = CreateProfileLockDialogFragment.newInstance(
                (d, w) -> createScreenLock());
        fragment.show(getSupportFragmentManager(), CREATE_PROFILE_LOCK_DIALOG_TAG);
    }

    private void createScreenLock() {
        if (isDeviceSecure()) {
            return;
        }
        logd(TAG, "User has not set a lock screen. Redirecting to set up.");
        Intent intent = new Intent(ACTION_LOCK_SETTINGS);
        mIsScreenLockNewlyCreated.set(true);
        startActivityForResult(intent, CREATE_LOCK_REQUEST_CODE);
    }

    private boolean isDeviceSecure() {
        KeyguardManager keyguardManager = getKeyguardManager();
        if (keyguardManager == null) {
            return false;
        }
        return keyguardManager.isDeviceSecure();
    }

    private void retrieveAssociatedDevice() {
        Intent intent = new Intent(ACTION_ASSOCIATION_SETTING);
        startActivityForResult(intent, RETRIEVE_ASSOCIATED_DEVICE_REQUEST_CODE);
    }

    private void showTrustedDeviceDetailFragment(AssociatedDevice device) {
        mToolbar.getProgressBar().setVisible(false);
        TrustedDeviceDetailFragment fragment = TrustedDeviceDetailFragment.newInstance(device);
        getSupportFragmentManager().beginTransaction()
                .replace(R.id.fragment_container, fragment, DEVICE_DETAIL_FRAGMENT_TAG)
                .commit();
    }

    private void showUnlockProfileDialogFragment() {
        mIsScreenLockNewlyCreated.set(false);
        UnlockProfileDialogFragment fragment = UnlockProfileDialogFragment.newInstance((d, w) ->
            validateCredentials());
        fragment.show(getSupportFragmentManager(), UNLOCK_PROFILE_TO_FINISH_DIALOG_TAG);
    }

    private void showEnrollmentSuccessToast(TrustedDevice device) {
        AssociatedDevice addedDevice = mModel.getAssociatedDevice().getValue();
        if (addedDevice == null) {
            loge(TAG, "No associated device retrieved when a trusted device has been added.");
            return;
        }
        if (!addedDevice.getDeviceId().equals(device.getDeviceId())) {
            loge(TAG, "Id of the enrolled trusted device doesn't match id of the current device");
            return;
        }
        String message = getString(R.string.trusted_device_enrollment_success_message,
                addedDevice.getDeviceName());
        Spanned styledMessage = Html.fromHtml(message, Html.FROM_HTML_MODE_LEGACY);
        runOnUiThread(() ->
                Toast.makeText(getApplicationContext(), styledMessage, Toast.LENGTH_SHORT).show());
    }

    private void showEnrollmentErrorDialogFragment(int error) {
        switch (error) {
            case TrustedDeviceConstants.TRUSTED_DEVICE_ERROR_DEVICE_NOT_SECURED:
                CreatePhoneLockDialogFragment createPhoneLockDialogFragment =
                        new CreatePhoneLockDialogFragment();
                createPhoneLockDialogFragment.show(getSupportFragmentManager(),
                        CREATE_PHONE_LOCK_DIALOG_TAG);
                break;
            case TrustedDeviceConstants.TRUSTED_DEVICE_ERROR_MESSAGE_TYPE_UNKNOWN:
            case TrustedDeviceConstants.TRUSTED_DEVICE_ERROR_UNKNOWN:
                EnrollmentErrorDialogFragment enrollmentErrorDialogFragment =
                        new EnrollmentErrorDialogFragment();
                enrollmentErrorDialogFragment.show(getSupportFragmentManager(),
                        ENROLLMENT_ERROR_DIALOG_TAG);
                break;
            default:
                loge(TAG, "Encountered unexpected error: " + error + ".");
        }
    }

    private void registerCallbacks() throws RemoteException {
        if (mTrustedDeviceManager == null) {
            loge(TAG, "Server not connected when attempting to register callbacks.");
            return;
        }
        mTrustedDeviceManager.registerTrustedDeviceEnrollmentCallback(
                mTrustedDeviceEnrollmentCallback);
        mTrustedDeviceManager.registerTrustedDeviceCallback(mTrustedDeviceCallback);
        mTrustedDeviceManager.registerAssociatedDeviceCallback(mDeviceAssociationCallback);
    }

    private void unregisterCallbacks() throws RemoteException {
        if (mTrustedDeviceManager == null) {
            loge(TAG, "Server not connected when attempting to unregister callbacks.");
            return;
        }
        mTrustedDeviceManager.unregisterTrustedDeviceEnrollmentCallback(
                mTrustedDeviceEnrollmentCallback);
        mTrustedDeviceManager.unregisterTrustedDeviceCallback(mTrustedDeviceCallback);
        mTrustedDeviceManager.unregisterAssociatedDeviceCallback(mDeviceAssociationCallback);
    }

    private final ServiceConnection mServiceConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            mTrustedDeviceManager = ITrustedDeviceManager.Stub.asInterface(service);
            try {
                registerCallbacks();
                mModel.setTrustedDevices(mTrustedDeviceManager.getTrustedDevicesForActiveUser());
            } catch (RemoteException e) {
                loge(TAG, "Error while connecting to service.");
            }

            logd(TAG, "Successfully connected to TrustedDeviceManager.");

            if (!hasAssociatedDevice()) {
                retrieveAssociatedDevice();
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
        }
    };

    private final ITrustedDeviceCallback mTrustedDeviceCallback =
            new ITrustedDeviceCallback.Stub() {
        @Override
        public void onTrustedDeviceAdded(TrustedDevice device) {
            logd(TAG, "Added trusted device: " + device + ".");
            mModel.setEnabledDevice(device);
            showEnrollmentSuccessToast(device);
            finishEnrollment();
        }

        @Override
        public void onTrustedDeviceRemoved(TrustedDevice device) {
            logd(TAG, "Removed trusted device: " + device +".");
            mModel.setDisabledDevice(device);
        }
    };

    private final IDeviceAssociationCallback mDeviceAssociationCallback =
            new IDeviceAssociationCallback.Stub() {
        @Override
        public void onAssociatedDeviceAdded(AssociatedDevice device) { }

        @Override
        public void onAssociatedDeviceRemoved(AssociatedDevice device) {
            AssociatedDevice currentDevice = mModel.getAssociatedDevice().getValue();
            if (device.equals(currentDevice)) {
                finish();
            }
        }

        @Override
        public void onAssociatedDeviceUpdated(AssociatedDevice device) {
            if (device != null) {
                mModel.setAssociatedDevice(device);
            }
        }
    };

    @Nullable
    private KeyguardManager getKeyguardManager() {
        if (mKeyguardManager == null) {
            mKeyguardManager = (KeyguardManager) getSystemService(Context.KEYGUARD_SERVICE);
        }
        if (mKeyguardManager == null) {
            loge(TAG, "Unable to get KeyguardManager.");
        }
        return mKeyguardManager;
    }

    private ITrustedDeviceEnrollmentCallback mTrustedDeviceEnrollmentCallback =
            new ITrustedDeviceEnrollmentCallback.Stub() {

        @Override
        public void onValidateCredentialsRequest() {
            validateCredentials();
        }

        @Override
        public void onTrustedDeviceEnrollmentError(int error) {
            loge(TAG, "Failed to enroll trusted device, encountered error: " + error + ".");
            showEnrollmentErrorDialogFragment(error);
        }
    };

    /** Dialog Fragment to notify that the device is not actively connected. */
    public static class DeviceNotConnectedDialogFragment extends DialogFragment {
        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            return new AlertDialog.Builder(getActivity())
                    .setTitle(getString(R.string.device_not_connected_dialog_title))
                    .setMessage(getString(R.string.device_not_connected_dialog_message))
                    .setNegativeButton(getString(R.string.ok), null)
                    .setCancelable(true)
                    .create();
        }
    }

    /** Dialog Fragment to notify that a profile lock is needed to continue enrollment. */
    public static class CreateProfileLockDialogFragment extends DialogFragment {
        private DialogInterface.OnClickListener mOnConfirmListener;

        static CreateProfileLockDialogFragment newInstance(
                DialogInterface.OnClickListener listener) {
            CreateProfileLockDialogFragment fragment = new CreateProfileLockDialogFragment();
            fragment.setOnConfirmListener(listener);
            return fragment;
        }

        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            return new AlertDialog.Builder(getActivity())
                    .setTitle(getString(R.string.create_profile_lock_dialog_title))
                    .setMessage(getString(R.string.create_profile_lock_dialog_message))
                    .setNegativeButton(getString(R.string.cancel), null)
                    .setPositiveButton(getString(R.string.continue_button), mOnConfirmListener)
                    .setCancelable(true)
                    .create();
        }

        void setOnConfirmListener(DialogInterface.OnClickListener onConfirmListener) {
            mOnConfirmListener = onConfirmListener;
        }
    }

    /** Dialog Fragment to notify that the user needs to unlock again to finish enrollment. */
    public static class UnlockProfileDialogFragment extends DialogFragment {
        private DialogInterface.OnClickListener mOnConfirmListener;

        static UnlockProfileDialogFragment newInstance(DialogInterface.OnClickListener listener) {
            UnlockProfileDialogFragment fragment = new UnlockProfileDialogFragment();
            fragment.setOnConfirmListener(listener);
            return fragment;
        }

        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            return new AlertDialog.Builder(getActivity())
                    .setTitle(getString(R.string.unlock_profile_to_finish_title))
                    .setMessage(getString(R.string.unlock_profile_to_finish_message))
                    .setNegativeButton(getString(R.string.cancel), null)
                    .setPositiveButton(getString(R.string.continue_button), mOnConfirmListener)
                    .setCancelable(true)
                    .create();
        }

        void setOnConfirmListener(DialogInterface.OnClickListener onConfirmListener) {
            mOnConfirmListener = onConfirmListener;
        }
    }

    /** Dialog Fragment to notify that the user needs to set up phone unlock before enrollment.*/
    public static class CreatePhoneLockDialogFragment extends DialogFragment {
        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            return new AlertDialog.Builder(getActivity())
                    .setTitle(getString(R.string.create_phone_lock_dialog_title))
                    .setMessage(getString(R.string.create_phone_lock_dialog_message))
                    .setPositiveButton(getString(R.string.ok), null)
                    .setCancelable(true)
                    .create();
        }
    }

    /** Dialog Fragment to notify error during enrollment.*/
    public static class EnrollmentErrorDialogFragment extends DialogFragment {
        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            return new AlertDialog.Builder(getActivity())
                    .setTitle(getString(R.string.trusted_device_enrollment_error_dialog_title))
                    .setMessage(getString(R.string.trusted_device_enrollment_error_dialog_message))
                    .setPositiveButton(getString(R.string.ok), null)
                    .setCancelable(true)
                    .create();
        }
    }
}
