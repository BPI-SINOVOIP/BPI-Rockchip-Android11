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

import static com.android.cts.usespermissiondiffcertapp.AccessPermissionWithDiffSigTest.PERM_URI_GRANTING;
import static com.android.cts.usespermissiondiffcertapp.Asserts.assertNoPersistedUriPermission;
import static com.android.cts.usespermissiondiffcertapp.Asserts.assertPersistedUriPermission;
import static com.android.cts.usespermissiondiffcertapp.Asserts.assertReadingClipAllowed;
import static com.android.cts.usespermissiondiffcertapp.Asserts.assertReadingClipNotAllowed;
import static com.android.cts.usespermissiondiffcertapp.Asserts.assertWritingClipAllowed;
import static com.android.cts.usespermissiondiffcertapp.Asserts.assertWritingClipNotAllowed;
import static com.android.cts.usespermissiondiffcertapp.Utils.grantClipUriPermissionViaActivity;
import static com.android.cts.usespermissiondiffcertapp.Utils.grantClipUriPermissionViaContext;
import static com.android.cts.usespermissiondiffcertapp.Utils.revokeClipUriPermissionViaContext;

import static junit.framework.Assert.fail;

import android.content.ClipData;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class UriGrantsTest {
    static final String TAG = "UriGrantsTest";

    private static Context getContext() {
        return InstrumentationRegistry.getTargetContext();
    }

    static ClipData makeSingleClipData(Uri uri) {
        return new ClipData("foo", new String[] { "foo/bar" },
                new ClipData.Item(uri));
    }

    static ClipData makeMultiClipData(Uri uri) {
        Uri grantClip1Uri = uri;
        Uri grantClip2Uri = Uri.withAppendedPath(uri, "clip2");
        Uri grantClip3Uri = Uri.withAppendedPath(uri, "clip3");
        Uri grantClip4Uri = Uri.withAppendedPath(uri, "clip4");
        Uri grantClip5Uri = Uri.withAppendedPath(uri, "clip5");
        ClipData clip = new ClipData("foo", new String[] { "foo/bar" },
                new ClipData.Item(grantClip1Uri));
        clip.addItem(new ClipData.Item(grantClip2Uri));
        // Intents in the ClipData should allow their data: and clip URIs
        // to be granted, but only respect the grant flags of the top-level
        // Intent.
        clip.addItem(new ClipData.Item(new Intent(Intent.ACTION_VIEW, grantClip3Uri)));
        Intent intent = new Intent(Intent.ACTION_VIEW, grantClip4Uri);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        clip.addItem(new ClipData.Item(intent));
        intent = new Intent(Intent.ACTION_VIEW);
        intent.setClipData(new ClipData("foo", new String[] { "foo/bar" },
                new ClipData.Item(grantClip5Uri)));
        clip.addItem(new ClipData.Item(intent));
        return clip;
    }

    /**
     * Validate behavior of persistable permission grants.
     */
    @Test
    public void testGrantPersistableUriPermission() {
        final ContentResolver resolver = getContext().getContentResolver();

        final Uri target = Uri.withAppendedPath(PERM_URI_GRANTING, "foo");
        final ClipData clip = makeSingleClipData(target);

        // Make sure we can't see the target
        assertReadingClipNotAllowed(clip, "reading should have failed");
        assertWritingClipNotAllowed(clip, "writing should have failed");

        // Make sure we can't take a grant we don't have
        try {
            resolver.takePersistableUriPermission(target, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            fail("taking read should have failed");
        } catch (SecurityException expected) {
        }

        // And since we were just installed, no persisted grants yet
        assertNoPersistedUriPermission();

        // Now, let's grant ourselves some access
        ReceiveUriActivity.clearStarted();
        grantClipUriPermissionViaActivity(clip, Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        ReceiveUriActivity.waitForStart();

        // We should now have reading access, even before taking the persistable
        // grant. Persisted grants should still be empty.
        assertReadingClipAllowed(clip);
        assertWritingClipNotAllowed(clip, "writing should have failed");
        assertNoPersistedUriPermission();

        // Take the read grant and verify we have it!
        long before = System.currentTimeMillis();
        resolver.takePersistableUriPermission(target, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        long after = System.currentTimeMillis();
        assertPersistedUriPermission(target, Intent.FLAG_GRANT_READ_URI_PERMISSION, before, after);

        // Make sure we can't take a grant we don't have
        try {
            resolver.takePersistableUriPermission(target, Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            fail("taking write should have failed");
        } catch (SecurityException expected) {
        }

        // Launch again giving ourselves persistable read and write access
        ReceiveUriActivity.clearNewIntent();
        grantClipUriPermissionViaActivity(clip, Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        ReceiveUriActivity.waitForNewIntent();

        // Previous persisted grant should be unchanged
        assertPersistedUriPermission(target, Intent.FLAG_GRANT_READ_URI_PERMISSION, before, after);

        // We should have both read and write; read is persisted, and write
        // isn't persisted yet.
        assertReadingClipAllowed(clip);
        assertWritingClipAllowed(clip);

        // Take again, but still only read; should just update timestamp
        before = System.currentTimeMillis();
        resolver.takePersistableUriPermission(target, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        after = System.currentTimeMillis();
        assertPersistedUriPermission(target, Intent.FLAG_GRANT_READ_URI_PERMISSION, before, after);

        // And take yet again, both read and write
        before = System.currentTimeMillis();
        resolver.takePersistableUriPermission(target,
                Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        after = System.currentTimeMillis();
        assertPersistedUriPermission(target,
                Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION,
                before, after);

        // Now drop the persisted grant; write first, then read
        resolver.releasePersistableUriPermission(target, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        assertPersistedUriPermission(target, Intent.FLAG_GRANT_WRITE_URI_PERMISSION, before, after);
        resolver.releasePersistableUriPermission(target, Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        assertNoPersistedUriPermission();

        // And even though we dropped the persistable grants, our activity is
        // still running with the global grants (until reboot).
        assertReadingClipAllowed(clip);
        assertWritingClipAllowed(clip);

        ReceiveUriActivity.finishCurInstanceSync();
    }

    /**
     * Validate behavior of prefix permission grants.
     */
    @Test
    public void testGrantPrefixUriPermission() throws Exception {
        final Uri target = Uri.withAppendedPath(PERM_URI_GRANTING, "foo1");
        final Uri targetMeow = Uri.withAppendedPath(target, "meow");
        final Uri targetMeowCat = Uri.withAppendedPath(targetMeow, "cat");

        final ClipData clip = makeSingleClipData(target);
        final ClipData clipMeow = makeSingleClipData(targetMeow);
        final ClipData clipMeowCat = makeSingleClipData(targetMeowCat);

        // Make sure we can't see the target
        assertReadingClipNotAllowed(clip, "reading should have failed");
        assertWritingClipNotAllowed(clip, "writing should have failed");

        // Give ourselves prefix read access
        ReceiveUriActivity.clearStarted();
        grantClipUriPermissionViaActivity(clipMeow, Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        ReceiveUriActivity.waitForStart();

        // Verify prefix read access
        assertReadingClipNotAllowed(clip, "reading should have failed");
        assertReadingClipAllowed(clipMeow);
        assertReadingClipAllowed(clipMeowCat);
        assertWritingClipNotAllowed(clip, "writing should have failed");
        assertWritingClipNotAllowed(clipMeow, "writing should have failed");
        assertWritingClipNotAllowed(clipMeowCat, "writing should have failed");

        // Now give ourselves exact write access
        ReceiveUriActivity.clearNewIntent();
        grantClipUriPermissionViaActivity(clip, Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        ReceiveUriActivity.waitForNewIntent();

        // Verify we have exact write access, but not prefix write
        assertReadingClipNotAllowed(clip, "reading should have failed");
        assertReadingClipAllowed(clipMeow);
        assertReadingClipAllowed(clipMeowCat);
        assertWritingClipAllowed(clip);
        assertWritingClipNotAllowed(clipMeow, "writing should have failed");
        assertWritingClipNotAllowed(clipMeowCat, "writing should have failed");

        ReceiveUriActivity.finishCurInstanceSync();
    }

    @Test
    public void testGrantPersistablePrefixUriPermission() {
        final ContentResolver resolver = getContext().getContentResolver();

        final Uri target = Uri.withAppendedPath(PERM_URI_GRANTING, "foo2");
        final Uri targetMeow = Uri.withAppendedPath(target, "meow");

        final ClipData clip = makeSingleClipData(target);
        final ClipData clipMeow = makeSingleClipData(targetMeow);

        // Make sure we can't see the target
        assertReadingClipNotAllowed(clip, "reading should have failed");

        // Give ourselves prefix read access
        ReceiveUriActivity.clearStarted();
        grantClipUriPermissionViaActivity(clip, Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
                | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        ReceiveUriActivity.waitForStart();

        // Verify prefix read access
        assertReadingClipAllowed(clip);
        assertReadingClipAllowed(clipMeow);

        // Verify we can persist direct grant
        long before = System.currentTimeMillis();
        resolver.takePersistableUriPermission(target, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        long after = System.currentTimeMillis();
        assertPersistedUriPermission(target, Intent.FLAG_GRANT_READ_URI_PERMISSION, before, after);

        // But we can't take anywhere under the prefix
        try {
            resolver.takePersistableUriPermission(targetMeow,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION);
            fail("taking under prefix should have failed");
        } catch (SecurityException expected) {
        }

        // Should still have access regardless of taking
        assertReadingClipAllowed(clip);
        assertReadingClipAllowed(clipMeow);

        // And clean up our grants
        resolver.releasePersistableUriPermission(target, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        assertNoPersistedUriPermission();

        ReceiveUriActivity.finishCurInstanceSync();
    }

    /**
     * Validate behavior of directly granting/revoking permission grants.
     */
    @Test
    public void testDirectGrantRevokeUriPermission() throws Exception {
        final ContentResolver resolver = getContext().getContentResolver();

        final Uri target = Uri.withAppendedPath(PERM_URI_GRANTING, "foo3");
        final Uri targetMeow = Uri.withAppendedPath(target, "meow");
        final Uri targetMeowCat = Uri.withAppendedPath(targetMeow, "cat");

        final ClipData clip = makeSingleClipData(target);
        final ClipData clipMeow = makeSingleClipData(targetMeow);
        final ClipData clipMeowCat = makeSingleClipData(targetMeowCat);

        // Make sure we can't see the target
        assertReadingClipNotAllowed(clipMeow, "reading should have failed");
        assertWritingClipNotAllowed(clipMeow, "writing should have failed");

        // Give ourselves some grants:
        // /meow/cat  WRITE|PERSISTABLE
        // /meow      READ|PREFIX
        // /meow      WRITE
        grantClipUriPermissionViaContext(targetMeowCat, Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        grantClipUriPermissionViaContext(targetMeow, Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        grantClipUriPermissionViaContext(targetMeow, Intent.FLAG_GRANT_WRITE_URI_PERMISSION);

        long before = System.currentTimeMillis();
        resolver.takePersistableUriPermission(targetMeowCat, Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        long after = System.currentTimeMillis();
        assertPersistedUriPermission(targetMeowCat, Intent.FLAG_GRANT_WRITE_URI_PERMISSION, before, after);

        // Verify they look good
        assertReadingClipNotAllowed(clip, "reading should have failed");
        assertReadingClipAllowed(clipMeow);
        assertReadingClipAllowed(clipMeowCat);
        assertWritingClipNotAllowed(clip, "writing should have failed");
        assertWritingClipAllowed(clipMeow);
        assertWritingClipAllowed(clipMeowCat);

        // Revoke anyone with write under meow
        revokeClipUriPermissionViaContext(targetMeow, Intent.FLAG_GRANT_WRITE_URI_PERMISSION);

        // This should have nuked persisted permission at lower level, but it
        // shoulnd't have touched our prefix read.
        assertReadingClipNotAllowed(clip, "reading should have failed");
        assertReadingClipAllowed(clipMeow);
        assertReadingClipAllowed(clipMeowCat);
        assertWritingClipNotAllowed(clip, "writing should have failed");
        assertWritingClipNotAllowed(clipMeow, "writing should have failed");
        assertWritingClipNotAllowed(clipMeowCat, "writing should have failed");
        assertNoPersistedUriPermission();

        // Revoking read at top of tree should nuke everything else
        revokeClipUriPermissionViaContext(target, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        assertReadingClipNotAllowed(clip, "reading should have failed");
        assertReadingClipNotAllowed(clipMeow, "reading should have failed");
        assertReadingClipNotAllowed(clipMeowCat, "reading should have failed");
        assertWritingClipNotAllowed(clip, "writing should have failed");
        assertWritingClipNotAllowed(clipMeow, "writing should have failed");
        assertWritingClipNotAllowed(clipMeowCat, "writing should have failed");
        assertNoPersistedUriPermission();
    }

    /**
     * Validate behavior of a direct permission grant, where the receiver of
     * that permission revokes it.
     */
    @Test
    public void testDirectGrantReceiverRevokeUriPermission() throws Exception {
        final ContentResolver resolver = getContext().getContentResolver();

        final Uri target = Uri.withAppendedPath(PERM_URI_GRANTING, "foo3");
        final Uri targetMeow = Uri.withAppendedPath(target, "meow");
        final Uri targetMeowCat = Uri.withAppendedPath(targetMeow, "cat");

        final ClipData clip = makeSingleClipData(target);
        final ClipData clipMeow = makeSingleClipData(targetMeow);
        final ClipData clipMeowCat = makeSingleClipData(targetMeowCat);

        // Make sure we can't see the target
        assertReadingClipNotAllowed(clipMeow, "reading should have failed");
        assertWritingClipNotAllowed(clipMeow, "writing should have failed");

        // Give ourselves some grants:
        // /meow/cat  WRITE|PERSISTABLE
        // /meow      READ|PREFIX
        // /meow      WRITE
        grantClipUriPermissionViaContext(targetMeowCat, Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        grantClipUriPermissionViaContext(targetMeow, Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        grantClipUriPermissionViaContext(targetMeow, Intent.FLAG_GRANT_WRITE_URI_PERMISSION);

        long before = System.currentTimeMillis();
        resolver.takePersistableUriPermission(targetMeowCat, Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        long after = System.currentTimeMillis();
        assertPersistedUriPermission(targetMeowCat, Intent.FLAG_GRANT_WRITE_URI_PERMISSION, before, after);

        // Verify they look good
        assertReadingClipNotAllowed(clip, "reading should have failed");
        assertReadingClipAllowed(clipMeow);
        assertReadingClipAllowed(clipMeowCat);
        assertWritingClipNotAllowed(clip, "writing should have failed");
        assertWritingClipAllowed(clipMeow);
        assertWritingClipAllowed(clipMeowCat);

        // Revoke anyone with write under meow
        getContext().revokeUriPermission(targetMeow, Intent.FLAG_GRANT_WRITE_URI_PERMISSION);

        // This should have nuked persisted permission at lower level, but it
        // shoulnd't have touched our prefix read.
        assertReadingClipNotAllowed(clip, "reading should have failed");
        assertReadingClipAllowed(clipMeow);
        assertReadingClipAllowed(clipMeowCat);
        assertWritingClipNotAllowed(clip, "writing should have failed");
        assertWritingClipNotAllowed(clipMeow, "writing should have failed");
        assertWritingClipNotAllowed(clipMeowCat, "writing should have failed");
        assertNoPersistedUriPermission();

        // Revoking read at top of tree should nuke everything else
        getContext().revokeUriPermission(target, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        assertReadingClipNotAllowed(clip, "reading should have failed");
        assertReadingClipNotAllowed(clipMeow, "reading should have failed");
        assertReadingClipNotAllowed(clipMeowCat, "reading should have failed");
        assertWritingClipNotAllowed(clip, "writing should have failed");
        assertWritingClipNotAllowed(clipMeow, "writing should have failed");
        assertWritingClipNotAllowed(clipMeowCat, "writing should have failed");
        assertNoPersistedUriPermission();
    }
}
