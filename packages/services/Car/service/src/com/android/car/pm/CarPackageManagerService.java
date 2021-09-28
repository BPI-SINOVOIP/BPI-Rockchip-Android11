/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.car.pm;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.UserIdInt;
import android.app.ActivityManager;
import android.app.ActivityManager.StackInfo;
import android.app.PendingIntent;
import android.car.Car;
import android.car.content.pm.AppBlockingPackageInfo;
import android.car.content.pm.CarAppBlockingPolicy;
import android.car.content.pm.CarAppBlockingPolicyService;
import android.car.content.pm.CarPackageManager;
import android.car.content.pm.ICarPackageManager;
import android.car.drivingstate.CarUxRestrictions;
import android.car.drivingstate.ICarUxRestrictionsChangeListener;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.content.pm.Signature;
import android.content.res.Resources;
import android.hardware.display.DisplayManager;
import android.os.Binder;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.os.Process;
import android.os.UserHandle;
import android.text.format.DateFormat;
import android.util.ArraySet;
import android.util.Log;
import android.util.Pair;
import android.util.SparseArray;
import android.view.Display;
import android.view.DisplayAddress;

import com.android.car.CarLog;
import com.android.car.CarServiceBase;
import com.android.car.CarServiceUtils;
import com.android.car.CarUxRestrictionsManagerService;
import com.android.car.R;
import com.android.car.SystemActivityMonitoringService;
import com.android.car.SystemActivityMonitoringService.TopTaskInfoContainer;
import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

import com.google.android.collect.Sets;

import java.io.PrintWriter;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

public class CarPackageManagerService extends ICarPackageManager.Stub implements CarServiceBase {
    private static final boolean DBG = false;
    private static final boolean DBG_POLICY_SET = false;
    private static final boolean DBG_POLICY_CHECK = false;
    private static final boolean DBG_POLICY_ENFORCEMENT = false;
    // Delimiters to parse packages and activities in the configuration XML resource.
    private static final String PACKAGE_DELIMITER = ",";
    private static final String PACKAGE_ACTIVITY_DELIMITER = "/";
    private static final int LOG_SIZE = 20;

    private final Context mContext;
    private final SystemActivityMonitoringService mSystemActivityMonitoringService;
    private final PackageManager mPackageManager;
    private final ActivityManager mActivityManager;
    private final DisplayManager mDisplayManager;

    private final HandlerThread mHandlerThread = CarServiceUtils.getHandlerThread(
            getClass().getSimpleName());
    private final PackageHandler mHandler  = new PackageHandler(mHandlerThread.getLooper(), this);
    private final Object mLock = new Object();

    // For dumpsys logging.
    private final LinkedList<String> mBlockedActivityLogs = new LinkedList<>();

    // Store the allowlist and blocklist strings from the resource file.
    private String mConfiguredAllowlist;
    private String mConfiguredSystemAllowlist;
    private String mConfiguredBlocklist;
    @GuardedBy("mLock")
    private Map<String, Set<String>> mConfiguredAllowlistMap;
    @GuardedBy("mLock")
    private Map<String, Set<String>> mConfiguredBlocklistMap;

    private final List<String> mAllowedAppInstallSources;

    /**
     * Hold policy set from policy service or client.
     * Key: packageName of policy service
     */
    @GuardedBy("mLock")
    private final HashMap<String, ClientPolicy> mClientPolicies = new HashMap<>();
    @GuardedBy("mLock")
    private HashMap<String, AppBlockingPackageInfoWrapper> mActivityAllowlistMap = new HashMap<>();
    @GuardedBy("mLock")
    private  HashSet<String> mActivityDenylistPackages = new HashSet<String>();

    @GuardedBy("mLock")
    private LinkedList<AppBlockingPolicyProxy> mProxies;

    @GuardedBy("mLock")
    private final LinkedList<CarAppBlockingPolicy> mWaitingPolicies = new LinkedList<>();

    private final CarUxRestrictionsManagerService mCarUxRestrictionsService;
    private boolean mEnableActivityBlocking;
    private final ComponentName mActivityBlockingActivity;

    private final ActivityLaunchListener mActivityLaunchListener = new ActivityLaunchListener();

    // K: (logical) display id of a physical display, V: UXR change listener of this display.
    // For multi-display, monitor UXR change on each display.
    private final SparseArray<UxRestrictionsListener> mUxRestrictionsListeners =
            new SparseArray<>();
    private final VendorServiceController mVendorServiceController;

    // Information related to when the installed packages should be parsed for building a allow and
    // block list
    private final Set<String> mPackageManagerActions = Sets.newArraySet(
            Intent.ACTION_PACKAGE_ADDED,
            Intent.ACTION_PACKAGE_CHANGED,
            Intent.ACTION_PACKAGE_REMOVED,
            Intent.ACTION_PACKAGE_REPLACED);

    private final PackageParsingEventReceiver mPackageParsingEventReceiver =
            new PackageParsingEventReceiver();

    /**
     * Name of blocked activity.
     *
     * @hide
     */
    public static final String BLOCKING_INTENT_EXTRA_BLOCKED_ACTIVITY_NAME = "blocked_activity";
    /**
     * int task id of the blocked task.
     *
     * @hide
     */
    public static final String BLOCKING_INTENT_EXTRA_BLOCKED_TASK_ID = "blocked_task_id";
    /**
     * Name of root activity of blocked task.
     *
     * @hide
     */
    public static final String BLOCKING_INTENT_EXTRA_ROOT_ACTIVITY_NAME = "root_activity_name";
    /**
     * Boolean indicating whether the root activity is distraction-optimized (DO).
     * Blocking screen should show a button to restart the task if {@code true}.
     *
     * @hide
     */
    public static final String BLOCKING_INTENT_EXTRA_IS_ROOT_ACTIVITY_DO = "is_root_activity_do";

    /**
     * int display id of the blocked task.
     *
     * @hide
     */
    public static final String BLOCKING_INTENT_EXTRA_DISPLAY_ID = "display_id";

    public CarPackageManagerService(Context context,
            CarUxRestrictionsManagerService uxRestrictionsService,
            SystemActivityMonitoringService systemActivityMonitoringService) {
        mContext = context;
        mCarUxRestrictionsService = uxRestrictionsService;
        mSystemActivityMonitoringService = systemActivityMonitoringService;
        mPackageManager = mContext.getPackageManager();
        mActivityManager = mContext.getSystemService(ActivityManager.class);
        mDisplayManager = mContext.getSystemService(DisplayManager.class);
        Resources res = context.getResources();
        mEnableActivityBlocking = res.getBoolean(R.bool.enableActivityBlockingForSafety);
        String blockingActivity = res.getString(R.string.activityBlockingActivity);
        mActivityBlockingActivity = ComponentName.unflattenFromString(blockingActivity);
        mAllowedAppInstallSources = Arrays.asList(
                res.getStringArray(R.array.allowedAppInstallSources));
        mVendorServiceController = new VendorServiceController(
                mContext, mHandler.getLooper());
    }


