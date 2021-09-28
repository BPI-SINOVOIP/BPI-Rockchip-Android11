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
import static com.android.cts.usespermissiondiffcertapp.Utils.grantClipUriPermissionViaService;

import static junit.framework.Assert.fail;

import android.content.ClipData;
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
public class UriGrantsServiceTest {
    private static Context getContext() {
        return InstrumentationRegistry.getTargetContext();
    }

    @Test
    public void testGrantableToService() {
        for (Uri uri : GRANTABLE) {
            for (int mode : GRANTABLE_MODES) {
                Log.d(TAG, "Testing " + uri + " " + mode);
                assertGrantableToService(uri, mode, UriGrantsTest::makeSingleClipData);
                assertGrantableToService(uri, mode, UriGrantsTest::makeMultiClipData);
            }
        }
    }

    private void assertGrantableToService(Uri uri, int mode, Function<Uri, ClipData> clipper) {
        final Uri subUri = Uri.withAppendedPath(uri, "foo");
        final Uri subSubUri = Uri.withAppendedPath(subUri, "bar");
        final Uri sub2Uri = Uri.withAppendedPath(uri, "yes");
        final Uri sub2SubUri = Uri.withAppendedPath(sub2Uri, "no");

        ReceiveUriService.stop(getContext());

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

        ReceiveUriService.clearStarted();
        grantClipUriPermissionViaService(subClip, mode);
        ReceiveUriService.waitForStart();

        int firstStartId = ReceiveUriService.getCurStartId();

        assertAccess(uri, 0);
        assertAccess(subClip, mode);
        assertAccess(subUri, mode);
        assertAccess(subSubUri, 0);
        assertAccess(sub2Clip, 0);
        assertAccess(sub2Uri, 0);
        assertAccess(sub2SubUri, 0);

        // --------------------------------

        // Send another Intent to it.
        ReceiveUriService.clearStarted();
        grantClipUriPermissionViaService(sub2Clip, mode);
        ReceiveUriService.waitForStart();

        assertAccess(uri, 0);
        assertAccess(subClip, mode);
        assertAccess(subUri, mode);
        assertAccess(subSubUri, 0);
        assertAccess(sub2Clip, mode);
        assertAccess(sub2Uri, mode);
        assertAccess(sub2SubUri, 0);

        // And make sure we can't generate a permission to a running service.
        assertNotGrantableToService(Uri.withAppendedPath(uri, "hah"), mode, clipper);

        // --------------------------------

        // Stop the first command.
        ReceiveUriService.stopCurWithId(firstStartId);

        assertAccess(uri, 0);
        assertAccess(subClip, 0);
        assertAccess(subUri, 0);
        assertAccess(subSubUri, 0);
        assertAccess(sub2Clip, mode);
        assertAccess(sub2Uri, mode);
        assertAccess(sub2SubUri, 0);

        // --------------------------------

        // Dispose of service.
        ReceiveUriService.stopSync(getContext());

        assertAccess(uri, 0);
        assertAccess(subClip, 0);
        assertAccess(subUri, 0);
        assertAccess(subSubUri, 0);
        assertAccess(sub2Clip, 0);
        assertAccess(sub2Uri, 0);
        assertAccess(sub2SubUri, 0);
    }

    @Test
    public void testNotGrantableToService() {
        for (Uri uri : NOT_GRANTABLE) {
            for (int mode : NOT_GRANTABLE_MODES) {
                Log.d(TAG, "Testing " + uri + " " + mode);
                assertNotGrantableToService(uri, mode, UriGrantsTest::makeSingleClipData);
                assertNotGrantableToService(uri, mode, UriGrantsTest::makeMultiClipData);
            }
        }
    }

    private void assertNotGrantableToService(Uri uri, int mode, Function<Uri, ClipData> clipper) {
        final Uri subUri = Uri.withAppendedPath(uri, "foo");
        final ClipData subClip = clipper.apply(subUri);

        Intent grantIntent = new Intent();
        grantIntent.setClipData(subClip);
        grantIntent.addFlags(mode);
        grantIntent.setClass(getContext(), ReceiveUriService.class);
        try {
            getContext().startService(grantIntent);
            fail("expected SecurityException granting " + subClip + " to service");
        } catch (SecurityException e) {
            // This is what we want.
        }
    }
}
