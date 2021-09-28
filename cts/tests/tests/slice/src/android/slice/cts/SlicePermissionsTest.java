/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

package android.slice.cts;

import android.content.pm.PackageManager;
import static android.content.pm.PackageManager.PERMISSION_DENIED;
import static android.content.pm.PackageManager.PERMISSION_GRANTED;

import static org.junit.Assert.assertEquals;

import android.app.slice.SliceManager;
import android.content.Context;
import android.content.pm.PackageManager.NameNotFoundException;
import android.net.Uri;
import android.os.Process;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class SlicePermissionsTest {

    private static final Uri BASE_URI = Uri.parse("content://android.slice.cts.local/");
    private final Context mContext = InstrumentationRegistry.getContext();
    private String mTestPkg;
    private int mTestUid;
    private int mTestPid;
    private SliceManager mSliceManager;
    private boolean isSliceDisabled = mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_SLICES_DISABLED);

    @Before
    public void setup() throws NameNotFoundException {
        if(isSliceDisabled) {
          return;
        }
        mSliceManager = mContext.getSystemService(SliceManager.class);
        mTestPkg = mContext.getPackageName();
        mTestUid = mContext.getPackageManager().getPackageUid(mTestPkg, 0);
        mTestPid = Process.myPid();
    }

    @After
    public void tearDown() {
        if (isSliceDisabled) {
            return;
        }
        mSliceManager.revokeSlicePermission(mTestPkg, BASE_URI);
    }

    @Test
    public void testGrant() {
        if (isSliceDisabled) {
            return;
        }
        assertEquals(PERMISSION_DENIED,
                mSliceManager.checkSlicePermission(BASE_URI, mTestPid, mTestUid));

        mSliceManager.grantSlicePermission(mTestPkg, BASE_URI);

        assertEquals(PERMISSION_GRANTED,
                mSliceManager.checkSlicePermission(BASE_URI, mTestPid, mTestUid));
    }

    @Test
    public void testGrantParent() {
        if (isSliceDisabled) {
            return;
        }
        Uri uri = BASE_URI.buildUpon()
                .appendPath("something")
                .build();

        assertEquals(PERMISSION_DENIED,
                mSliceManager.checkSlicePermission(uri, mTestPid, mTestUid));

        mSliceManager.grantSlicePermission(mTestPkg, BASE_URI);

        assertEquals(PERMISSION_GRANTED,
                mSliceManager.checkSlicePermission(uri, mTestPid, mTestUid));
    }

    @Test
    public void testGrantParentExpands() {
        if (isSliceDisabled) {
            return;
        }
        Uri uri = BASE_URI.buildUpon()
                .appendPath("something")
                .build();

        assertEquals(PERMISSION_DENIED,
                mSliceManager.checkSlicePermission(uri, mTestPid, mTestUid));

        mSliceManager.grantSlicePermission(mTestPkg, uri);

        // Only sub-path granted.
        assertEquals(PERMISSION_GRANTED,
                mSliceManager.checkSlicePermission(uri, mTestPid, mTestUid));
        assertEquals(PERMISSION_DENIED,
                mSliceManager.checkSlicePermission(BASE_URI, mTestPid, mTestUid));

        mSliceManager.grantSlicePermission(mTestPkg, BASE_URI);

        // Now all granted.
        assertEquals(PERMISSION_GRANTED,
                mSliceManager.checkSlicePermission(uri, mTestPid, mTestUid));
        assertEquals(PERMISSION_GRANTED,
                mSliceManager.checkSlicePermission(BASE_URI, mTestPid, mTestUid));
    }

    @Test
    public void testGrantChild() {
        if (isSliceDisabled) {
            return;
        }
        Uri uri = BASE_URI.buildUpon()
                .appendPath("something")
                .build();

        assertEquals(PERMISSION_DENIED,
                mSliceManager.checkSlicePermission(BASE_URI, mTestPid, mTestUid));

        mSliceManager.grantSlicePermission(mTestPkg, uri);

        // Still no permission because only a child was granted
        assertEquals(PERMISSION_DENIED,
                mSliceManager.checkSlicePermission(BASE_URI, mTestPid, mTestUid));
    }

    @Test
    public void testRevoke() {
        if (isSliceDisabled) {
            return;
        }
        assertEquals(PERMISSION_DENIED,
                mSliceManager.checkSlicePermission(BASE_URI, mTestPid, mTestUid));

        mSliceManager.grantSlicePermission(mTestPkg, BASE_URI);

        assertEquals(PERMISSION_GRANTED,
                mSliceManager.checkSlicePermission(BASE_URI, mTestPid, mTestUid));

        mSliceManager.revokeSlicePermission(mTestPkg, BASE_URI);

        assertEquals(PERMISSION_DENIED,
                mSliceManager.checkSlicePermission(BASE_URI, mTestPid, mTestUid));
    }

    @Test
    public void testRevokeParent() {
        if (isSliceDisabled) {
            return;
        }
        Uri uri = BASE_URI.buildUpon()
                .appendPath("something")
                .build();
        assertEquals(PERMISSION_DENIED,
                mSliceManager.checkSlicePermission(uri, mTestPid, mTestUid));

        mSliceManager.grantSlicePermission(mTestPkg, uri);

        assertEquals(PERMISSION_GRANTED,
                mSliceManager.checkSlicePermission(uri, mTestPid, mTestUid));

        mSliceManager.revokeSlicePermission(mTestPkg, BASE_URI);

        // Revoked because parent was revoked
        assertEquals(PERMISSION_DENIED,
                mSliceManager.checkSlicePermission(uri, mTestPid, mTestUid));
    }

    @Test
    public void testRevokeChild() {
        if (isSliceDisabled) {
            return;
        }
        Uri uri = BASE_URI.buildUpon()
                .appendPath("something")
                .build();
        assertEquals(PERMISSION_DENIED,
                mSliceManager.checkSlicePermission(BASE_URI, mTestPid, mTestUid));

        mSliceManager.grantSlicePermission(mTestPkg, BASE_URI);

        assertEquals(PERMISSION_GRANTED,
                mSliceManager.checkSlicePermission(BASE_URI, mTestPid, mTestUid));

        mSliceManager.revokeSlicePermission(mTestPkg, uri);

        // Not revoked because child was revoked.
        assertEquals(PERMISSION_GRANTED,
                mSliceManager.checkSlicePermission(BASE_URI, mTestPid, mTestUid));
    }

}
