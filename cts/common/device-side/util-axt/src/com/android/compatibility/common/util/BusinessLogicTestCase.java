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
package com.android.compatibility.common.util;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.app.Instrumentation;
import android.content.ContentResolver;
import android.content.Context;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.rules.TestName;

import java.io.File;
import java.io.FileNotFoundException;
import java.lang.reflect.Field;
import java.util.Map;

/**
 * Device-side base class for tests leveraging the Business Logic service.
 */
public class BusinessLogicTestCase {
    private static final String TAG = "BusinessLogicTestCase";

    /* String marking the beginning of the parameter in a test name */
    private static final String PARAM_START = "[";

    public static final String CONTENT_PROVIDER =
            String.format("%s://android.tradefed.contentprovider", ContentResolver.SCHEME_CONTENT);

    /* Test name rule that tracks the current test method under execution */
    @Rule public TestName mTestCase = new TestName();

    protected BusinessLogic mBusinessLogic;
    protected boolean mCanReadBusinessLogic = true;

    @Before
    public void handleBusinessLogic() {
        loadBusinessLogic();
        executeBusinessLogic();
    }

    protected void executeBusinessLogic() {
        executeBusinessLogifForTest(mTestCase.getMethodName());
    }

    protected void executeBusinessLogifForTest(String methodName) {
        assertTrue(String.format("Test \"%s\" is unable to execute as it depends on the missing "
                + "remote configuration.", methodName), mCanReadBusinessLogic);
        if (methodName.contains(PARAM_START)) {
            // Strip parameter suffix (e.g. "[0]") from method name
            methodName = methodName.substring(0, methodName.lastIndexOf(PARAM_START));
        }
        String testName = String.format("%s#%s", this.getClass().getName(), methodName);
        if (mBusinessLogic.hasLogicFor(testName)) {
            Log.i(TAG, "Finding business logic for test case: " + testName);
            BusinessLogicExecutor executor = new BusinessLogicDeviceExecutor(getContext(), this, mBusinessLogic.getRedactionRegexes());
            mBusinessLogic.applyLogicFor(testName, executor);
        } else {
            /* There are cases in which this is an acceptable outcome, and we do not want to fail.
             * For instance, some business logic rule lists are only sent from the server
             * for certain devices (see go/aes-gts).  Devices exempt from those rules will
             * receive no BL config for some tests, and this should result in a pass.
             */
            Log.d(TAG, "No business logic found for test: " + testName);
        }
    }

    protected void loadBusinessLogic() {
        String uriPath = String.format("%s/%s", CONTENT_PROVIDER, BusinessLogic.DEVICE_FILE);
        Uri sdcardUri = Uri.parse(uriPath);
        Context appContext = InstrumentationRegistry.getTargetContext();
        try {
            ContentResolver resolver = appContext.getContentResolver();
            ParcelFileDescriptor descriptor = resolver.openFileDescriptor(sdcardUri, "r");
            mBusinessLogic = BusinessLogicFactory.createFromFile(
                    new ParcelFileDescriptor.AutoCloseInputStream(descriptor));
            return;
        } catch (FileNotFoundException e) {
            // Log the error and use the fallback too
            Log.e(TAG, "Error while using content provider for config", e);
        }
        // Fallback to reading the business logic directly.
        File businessLogicFile = new File(BusinessLogic.DEVICE_FILE);
        if (businessLogicFile.canRead()) {
            mBusinessLogic = BusinessLogicFactory.createFromFile(businessLogicFile);
        } else {
            mCanReadBusinessLogic = false;
        }
    }

    protected static Instrumentation getInstrumentation() {
        return InstrumentationRegistry.getInstrumentation();
    }

    protected static Context getContext() {
        return getInstrumentation().getTargetContext();
    }

    public static void skipTest(String message) {
        assumeTrue(message, false);
    }

    public static void failTest(String message) {
        fail(message);
    }

    public void mapPut(String mapName, String key, String value) {
        boolean put = false;
        for (Field f : getClass().getDeclaredFields()) {
            if (f.getName().equalsIgnoreCase(mapName) && Map.class.isAssignableFrom(f.getType())) {
                try {
                    ((Map) f.get(this)).put(key, value);
                    put = true;
                } catch (IllegalAccessException e) {
                    Log.w(String.format("failed to invoke mapPut on field \"%s\". Resuming...",
                            f.getName()), e);
                    // continue iterating through fields, throw exception if no other fields match
                }
            }
        }
        if (!put) {
            throw new RuntimeException(String.format("Failed to find map %s in class %s", mapName,
                    getClass().getName()));
        }
    }
}
