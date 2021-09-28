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
package com.android.cts.overlay.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.res.AssetFileDescriptor;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import java.io.InputStreamReader;
import java.util.concurrent.Executor;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class OverlayableTest {
    private static final String TARGET_PACKAGE = "com.android.cts.overlay.target";

    private static final String POLICY_ALL_PACKAGE = "com.android.cts.overlay.all";

    private static final String OVERLAID = "You have been overlaid";
    private static final String NOT_OVERLAID = "Not overlaid";

    private static final long TIMEOUT = 30;

    private static class R {
        class string {
            private static final int not_overlayable = 0x7f010000;
            private static final int policy_product = 0x7f010001;
            private static final int policy_public = 0x7f010002;
            private static final int policy_system = 0x7f010003;
            private static final int policy_vendor = 0x7f010004;
            private static final int policy_signature = 0x7f010005;
            private static final int other_policy_public = 0x7f010006;
        }
    }

    private Executor mExecutor;

    @Before
    public void setUp() throws Exception {
        mExecutor = (command) -> new Thread(command).start();
    }

    private Context getTargetContext() throws Exception {
        return InstrumentationRegistry.getTargetContext().createPackageContext(TARGET_PACKAGE, 0);
    }

    private void assertOverlayEnabled(Context context, String overlayPackage) throws Exception {
        // Wait for the overlay changes to propagate
        FutureTask<Boolean> task = new FutureTask<>(() -> {
            while (true) {
                for (String path : context.getAssets().getApkPaths()) {
                    if (path.contains(overlayPackage)) {
                        return true;
                    }
                }
            }
        });

        mExecutor.execute(task);
        assertTrue("Failed to load overlay " + overlayPackage, task.get(TIMEOUT, TimeUnit.SECONDS));
    }

    @Test
    public void testOverlayPolicyAll() throws Exception {
        Context context = getTargetContext();
        assertOverlayEnabled(context, POLICY_ALL_PACKAGE);

        String result = context.getResources().getString(R.string.not_overlayable);
        assertEquals(NOT_OVERLAID, result);

        result = context.getResources().getString(R.string.policy_public);
        assertEquals(OVERLAID, result);

        result = context.getResources().getString(R.string.policy_product);
        assertEquals(NOT_OVERLAID, result);

        result = context.getResources().getString(R.string.policy_system);
        assertEquals(NOT_OVERLAID, result);

        result = context.getResources().getString(R.string.policy_vendor);
        assertEquals(NOT_OVERLAID, result);

        result = context.getResources().getString(R.string.policy_signature);
        assertEquals(OVERLAID, result);

        result = context.getResources().getString(R.string.other_policy_public);
        assertEquals(NOT_OVERLAID, result);
    }

    @Test
    public void testSameSignatureNoOverlayableSucceeds() throws Exception {
        Context context = getTargetContext();
        assertOverlayEnabled(context, POLICY_ALL_PACKAGE);

        String result = context.getResources().getString(R.string.not_overlayable);
        assertEquals(OVERLAID, result);

        result = context.getResources().getString(R.string.policy_public);
        assertEquals(OVERLAID, result);

        result = context.getResources().getString(R.string.policy_product);
        assertEquals(OVERLAID, result);

        result = context.getResources().getString(R.string.policy_system);
        assertEquals(OVERLAID, result);

        result = context.getResources().getString(R.string.policy_vendor);
        assertEquals(OVERLAID, result);

        result = context.getResources().getString(R.string.policy_signature);
        assertEquals(OVERLAID, result);

        result = context.getResources().getString(R.string.other_policy_public);
        assertEquals(OVERLAID, result);
    }

    @Test
    public void testFrameworkDoesNotDefineOverlayable() throws Exception {
        assertTrue(InstrumentationRegistry.getTargetContext().getAssets().getOverlayablesToString(
                "android").isEmpty());
    }

    @Test
    public void testCannotOverlayAssets() throws Exception {
        Context context = getTargetContext();
        AssetFileDescriptor d = context.getAssets().openNonAssetFd("assets/asset.txt");
        InputStreamReader inputStreamReader = new InputStreamReader(d.createInputStream());

        StringBuilder output = new StringBuilder();
        for (int c = inputStreamReader.read(); c >= 0; c = inputStreamReader.read()) {
            output.append((char) c);
        }

        assertEquals(NOT_OVERLAID, output.toString());
    }
}