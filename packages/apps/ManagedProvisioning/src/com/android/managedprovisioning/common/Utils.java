/*
 * Copyright 2014, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.managedprovisioning.common;

import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_FINANCED_DEVICE;
import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_DEVICE;
import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_DEVICE_FROM_TRUSTED_SOURCE;
import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_PROFILE;
import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_SHAREABLE_DEVICE;
import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_USER;
import static android.app.admin.DevicePolicyManager.MIME_TYPE_PROVISIONING_NFC;
import static android.app.admin.DevicePolicyManager.PROVISIONING_TRIGGER_CLOUD_ENROLLMENT;
import static android.app.admin.DevicePolicyManager.PROVISIONING_TRIGGER_QR_CODE;
import static android.app.admin.DevicePolicyManager.PROVISIONING_TRIGGER_UNSPECIFIED;
import static android.content.pm.PackageManager.MATCH_HIDDEN_UNTIL_INSTALLED_COMPONENTS;
import static android.content.pm.PackageManager.MATCH_UNINSTALLED_PACKAGES;
import static android.net.NetworkCapabilities.NET_CAPABILITY_INTERNET;
import static android.net.NetworkCapabilities.NET_CAPABILITY_VALIDATED;
import static android.net.NetworkCapabilities.TRANSPORT_CELLULAR;
import static android.nfc.NfcAdapter.ACTION_NDEF_DISCOVERED;

import static com.android.managedprovisioning.common.Globals.ACTION_PROVISION_MANAGED_DEVICE_SILENTLY;
import static com.android.managedprovisioning.model.ProvisioningParams.PROVISIONING_MODE_FULLY_MANAGED_DEVICE;
import static com.android.managedprovisioning.model.ProvisioningParams.PROVISIONING_MODE_MANAGED_PROFILE;
import static com.android.managedprovisioning.model.ProvisioningParams.PROVISIONING_MODE_MANAGED_PROFILE_ON_FULLY_NAMAGED_DEVICE;

import android.annotation.WorkerThread;
import android.net.NetworkCapabilities;
import android.os.Handler;
import android.os.Looper;
import com.android.managedprovisioning.R;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.StringRes;
import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.IPackageManager;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.content.pm.UserInfo;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.SystemProperties;
import android.os.UserHandle;
import android.os.UserManager;
import android.os.storage.StorageManager;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.View.OnClickListener;
import android.widget.TextView;

import com.android.internal.annotations.VisibleForTesting;
import com.android.managedprovisioning.TrampolineActivity;
import com.android.managedprovisioning.model.CustomizationParams;
import com.android.managedprovisioning.model.PackageDownloadInfo;
import com.android.managedprovisioning.model.ProvisioningParams;
import com.android.managedprovisioning.preprovisioning.WebActivity;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;

import com.google.android.setupdesign.GlifLayout;
import com.google.android.setupcompat.template.FooterBarMixin;
import com.google.android.setupcompat.template.FooterButton;
import com.google.android.setupcompat.template.FooterButton.ButtonType;

/**
 * Class containing various auxiliary methods.
 */
public class Utils {
    public static final String SHA256_TYPE = "SHA-256";

    // value chosen to match UX designs; when updating check status bar icon colors
    private static final int THRESHOLD_BRIGHT_COLOR = 190;

    public Utils() {}

    /**
     * Returns the system apps currently available to a given user.
     *
     * <p>Calls the {@link IPackageManager} to retrieve all system apps available to a user and
     * returns their package names.
     *
     * @param ipm an {@link IPackageManager} object
     * @param userId the id of the user to check the apps for
     */
    public Set<String> getCurrentSystemApps(IPackageManager ipm, int userId) {
        Set<String> apps = new HashSet<>();
        List<ApplicationInfo> aInfos = null;
        try {
            aInfos = ipm.getInstalledApplications(
                    MATCH_UNINSTALLED_PACKAGES | MATCH_HIDDEN_UNTIL_INSTALLED_COMPONENTS, userId)
                    .getList();
        } catch (RemoteException neverThrown) {
            ProvisionLogger.loge("This should not happen.", neverThrown);
        }
        for (ApplicationInfo aInfo : aInfos) {
            if ((aInfo.flags & ApplicationInfo.FLAG_SYSTEM) != 0) {
                apps.add(aInfo.packageName);
            }
        }
        return apps;
    }

