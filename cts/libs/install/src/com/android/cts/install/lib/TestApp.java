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

package com.android.cts.install.lib;

import android.content.pm.VersionedPackage;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.InputStream;
import java.util.function.Function;

/**
 * Collection of dummy apps used in tests.
 */
public class TestApp {
    public static final String A = "com.android.cts.install.lib.testapp.A";
    public static final String B = "com.android.cts.install.lib.testapp.B";
    public static final String C = "com.android.cts.install.lib.testapp.C";
    public static final String Apex = "com.android.apex.cts.shim";
    public static final String NotPreInstalledApex = "com.android.apex.cts.shim_not_pre_installed";

    // Apk collection
    public static final TestApp A1 = new TestApp("Av1", A, 1, /*isApex*/false,
            "TestAppAv1.apk");
    public static final TestApp A2 = new TestApp("Av2", A, 2, /*isApex*/false,
            "TestAppAv2.apk");
    public static final TestApp A3 = new TestApp("Av3", A, 3, /*isApex*/false,
            "TestAppAv3.apk");
    public static final TestApp ACrashing2 = new TestApp("ACrashingV2", A, 2, /*isApex*/false,
            "TestAppACrashingV2.apk");
    public static final TestApp ASplit1 = new TestApp("ASplitV1", A, 1, /*isApex*/false,
            "TestAppASplitV1.apk", "TestAppASplitV1_anydpi.apk");
    public static final TestApp ASplit2 = new TestApp("ASplitV2", A, 2, /*isApex*/false,
            "TestAppASplitV2.apk", "TestAppASplitV2_anydpi.apk");
    public static final TestApp AIncompleteSplit = new TestApp("AIncompleteSplit", A, 1,
            /*isApex*/false, "TestAppASplitV1_anydpi.apk");

    public static final TestApp B1 = new TestApp("Bv1", B, 1, /*isApex*/false,
            "TestAppBv1.apk");
    public static final TestApp B2 = new TestApp("Bv2", B, 2, /*isApex*/false,
            "TestAppBv2.apk");

    public static final TestApp C1 = new TestApp("Cv1", C, 1, /*isApex*/false,
            "TestAppCv1.apk");

    // Apex collection
    public static final TestApp Apex1 = new TestApp("Apex1", Apex, 1, /*isApex*/true,
            "com.android.apex.cts.shim.v1.apex");
    public static final TestApp Apex2 = new TestApp("Apex2", Apex, 2, /*isApex*/true,
            "com.android.apex.cts.shim.v2.apex");
    public static final TestApp Apex3 = new TestApp("Apex3", Apex, 3, /*isApex*/true,
            "com.android.apex.cts.shim.v3.apex");

    private final String mName;
    private final String mPackageName;
    private final long mVersionCode;
    private final String[] mResourceNames;
    private final boolean mIsApex;
    private final Function<String, InputStream> mGetResourceStream;

    public TestApp(String name, String packageName, long versionCode, boolean isApex,
            String... resourceNames) {
        mName = name;
        mPackageName = packageName;
        mVersionCode = versionCode;
        mResourceNames = resourceNames;
        mIsApex = isApex;
        mGetResourceStream = (res) -> TestApp.class.getClassLoader().getResourceAsStream(res);
    }

    public TestApp(String name, String packageName, long versionCode, boolean isApex,
            File[] paths) {
        mName = name;
        mPackageName = packageName;
        mVersionCode = versionCode;
        mResourceNames = new String[paths.length];
        for (int i = 0; i < paths.length; ++i) {
            mResourceNames[i] = paths[i].getName();
        }
        mIsApex = isApex;
        mGetResourceStream = (res) -> {
            try {
                for (int i = 0; i < paths.length; ++i) {
                    if (paths[i].getName().equals(res)) {
                        return new FileInputStream(paths[i]);
                    }
                }
            } catch (FileNotFoundException ignore) {
            }
            return null;
        };
    }

    public TestApp(String name, String packageName, long versionCode, boolean isApex, File path) {
        this(name, packageName, versionCode, isApex, new File[]{ path });
    }

    public String getPackageName() {
        return mPackageName;
    }

    public long getVersionCode() {
        return mVersionCode;
    }

    public VersionedPackage getVersionedPackage() {
        return new VersionedPackage(mPackageName, mVersionCode);
    }

    @Override
    public String toString() {
        return mName;
    }

    boolean isApex() {
        return mIsApex;
    }

    public String[] getResourceNames() {
        return mResourceNames;
    }

    /**
     * Returns an InputStream for the resource name.
     */
    public InputStream getResourceStream(String name) {
        return mGetResourceStream.apply(name);
    }
}
