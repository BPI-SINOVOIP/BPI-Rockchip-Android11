/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.cts.helpers;

import android.app.Instrumentation;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.SystemProperties;
import android.platform.helpers.HelperManager;
import android.platform.helpers.exceptions.TestHelperException;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import com.google.common.annotations.VisibleForTesting;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import java.util.ArrayList;
import java.util.List;
import java.util.regex.Pattern;

/**
 * This rule connects a CTS test to its device interaction helpers that were installed by a {@link
 * DeviceInteractionHelperInstaller} target preparer. Given a helper interface {@code T}, it uses
 * the ro.vendor.cts_interaction_helper_packages property to look up a concrete instance of {@code
 * T}.
 *
 * <p>The ro.vendor.cts_interaction_helper_packages property contains a colon-separated named of
 * packages that will be searched for implementations. The packages will be searched in the listed
 * order, with a fallback to the default com.android.cts.helpers.aosp implementation whether it is
 * explicitly listed or not. Any packages that are not installed on the device will be silently
 * skipped; this allows falling back to the default helpers if the device is updated with a generic
 * system image.
 *
 * <p>AndroidManifest.xml for each package in the list should contain a meta-data tag named
 * interaction-helpers-prefix. The value of the tag is used as a prefix when looking up classes;
 * this acts as a double-check on the listed package ordering. If this tag is missing, the package
 * will still be searched, but classes contained in it will not be found unless they have a prefix
 * declared by another package. The default com.android.cts.helpers.aosp package declares a prefix
 * of Default; this prefix is always search after other declared prefixes, so a default
 * implementation in the default package should always be found as a fallback.
 *
 * @param <T> The interface that the desired helper should implement.
 */
