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
import static com.android.cts.usespermissiondiffcertapp.Utils.grantClipUriPermissionViaActivities;
import static com.android.cts.usespermissiondiffcertapp.Utils.grantClipUriPermissionViaActivity;

import static junit.framework.Assert.fail;

import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.SystemClock;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.function.Function;

@RunWith(AndroidJUnit4.class)
public class UriGrantsActivityTest {
    private static Context getContext() {
        return InstrumentationRegistry.getTargetContext();
    }

    @After
    public void tearDown() throws Exception {
        // Always dispose, usually to clean up from failed tests
        ReceiveUriActivity.finishCurInstanceSync();
    }

    @Test
    public void testGrantableToActivity() {
        for (Uri uri : GRANTABLE) {
            for (int mode : GRANTABLE_MODES) {
                Log.d(TAG, "Testing " + uri + " " + mode);
                assertGrantableToActivity(uri, mode, UriGrantsTest::makeSingleClipData);
                assertGrantableToActivity(uri, mode, UriGrantsTest::makeMultiClipData);
            }
        }
    }

    private void assertGrantableToActivity(Uri uri, int mode, Function<Uri, ClipData> clipper) {
        final Uri subUri = Uri.withAppendedPath(uri, "foo");
        final Uri subSubUri = Uri.withAppendedPath(subUri, "bar");
        final Uri sub2Uri = Uri.withAppendedPath(uri, "yes");
        final Uri sub2SubUri = Uri.withAppendedPath(sub2Uri, "no");

        final ClipData subClip = clipper.apply(subUri);
        final ClipData sub2Clip = clipper.apply(sub2Uri);

        assertAccess(uri, 0);
        assertAccess(subClip, 0);
        assertAccess(subUri, 0);
        assertAccess(subSubUri, 0);
        assertAccess(sub2Clip, 0);
        assertAccess(sub2Uri, 0);
        assertAccess(sub2SubUri, 0);

        // --------------------------------

        ReceiveUriActivity.clearStarted();
        grantClipUriPermissionViaActivities(subClip, mode);
        ReceiveUriActivity.waitForStart();

        assertAccess(uri, 0);
        assertAccess(subClip, mode);
        assertAccess(subUri, mode);
        assertAccess(subSubUri, 0);
        assertAccess(sub2Clip, 0);
        assertAccess(sub2Uri, 0);
        assertAccess(sub2SubUri, 0);

        // --------------------------------

        ReceiveUriActivity.clearNewIntent();
        grantClipUriPermissionViaActivity(sub2Clip, mode);
        ReceiveUriActivity.waitForNewIntent();

        assertAccess(uri, 0);
        assertAccess(subClip, mode);
        assertAccess(subUri, mode);
        assertAccess(subSubUri, 0);
        assertAccess(sub2Clip, mode);
        assertAccess(sub2Uri, mode);
        assertAccess(sub2SubUri, 0);

        // And make sure we can't generate a permission to a running activity.
        assertNotGrantableToActivity(Uri.withAppendedPath(uri, "hah"), mode, clipper);

        // --------------------------------

        // Dispose of activity.
        ReceiveUriActivity.finishCurInstanceSync();
        SystemClock.sleep(200);

        assertAccess(uri, 0);
        assertAccess(subClip, 0);
        assertAccess(subUri, 0);
        assertAccess(subSubUri, 0);
        assertAccess(sub2Clip, 0);
        assertAccess(sub2Uri, 0);
        assertAccess(sub2SubUri, 0);
    }

    @Test
    public void testNotGrantableToActivity() {
        for (Uri uri : NOT_GRANTABLE) {
            for (int mode : NOT_GRANTABLE_MODES) {
                Log.d(TAG, "Testing " + uri + " " + mode);
                assertNotGrantableToActivity(uri, mode, UriGrantsTest::makeSingleClipData);
                assertNotGrantableToActivity(uri, mode, UriGrantsTest::makeMultiClipData);
            }
        }
    }

    private void assertNotGrantableToActivity(Uri uri, int mode, Function<Uri, ClipData> clipper) {
        final Uri subUri = Uri.withAppendedPath(uri, "foo");
        final ClipData subClip = clipper.apply(subUri);

        Intent grantIntent = new Intent();
        grantIntent.setClipData(subClip);
        grantIntent.addFlags(mode | Intent.FLAG_ACTIVITY_NEW_TASK);
        grantIntent.setClass(getContext(), ReceiveUriActivity.class);
        try {
            ReceiveUriActivity.clearStarted();
            getContext().startActivity(grantIntent);
            ReceiveUriActivity.waitForStart();
            fail("expected SecurityException granting " + subClip + " to activity");
        } catch (SecurityException e) {
            // This is what we want.
        }
    }
}