    @Override
    public void setAppBlockingPolicy(String packageName, CarAppBlockingPolicy policy, int flags) {
        if (DBG_POLICY_SET) {
            Log.i(CarLog.TAG_PACKAGE, "policy setting from binder call, client:" + packageName);
        }
        doSetAppBlockingPolicy(packageName, policy, flags);
    }

    /**
     * Restarts the requested task. If task with {@code taskId} does not exist, do nothing.
     */
    @Override
    public void restartTask(int taskId) {
        if (mContext.checkCallingOrSelfPermission(android.Manifest.permission.REAL_GET_TASKS)
                != PackageManager.PERMISSION_GRANTED) {
            throw new SecurityException(
                    "requires permission " + android.Manifest.permission.REAL_GET_TASKS);
        }
        mSystemActivityMonitoringService.restartTask(taskId);
    }

    private void doSetAppBlockingPolicy(String packageName, CarAppBlockingPolicy policy,
            int flags) {
        if (mContext.checkCallingOrSelfPermission(Car.PERMISSION_CONTROL_APP_BLOCKING)
                != PackageManager.PERMISSION_GRANTED) {
            throw new SecurityException(
                    "requires permission " + Car.PERMISSION_CONTROL_APP_BLOCKING);
        }
        CarServiceUtils.assertPackageName(mContext, packageName);
        if (policy == null) {
            throw new IllegalArgumentException("policy cannot be null");
        }
        if ((flags & CarPackageManager.FLAG_SET_POLICY_ADD) != 0 &&
                (flags & CarPackageManager.FLAG_SET_POLICY_REMOVE) != 0) {
            throw new IllegalArgumentException(
                    "Cannot set both FLAG_SET_POLICY_ADD and FLAG_SET_POLICY_REMOVE flag");
        }
        synchronized (mLock) {
            if ((flags & CarPackageManager.FLAG_SET_POLICY_WAIT_FOR_CHANGE) != 0) {
                mWaitingPolicies.add(policy);
            }
        }
        mHandler.requestUpdatingPolicy(packageName, policy, flags);
        if ((flags & CarPackageManager.FLAG_SET_POLICY_WAIT_FOR_CHANGE) != 0) {
            synchronized (mLock) {
                try {
                    while (mWaitingPolicies.contains(policy)) {
                        mLock.wait();
                    }
                } catch (InterruptedException e) {
                    // Pass it over binder call
                    throw new IllegalStateException(
                            "Interrupted while waiting for policy completion", e);
                }
            }
        }
    }

    @Override
    public boolean isActivityDistractionOptimized(String packageName, String className) {
        assertPackageAndClassName(packageName, className);
        synchronized (mLock) {
            if (DBG_POLICY_CHECK) {
                Log.i(CarLog.TAG_PACKAGE, "isActivityDistractionOptimized"
                        + dumpPoliciesLocked(false));
            }

            if (searchFromClientPolicyBlocklistsLocked(packageName)) {
                return false;
            }

            if (isActivityInClientPolicyAllowlistsLocked(packageName, className)) {
                return true;
            }

            // Check deny and allow list
            boolean packageBlocked = mActivityDenylistPackages.contains(packageName);
            AppBlockingPackageInfoWrapper infoWrapper = mActivityAllowlistMap.get(packageName);
            if (!packageBlocked && infoWrapper == null) {
                // Update cache
                updateActivityAllowlistAndDenylistMap(packageName);
                packageBlocked = mActivityDenylistPackages.contains(packageName);
                infoWrapper = mActivityAllowlistMap.get(packageName);
            }

            if (packageBlocked
                    || !isActivityInMapAndMatching(infoWrapper, packageName, className)) {
                return false;
            }

            return true;
        }
    }

    @Override
    public boolean isPendingIntentDistractionOptimized(PendingIntent pendingIntent) {
        ResolveInfo info = mPackageManager.resolveActivity(
                pendingIntent.getIntent(), PackageManager.MATCH_DEFAULT_ONLY);
        if (info == null) return false;
        ActivityInfo activityInfo = info.activityInfo;
        return isActivityDistractionOptimized(activityInfo.packageName, activityInfo.name);
    }

    @Override
    public boolean isServiceDistractionOptimized(String packageName, String className) {
        if (packageName == null) {
            throw new IllegalArgumentException("Package name null");
        }
        synchronized (mLock) {
            if (DBG_POLICY_CHECK) {
                Log.i(CarLog.TAG_PACKAGE,
                        "isServiceDistractionOptimized" + dumpPoliciesLocked(false));
            }

            if (searchFromClientPolicyBlocklistsLocked(packageName)) {
                return false;
            }

            if (searchFromClientPolicyAllowlistsLocked(packageName)) {
                return true;
            }

            // Check deny and allow list
            boolean packageBlocked = mActivityDenylistPackages.contains(packageName);
            AppBlockingPackageInfoWrapper infoWrapper = mActivityAllowlistMap.get(packageName);
            if (!packageBlocked && infoWrapper == null) {
                // Update cache
                updateActivityAllowlistAndDenylistMap(packageName);
                packageBlocked = mActivityDenylistPackages.contains(packageName);
                infoWrapper = mActivityAllowlistMap.get(packageName);
            }

            if (packageBlocked || infoWrapper == null || infoWrapper.info == null) {
                return false;
            }

            return true;
        }
    }

    @Override
    public boolean isActivityBackedBySafeActivity(ComponentName activityName) {
        StackInfo info = mSystemActivityMonitoringService.getFocusedStackForTopActivity(
                activityName);
        if (info == null) { // not top in focused stack
            return true;
        }
        if (!isUxRestrictedOnDisplay(info.displayId)) {
            return true;
        }
        if (info.taskNames.length <= 1) { // nothing below this.
            return false;
        }
        ComponentName activityBehind = ComponentName.unflattenFromString(
                info.taskNames[info.taskNames.length - 2]);
        return isActivityDistractionOptimized(activityBehind.getPackageName(),
                activityBehind.getClassName());
    }

    public Looper getLooper() {
        return mHandlerThread.getLooper();
    }

    private void assertPackageAndClassName(String packageName, String className) {
        if (packageName == null) {
            throw new IllegalArgumentException("Package name null");
        }
        if (className == null) {
            throw new IllegalArgumentException("Class name null");
        }
    }

    @GuardedBy("mLock")
    private boolean searchFromClientPolicyBlocklistsLocked(String packageName) {
        for (ClientPolicy policy : mClientPolicies.values()) {
            AppBlockingPackageInfoWrapper wrapper = policy.mBlocklistsMap.get(packageName);
            if (wrapper != null && wrapper.isMatching && wrapper.info != null) {
                return true;
            }
        }

        return false;
    }

    @GuardedBy("mLock")
    private boolean searchFromClientPolicyAllowlistsLocked(String packageName) {
        for (ClientPolicy policy : mClientPolicies.values()) {
            AppBlockingPackageInfoWrapper wrapper = policy.mAllowlistsMap.get(packageName);
            if (wrapper != null && wrapper.isMatching && wrapper.info != null) {
                return true;
            }
        }
        return false;
    }

