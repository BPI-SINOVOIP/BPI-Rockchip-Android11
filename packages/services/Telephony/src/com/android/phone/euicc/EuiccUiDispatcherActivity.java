/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.phone.euicc;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Bundle;
import android.os.UserHandle;
import android.permission.PermissionManager;
import android.service.euicc.EuiccService;
import android.telephony.euicc.EuiccManager;
import android.util.Log;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.telephony.euicc.EuiccConnector;
import com.android.internal.telephony.util.TelephonyUtils;

import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/** Trampoline activity to forward eUICC intents from apps to the active UI implementation. */
public class EuiccUiDispatcherActivity extends Activity {
    private static final String TAG = "EuiccUiDispatcher";
    private static final long CHANGE_PERMISSION_TIMEOUT_MS = 15 * 1000; // 15 seconds

    /** Flags to use when querying PackageManager for Euicc component implementations. */
    private static final int EUICC_QUERY_FLAGS =
            PackageManager.MATCH_SYSTEM_ONLY | PackageManager.MATCH_DEBUG_TRIAGED_MISSING
                    | PackageManager.GET_RESOLVED_FILTER;

    private PermissionManager mPermissionManager;
    private boolean mGrantPermissionDone = false;
    private ThreadPoolExecutor mExecutor;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mPermissionManager = (PermissionManager) getSystemService(Context.PERMISSION_SERVICE);
        mExecutor = new ThreadPoolExecutor(
                1 /* corePoolSize */,
                1 /* maxPoolSize */,
                15, TimeUnit.SECONDS, /* keepAliveTime */
                new LinkedBlockingQueue<>(), /* workQueue */
                new ThreadFactory() {
                    private final AtomicInteger mCount = new AtomicInteger(1);

                    @Override
                    public Thread newThread(Runnable r) {
                        return new Thread(r, "EuiccService #" + mCount.getAndIncrement());
                    }
                }
        );
        try {
            Intent euiccUiIntent = resolveEuiccUiIntent();
            if (euiccUiIntent == null) {
                setResult(RESULT_CANCELED);
                onDispatchFailure();
                return;
            }

            euiccUiIntent.setFlags(Intent.FLAG_ACTIVITY_FORWARD_RESULT);
            startActivity(euiccUiIntent);
        } finally {
            // Since we're using Theme.NO_DISPLAY, we must always finish() at the end of onCreate().
            finish();
        }
    }

    @VisibleForTesting
    @Nullable
    Intent resolveEuiccUiIntent() {
        EuiccManager euiccManager = (EuiccManager) getSystemService(Context.EUICC_SERVICE);
        if (!euiccManager.isEnabled()) {
            Log.w(TAG, "eUICC not enabled");
            return null;
        }

        Intent euiccUiIntent = getEuiccUiIntent();
        if (euiccUiIntent == null) {
            Log.w(TAG, "Unable to handle intent");
            return null;
        }

        revokePermissionFromLuiApps(euiccUiIntent);

        ActivityInfo activityInfo = findBestActivity(euiccUiIntent);
        if (activityInfo == null) {
            Log.w(TAG, "Could not resolve activity for intent: " + euiccUiIntent);
            return null;
        }

        grantDefaultPermissionsToLuiApp(activityInfo);

        euiccUiIntent.setComponent(new ComponentName(activityInfo.packageName, activityInfo.name));
        return euiccUiIntent;
    }

    /** Called when dispatch fails. May be overridden to perform some operation here. */
    protected void onDispatchFailure() {
    }

    /**
     * Return an Intent to start the Euicc app's UI for the given intent, or null if given intent
     * cannot be handled.
     */
    @Nullable
    protected Intent getEuiccUiIntent() {
        String action = getIntent().getAction();

        Intent intent = new Intent();
        intent.putExtras(getIntent());
        switch (action) {
            case EuiccManager.ACTION_MANAGE_EMBEDDED_SUBSCRIPTIONS:
                intent.setAction(EuiccService.ACTION_MANAGE_EMBEDDED_SUBSCRIPTIONS);
                break;
            case EuiccManager.ACTION_PROVISION_EMBEDDED_SUBSCRIPTION:
                intent.setAction(EuiccService.ACTION_PROVISION_EMBEDDED_SUBSCRIPTION);
                break;
            default:
                Log.w(TAG, "Unsupported action: " + action);
                return null;
        }

        return intent;
    }

    @Override
    protected void onDestroy() {
        mExecutor.shutdownNow();
        super.onDestroy();
    }

    @VisibleForTesting
    @Nullable
    ActivityInfo findBestActivity(Intent euiccUiIntent) {
        return EuiccConnector.findBestActivity(getPackageManager(), euiccUiIntent);
    }

    /** Grants default permissions to the active LUI app. */
    @VisibleForTesting
    protected void grantDefaultPermissionsToLuiApp(ActivityInfo activityInfo) {
        CountDownLatch latch = new CountDownLatch(1);
        try {
            mPermissionManager.grantDefaultPermissionsToLuiApp(
                    activityInfo.packageName, UserHandle.of(UserHandle.myUserId()), mExecutor,
                    isSuccess -> {
                        if (isSuccess) {
                            latch.countDown();
                        } else {
                            Log.e(TAG, "Failed to revoke LUI app permissions.");
                        }
                    });
            TelephonyUtils.waitUntilReady(latch, CHANGE_PERMISSION_TIMEOUT_MS);
        } catch (RuntimeException e) {
            Log.e(TAG, "Failed to grant permissions to active LUI app.", e);
        }
    }

    /** Cleans up all the packages that shouldn't have permission. */
    @VisibleForTesting
    protected void revokePermissionFromLuiApps(Intent intent) {
        CountDownLatch latch = new CountDownLatch(1);
        try {
            Set<String> luiApps = getAllLuiAppPackageNames(intent);
            String[] luiAppsArray = luiApps.toArray(new String[luiApps.size()]);
            mPermissionManager.revokeDefaultPermissionsFromLuiApps(luiAppsArray,
                    UserHandle.of(UserHandle.myUserId()), mExecutor, isSuccess -> {
                        if (isSuccess) {
                            latch.countDown();
                        } else {
                            Log.e(TAG, "Failed to revoke LUI app permissions.");
                        }
                    });
            TelephonyUtils.waitUntilReady(latch, CHANGE_PERMISSION_TIMEOUT_MS);
        } catch (RuntimeException e) {
            Log.e(TAG, "Failed to revoke LUI app permissions.");
            throw e;
        }
    }

    @NonNull
    private Set<String> getAllLuiAppPackageNames(Intent intent) {
        List<ResolveInfo> luiPackages =
                getPackageManager().queryIntentServices(intent, EUICC_QUERY_FLAGS);
        HashSet<String> packageNames = new HashSet<>();
        for (ResolveInfo info : luiPackages) {
            if (info.serviceInfo == null) continue;
            packageNames.add(info.serviceInfo.packageName);
        }
        return packageNames;
    }
}
