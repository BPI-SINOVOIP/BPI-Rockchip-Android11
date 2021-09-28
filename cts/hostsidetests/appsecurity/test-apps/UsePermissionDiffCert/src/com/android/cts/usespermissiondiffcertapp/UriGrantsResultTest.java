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

package com.android.cts.usespermissiondiffcertapp;

import static com.android.cts.usespermissiondiffcertapp.AccessPermissionWithDiffSigTest.GRANTABLE;
import static com.android.cts.usespermissiondiffcertapp.AccessPermissionWithDiffSigTest.GRANTABLE_MODES;
import static com.android.cts.usespermissiondiffcertapp.AccessPermissionWithDiffSigTest.NOT_GRANTABLE;
import static com.android.cts.usespermissiondiffcertapp.AccessPermissionWithDiffSigTest.NOT_GRANTABLE_MODES;
import static com.android.cts.usespermissiondiffcertapp.Asserts.assertAccess;
import static com.android.cts.usespermissiondiffcertapp.UriGrantsTest.TAG;

import android.app.Instrumentation;
import android.content.ClipData;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.function.Function;

@RunWith(AndroidJUnit4.class)
public class UriGrantsResultTest {
    private static final int REQUEST_CODE = 42;

    private Instrumentation mInstrumentation;
    private Context mContext;
    private GetResultActivity mActivity;

    public void createActivity() throws Exception {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mContext = InstrumentationRegistry.getTargetContext();

        final Intent intent = new Intent(mContext, GetResultActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mActivity = (GetResultActivity) mInstrumentation.startActivitySync(intent);
        mInstrumentation.waitForIdleSync();
        mActivity.clearResult();
    }

    public void destroyActivity() throws Exception {
        mActivity.finish();
    }

    @Test
    public void testGrantableToResult() throws Exception {
        for (Uri uri : GRANTABLE) {
            for (int mode : GRANTABLE_MODES) {
                Log.d(TAG, "Testing " + uri + " " + mode);
                assertGrantableToResult(uri, mode, UriGrantsTest::makeSingleClipData);
                assertGrantableToResult(uri, mode, UriGrantsTest::makeMultiClipData);
            }
        }
    }

    private void assertGrantableToResult(Uri uri, int mode,
            Function<Uri, ClipData> clipper) throws Exception {
        try {
            createActivity();
            assertGrantableToResultInternal(uri, mode, clipper);
        } finally {
            destroyActivity();
        }
    }

    private void assertGrantableToResultInternal(Uri uri, int mode,
            Function<Uri, ClipData> clipper) {
        final Uri subUri = Uri.withAppendedPath(uri, "foo");
        final Uri subSubUri = Uri.withAppendedPath(subUri, "bar");
        final ClipData subClip = clipper.apply(subUri);

        assertAccess(uri, 0);
        assertAccess(subClip, 0);
        assertAccess(subUri, 0);
        assertAccess(subSubUri, 0);

        // --------------------------------

        final Intent intent = buildIntent(subClip, mode);
        mActivity.startActivityForResult(intent, REQUEST_CODE);
        mActivity.getResult();

        assertAccess(uri, 0);
        assertAccess(subClip, mode);
        assertAccess(subUri, mode);
        assertAccess(subSubUri, 0);

        // --------------------------------

        // Dispose of activity.
        mActivity.finish();
        mActivity.getDestroyed();

        assertAccess(uri, 0);
        assertAccess(subClip, 0);
        assertAccess(subUri, 0);
        assertAccess(subSubUri, 0);
    }

    @Test
    public void testNotGrantableToResult() throws Exception {
        for (Uri uri : NOT_GRANTABLE) {
            for (int mode : NOT_GRANTABLE_MODES) {
                Log.d(TAG, "Testing " + uri + " " + mode);
                assertNotGrantableToResult(uri, mode, UriGrantsTest::makeSingleClipData);
                assertNotGrantableToResult(uri, mode, UriGrantsTest::makeMultiClipData);
            }
        }
    }

    private void assertNotGrantableToResult(Uri uri, int mode,
            Function<Uri, ClipData> clipper) throws Exception {
        try {
            createActivity();
            assertNotGrantableToResultInternal(uri, mode, clipper);
        } finally {
            destroyActivity();
        }
    }

    private void assertNotGrantableToResultInternal(Uri uri, int mode,
            Function<Uri, ClipData> clipper) {
        final Uri subUri = Uri.withAppendedPath(uri, "foo");
        final Uri subSubUri = Uri.withAppendedPath(subUri, "bar");
        final ClipData subClip = clipper.apply(subUri);

        final Intent intent = buildIntent(subClip, mode);
        mActivity.startActivityForResult(intent, REQUEST_CODE);
        mActivity.getResult();

        assertAccess(uri, 0);
        assertAccess(subClip, 0);
        assertAccess(subUri, 0);
        assertAccess(subSubUri, 0);
    }

    private Intent buildIntent(ClipData clip, int mode) {
        final Intent intent = new Intent();
        intent.setComponent(new ComponentName("com.android.cts.permissiondeclareapp",
                "com.android.cts.permissiondeclareapp.SendResultActivity"));
        intent.putExtra(Intent.EXTRA_TEXT, clip);
        intent.putExtra(Intent.EXTRA_INDEX, mode);
        return intent;
    }
}