    @GuardedBy("mLock")
    private boolean isActivityInClientPolicyAllowlistsLocked(String packageName, String className) {
        for (ClientPolicy policy : mClientPolicies.values()) {
            if (isActivityInMapAndMatching(policy.mAllowlistsMap.get(packageName), packageName,
                    className)) {
                return true;
            }
        }
        return false;
    }

    private boolean isActivityInMapAndMatching(AppBlockingPackageInfoWrapper wrapper,
            String packageName, String className) {
        if (wrapper == null || !wrapper.isMatching) {
            if (DBG_POLICY_CHECK) {
                Log.d(CarLog.TAG_PACKAGE, "Pkg not in allowlist:" + packageName);
            }
            return false;
        }
        return wrapper.info.isActivityCovered(className);
    }

    @Override
    public void init() {
        synchronized (mLock) {
            mHandler.requestInit();
        }
    }

    @Override
    public void release() {
        synchronized (mLock) {
            mHandler.requestRelease();
            // wait for release do be done. This guarantees that init is done.
            try {
                mLock.wait();
            } catch (InterruptedException e) {
                Log.e(CarLog.TAG_PACKAGE,
                        "Interrupted wait during release");
                Thread.currentThread().interrupt();
            }
            mActivityAllowlistMap.clear();
            mActivityDenylistPackages.clear();
            mClientPolicies.clear();
            if (mProxies != null) {
                for (AppBlockingPolicyProxy proxy : mProxies) {
                    proxy.disconnect();
                }
                mProxies.clear();
            }
            mWaitingPolicies.clear();
            mLock.notifyAll();
        }
        mContext.unregisterReceiver(mPackageParsingEventReceiver);
        mSystemActivityMonitoringService.registerActivityLaunchListener(null);
        for (int i = 0; i < mUxRestrictionsListeners.size(); i++) {
            UxRestrictionsListener listener = mUxRestrictionsListeners.valueAt(i);
            mCarUxRestrictionsService.unregisterUxRestrictionsChangeListener(listener);
        }
    }

    // run from HandlerThread
    private void doHandleInit() {
        startAppBlockingPolicies();
        IntentFilter pkgParseIntent = new IntentFilter();
        for (String action : mPackageManagerActions) {
            pkgParseIntent.addAction(action);
        }
        pkgParseIntent.addDataScheme("package");
        mContext.registerReceiverAsUser(mPackageParsingEventReceiver, UserHandle.ALL,
                pkgParseIntent, null, null);

        List<Display> physicalDisplays = getPhysicalDisplays();

        // Assume default display (display 0) is always a physical display.
        Display defaultDisplay = mDisplayManager.getDisplay(Display.DEFAULT_DISPLAY);
        if (!physicalDisplays.contains(defaultDisplay)) {
            if (Log.isLoggable(CarLog.TAG_PACKAGE, Log.INFO)) {
                Log.i(CarLog.TAG_PACKAGE, "Adding default display to physical displays.");
            }
            physicalDisplays.add(defaultDisplay);
        }
        for (Display physicalDisplay : physicalDisplays) {
            int displayId = physicalDisplay.getDisplayId();
            UxRestrictionsListener listener = new UxRestrictionsListener(mCarUxRestrictionsService);
            mUxRestrictionsListeners.put(displayId, listener);
            mCarUxRestrictionsService.registerUxRestrictionsChangeListener(listener, displayId);
        }
        mVendorServiceController.init();
        mSystemActivityMonitoringService.registerActivityLaunchListener(mActivityLaunchListener);
    }

    private void doParseInstalledPackage(String packageName) {
        // Delete the package from allowlist and denylist mapping
        synchronized (mLock) {
            mActivityDenylistPackages.remove(packageName);
            mActivityAllowlistMap.remove(packageName);
        }

        // Generate allowlist and denylist mapping for the package
        updateActivityAllowlistAndDenylistMap(packageName);
        blockTopActivitiesIfNecessary();
    }

    private void doHandleRelease() {
        synchronized (mLock) {
            mVendorServiceController.release();
            mLock.notifyAll();
        }
    }

    private void doUpdatePolicy(String packageName, CarAppBlockingPolicy policy, int flags) {
        if (DBG_POLICY_SET) {
            Log.i(CarLog.TAG_PACKAGE, "setting policy from:" + packageName + ",policy:" + policy
                    + ",flags:0x" + Integer.toHexString(flags));
        }
        AppBlockingPackageInfoWrapper[] blocklistWrapper = verifyList(policy.blacklists);
        AppBlockingPackageInfoWrapper[] allowlistWrapper = verifyList(policy.whitelists);
        synchronized (mLock) {
            ClientPolicy clientPolicy = mClientPolicies.get(packageName);
            if (clientPolicy == null) {
                clientPolicy = new ClientPolicy();
                mClientPolicies.put(packageName, clientPolicy);
            }
            if ((flags & CarPackageManager.FLAG_SET_POLICY_ADD) != 0) {
                clientPolicy.addToBlocklists(blocklistWrapper);
                clientPolicy.addToAllowlists(allowlistWrapper);
            } else if ((flags & CarPackageManager.FLAG_SET_POLICY_REMOVE) != 0) {
                clientPolicy.removeBlocklists(blocklistWrapper);
                clientPolicy.removeAllowlists(allowlistWrapper);
            } else { //replace.
                clientPolicy.replaceBlocklists(blocklistWrapper);
                clientPolicy.replaceAllowlists(allowlistWrapper);
            }
            if ((flags & CarPackageManager.FLAG_SET_POLICY_WAIT_FOR_CHANGE) != 0) {
                mWaitingPolicies.remove(policy);
                mLock.notifyAll();
            }
            if (DBG_POLICY_SET) {
                Log.i(CarLog.TAG_PACKAGE, "policy set:" + dumpPoliciesLocked(false));
            }
        }
        blockTopActivitiesIfNecessary();
    }

    private AppBlockingPackageInfoWrapper[] verifyList(AppBlockingPackageInfo[] list) {
        if (list == null) {
            return null;
        }
        LinkedList<AppBlockingPackageInfoWrapper> wrappers = new LinkedList<>();
        for (int i = 0; i < list.length; i++) {
            AppBlockingPackageInfo info = list[i];
            if (info == null) {
                continue;
            }
            boolean isMatching = isInstalledPackageMatching(info);
            wrappers.add(new AppBlockingPackageInfoWrapper(info, isMatching));
        }
        return wrappers.toArray(new AppBlockingPackageInfoWrapper[wrappers.size()]);
    }

