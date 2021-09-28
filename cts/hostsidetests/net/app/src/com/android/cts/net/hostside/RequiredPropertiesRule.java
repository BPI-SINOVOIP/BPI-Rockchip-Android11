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
package com.android.cts.net.hostside;

import static com.android.cts.net.hostside.AbstractRestrictBackgroundNetworkTestCase.TAG;

import android.text.TextUtils;
import android.util.ArraySet;
import android.util.Log;

import com.android.compatibility.common.util.BeforeAfterRule;

import org.junit.Assume;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import java.util.ArrayList;
import java.util.Collections;

public class RequiredPropertiesRule extends BeforeAfterRule {

    private static ArraySet<Property> mRequiredProperties;

    @Override
    public void onBefore(Statement base, Description description) {
        mRequiredProperties = getAllRequiredProperties(description);

        final String testName = description.getClassName() + "#" + description.getMethodName();
        assertTestIsValid(testName, mRequiredProperties);
        Log.i(TAG, "Running test " + testName + " with required properties: "
                + propertiesToString(mRequiredProperties));
    }

    private ArraySet<Property> getAllRequiredProperties(Description description) {
        final ArraySet<Property> allRequiredProperties = new ArraySet<>();
        RequiredProperties requiredProperties = description.getAnnotation(RequiredProperties.class);
        if (requiredProperties != null) {
            Collections.addAll(allRequiredProperties, requiredProperties.value());
        }

        for (Class<?> clazz = description.getTestClass();
                clazz != null; clazz = clazz.getSuperclass()) {
            requiredProperties = clazz.getDeclaredAnnotation(RequiredProperties.class);
            if (requiredProperties == null) {
                continue;
            }
            for (Property requiredProperty : requiredProperties.value()) {
                for (Property p : Property.values()) {
                    if (p.getValue() == ~requiredProperty.getValue()
                            && allRequiredProperties.contains(p)) {
                        continue;
                    }
                }
                allRequiredProperties.add(requiredProperty);
            }
        }
        return allRequiredProperties;
    }

    private void assertTestIsValid(String testName, ArraySet<Property> requiredProperies) {
        if (requiredProperies == null) {
            return;
        }
        final ArrayList<Property> unsupportedProperties = new ArrayList<>();
        for (Property property : requiredProperies) {
            if (!property.isSupported()) {
                unsupportedProperties.add(property);
            }
        }
        Assume.assumeTrue("Unsupported properties: "
                + propertiesToString(unsupportedProperties), unsupportedProperties.isEmpty());
    }

    public static ArraySet<Property> getRequiredProperties() {
        return mRequiredProperties;
    }

    private static String propertiesToString(Iterable<Property> properties) {
        return "[" + TextUtils.join(",", properties) + "]";
    }
}