    /**
     * Disables a given component in a given user.
     *
     * @param toDisable the component that should be disabled
     * @param userId the id of the user where the component should be disabled.
     */
    public void disableComponent(ComponentName toDisable, int userId) {
        setComponentEnabledSetting(
                IPackageManager.Stub.asInterface(ServiceManager.getService("package")),
                toDisable,
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
                userId);
    }

    /**
     * Enables a given component in a given user.
     *
     * @param toEnable the component that should be enabled
     * @param userId the id of the user where the component should be disabled.
     */
    public void enableComponent(ComponentName toEnable, int userId) {
        setComponentEnabledSetting(
                IPackageManager.Stub.asInterface(ServiceManager.getService("package")),
                toEnable,
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
                userId);
    }

    /**
     * Disables a given component in a given user.
     *
     * @param ipm an {@link IPackageManager} object
     * @param toDisable the component that should be disabled
     * @param userId the id of the user where the component should be disabled.
     */
    @VisibleForTesting
    void setComponentEnabledSetting(IPackageManager ipm, ComponentName toDisable,
            int enabledSetting, int userId) {
        try {
            ipm.setComponentEnabledSetting(toDisable,
                    enabledSetting, PackageManager.DONT_KILL_APP,
                    userId);
        } catch (RemoteException neverThrown) {
            ProvisionLogger.loge("This should not happen.", neverThrown);
        } catch (Exception e) {
            ProvisionLogger.logw("Component not found, not changing enabled setting: "
                + toDisable.toShortString());
        }
    }

    /**
     * Check the validity of the admin component name supplied, or try to infer this componentName
     * from the package.
     *
     * We are supporting lookup by package name for legacy reasons.
     *
     * If dpcComponentName is supplied (not null): dpcPackageName is ignored.
     * Check that the package of dpcComponentName is installed, that dpcComponentName is a
     * receiver in this package, and return it. The receiver can be in disabled state.
     *
     * Otherwise: dpcPackageName must be supplied (not null).
     * Check that this package is installed, try to infer a potential device admin in this package,
     * and return it.
     */
    @NonNull
    public ComponentName findDeviceAdmin(String dpcPackageName, ComponentName dpcComponentName,
            Context context, int userId) throws IllegalProvisioningArgumentException {
        if (dpcComponentName != null) {
            dpcPackageName = dpcComponentName.getPackageName();
        }
        if (dpcPackageName == null) {
            throw new IllegalProvisioningArgumentException("Neither the package name nor the"
                    + " component name of the admin are supplied");
        }
        PackageInfo pi;
        try {
            pi = context.getPackageManager().getPackageInfoAsUser(dpcPackageName,
                    PackageManager.GET_RECEIVERS | PackageManager.MATCH_DISABLED_COMPONENTS,
                    userId);
        } catch (NameNotFoundException e) {
            throw new IllegalProvisioningArgumentException("Dpc " + dpcPackageName
                    + " is not installed. ", e);
        }

        final ComponentName componentName = findDeviceAdminInPackageInfo(dpcPackageName,
                dpcComponentName, pi);
        if (componentName == null) {
            throw new IllegalProvisioningArgumentException("Cannot find any admin receiver in "
                    + "package " + dpcPackageName + " with component " + dpcComponentName);
        }
        return componentName;
    }

    /**
     * If dpcComponentName is not null: dpcPackageName is ignored.
     * Check that the package of dpcComponentName is installed, that dpcComponentName is a
     * receiver in this package, and return it. The receiver can be in disabled state.
     *
     * Otherwise, try to infer a potential device admin component in this package info.
     *
     * @return infered device admin component in package info. Otherwise, null
     */
    @Nullable
    public ComponentName findDeviceAdminInPackageInfo(@NonNull String dpcPackageName,
            @Nullable ComponentName dpcComponentName, @NonNull PackageInfo pi) {
        if (dpcComponentName != null) {
            if (!isComponentInPackageInfo(dpcComponentName, pi)) {
                ProvisionLogger.logw("The component " + dpcComponentName + " isn't registered in "
                        + "the apk");
                return null;
            }
            return dpcComponentName;
        } else {
            return findDeviceAdminInPackage(dpcPackageName, pi);
        }
    }

