/*
 * Copyright (C) 2009 The Android Open Source Project
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

package com.android.cts.instrumentationdiffcertapp;

import static org.junit.Assert.assertFalse;

import android.app.Instrumentation;
import android.content.ComponentName;
import android.content.Context;
import android.os.Bundle;
import android.test.InstrumentationTestCase;

/**
 * Test that a instrumentation targeting another app with a different cert fails.
 */
public class InstrumentationFailToRunTest extends InstrumentationTestCase {

    public void testInstrumentationNotAllowed_exception() {
        final Context myContext = getInstrumentation().getContext();
        // assumes android.app.Instrumentation has been defined in this app's AndroidManifest.xml
        // as targeting an app with a different cert
        final ComponentName appDiffCertInstrumentation =
                new ComponentName(myContext, Instrumentation.class);
        try {
            assertFalse("instrumentation started",
                    myContext.startInstrumentation(appDiffCertInstrumentation, null, new Bundle()));
            fail("SecurityException not thrown");
        } catch (SecurityException expected) {
        }
    }

    public void testInstrumentationNotAllowed_fail() {
        final Context myContext = getInstrumentation().getContext();
        // assumes android.app.Instrumentation has been defined in this app's AndroidManifest.xml
        // as targeting an app with a different cert
        final ComponentName appDiffCertInstrumentation =
                new ComponentName(myContext, Instrumentation.class);
        assertFalse("instrumentation started",
                myContext.startInstrumentation(appDiffCertInstrumentation, null, new Bundle()));
    }
}