    boolean isInstalledPackageMatching(AppBlockingPackageInfo info) {
        PackageInfo packageInfo;
        try {
            packageInfo = mPackageManager.getPackageInfo(info.packageName,
                    PackageManager.GET_SIGNATURES);
        } catch (NameNotFoundException e) {
            return false;
        }
        if (packageInfo == null) {
            return false;
        }
        // if it is system app and client specified the flag, do not check signature
        if ((info.flags & AppBlockingPackageInfo.FLAG_SYSTEM_APP) == 0 ||
                (!packageInfo.applicationInfo.isSystemApp() &&
                        !packageInfo.applicationInfo.isUpdatedSystemApp())) {
            Signature[] signatures = packageInfo.signatures;
            if (!isAnySignatureMatching(signatures, info.signatures)) {
                return false;
            }
        }
        int version = packageInfo.versionCode;
        if (info.minRevisionCode == 0) {
            if (info.maxRevisionCode == 0) { // all versions
                return true;
            } else { // only max version matters
                return info.maxRevisionCode > version;
            }
        } else { // min version matters
            if (info.maxRevisionCode == 0) {
                return info.minRevisionCode < version;
            } else {
                return (info.minRevisionCode < version) && (info.maxRevisionCode > version);
            }
        }
    }

    /**
     * Any signature from policy matching with package's signatures is treated as matching.
     */
    boolean isAnySignatureMatching(Signature[] fromPackage, Signature[] fromPolicy) {
        if (fromPackage == null) {
            return false;
        }
        if (fromPolicy == null) {
            return false;
        }
        ArraySet<Signature> setFromPackage = new ArraySet<Signature>();
        for (Signature sig : fromPackage) {
            setFromPackage.add(sig);
        }
        for (Signature sig : fromPolicy) {
            if (setFromPackage.contains(sig)) {
                return true;
            }
        }
        return false;
    }

    private AppBlockingPackageInfoWrapper getPackageInfoWrapperForUser(String packageName,
            @UserIdInt int userId, Map<String, Set<String>> configAllowlist,
            Map<String, Set<String>> configBlocklist) {
        PackageInfo info;
        try {
            info = mPackageManager.getPackageInfoAsUser(packageName,
                    PackageManager.GET_SIGNATURES | PackageManager.GET_ACTIVITIES
                            | PackageManager.MATCH_DIRECT_BOOT_AWARE
                            | PackageManager.MATCH_DIRECT_BOOT_UNAWARE
                            | PackageManager.MATCH_DISABLED_COMPONENTS,
                    userId);
        } catch (NameNotFoundException e) {
            Log.w(CarLog.TAG_PACKAGE, packageName + " not installed! User Id: " + userId);
            return null;
        }


        if (info == null || info.applicationInfo == null) {
            return null;
        }

        int flags = 0;
        Set<String> activities = new ArraySet<>();

        if (info.applicationInfo.isSystemApp()
                || info.applicationInfo.isUpdatedSystemApp()) {
            flags = AppBlockingPackageInfo.FLAG_SYSTEM_APP;
        }

        /* 1. Check if all or some of this app is in the <activityAllowlist> or
              <systemActivityAllowlist> in config.xml */
        Set<String> configActivitiesForPackage = configAllowlist.get(info.packageName);
        if (configActivitiesForPackage != null) {
            if (DBG_POLICY_CHECK) {
                Log.d(CarLog.TAG_PACKAGE, info.packageName + " allowlisted");
            }
            if (configActivitiesForPackage.isEmpty()) {
                // Whole Pkg has been allowlisted
                flags |= AppBlockingPackageInfo.FLAG_WHOLE_ACTIVITY;
                // Add all activities to the allowlist
                List<String> activitiesForPackage = getActivitiesInPackage(info);
                if (activitiesForPackage != null) {
                    activities.addAll(activitiesForPackage);
                } else {
                    if (DBG_POLICY_CHECK) {
                        Log.d(CarLog.TAG_PACKAGE, info.packageName + ": Activities null");
                    }
                }
            } else {
                if (DBG_POLICY_CHECK) {
                    Log.d(CarLog.TAG_PACKAGE,
                            "Partially Allowlisted. WL Activities: " + configActivitiesForPackage);
                }
                activities.addAll(configActivitiesForPackage);
            }
        }
        /* 2. If app is not listed in the config.xml check their Manifest meta-data to
          see if they have any Distraction Optimized(DO) activities.
          For non system apps, we check if the app install source was a permittable
          source. This prevents side-loaded apps to fake DO.  Bypass the check
          for debug builds for development convenience. */
        if (!isDebugBuild()
                && !info.applicationInfo.isSystemApp()
                && !info.applicationInfo.isUpdatedSystemApp()) {
            try {
                if (mAllowedAppInstallSources != null) {
                    String installerName = mPackageManager.getInstallerPackageName(
                            info.packageName);
                    if (installerName == null || (installerName != null
                            && !mAllowedAppInstallSources.contains(installerName))) {
                        Log.w(CarLog.TAG_PACKAGE,
                                info.packageName + " not installed from permitted sources "
                                        + (installerName == null ? "NULL" : installerName));
                        return null;
                    }
                }
            } catch (IllegalArgumentException e) {
                Log.w(CarLog.TAG_PACKAGE, info.packageName + " not installed!");
                return null;
            }
        }

        try {
            String[] doActivities =
                    CarAppMetadataReader.findDistractionOptimizedActivitiesAsUser(
                            mContext, info.packageName, userId);
            if (doActivities != null) {
                // Some of the activities in this app are Distraction Optimized.
                if (DBG_POLICY_CHECK) {
                    for (String activity : doActivities) {
                        Log.d(CarLog.TAG_PACKAGE, "adding " + activity + " from "
                                + info.packageName + " to allowlist");
                    }
                }
                activities.addAll(Arrays.asList(doActivities));
            }
        } catch (NameNotFoundException e) {
            Log.w(CarLog.TAG_PACKAGE, "Error reading metadata: " + info.packageName);
            return null;
        }

        // Nothing to add to allowlist
        if (activities.isEmpty()) {
            return null;
        }

        /* 3. Check if parsed activity is in <activityBlocklist> in config.xml. Anything
              in blocklist should not be allowlisted, either as D.O. or by config. */
        if (configBlocklist.containsKey(info.packageName)) {
            Set<String> configBlocklistActivities = configBlocklist.get(info.packageName);
            if (configBlocklistActivities.isEmpty()) {
                // Whole package should be blocklisted.
                return null;
            }
            activities.removeAll(configBlocklistActivities);
        }

        Signature[] signatures;
        signatures = info.signatures;
        AppBlockingPackageInfo appBlockingInfo = new AppBlockingPackageInfo(info.packageName,
                /* minRevisionCode = */ 0, /* maxRevisionCode = */ 0, flags, signatures,
                activities.toArray(new String[activities.size()]));
        AppBlockingPackageInfoWrapper wrapper = new AppBlockingPackageInfoWrapper(
                appBlockingInfo, true);
        return wrapper;
    }

