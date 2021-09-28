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

package com.android.experimentalcar;

import android.annotation.MainThread;
import android.annotation.Nullable;
import android.car.IExperimentalCar;
import android.car.IExperimentalCarHelper;
import android.car.experimental.CarDriverDistractionManager;
import android.car.experimental.CarTestDemoExperimentalFeatureManager;
import android.car.experimental.ExperimentalCar;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Binder;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Process;
import android.os.RemoteException;
import android.util.Log;

import com.android.car.CarServiceBase;
import com.android.internal.annotations.GuardedBy;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Implements IExperimentalCar for experimental features.
 */
public final class IExperimentalCarImpl extends IExperimentalCar.Stub {

    private static final String TAG = "CAR.EXPIMPL";

    private static final List<String> ALL_AVAILABLE_FEATURES = Arrays.asList(
            ExperimentalCar.TEST_EXPERIMENTAL_FEATURE_SERVICE,
            ExperimentalCar.DRIVER_DISTRACTION_EXPERIMENTAL_FEATURE_SERVICE
    );

    private final Context mContext;

    private final Handler mMainThreadHandler = new Handler(Looper.getMainLooper());

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private boolean mReleased;

    @GuardedBy("mLock")
    private IExperimentalCarHelper mHelper;

    @GuardedBy("mLock")
    private ArrayList<CarServiceBase> mRunningServices = new ArrayList<>();

    public IExperimentalCarImpl(Context context) {
        mContext = context;
    }

    @Override
    public void init(IExperimentalCarHelper helper, List<String> enabledFeatures) {
        // From car service or unit testing only
        assertCallingFromSystemProcessOrSelf();

        // dispatch to main thread as release is always done in main.
        mMainThreadHandler.post(() -> {
            synchronized (mLock) {
                if (mReleased) {
                    Log.w(TAG, "init binder call after onDestroy, will ignore");
                    return;
                }
            }
            ArrayList<CarServiceBase> services = new ArrayList<>();
            ArrayList<String> startedFeatures = new ArrayList<>();
            ArrayList<String> classNames = new ArrayList<>();
            ArrayList<IBinder> binders = new ArrayList<>();

            // This cannot address inter-dependency. That requires re-ordering this in dependency
            // order.
            // That should be done when we find such needs. For now, each feature inside here should
            // not have inter-dependency as they are all optional.
            for (String feature : enabledFeatures) {
                CarServiceBase service = constructServiceForFeature(feature);
                if (service == null) {
                    Log.e(TAG, "Failed to construct requested feature:" + feature);
                    continue;
                }
                service.init();
                services.add(service);
                startedFeatures.add(feature);
                // If it is not IBinder, then it is internal feature.
                if (service instanceof IBinder) {
                    binders.add((IBinder) service);
                } else {
                    binders.add(null);
                }
                classNames.add(getClassNameForFeature(feature));
            }
            try {
                helper.onInitComplete(ALL_AVAILABLE_FEATURES, startedFeatures, classNames, binders);
            } catch (RemoteException e) {
                Log.w(TAG, "Car service crashed?", e);
                // will be destroyed soon. Just continue and register services for possible cleanup.
            }
            synchronized (mLock) {
                mHelper = helper;
                mRunningServices.addAll(services);
            }
        });
    }

    // should be called in Service.onDestroy
    @MainThread
    void release() {
        // Copy to handle call release without lock
        ArrayList<CarServiceBase> services;
        synchronized (mLock) {
            if (mReleased) {
                return;
            }
            mReleased = true;
            services = new ArrayList<>(mRunningServices);
            mRunningServices.clear();
        }
        for (CarServiceBase service : services) {
            service.release();
        }
    }

    /** dump */
    public void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
        ArrayList<CarServiceBase> services;
        synchronized (mLock) {
            writer.println("mReleased:" + mReleased);
            writer.println("ALL_AVAILABLE_FEATURES:" + ALL_AVAILABLE_FEATURES);
            services = new ArrayList<>(mRunningServices);
        }
        writer.println(" Number of running services:" + services.size());
        int i = 0;
        for (CarServiceBase service : services) {
            writer.print(i + ":");
            service.dump(writer);
            i++;
        }
    }

    @Nullable
    private String getClassNameForFeature(String featureName) {
        switch (featureName) {
            case ExperimentalCar.TEST_EXPERIMENTAL_FEATURE_SERVICE:
                return CarTestDemoExperimentalFeatureManager.class.getName();
            case ExperimentalCar.DRIVER_DISTRACTION_EXPERIMENTAL_FEATURE_SERVICE:
                return CarDriverDistractionManager.class.getName();
            default:
                return null;
        }
    }

    @Nullable
    private CarServiceBase constructServiceForFeature(String featureName) {
        switch (featureName) {
            case ExperimentalCar.TEST_EXPERIMENTAL_FEATURE_SERVICE:
                return new TestDemoExperimentalFeatureService();
            case ExperimentalCar.DRIVER_DISTRACTION_EXPERIMENTAL_FEATURE_SERVICE:
                return new DriverDistractionExperimentalFeatureService(mContext);
            default:
                return null;
        }
    }

    private static void assertCallingFromSystemProcessOrSelf() {
        int uid = Binder.getCallingUid();
        int pid = Binder.getCallingPid();
        if (uid != Process.SYSTEM_UID && pid != Process.myPid()) {
            throw new SecurityException("Only allowed from system or self, uid:" + uid
                    + " pid:" + pid);
        }
    }

    /**
     * Assert that a permission has been granted for the current context.
     */
    public static void assertPermission(Context context, String permission) {
        if (context.checkCallingOrSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
            throw new SecurityException("requires " + permission);
        }
    }
}
