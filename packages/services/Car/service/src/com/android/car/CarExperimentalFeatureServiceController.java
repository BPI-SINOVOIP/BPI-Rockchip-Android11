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

package com.android.car;

import android.car.IExperimentalCar;
import android.car.IExperimentalCarHelper;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.Process;
import android.os.RemoteException;
import android.os.UserHandle;
import android.util.ArrayMap;
import android.util.Log;

import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

/**
 * Controls binding to ExperimentalCarService and interfaces for experimental features.
 */
public final class CarExperimentalFeatureServiceController implements CarServiceBase {

    private static final String TAG = "CAR.EXPERIMENTAL";

    private final Context mContext;

    private final ServiceConnection mServiceConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            IExperimentalCar experimentalCar;
            synchronized (mLock) {
                experimentalCar = IExperimentalCar.Stub.asInterface(service);
                mExperimentalCar = experimentalCar;
            }
            if (experimentalCar == null) {
                Log.e(TAG, "Experimental car returned null binder");
                return;
            }
            CarFeatureController featureController = CarLocalServices.getService(
                    CarFeatureController.class);
            List<String> enabledExperimentalFeatures =
                    featureController.getEnabledExperimentalFeatures();
            try {
                experimentalCar.init(mHelper, enabledExperimentalFeatures);
            } catch (RemoteException e) {
                Log.e(TAG, "Experimental car service crashed", e);
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            resetFeatures();
        }
    };

    private final IExperimentalCarHelper mHelper = new IExperimentalCarHelper.Stub() {
        @Override
        public void onInitComplete(List<String> allAvailableFeatures, List<String> startedFeatures,
                List<String> classNames, List<IBinder> binders) {
            if (allAvailableFeatures == null) {
                Log.e(TAG, "Experimental car passed null allAvailableFeatures");
                return;
            }
            if (startedFeatures == null || classNames == null || binders == null) {
                Log.i(TAG, "Nothing enabled in Experimental car");
                return;
            }
            int sizeOfStartedFeatures = startedFeatures.size();
            if (sizeOfStartedFeatures != classNames.size()
                    || sizeOfStartedFeatures != binders.size()) {
                Log.e(TAG,
                        "Experimental car passed wrong lists of enabled features, startedFeatures:"
                        + startedFeatures + " classNames:" + classNames + " binders:" + binders);
            }
            // Do conversion to make indexed accesses
            ArrayList<String> classNamesInArray = new ArrayList<>(classNames);
            ArrayList<IBinder> bindersInArray = new ArrayList<>(binders);
            synchronized (mLock) {
                for (int i = 0; i < startedFeatures.size(); i++) {
                    mEnabledFeatures.put(startedFeatures.get(i),
                            new FeatureInfo(classNamesInArray.get(i),
                                    bindersInArray.get(i)));
                }
            }
            CarFeatureController featureController = CarLocalServices.getService(
                    CarFeatureController.class);
            featureController.setAvailableExperimentalFeatureList(allAvailableFeatures);
            Log.i(TAG, "Available experimental features:" + allAvailableFeatures);
            Log.i(TAG, "Started experimental features:" + startedFeatures);
        }
    };

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private IExperimentalCar mExperimentalCar;

    @GuardedBy("mLock")
    private final ArrayMap<String, FeatureInfo> mEnabledFeatures = new ArrayMap<>();

    @GuardedBy("mLock")
    private boolean mBound;

    private static class FeatureInfo {
        public final String className;
        public final IBinder binder;

        FeatureInfo(String className, IBinder binder) {
            this.className = className;
            this.binder = binder;
        }
    }

    public CarExperimentalFeatureServiceController(Context context) {
        mContext = context;
    }

    @Override
    public void init() {
        // Do binding only for real car servie
        Intent intent = new Intent();
        intent.setComponent(new ComponentName("com.android.experimentalcar",
                "com.android.experimentalcar.ExperimentalCarService"));
        boolean bound = bindService(intent);
        if (!bound) {
            Log.e(TAG, "Cannot bind to experimental car service, intent:" + intent);
        }
        synchronized (mLock) {
            mBound = bound;
        }
    }

    /**
     * Bind service. Separated for testing.
     * Test will override this. Default behavior will not bind if it is not real run (=system uid).
     */
    @VisibleForTesting
    public boolean bindService(Intent intent) {
        int myUid = Process.myUid();
        if (myUid != Process.SYSTEM_UID) {
            Log.w(TAG, "Binding experimental service skipped as this may be test env, uid:"
                    + myUid);
            return false;
        }
        try {
            return mContext.bindServiceAsUser(intent, mServiceConnection,
                    Context.BIND_AUTO_CREATE, UserHandle.SYSTEM);
        } catch (Exception e) {
            // Do not crash car service for case like package not found and etc.
            Log.e(TAG, "Cannot bind to experimental car service", e);
            return false;
        }
    }

    @Override
    public void release() {
        synchronized (mLock) {
            if (mBound) {
                mContext.unbindService(mServiceConnection);
            }
            mBound = false;
            resetFeatures();
        }
    }

    @Override
    public void dump(PrintWriter writer) {
        writer.println("*CarExperimentalFeatureServiceController*");

        synchronized (mLock) {
            writer.println(" mEnabledFeatures, number of features:" + mEnabledFeatures.size()
                    + ", format: (feature, class)");
            for (int i = 0; i < mEnabledFeatures.size(); i++) {
                String feature = mEnabledFeatures.keyAt(i);
                FeatureInfo info = mEnabledFeatures.valueAt(i);
                writer.println(feature + "," + info.className);
            }
            writer.println("mBound:" + mBound);
        }
    }

    /**
     * Returns class name for experimental feature.
     */
    public String getCarManagerClassForFeature(String featureName) {
        FeatureInfo info;
        synchronized (mLock) {
            info = mEnabledFeatures.get(featureName);
        }
        if (info == null) {
            return null;
        }
        return info.className;
    }

    /**
     * Returns service binder for experimental feature.
     */
    public IBinder getCarService(String serviceName) {
        FeatureInfo info;
        synchronized (mLock) {
            info = mEnabledFeatures.get(serviceName);
        }
        if (info == null) {
            return null;
        }
        return info.binder;
    }

    private void resetFeatures() {
        synchronized (mLock) {
            mExperimentalCar = null;
            mEnabledFeatures.clear();
        }
    }
}