    /**
     * Update map of allowlisted packages and activities of the form {pkgName, Allowlisted
     * activities} and set of denylisted packages. The information can come from a configuration XML
     * resource or from the apps marking their activities as distraction optimized.
     */
    private void updateActivityAllowlistAndDenylistMap(String packageName) {
        int userId = mActivityManager.getCurrentUser();
        Log.i(CarLog.TAG_PACKAGE, "Updating allowlist and denylist mapping for package: "
                + packageName + " for UserId: " + userId);
        // Get the apps/activities that are allowlisted in the configuration XML resources.
        Map<String, Set<String>> configAllowlist = generateConfigAllowlist();
        Map<String, Set<String>> configBlocklist = generateConfigBlocklist();

        AppBlockingPackageInfoWrapper wrapper =
                getPackageInfoWrapperForUser(packageName, userId, configAllowlist, configBlocklist);

        if (wrapper == null && userId != UserHandle.USER_SYSTEM) {
            Log.i(CarLog.TAG_PACKAGE, "Updating allowlist and denylist mapping for package: "
                    + packageName + " for UserId: " + UserHandle.USER_SYSTEM);
            // check package for system user, in case package is disabled for current user
            wrapper = getPackageInfoWrapperForUser(packageName, UserHandle.USER_SYSTEM,
                    configAllowlist, configBlocklist);
        }

        synchronized (mLock) {
            if (wrapper != null) {
                Log.i(CarLog.TAG_PACKAGE, "Package: " + packageName + " added in allowlist.");
                mActivityAllowlistMap.put(packageName, wrapper);
            } else {
                Log.i(CarLog.TAG_PACKAGE, "Package: " + packageName + " added in denylist.");
                mActivityDenylistPackages.add(packageName);
            }
        }
    }

    private Map<String, Set<String>> generateConfigAllowlist() {
        synchronized (mLock) {
            if (mConfiguredAllowlistMap != null) return mConfiguredAllowlistMap;

            Map<String, Set<String>> configAllowlist = new HashMap<>();
            mConfiguredAllowlist = mContext.getString(R.string.activityWhitelist);
            if (mConfiguredAllowlist == null) {
                if (DBG_POLICY_CHECK) {
                    Log.w(CarLog.TAG_PACKAGE, "Allowlist is null.");
                }
            }
            parseConfigList(mConfiguredAllowlist, configAllowlist);

            mConfiguredSystemAllowlist = mContext.getString(R.string.systemActivityWhitelist);
            if (mConfiguredSystemAllowlist == null) {
                if (DBG_POLICY_CHECK) {
                    Log.w(CarLog.TAG_PACKAGE, "System allowlist is null.");
                }
            }
            parseConfigList(mConfiguredSystemAllowlist, configAllowlist);

            // Add the blocking overlay activity to the allowlist, since that needs to run in a
            // restricted state to communicate the reason an app was blocked.
            Set<String> defaultActivity = new ArraySet<>();
            if (mActivityBlockingActivity != null) {
                defaultActivity.add(mActivityBlockingActivity.getClassName());
                configAllowlist.put(mActivityBlockingActivity.getPackageName(), defaultActivity);
            }

            mConfiguredAllowlistMap = configAllowlist;
            return configAllowlist;
        }
    }

    private Map<String, Set<String>> generateConfigBlocklist() {
        synchronized (mLock) {
            if (mConfiguredBlocklistMap != null) return mConfiguredBlocklistMap;

            Map<String, Set<String>> configBlocklist = new HashMap<>();
            mConfiguredBlocklist = mContext.getString(R.string.activityBlacklist);
            if (mConfiguredBlocklist == null) {
                if (DBG_POLICY_CHECK) {
                    Log.d(CarLog.TAG_PACKAGE, "Null blocklist in config");
                }
            }
            parseConfigList(mConfiguredBlocklist, configBlocklist);

            mConfiguredBlocklistMap = configBlocklist;
            return configBlocklist;
        }
    }

    private boolean isDebugBuild() {
        return Build.IS_USERDEBUG || Build.IS_ENG;
    }

    /**
     * Parses the given resource and updates the input map of packages and activities.
     *
     * Key is package name and value is list of activities. Empty set implies whole package is
     * included.
     *
     * When there are multiple entries regarding one package, the entry with
     * greater scope wins. Namely if there were 2 entries such that one allowlists
     * an activity, and the other allowlists the entire package of the activity,
     * the package is allowlisted, regardless of input order.
     */
    @VisibleForTesting
    /* package */ void parseConfigList(String configList,
            @NonNull Map<String, Set<String>> packageToActivityMap) {
        if (configList == null) {
            return;
        }
        String[] entries = configList.split(PACKAGE_DELIMITER);
        for (String entry : entries) {
            String[] packageActivityPair = entry.split(PACKAGE_ACTIVITY_DELIMITER);
            Set<String> activities = packageToActivityMap.get(packageActivityPair[0]);
            boolean newPackage = false;
            if (activities == null) {
                activities = new ArraySet<>();
                newPackage = true;
                packageToActivityMap.put(packageActivityPair[0], activities);
            }
            if (packageActivityPair.length == 1) { // whole package
                activities.clear();
            } else if (packageActivityPair.length == 2) {
                // add class name only when the whole package is not allowlisted.
                if (newPackage || (activities.size() > 0)) {
                    activities.add(packageActivityPair[1]);
                }
            }
        }
    }

    @Nullable
    private List<String> getActivitiesInPackage(PackageInfo info) {
        if (info == null || info.activities == null) {
            return null;
        }
        List<String> activityList = new ArrayList<>();
        for (ActivityInfo aInfo : info.activities) {
            activityList.add(aInfo.name);
        }
        return activityList;
    }

    /**
     * Checks if there are any {@link CarAppBlockingPolicyService} and creates a proxy to
     * bind to them and retrieve the {@link CarAppBlockingPolicy}
     */
    @VisibleForTesting
    public void startAppBlockingPolicies() {
        Intent policyIntent = new Intent();
        policyIntent.setAction(CarAppBlockingPolicyService.SERVICE_INTERFACE);
        List<ResolveInfo> policyInfos = mPackageManager.queryIntentServices(policyIntent, 0);
        if (policyInfos == null) { //no need to wait for service binding and retrieval.
            return;
        }
        LinkedList<AppBlockingPolicyProxy> proxies = new LinkedList<>();
        for (ResolveInfo resolveInfo : policyInfos) {
            ServiceInfo serviceInfo = resolveInfo.serviceInfo;
            if (serviceInfo == null) {
                continue;
            }
            if (serviceInfo.isEnabled()) {
                if (mPackageManager.checkPermission(Car.PERMISSION_CONTROL_APP_BLOCKING,
                        serviceInfo.packageName) != PackageManager.PERMISSION_GRANTED) {
                    continue;
                }
                Log.i(CarLog.TAG_PACKAGE, "found policy holding service:" + serviceInfo);
                AppBlockingPolicyProxy proxy = new AppBlockingPolicyProxy(this, mContext,
                        serviceInfo);
                proxy.connect();
                proxies.add(proxy);
            }
        }
        synchronized (mLock) {
            mProxies = proxies;
        }
    }

    public void onPolicyConnectionAndSet(AppBlockingPolicyProxy proxy,
            CarAppBlockingPolicy policy) {
        doHandlePolicyConnection(proxy, policy);
    }

    public void onPolicyConnectionFailure(AppBlockingPolicyProxy proxy) {
        doHandlePolicyConnection(proxy, null);
    }

