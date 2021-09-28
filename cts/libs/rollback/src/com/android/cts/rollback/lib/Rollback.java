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

package com.android.cts.rollback.lib;

import android.content.pm.VersionedPackage;
import android.content.rollback.PackageRollbackInfo;

import com.android.cts.install.lib.TestApp;

/**
 * Helper class for asserting PackageRollbackInfo contents.
 */
public class Rollback {

    private VersionedPackage mFrom;
    private VersionedPackage mTo;

    Rollback(PackageRollbackInfo packageInfo) {
        mFrom = packageInfo.getVersionRolledBackFrom();
        mTo = packageInfo.getVersionRolledBackTo();
    }

    private Rollback(TestApp from) {
        mFrom = from.getVersionedPackage();
    }

    private Rollback(String packageName, long versionCode) {
        mFrom = new VersionedPackage(packageName, versionCode);
    }

    /**
     * Creates a package Rollback info with versionRolledBackFrom
     * matching the package name and version of the given test app.
     */
    public static Rollback from(TestApp app) {
        return new Rollback(app);
    }

    /**
     * Creates a package Rollback info with versionRolledBackFrom
     * matching the package name and version provided as parameter.
     */
    public static Rollback from(String packageName, long versionCode) {
        return new Rollback(packageName, versionCode);
    }

    /**
     * Sets versionRolledBackTo to have the package name and version of the
     * given test app.
     */
    public Rollback to(TestApp app) {
        mTo = app.getVersionedPackage();
        return this;
    }

    /**
     * Sets versionRolledBackTo to have the package name and version provided as parameter.
     */
    public Rollback to(String packageName, long versionCode) {
        mTo = new VersionedPackage(packageName, versionCode);
        return this;
    }

    @Override
    public String toString() {
        return "Rollback.from(" + mFrom + ").to(" + mTo + ")";
    }

    @Override
    public boolean equals(Object other) {
        if (!(other instanceof Rollback)) {
            return false;
        }

        Rollback r = (Rollback) other;
        return mFrom.equals(r.mFrom) && mTo.equals(r.mTo);
    }
}