public class DeviceInteractionHelperRule<T extends ICtsDeviceInteractionHelper>
        implements TestRule {
    private static final String LOG_TAG = DeviceInteractionHelperRule.class.getSimpleName();

    protected static final String DEFAULT_HELPERS = "com.android.cts.helpers.aosp";
    protected static final String PACKAGE_PATH_PROPERTY =
            "ro.vendor.cts_interaction_helper_packages";

    /**
     * Class of the interface that this @Rule will look up.  A concrete implementation will be
     * loaded at runtime.
     */
    private final Class<T> mHelperClass;
    private final Instrumentation mInstrumentation;
    private final Context mContext;

    /** Concrete helper implementation found at runtime when the @Rule is evaluated. */
    private T mHelper;

    /**
     * Ordered list of packages that will be searched for helper implementations.  The list is
     * loaded from the value of PACKAGE_PATH_PROPERTY, and the default helper package is always
     * appended to the end of the search list.
     */
    private List<String> mLoadPackages = new ArrayList<String>();

    /**
     * Ordered list of class prefixes that will be tried when searching for helper implementations.
     * Each prefix is declared by the interaction-helpers-prefix meta-data tag in a package from
     * mLoadPackages.
     */
    private List<String> mPrefixes = new ArrayList<String>();

    /**
     * Ordered list of apk directories to search for helper implementations.  Corresponds to the
     * apks from mLoadPackages that are found at runtime.
     */
    private List<String> mSourceDirs = new ArrayList<String>();

    /**
     * For each prefix from mPrefixes, searches apks at the paths in mSourceDirs for a concrete
     * class implementing T.  The first class found will be loaded.
     */
    private HelperManager mHelperManager;

    /**
     * Creates a {@link DeviceInteractionHelperRule} for the helper interface {@code T}.
     *
     * @param helperClass The class of the interface @{code T}.
     */
    public DeviceInteractionHelperRule(Class<T> helperClass) {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mContext = mInstrumentation.getTargetContext();
        mHelperClass = helperClass;
    }

    /** @return A concrete instance that implements {@code T}, if one could be found. */
    public T getHelper() {
        if (mHelper == null) {
            throw new TestHelperException("Helper not created");
        }
        return mHelper;
    }

    /** @return The list of helper prefixes extracted from the package search list. */
    public List<String> getPrefixes() {
        return mPrefixes;
    }

    /** @return A list of package names that will be searched for implementation classes. */
    public List<String> getPackageList() {
        return mLoadPackages;
    }

    /** @return A list of apk paths that will be searched for implementation classes. */
    public List<String> getSourceDirs() {
        return mSourceDirs;
    }

    @VisibleForTesting
    /**
     * Finds the package search paths and prefixes based on the colon-separated list passed in.
     *
     * @param property A colon-separated String containing the package names to search.
     * @param pm A PackageManager that will be used to get info about the packages.
     */
    void buildPackageList(String property, PackageManager pm) {
        // Make sure we always search the default helpers package last.  Since they provide
        // an implementation for every interface, allowing another package to be searched
        // later wouldn't make sense.  If the default is found in the list, move it to the end;
        // otherwise append it to the end.
        String searchPath = property;
        Log.d(LOG_TAG, "Search path from property: <" + property + ">");
        if (searchPath == null) {
            searchPath = "";
        }
        searchPath = searchPath.replaceAll("(?:^|:)" + DEFAULT_HELPERS + "(:|$)", "$1");
        searchPath += ":" + DEFAULT_HELPERS;

        mLoadPackages.clear();
        mSourceDirs.clear();
        mPrefixes.clear();
        for (String pkg : searchPath.split(":")) {
            if (pkg.isEmpty()) {
                continue;
            }

            ApplicationInfo info = null;
            try {
                info = pm.getApplicationInfo(pkg, PackageManager.GET_META_DATA);
            } catch (NameNotFoundException e) {
                Log.w(LOG_TAG, String.format("Unable to find helper package %s", pkg));
                continue;
            }
            mLoadPackages.add(pkg);
            mSourceDirs.add(info.publicSourceDir);

            if (info.metaData == null) {
                Log.w(LOG_TAG, String.format("Helper apk %s is missing meta-data.", pkg));
                continue;
            }
            String prefix = info.metaData.getString("interaction-helpers-prefix");
            if (prefix == null || prefix.isEmpty()) {
                Log.w(LOG_TAG, String.format("Helper apk %s does not set a class prefix", pkg));
            } else if (!mPrefixes.contains(prefix)) {
                mPrefixes.add(prefix);
            }
        }
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        return new TestHelperStatement(base);
    }

    /**
     * <a href="http://junit.org/apidocs/org/junit/runners/model/Statement.html"><code>Statement
     * </code></a> that uses the list from ro.vendor.cts_interaction_helpers_packages to find a
     * concrete helper implementation for the {@code T} interface given to {@link
     * DeviceInteractionHelperRule}.
     */
    private class TestHelperStatement extends Statement {
        private final Statement mBase;

        TestHelperStatement(Statement base) {
            mBase = base;
        }

        @Override
        public void evaluate() throws Throwable {
            PackageManager pm = mContext.getPackageManager();

            buildPackageList(SystemProperties.get(PACKAGE_PATH_PROPERTY), pm);
            if (mLoadPackages.isEmpty()) {
                throw new TestHelperException("No installed helper packages found");
            }

            Log.d(LOG_TAG, "Helper search path: " + String.join(":", mSourceDirs));
            mHelperManager = HelperManager.getInstance(mSourceDirs, mInstrumentation);
            for (String prefix : mPrefixes) {
                try {
                    // Match the prefix against the simple name of the class, not the full package.
                    Pattern classMatch = Pattern.compile("^.*\\.\\Q" + prefix + "\\E[^.]*$");
                    mHelper = mHelperManager.get(mHelperClass, classMatch);
                    break;
                } catch (TestHelperException e) {
                    Log.d(LOG_TAG, String.format(
                            "No %s implementation found with prefix %s.", mHelperClass, prefix), e);
                }
            }
            if (mHelper == null) {
                throw new TestHelperException("No implementation found for " + mHelperClass);
            }

            mBase.evaluate();
        }
    }
}