    private void doHandlePolicyConnection(AppBlockingPolicyProxy proxy,
            CarAppBlockingPolicy policy) {
        synchronized (mLock) {
            if (mProxies == null) {
                proxy.disconnect();
                return;
            }
            mProxies.remove(proxy);
            if (mProxies.size() == 0) {
                mProxies = null;
            }
        }
        try {
            if (policy != null) {
                if (DBG_POLICY_SET) {
                    Log.i(CarLog.TAG_PACKAGE, "policy setting from policy service:"
                            + proxy.getPackageName());
                }
                doSetAppBlockingPolicy(proxy.getPackageName(), policy, 0);
            }
        } finally {
            proxy.disconnect();
        }
    }

    @Override
    public void dump(PrintWriter writer) {
        synchronized (mLock) {
            writer.println("*CarPackageManagerService*");
            writer.println("mEnableActivityBlocking:" + mEnableActivityBlocking);
            List<String> restrictions = new ArrayList<>(mUxRestrictionsListeners.size());
            for (int i = 0; i < mUxRestrictionsListeners.size(); i++) {
                int displayId = mUxRestrictionsListeners.keyAt(i);
                UxRestrictionsListener listener = mUxRestrictionsListeners.valueAt(i);
                restrictions.add(String.format("Display %d is %s",
                        displayId, (listener.isRestricted() ? "restricted" : "unrestricted")));
            }
            writer.println("Display Restrictions:\n" + String.join("\n", restrictions));
            writer.println(" Blocked activity log:");
            writer.println(String.join("\n", mBlockedActivityLogs));
            writer.print(dumpPoliciesLocked(true));
        }
    }

    @GuardedBy("mLock")
    private String dumpPoliciesLocked(boolean dumpAll) {
        StringBuilder sb = new StringBuilder();
        if (dumpAll) {
            sb.append("**System allowlist**\n");
            for (AppBlockingPackageInfoWrapper wrapper : mActivityAllowlistMap.values()) {
                sb.append(wrapper.toString() + "\n");
            }
        }
        sb.append("**Client Policies**\n");
        for (Entry<String, ClientPolicy> entry : mClientPolicies.entrySet()) {
            sb.append("Client:" + entry.getKey() + "\n");
            sb.append("  allowlists:\n");
            for (AppBlockingPackageInfoWrapper wrapper : entry.getValue().mAllowlistsMap.values()) {
                sb.append(wrapper.toString() + "\n");
            }
            sb.append("  blocklists:\n");
            for (AppBlockingPackageInfoWrapper wrapper : entry.getValue().mBlocklistsMap.values()) {
                sb.append(wrapper.toString() + "\n");
            }
        }
        sb.append("**Unprocessed policy services**\n");
        if (mProxies != null) {
            for (AppBlockingPolicyProxy proxy : mProxies) {
                sb.append(proxy.toString() + "\n");
            }
        }
        sb.append("**Allowlist string in resource**\n");
        sb.append(mConfiguredAllowlist + "\n");

        sb.append("**System allowlist string in resource**\n");
        sb.append(mConfiguredSystemAllowlist + "\n");

        sb.append("**Blocklist string in resource**\n");
        sb.append(mConfiguredBlocklist + "\n");

        sb.append("**Allowlist map from resource**\n");
        sb.append(mConfiguredAllowlistMap + "\n");

        sb.append("**Blocklist from resource**\n");
        sb.append(mConfiguredBlocklist + "\n");

        return sb.toString();
    }

    /**
     * Returns display with physical address.
     */
    private List<Display> getPhysicalDisplays() {
        List<Display> displays = new ArrayList<>();
        for (Display display : mDisplayManager.getDisplays()) {
            if (display.getAddress() instanceof DisplayAddress.Physical) {
                displays.add(display);
            }
        }
        return displays;
    }

    /**
     * Returns whether UX restrictions is required for display.
     *
     * Non-physical display will use restrictions for {@link Display#DEFAULT_DISPLAY}.
     */
    private boolean isUxRestrictedOnDisplay(int displayId) {
        UxRestrictionsListener listenerForTopTaskDisplay;
        if (mUxRestrictionsListeners.indexOfKey(displayId) < 0) {
            listenerForTopTaskDisplay = mUxRestrictionsListeners.get(Display.DEFAULT_DISPLAY);
            if (listenerForTopTaskDisplay == null) {
                // This should never happen.
                Log.e(CarLog.TAG_PACKAGE, "Missing listener for default display.");
                return true;
            }
        } else {
            listenerForTopTaskDisplay = mUxRestrictionsListeners.get(displayId);
        }

        return listenerForTopTaskDisplay.isRestricted();
    }

    private void blockTopActivitiesIfNecessary() {
        List<TopTaskInfoContainer> topTasks = mSystemActivityMonitoringService.getTopTasks();
        for (TopTaskInfoContainer topTask : topTasks) {
            if (topTask == null) {
                Log.e(CarLog.TAG_PACKAGE, "Top tasks contains null.");
                continue;
            }
            blockTopActivityIfNecessary(topTask);
        }
    }

    private void blockTopActivityIfNecessary(TopTaskInfoContainer topTask) {
        if (isUxRestrictedOnDisplay(topTask.displayId)) {
            doBlockTopActivityIfNotAllowed(topTask);
        }
    }

    private void doBlockTopActivityIfNotAllowed(TopTaskInfoContainer topTask) {
        if (topTask.topActivity == null) {
            return;
        }

        boolean allowed = isActivityDistractionOptimized(
                topTask.topActivity.getPackageName(),
                topTask.topActivity.getClassName());
        if (DBG_POLICY_ENFORCEMENT) {
            Log.i(CarLog.TAG_PACKAGE, "new activity:" + topTask.toString() + " allowed:"
                    + allowed);
        }
        if (allowed) {
            return;
        }
        synchronized (mLock) {
            if (!mEnableActivityBlocking) {
                Log.d(CarLog.TAG_PACKAGE, "Current activity " + topTask.topActivity
                        + " not allowed, blocking disabled. Number of tasks in stack:"
                        + topTask.stackInfo.taskIds.length);
                return;
            }
        }
        if (DBG_POLICY_ENFORCEMENT) {
            Log.i(CarLog.TAG_PACKAGE, "Current activity " + topTask.topActivity
                    + " not allowed, will block, number of tasks in stack:"
                    + topTask.stackInfo.taskIds.length);
        }

        // Figure out the root activity of blocked task.
        String taskRootActivity = null;
        for (int i = 0; i < topTask.stackInfo.taskIds.length; i++) {
            // topTask.taskId is the task that should be blocked.
            if (topTask.stackInfo.taskIds[i] == topTask.taskId) {
                // stackInfo represents an ActivityStack. Its fields taskIds and taskNames
                // are 1:1 mapped, where taskNames is the name of root activity in this task.
                taskRootActivity = topTask.stackInfo.taskNames[i];
                break;
            }
        }

        boolean isRootDO = false;
        if (taskRootActivity != null) {
            ComponentName componentName = ComponentName.unflattenFromString(taskRootActivity);
            isRootDO = isActivityDistractionOptimized(
                    componentName.getPackageName(), componentName.getClassName());
        }

        Intent newActivityIntent = createBlockingActivityIntent(
                mActivityBlockingActivity, topTask.displayId,
                topTask.topActivity.flattenToShortString(), topTask.taskId, taskRootActivity,
                isRootDO);

        // Intent contains all info to debug what is blocked - log into both logcat and dumpsys.
        String log = "Starting blocking activity with intent: " + newActivityIntent.toUri(0);
        if (Log.isLoggable(CarLog.TAG_PACKAGE, Log.INFO)) {
            Log.i(CarLog.TAG_PACKAGE, log);
        }
        addLog(log);
        mSystemActivityMonitoringService.blockActivity(topTask, newActivityIntent);
    }