    /**
     * Finds a device admin in a given {@link PackageInfo} object.
     *
     * <p>This function returns {@code null} if no or multiple admin receivers were found, and if
     * the package name does not match dpcPackageName.</p>
     * @param packageName packge name that should match the {@link PackageInfo} object.
     * @param packageInfo package info to be examined.
     * @return admin receiver or null in case of error.
     */
    @Nullable
    private ComponentName findDeviceAdminInPackage(String packageName, PackageInfo packageInfo) {
        if (packageInfo == null || !TextUtils.equals(packageInfo.packageName, packageName)) {
            return null;
        }

        ComponentName mdmComponentName = null;
        for (ActivityInfo ai : packageInfo.receivers) {
            if (TextUtils.equals(ai.permission, android.Manifest.permission.BIND_DEVICE_ADMIN)) {
                if (mdmComponentName != null) {
                    ProvisionLogger.logw("more than 1 device admin component are found");
                    return null;
                } else {
                    mdmComponentName = new ComponentName(packageName, ai.name);
                }
            }
        }
        return mdmComponentName;
    }

    private boolean isComponentInPackageInfo(ComponentName dpcComponentName,
            PackageInfo pi) {
        for (ActivityInfo ai : pi.receivers) {
            if (dpcComponentName.getClassName().equals(ai.name)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Return if a given package has testOnly="true", in which case we'll relax certain rules
     * for CTS.
     *
     * The system allows this flag to be changed when an app is updated. But
     * {@link DevicePolicyManager} uses the persisted version to do actual checks for relevant
     * dpm command.
     *
     * @see DevicePolicyManagerService#isPackageTestOnly for more info
     */
    public static boolean isPackageTestOnly(PackageManager pm, String packageName, int userHandle) {
        if (TextUtils.isEmpty(packageName)) {
            return false;
        }

        try {
            final ApplicationInfo ai = pm.getApplicationInfoAsUser(packageName,
                    PackageManager.MATCH_DIRECT_BOOT_AWARE
                            | PackageManager.MATCH_DIRECT_BOOT_UNAWARE, userHandle);
            return ai != null && (ai.flags & ApplicationInfo.FLAG_TEST_ONLY) != 0;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }

    }

    /**
     * Returns whether the current user is the system user.
     */
    public boolean isCurrentUserSystem() {
        return UserHandle.myUserId() == UserHandle.USER_SYSTEM;
    }

    /**
     * Returns whether the device is currently managed.
     */
    public boolean isDeviceManaged(Context context) {
        DevicePolicyManager dpm =
                (DevicePolicyManager) context.getSystemService(Context.DEVICE_POLICY_SERVICE);
        return dpm.isDeviceManaged();
    }

    /**
     * Returns true if the given package requires an update.
     *
     * <p>There are two cases where an update is required:
     * 1. The package is not currently present on the device.
     * 2. The package is present, but the version is below the minimum supported version.
     *
     * @param packageName the package to be checked for updates
     * @param minSupportedVersion the minimum supported version
     * @param context a {@link Context} object
     */
    public boolean packageRequiresUpdate(String packageName, int minSupportedVersion,
            Context context) {
        try {
            PackageInfo packageInfo = context.getPackageManager().getPackageInfo(packageName, 0);
            // Always download packages if no minimum version given.
            if (minSupportedVersion != PackageDownloadInfo.DEFAULT_MINIMUM_VERSION
                    && packageInfo.versionCode >= minSupportedVersion) {
                return false;
            }
        } catch (NameNotFoundException e) {
            // Package not on device.
        }

        return true;
    }

    /**
     * Returns the first existing managed profile if any present, null otherwise.
     *
     * <p>Note that we currently only support one managed profile per device.
     */
    // TODO: Add unit tests
    public UserHandle getManagedProfile(Context context) {
        UserManager userManager = (UserManager) context.getSystemService(Context.USER_SERVICE);
        int currentUserId = userManager.getUserHandle();
        List<UserInfo> userProfiles = userManager.getProfiles(currentUserId);
        for (UserInfo profile : userProfiles) {
            if (profile.isManagedProfile()) {
                return new UserHandle(profile.id);
            }
        }
        return null;
    }

    /**
     * Returns the user id of an already existing managed profile or -1 if none exists.
     */
    // TODO: Add unit tests
    public int alreadyHasManagedProfile(Context context) {
        UserHandle managedUser = getManagedProfile(context);
        if (managedUser != null) {
            return managedUser.getIdentifier();
        } else {
            return -1;
        }
    }

    /**
     * Removes an account asynchronously.
     *
     * @see #removeAccount(Context, Account)
     */
    public void removeAccountAsync(Context context, Account accountToRemove,
            RemoveAccountListener callback) {
        new RemoveAccountAsyncTask(context, accountToRemove, this, callback).execute();
    }

    /**
     * Removes an account synchronously.
     *
     * This method is blocking and must never be called from the main thread.
     *
     * <p>This removes the given account from the calling user's list of accounts.
     *
     * @param context a {@link Context} object
     * @param account the account to be removed
     */
    // TODO: Add unit tests
    @WorkerThread
    void removeAccount(Context context, Account account) {
        final AccountManager accountManager =
                (AccountManager) context.getSystemService(Context.ACCOUNT_SERVICE);
        final AccountManagerFuture<Bundle> bundle = accountManager.removeAccount(account,
                null, null /* callback */, null /* handler */);
        // Block to get the result of the removeAccount operation
        try {
            final Bundle result = bundle.getResult();
            if (result.getBoolean(AccountManager.KEY_BOOLEAN_RESULT, /* default */ false)) {
                ProvisionLogger.logw("Account removed from the primary user.");
            } else {
                final Intent removeIntent = result.getParcelable(AccountManager.KEY_INTENT);
                if (removeIntent != null) {
                    ProvisionLogger.logi("Starting activity to remove account");
                    new Handler(Looper.getMainLooper()).post(() -> {
                        TrampolineActivity.startActivity(context, removeIntent);
                    });
                } else {
                    ProvisionLogger.logw("Could not remove account from the primary user.");
                }
            }
        } catch (OperationCanceledException | AuthenticatorException | IOException e) {
            ProvisionLogger.logw("Exception removing account from the primary user.", e);
        }
    }

    /**
     * Returns whether FRP is supported on the device.
     */
    public boolean isFrpSupported(Context context) {
        Object pdbManager = context.getSystemService(Context.PERSISTENT_DATA_BLOCK_SERVICE);
        return pdbManager != null;
    }

    /**
     * Translates a given managed provisioning intent to its corresponding provisioning flow, using
     * the action from the intent.
     *
     * <p/>This is necessary because, unlike other provisioning actions which has 1:1 mapping, there
     * are multiple actions that can trigger the device owner provisioning flow. This includes
     * {@link ACTION_PROVISION_MANAGED_DEVICE}, {@link ACTION_NDEF_DISCOVERED} and
     * {@link ACTION_PROVISION_MANAGED_DEVICE_FROM_TRUSTED_SOURCE}. These 3 actions are equivalent
     * excepts they are sent from a different source.
     *
     * @return the appropriate DevicePolicyManager declared action for the given incoming intent.
     * @throws IllegalProvisioningArgumentException if intent is malformed
     */
    // TODO: Add unit tests
    public String mapIntentToDpmAction(Intent intent)
            throws IllegalProvisioningArgumentException {
        if (intent == null || intent.getAction() == null) {
            throw new IllegalProvisioningArgumentException("Null intent action.");
        }

        // Map the incoming intent to a DevicePolicyManager.ACTION_*, as there is a N:1 mapping in
        // some cases.
        String dpmProvisioningAction;
        switch (intent.getAction()) {
            // Trivial cases.
            case ACTION_PROVISION_MANAGED_DEVICE:
            case ACTION_PROVISION_MANAGED_SHAREABLE_DEVICE:
            case ACTION_PROVISION_MANAGED_USER:
            case ACTION_PROVISION_MANAGED_PROFILE:
            case ACTION_PROVISION_FINANCED_DEVICE:
                dpmProvisioningAction = intent.getAction();
                break;

            // Silent device owner is same as device owner.
            case ACTION_PROVISION_MANAGED_DEVICE_SILENTLY:
                dpmProvisioningAction = ACTION_PROVISION_MANAGED_DEVICE;
                break;

            // NFC cases which need to take mime-type into account.
            case ACTION_NDEF_DISCOVERED:
                String mimeType = intent.getType();
                if (mimeType == null) {
                    throw new IllegalProvisioningArgumentException(
                            "Unknown NFC bump mime-type: " + mimeType);
                }
                switch (mimeType) {
                    case MIME_TYPE_PROVISIONING_NFC:
                        dpmProvisioningAction = ACTION_PROVISION_MANAGED_DEVICE;
                        break;

                    default:
                        throw new IllegalProvisioningArgumentException(
                                "Unknown NFC bump mime-type: " + mimeType);
                }
                break;

            // Device owner provisioning from a trusted app.
            // TODO (b/27217042): review for new management modes in split system-user model
            case ACTION_PROVISION_MANAGED_DEVICE_FROM_TRUSTED_SOURCE:
                dpmProvisioningAction = ACTION_PROVISION_MANAGED_DEVICE;
                break;

            default:
                throw new IllegalProvisioningArgumentException("Unknown intent action "
                        + intent.getAction());
        }
        return dpmProvisioningAction;
    }

    /**
     * Returns if the given intent for a organization owned provisioning.
     * Only QR, cloud enrollment and NFC are owned by organization.
     */
    public boolean isOrganizationOwnedProvisioning(Intent intent) {
        if (ACTION_NDEF_DISCOVERED.equals(intent.getAction())) {
            return true;
        }
        if (!ACTION_PROVISION_MANAGED_DEVICE_FROM_TRUSTED_SOURCE.equals(intent.getAction())) {
            return false;
        }
        //  Do additional check under ACTION_PROVISION_MANAGED_DEVICE_FROM_TRUSTED_SOURCE
        // in order to exclude force DO.
        switch (intent.getIntExtra(DevicePolicyManager.EXTRA_PROVISIONING_TRIGGER,
                PROVISIONING_TRIGGER_UNSPECIFIED)) {
            case PROVISIONING_TRIGGER_CLOUD_ENROLLMENT:
            case PROVISIONING_TRIGGER_QR_CODE:
                return true;
            default:
                return false;
        }
    }

    public boolean isQrProvisioning(Intent intent) {
        return PROVISIONING_TRIGGER_QR_CODE ==
                intent.getIntExtra(
                        DevicePolicyManager.EXTRA_PROVISIONING_TRIGGER,
                        /* defValue= */ PROVISIONING_TRIGGER_UNSPECIFIED);
    }

    /**
     * Returns if the given parameter is for provisioning the admin integrated flow.
     */
    public boolean isAdminIntegratedFlow(ProvisioningParams params) {
        if (!params.isOrganizationOwnedProvisioning) {
            return false;
        }
        return params.provisioningMode == PROVISIONING_MODE_FULLY_MANAGED_DEVICE
                || params.provisioningMode == PROVISIONING_MODE_MANAGED_PROFILE
                || params.provisioningMode
                    == PROVISIONING_MODE_MANAGED_PROFILE_ON_FULLY_NAMAGED_DEVICE;
    }

    /**
     * Sends an intent to trigger a factory reset.
     */
    // TODO: Move the FR intent into a Globals class.
    public void sendFactoryResetBroadcast(Context context, String reason) {
        Intent intent = new Intent(Intent.ACTION_FACTORY_RESET);
        // Send explicit broadcast due to Broadcast Limitations
        intent.setPackage("android");
        intent.addFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        intent.putExtra(Intent.EXTRA_REASON, reason);
        context.sendBroadcast(intent);
    }

    /**
     * Returns whether the given provisioning action is a profile owner action.
     */
    // TODO: Move the list of device owner actions into a Globals class.
    public final boolean isProfileOwnerAction(String action) {
        return ACTION_PROVISION_MANAGED_PROFILE.equals(action)
                || ACTION_PROVISION_MANAGED_USER.equals(action);
    }

    /**
     * Returns whether the given provisioning action is a device owner action.
     */
    // TODO: Move the list of device owner actions into a Globals class.
    public final boolean isDeviceOwnerAction(String action) {
        return ACTION_PROVISION_MANAGED_DEVICE.equals(action)
                || ACTION_PROVISION_MANAGED_SHAREABLE_DEVICE.equals(action);
    }

    /**
     * Returns whether the given provisioning action is a financed device action.
     */
    public final boolean isFinancedDeviceAction(String action) {
        return ACTION_PROVISION_FINANCED_DEVICE.equals(action);
    }

    /**
     * Returns whether the device currently has connectivity.
     */
    public boolean isConnectedToNetwork(Context context) {
        NetworkInfo info = getActiveNetworkInfo(context);
        return info != null && info.isConnected();
    }

    public boolean isMobileNetworkConnectedToInternet(Context context) {
        final ConnectivityManager connectivityManager =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        return Arrays.stream(connectivityManager.getAllNetworks())
                .filter(network -> {
                    return Objects.nonNull(connectivityManager.getNetworkCapabilities(network));
                })
                .map(connectivityManager::getNetworkCapabilities)
                .filter(this::isCellularNetwork)
                .anyMatch(this::isConnectedToInternet);
    }

    private boolean isConnectedToInternet(NetworkCapabilities capabilities) {
        return capabilities.hasCapability(NET_CAPABILITY_INTERNET)
                && capabilities.hasCapability(NET_CAPABILITY_VALIDATED);
    }

    private boolean isCellularNetwork(NetworkCapabilities capabilities) {
        return capabilities.hasTransport(TRANSPORT_CELLULAR);
    }

    /**
     * Returns whether the device is currently connected to specific network type, such as {@link
     * ConnectivityManager.TYPE_WIFI} or {@link ConnectivityManager.TYPE_ETHERNET}
     *
     * {@see ConnectivityManager}
     */
    public boolean isNetworkTypeConnected(Context context, int... types) {
        final NetworkInfo networkInfo = getActiveNetworkInfo(context);
        if (networkInfo != null && networkInfo.isConnected()) {
            final int activeNetworkType = networkInfo.getType();
            for (int type : types) {
                if (activeNetworkType == type) {
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * Returns the active network info of the device.
     */
    public NetworkInfo getActiveNetworkInfo(Context context) {
        ConnectivityManager cm =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        return cm.getActiveNetworkInfo();
    }

    /**
     * Returns whether encryption is required on this device.
     *
     * <p>Encryption is required if the device is not currently encrypted and the persistent
     * system flag {@code persist.sys.no_req_encrypt} is not set.
     */
    public boolean isEncryptionRequired() {
        return !isPhysicalDeviceEncrypted()
                && !SystemProperties.getBoolean("persist.sys.no_req_encrypt", false);
    }

    /**
     * Returns whether the device is currently encrypted.
     */
    public boolean isPhysicalDeviceEncrypted() {
        return StorageManager.isEncrypted();
    }

    /**
     * Returns the wifi pick intent.
     */
    // TODO: Move this intent into a Globals class.
    public Intent getWifiPickIntent() {
        Intent wifiIntent = new Intent(WifiManager.ACTION_PICK_WIFI_NETWORK);
        wifiIntent.putExtra("extra_prefs_show_button_bar", true);
        wifiIntent.putExtra("wifi_enable_next_on_connect", true);
        return wifiIntent;
    }

    /**
     * Returns whether the device has a split system user.
     *
     * <p>Split system user means that user 0 is system only and all meat users are separate from
     * the system user.
     */
    public boolean isSplitSystemUser() {
        return UserManager.isSplitSystemUser();
    }

    /**
     * Returns whether the currently chosen launcher supports managed profiles.
     *
     * <p>A launcher is deemed to support managed profiles when its target API version is at least
     * {@link Build.VERSION_CODES#LOLLIPOP}.
     */
    public boolean currentLauncherSupportsManagedProfiles(Context context) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_HOME);

        PackageManager pm = context.getPackageManager();
        ResolveInfo launcherResolveInfo = pm.resolveActivity(intent,
                PackageManager.MATCH_DEFAULT_ONLY);
        if (launcherResolveInfo == null) {
            return false;
        }
        try {
            // If the user has not chosen a default launcher, then launcherResolveInfo will be
            // referring to the resolver activity. It is fine to create a managed profile in
            // this case since there will always be at least one launcher on the device that
            // supports managed profile feature.
            ApplicationInfo launcherAppInfo = pm.getApplicationInfo(
                    launcherResolveInfo.activityInfo.packageName, 0 /* default flags */);
            return versionNumberAtLeastL(launcherAppInfo.targetSdkVersion);
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    /**
     * Returns whether the given version number is at least lollipop.
     *
     * @param versionNumber the version number to be verified.
     */
    private boolean versionNumberAtLeastL(int versionNumber) {
        return versionNumber >= Build.VERSION_CODES.LOLLIPOP;
    }

    /**
     * Computes the sha 256 hash of a byte array.
     */
    @Nullable
    public byte[] computeHashOfByteArray(byte[] bytes) {
        try {
            MessageDigest md = MessageDigest.getInstance(SHA256_TYPE);
            md.update(bytes);
            return md.digest();
        } catch (NoSuchAlgorithmException e) {
            ProvisionLogger.loge("Hashing algorithm " + SHA256_TYPE + " not supported.", e);
            return null;
        }
    }

    /**
     * Computes a hash of a file with a spcific hash algorithm.
     */
    // TODO: Add unit tests
    @Nullable
    public byte[] computeHashOfFile(String fileLocation, String hashType) {
        InputStream fis = null;
        MessageDigest md;
        byte hash[] = null;
        try {
            md = MessageDigest.getInstance(hashType);
        } catch (NoSuchAlgorithmException e) {
            ProvisionLogger.loge("Hashing algorithm " + hashType + " not supported.", e);
            return null;
        }
        try {
            fis = new FileInputStream(fileLocation);

            byte[] buffer = new byte[256];
            int n = 0;
            while (n != -1) {
                n = fis.read(buffer);
                if (n > 0) {
                    md.update(buffer, 0, n);
                }
            }
            hash = md.digest();
        } catch (IOException e) {
            ProvisionLogger.loge("IO error.", e);
        } finally {
            // Close input stream quietly.
            try {
                if (fis != null) {
                    fis.close();
                }
            } catch (IOException e) {
                // Ignore.
            }
        }
        return hash;
    }

    public boolean isBrightColor(int color) {
        // This comes from the YIQ transformation. We're using the formula:
        // Y = .299 * R + .587 * G + .114 * B
        return Color.red(color) * 299 + Color.green(color) * 587 + Color.blue(color) * 114
                >= 1000 * THRESHOLD_BRIGHT_COLOR;
    }

    /**
     * Returns whether given intent can be resolved for the user.
     */
    public boolean canResolveIntentAsUser(Context context, Intent intent, int userId) {
        return intent != null
                && context.getPackageManager().resolveActivityAsUser(intent, 0, userId) != null;
    }

    public boolean isPackageDeviceOwner(DevicePolicyManager dpm, String packageName) {
        final ComponentName deviceOwner = dpm.getDeviceOwnerComponentOnCallingUser();
        return deviceOwner != null && deviceOwner.getPackageName().equals(packageName);
    }

    public int getAccentColor(Context context) {
        return getAttrColor(context, android.R.attr.colorAccent);
    }

    private int getAttrColor(Context context, int attr) {
        TypedArray ta = context.obtainStyledAttributes(new int[]{attr});
        int attrColor = ta.getColor(0, 0);
        ta.recycle();
        return attrColor;
    }

    public void handleSupportUrl(Context context, CustomizationParams customizationParams,
                ClickableSpanFactory clickableSpanFactory,
                AccessibilityContextMenuMaker contextMenuMaker, TextView textView,
                String deviceProvider, String contactDeviceProvider) {
        if (customizationParams.supportUrl == null) {
            textView.setText(contactDeviceProvider);
            return;
        }
        final Intent intent = WebActivity.createIntent(
                context, customizationParams.supportUrl, customizationParams.statusBarColor);

        handlePartialClickableTextView(textView, contactDeviceProvider, deviceProvider, intent,
                clickableSpanFactory);

        contextMenuMaker.registerWithActivity(textView);
    }

    /**
     * Utility function to make a TextView partial clickable. It also associates the TextView with
     * an Intent. The intent will be triggered when the clickable part is clicked.
     *
     * @param textView The TextView which hosts the clickable string.
     * @param content The content of the TextView.
     * @param clickableString The substring which is clickable.
     * @param intent The Intent that will be launched.
     * @param clickableSpanFactory The factory which is used to create ClickableSpan to decorate
     *                             clickable string.
     */
    public void handlePartialClickableTextView(TextView textView, String content,
            String clickableString, Intent intent, ClickableSpanFactory clickableSpanFactory) {
        final SpannableString spannableString = new SpannableString(content);
        if (intent != null) {
            final ClickableSpan span = clickableSpanFactory.create(intent);
            final int startIdx = content.indexOf(clickableString);
            final int endIdx = startIdx + clickableString.length();

            spannableString.setSpan(span, startIdx, endIdx, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            textView.setMovementMethod(LinkMovementMethod.getInstance());
        }

        textView.setText(spannableString);
    }

    public static boolean isSilentProvisioningForTestingDeviceOwner(
                Context context, ProvisioningParams params) {
        final DevicePolicyManager dpm = context.getSystemService(DevicePolicyManager.class);
        final ComponentName currentDeviceOwner =
                dpm.getDeviceOwnerComponentOnCallingUser();
        final ComponentName targetDeviceAdmin = params.deviceAdminComponentName;

        switch (params.provisioningAction) {
            case DevicePolicyManager.ACTION_PROVISION_MANAGED_DEVICE:
                return isPackageTestOnly(context, params)
                        && currentDeviceOwner != null
                        && targetDeviceAdmin != null
                        && currentDeviceOwner.equals(targetDeviceAdmin);
            default:
                return false;
        }
    }

    private static boolean isSilentProvisioningForTestingManagedProfile(
        Context context, ProvisioningParams params) {
        return DevicePolicyManager.ACTION_PROVISION_MANAGED_PROFILE.equals(
                params.provisioningAction) && isPackageTestOnly(context, params);
    }

    public static boolean isSilentProvisioning(Context context, ProvisioningParams params) {
        return isSilentProvisioningForTestingManagedProfile(context, params)
                || isSilentProvisioningForTestingDeviceOwner(context, params);
    }

    private static boolean isPackageTestOnly(Context context, ProvisioningParams params) {
        final UserManager userManager = context.getSystemService(UserManager.class);
        return isPackageTestOnly(context.getPackageManager(),
                params.inferDeviceAdminPackageName(), userManager.getUserHandle());
    }

    public static FooterButton addNextButton(GlifLayout layout, @NonNull OnClickListener listener) {
        return setPrimaryButton(layout, listener, ButtonType.NEXT, R.string.next);
    }

    public static FooterButton addDoneButton(GlifLayout layout, @NonNull OnClickListener listener) {
        return setPrimaryButton(layout, listener, ButtonType.DONE, R.string.done);
    }

    public static FooterButton addAcceptAndContinueButton(GlifLayout layout,
        @NonNull OnClickListener listener) {
        return setPrimaryButton(layout, listener, ButtonType.NEXT, R.string.accept_and_continue);
    }

    private static FooterButton setPrimaryButton(GlifLayout layout, OnClickListener listener,
        @ButtonType int buttonType, @StringRes int label) {
        final FooterBarMixin mixin = layout.getMixin(FooterBarMixin.class);
        final FooterButton primaryButton = new FooterButton.Builder(layout.getContext())
            .setText(label)
            .setListener(listener)
            .setButtonType(buttonType)
            .setTheme(R.style.SudGlifButton_Primary)
            .build();
        mixin.setPrimaryButton(primaryButton);
        return primaryButton;
    }

    public SimpleDialog.Builder createCancelProvisioningResetDialogBuilder() {
        final int positiveResId = R.string.reset;
        final int negativeResId = R.string.device_owner_cancel_cancel;
        final int dialogMsgResId = R.string.this_will_reset_take_back_first_screen;
        return getBaseDialogBuilder(positiveResId, negativeResId, dialogMsgResId)
                .setTitle(R.string.stop_setup_reset_device_question);
    }

    public SimpleDialog.Builder createCancelProvisioningDialogBuilder() {
        final int positiveResId = R.string.profile_owner_cancel_ok;
        final int negativeResId = R.string.profile_owner_cancel_cancel;
        final int dialogMsgResId = R.string.profile_owner_cancel_message;
        return getBaseDialogBuilder(positiveResId, negativeResId, dialogMsgResId);
    }

    private SimpleDialog.Builder getBaseDialogBuilder(
            int positiveResId, int negativeResId, int dialogMsgResId) {
        return new SimpleDialog.Builder()
                .setCancelable(false)
                .setMessage(dialogMsgResId)
                .setNegativeButtonMessage(negativeResId)
                .setPositiveButtonMessage(positiveResId);
    }
}
