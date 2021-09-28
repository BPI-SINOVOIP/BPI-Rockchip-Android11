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

package com.android.compatibility.common.util;

import static org.junit.Assume.assumeTrue;

import android.content.res.Resources;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.junit.AssumptionViolatedException;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

/**
 * Custom JUnit4 rule that does not run a test case if the device does not define the given system
 * resource.
 *
 * <p>The tests are skipped by throwing a {@link AssumptionViolatedException}.  CTS test runners
 * will report this as a {@code ASSUMPTION_FAILED}.
 */
public class RequiredSystemResourceRule implements TestRule {

    private static final String TAG = "RequiredSystemResourceRule";

    @NonNull private final String mName;
    private final boolean mHasResource;

    /**
     * Creates a rule for the given system resource.
     *
     * @param resourceId resource per se
     * @param name resource name used for debugging purposes
     */
    public RequiredSystemResourceRule(@NonNull String name) {
        mName = name;
        mHasResource = !TextUtils.isEmpty(getSystemResource(name));
    }

    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {

            @Override
            public void evaluate() throws Throwable {
                if (!mHasResource) {
                    Log.d(TAG, "skipping "
                            + description.getClassName() + "#" + description.getMethodName()
                            + " because device does not have system resource '" + mName + "'");
                    assumeTrue("Device does not have system resource '" + mName + "'",
                            mHasResource);
                    return;
                }
                base.evaluate();
            }
        };
    }

    /**
     * Gets the given system resource.
     */
    @Nullable
    public static String getSystemResource(@NonNull String name) {
        try {
            final int resourceId = Resources.getSystem().getIdentifier(name, "string", "android");
            return Resources.getSystem().getString(resourceId);
        } catch (Exception e) {
            Log.e(TAG, "could not get value of resource '" + name + "': ", e);
        }
        return null;
    }

    @Override
    public String toString() {
        return "RequiredSystemResourceRule[" + mName + "]";
    }
}