    /**
     * Creates an intent to start blocking activity.
     *
     * @param blockingActivity the activity to launch
     * @param blockedActivity  the activity being blocked
     * @param blockedTaskId    the blocked task id, which contains the blocked activity
     * @param taskRootActivity root activity of the blocked task
     * @return an intent to launch the blocking activity.
     */
    private static Intent createBlockingActivityIntent(ComponentName blockingActivity,
            int displayId, String blockedActivity, int blockedTaskId, String taskRootActivity,
            boolean isRootDo) {
        Intent newActivityIntent = new Intent();
        newActivityIntent.setFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        newActivityIntent.setComponent(blockingActivity);
        newActivityIntent.putExtra(
                BLOCKING_INTENT_EXTRA_DISPLAY_ID, displayId);
        newActivityIntent.putExtra(
                BLOCKING_INTENT_EXTRA_BLOCKED_ACTIVITY_NAME, blockedActivity);
        newActivityIntent.putExtra(
                BLOCKING_INTENT_EXTRA_BLOCKED_TASK_ID, blockedTaskId);
        newActivityIntent.putExtra(
                BLOCKING_INTENT_EXTRA_ROOT_ACTIVITY_NAME, taskRootActivity);
        newActivityIntent.putExtra(
                BLOCKING_INTENT_EXTRA_IS_ROOT_ACTIVITY_DO, isRootDo);

        return newActivityIntent;
    }

    /**
     * Enable/Disable activity blocking by correspondingly enabling/disabling broadcasting UXR
     * changes in {@link CarUxRestrictionsManagerService}. This is only available in
     * engineering builds for development convenience.
     */
    @Override
    public void setEnableActivityBlocking(boolean enable) {
        if (!isDebugBuild()) {
            Log.e(CarLog.TAG_PACKAGE, "Cannot enable/disable activity blocking");
            return;
        }

        // Check if the caller has the same signature as that of the car service.
        if (mPackageManager.checkSignatures(Process.myUid(), Binder.getCallingUid())
                != PackageManager.SIGNATURE_MATCH) {
            throw new SecurityException(
                    "Caller " + mPackageManager.getNameForUid(Binder.getCallingUid())
                    + " does not have the right signature");
        }
        mCarUxRestrictionsService.setUxRChangeBroadcastEnabled(enable);
    }

    /**
     * Get the distraction optimized activities for the given package.
     *
     * @param pkgName Name of the package
     * @return Array of the distraction optimized activities in the package
     */
    @Nullable
    public String[] getDistractionOptimizedActivities(String pkgName) {
        try {
            return CarAppMetadataReader.findDistractionOptimizedActivitiesAsUser(mContext, pkgName,
                    mActivityManager.getCurrentUser());
        } catch (NameNotFoundException e) {
            return null;
        }
    }

    /**
     * Append one line of log for dumpsys.
     *
     * <p>Maintains the size of log by {@link #LOG_SIZE} and appends tag and timestamp to the line.
     */
    private void addLog(String log) {
        while (mBlockedActivityLogs.size() >= LOG_SIZE) {
            mBlockedActivityLogs.remove();
        }
        StringBuffer sb = new StringBuffer()
                .append(CarLog.TAG_PACKAGE).append(':')
                .append(DateFormat.format(
                        "MM-dd HH:mm:ss", System.currentTimeMillis())).append(": ")
                .append(log);
        mBlockedActivityLogs.add(sb.toString());
    }

    /**
     * Reading policy and setting policy can take time. Run it in a separate handler thread.
     */
    private static final class PackageHandler extends Handler {
        private static final String TAG = PackageHandler.class.getSimpleName();

        private static final int MSG_INIT = 0;
        private static final int MSG_PARSE_PKG = 1;
        private static final int MSG_UPDATE_POLICY = 2;
        private static final int MSG_RELEASE = 3;

        private final WeakReference<CarPackageManagerService> mService;

        private PackageHandler(Looper looper, CarPackageManagerService service) {
            super(looper);
            mService = new WeakReference<CarPackageManagerService>(service);
        }

        private void requestInit() {
            Message msg = obtainMessage(MSG_INIT);
            sendMessage(msg);
        }

        private void requestRelease() {
            removeMessages(MSG_UPDATE_POLICY);
            Message msg = obtainMessage(MSG_RELEASE);
            sendMessage(msg);
        }

        private void requestUpdatingPolicy(String packageName, CarAppBlockingPolicy policy,
                int flags) {
            Pair<String, CarAppBlockingPolicy> pair = new Pair<>(packageName, policy);
            Message msg = obtainMessage(MSG_UPDATE_POLICY, flags, 0, pair);
            sendMessage(msg);
        }

        private void requestParsingInstalledPkg(String packageName) {
            Message msg = obtainMessage(MSG_PARSE_PKG, packageName);
            sendMessage(msg);
        }

        @Override
        public void handleMessage(Message msg) {
            CarPackageManagerService service = mService.get();
            if (service == null) {
                Log.i(TAG, "handleMessage null service");
                return;
            }
            switch (msg.what) {
                case MSG_INIT:
                    service.doHandleInit();
                    break;
                case MSG_PARSE_PKG:
                    service.doParseInstalledPackage((String) msg.obj);
                    break;
                case MSG_UPDATE_POLICY:
                    Pair<String, CarAppBlockingPolicy> pair =
                            (Pair<String, CarAppBlockingPolicy>) msg.obj;
                    service.doUpdatePolicy(pair.first, pair.second, msg.arg1);
                    break;
                case MSG_RELEASE:
                    service.doHandleRelease();
                    break;
            }
        }
    }

    private static class AppBlockingPackageInfoWrapper {
        private final AppBlockingPackageInfo info;
        /**
         * Whether the current info is matching with the target package in system. Mismatch can
         * happen for version out of range or signature mismatch.
         */
        private boolean isMatching;

        private AppBlockingPackageInfoWrapper(AppBlockingPackageInfo info, boolean isMatching) {
            this.info = info;
            this.isMatching = isMatching;
        }

        @Override
        public String toString() {
            return "AppBlockingPackageInfoWrapper [info=" + info + ", isMatching=" + isMatching +
                    "]";
        }
    }

