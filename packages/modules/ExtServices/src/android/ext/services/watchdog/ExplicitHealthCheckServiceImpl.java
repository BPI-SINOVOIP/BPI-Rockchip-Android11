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

package android.ext.services.watchdog;

import static android.service.watchdog.ExplicitHealthCheckService.PackageConfig;

import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.provider.DeviceConfig;
import android.service.watchdog.ExplicitHealthCheckService;
import android.util.Log;

import androidx.annotation.VisibleForTesting;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Routes explicit health check requests to the appropriate {@link ExplicitHealthChecker}.
 */
public final class ExplicitHealthCheckServiceImpl extends ExplicitHealthCheckService {
    private static final String TAG = "ExplicitHealthCheckServiceImpl";
    // TODO: Add build dependency on NetworkStack stable AIDL so we can stop hard coding class name
    private static final String NETWORK_STACK_CONNECTOR_CLASS =
            "android.net.INetworkStackConnector";
    public static final String PROPERTY_WATCHDOG_REQUEST_TIMEOUT_MILLIS =
            "watchdog_request_timeout_millis";
    // TODO(b/153701690): Use TimeUnit class to get time information instead of using constant.
    public static final long DEFAULT_REQUEST_TIMEOUT_MILLIS = 24 * 60 * 60 * 1000; // 1 day
    // Modified only #onCreate, using concurrent collection to ensure thread visibility
    @VisibleForTesting
    final Map<String, ExplicitHealthChecker> mSupportedCheckers = new ConcurrentHashMap<>();

    @Override
    public void onCreate() {
        super.onCreate();
        initHealthCheckers();
    }

    @Override
    public void onRequestHealthCheck(String packageName) {
        ExplicitHealthChecker checker = mSupportedCheckers.get(packageName);
        if (checker != null) {
            checker.request();
        } else {
            Log.w(TAG, "Ignoring request for explicit health check for unsupported package "
                    + packageName);
        }
    }

    @Override
    public void onCancelHealthCheck(String packageName) {
        ExplicitHealthChecker checker = mSupportedCheckers.get(packageName);
        if (checker != null) {
            checker.cancel();
        } else {
            Log.w(TAG, "Ignoring request to cancel explicit health check for unsupported package "
                    + packageName);
        }
    }

    @Override
    public List<PackageConfig> onGetSupportedPackages() {
        List<PackageConfig> packages = new ArrayList<>();
        long requestTimeoutMillis = DeviceConfig.getLong(
                DeviceConfig.NAMESPACE_ROLLBACK,
                PROPERTY_WATCHDOG_REQUEST_TIMEOUT_MILLIS,
                DEFAULT_REQUEST_TIMEOUT_MILLIS);
        if (requestTimeoutMillis <= 0) {
            requestTimeoutMillis = DEFAULT_REQUEST_TIMEOUT_MILLIS;
        }
        for (ExplicitHealthChecker checker : mSupportedCheckers.values()) {
            PackageConfig pkg = new PackageConfig(checker.getSupportedPackageName(),
                    requestTimeoutMillis);
            packages.add(pkg);
        }
        return packages;
    }

    @Override
    public List<String> onGetRequestedPackages() {
        List<String> packages = new ArrayList<>();
        Iterator<ExplicitHealthChecker> it = mSupportedCheckers.values().iterator();
        // Could potentially race, where we read a checker#isPending and it changes before we
        // return list. However, if it races and it is in the list, the caller might call #cancel
        // which would fail, but that is fine. If it races and it ends up *not* in the list, it was
        // already cancelled, so there's no need for the caller to cancel it
        while (it.hasNext()) {
            ExplicitHealthChecker checker = it.next();
            if (checker.isPending()) {
                packages.add(checker.getSupportedPackageName());
            }
        }
        return packages;
    }

    private void initHealthCheckers() {
        ComponentName comp = resolveSystemService(new Intent(NETWORK_STACK_CONNECTOR_CLASS),
                getPackageManager(), 0);
        if (comp != null) {
            String networkStackPackageName = comp.getPackageName();
            mSupportedCheckers.put(networkStackPackageName,
                    new NetworkChecker(this, networkStackPackageName));
        } else {
            // On Go devices, or any device that does not ship the network stack module.
            // The network stack will live in system_server process, so no need to monitor.
            Log.i(TAG, "Network stack module not found");
        }
    }

    private ComponentName resolveSystemService(Intent intent, PackageManager pm, int flags) {
        List<ResolveInfo> results =  pm.queryIntentServices(intent, flags);
        if (results == null) {
            return null;
        }
        ComponentName comp = null;
        for (int i=0; i<results.size(); i++) {
            ResolveInfo ri = results.get(i);
            if ((ri.serviceInfo.applicationInfo.flags & ApplicationInfo.FLAG_SYSTEM) == 0) {
                continue;
            }
            ComponentName foundComp = new ComponentName(ri.serviceInfo.applicationInfo.packageName,
                    ri.serviceInfo.name);
            if (comp != null) {
                throw new IllegalStateException("Multiple system services handle " + this
                        + ": " + comp + ", " + foundComp);
            }
            comp = foundComp;
        }
        return comp;
    }
}