    /**
     * Client policy holder per each client. Should be accessed with CarpackageManagerService.this
     * held.
     */
    private static class ClientPolicy {
        private final HashMap<String, AppBlockingPackageInfoWrapper> mAllowlistsMap =
                new HashMap<>();
        private final HashMap<String, AppBlockingPackageInfoWrapper> mBlocklistsMap =
                new HashMap<>();

        private void replaceAllowlists(AppBlockingPackageInfoWrapper[] allowlists) {
            mAllowlistsMap.clear();
            addToAllowlists(allowlists);
        }

        private void addToAllowlists(AppBlockingPackageInfoWrapper[] allowlists) {
            if (allowlists == null) {
                return;
            }
            for (AppBlockingPackageInfoWrapper wrapper : allowlists) {
                if (wrapper != null) {
                    mAllowlistsMap.put(wrapper.info.packageName, wrapper);
                }
            }
        }

        private void removeAllowlists(AppBlockingPackageInfoWrapper[] allowlists) {
            if (allowlists == null) {
                return;
            }
            for (AppBlockingPackageInfoWrapper wrapper : allowlists) {
                if (wrapper != null) {
                    mAllowlistsMap.remove(wrapper.info.packageName);
                }
            }
        }

        private void replaceBlocklists(AppBlockingPackageInfoWrapper[] blocklists) {
            mBlocklistsMap.clear();
            addToBlocklists(blocklists);
        }

        private void addToBlocklists(AppBlockingPackageInfoWrapper[] blocklists) {
            if (blocklists == null) {
                return;
            }
            for (AppBlockingPackageInfoWrapper wrapper : blocklists) {
                if (wrapper != null) {
                    mBlocklistsMap.put(wrapper.info.packageName, wrapper);
                }
            }
        }

        private void removeBlocklists(AppBlockingPackageInfoWrapper[] blocklists) {
            if (blocklists == null) {
                return;
            }
            for (AppBlockingPackageInfoWrapper wrapper : blocklists) {
                if (wrapper != null) {
                    mBlocklistsMap.remove(wrapper.info.packageName);
                }
            }
        }
    }

    private class ActivityLaunchListener
            implements SystemActivityMonitoringService.ActivityLaunchListener {
        @Override
        public void onActivityLaunch(TopTaskInfoContainer topTask) {
            if (topTask == null) {
                Log.e(CarLog.TAG_PACKAGE, "Received callback with null top task.");
                return;
            }
            blockTopActivityIfNecessary(topTask);
        }
    }

    /**
     * Listens to the UX restrictions from {@link CarUxRestrictionsManagerService} and initiates
     * checking if the foreground Activity should be blocked.
     */
    private class UxRestrictionsListener extends ICarUxRestrictionsChangeListener.Stub {
        @GuardedBy("mLock")
        @Nullable
        private CarUxRestrictions mCurrentUxRestrictions;
        private final CarUxRestrictionsManagerService uxRestrictionsService;

        public UxRestrictionsListener(CarUxRestrictionsManagerService service) {
            uxRestrictionsService = service;
        }

        @Override
        public void onUxRestrictionsChanged(CarUxRestrictions restrictions) {
            if (DBG_POLICY_ENFORCEMENT) {
                Log.d(CarLog.TAG_PACKAGE, "Received uxr restrictions: "
                        + restrictions.isRequiresDistractionOptimization()
                        + " : " + restrictions.getActiveRestrictions());
            }

            synchronized (mLock) {
                mCurrentUxRestrictions = new CarUxRestrictions(restrictions);
            }
            checkIfTopActivityNeedsBlocking();
        }

        private void checkIfTopActivityNeedsBlocking() {
            boolean shouldCheck = false;
            synchronized (mLock) {
                if (mCurrentUxRestrictions != null
                        && mCurrentUxRestrictions.isRequiresDistractionOptimization()) {
                    shouldCheck = true;
                }
            }
            if (DBG_POLICY_ENFORCEMENT) {
                Log.d(CarLog.TAG_PACKAGE, "Should check top tasks?: " + shouldCheck);
            }
            if (shouldCheck) {
                // Loop over all top tasks to ensure tasks on virtual display can also be blocked.
                blockTopActivitiesIfNecessary();
            }
        }

        private boolean isRestricted() {
            // if current restrictions is null, try querying the service, once.
            synchronized (mLock) {
                if (mCurrentUxRestrictions == null) {
                    mCurrentUxRestrictions = uxRestrictionsService.getCurrentUxRestrictions();
                }
                if (mCurrentUxRestrictions != null) {
                    return mCurrentUxRestrictions.isRequiresDistractionOptimization();
                }
            }

            // If restriction information is still not available (could happen during bootup),
            // return not restricted.  This maintains parity with previous implementation but needs
            // a revisit as we test more.
            return false;
        }
    }

    /**
     * Listens to the package install/uninstall events to know when to initiate parsing
     * installed packages.
     */
    private class PackageParsingEventReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent == null || intent.getAction() == null) {
                return;
            }
            if (DBG_POLICY_CHECK) {
                Log.d(CarLog.TAG_PACKAGE,
                        "PackageParsingEventReceiver Received " + intent.getAction());
            }
            String action = intent.getAction();
            if (isPackageManagerAction(action)) {
                // send a delayed message so if we received multiple related intents, we parse
                // only once.
                logEventChange(intent);
                String packageName = getPackageName(intent);
                mHandler.requestParsingInstalledPkg(packageName);
            }
        }

        private String getPackageName(Intent intent) {
            // For mPackageManagerActions, data should contain package name.
            String dataString = intent.getDataString();
            if (dataString == null) return null;

            String scheme = intent.getScheme();
            if (!scheme.equals("package")) return null;

            String[] splitData = intent.getDataString().split(":");
            if (splitData.length < 2) return null;

            return splitData[1];
        }

        private boolean isPackageManagerAction(String action) {
            return mPackageManagerActions.contains(action);
        }

        /**
         * Convenience log function to log what changed.  Logs only when more debug logs
         * are needed - DBG_POLICY_CHECK needs to be true
         */
        private void logEventChange(Intent intent) {
            if (!DBG_POLICY_CHECK || intent == null) {
                return;
            }

            String packageName = intent.getData().getSchemeSpecificPart();
            Log.d(CarLog.TAG_PACKAGE, "Pkg Changed:" + packageName);
            String action = intent.getAction();
            if (action == null) {
                return;
            }
            if (action.equals(Intent.ACTION_PACKAGE_CHANGED)) {
                Log.d(CarLog.TAG_PACKAGE, "Changed components");
                String[] cc = intent.getStringArrayExtra(Intent.EXTRA_CHANGED_COMPONENT_NAME_LIST);
                if (cc != null) {
                    for (String c : cc) {
                        Log.d(CarLog.TAG_PACKAGE, c);
                    }
                }
            } else if (action.equals(Intent.ACTION_PACKAGE_REMOVED)
                    || action.equals(Intent.ACTION_PACKAGE_ADDED)) {
                Log.d(CarLog.TAG_PACKAGE, action + " Replacing?: " + intent.getBooleanExtra(
                        Intent.EXTRA_REPLACING, false));
            }
        }
    }
}
