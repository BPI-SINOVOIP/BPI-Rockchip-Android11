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

package com.android.cts.usespermissiondiffcertapp;

import static com.android.cts.usespermissiondiffcertapp.Asserts.assertContentUriAllowed;
import static com.android.cts.usespermissiondiffcertapp.Asserts.assertContentUriNotAllowed;
import static com.android.cts.usespermissiondiffcertapp.Asserts.assertReadingContentUriAllowed;
import static com.android.cts.usespermissiondiffcertapp.Asserts.assertReadingContentUriNotAllowed;
import static com.android.cts.usespermissiondiffcertapp.Asserts.assertWritingContentUriAllowed;
import static com.android.cts.usespermissiondiffcertapp.Asserts.assertWritingContentUriNotAllowed;

import static junit.framework.Assert.assertEquals;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.CalendarContract;
import android.provider.ContactsContract;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests that signature-enforced permissions cannot be accessed by apps signed
 * with different certs than app that declares the permission.
 * 
 * Accesses app cts/tests/appsecurity-tests/test-apps/PermissionDeclareApp/...
 */
@RunWith(AndroidJUnit4.class)
public class AccessPermissionWithDiffSigTest {
    private static Context getContext() {
        return InstrumentationRegistry.getTargetContext();
    }

    static final Uri PERM_URI = Uri.parse("content://ctspermissionwithsignature");
    static final Uri PERM_URI_GRANTING = Uri.parse("content://ctspermissionwithsignaturegranting");
    static final Uri PERM_URI_PATH = Uri.parse("content://ctspermissionwithsignaturepath");
    static final Uri PERM_URI_PATH_RESTRICTING = Uri.parse(
            "content://ctspermissionwithsignaturepathrestricting");
    static final Uri PRIV_URI = Uri.parse("content://ctsprivateprovider");
    static final Uri PRIV_URI_GRANTING = Uri.parse("content://ctsprivateprovidergranting");
    static final String EXPECTED_MIME_TYPE = "got/theMIME";

    static final Uri AMBIGUOUS_URI_COMPAT = Uri.parse("content://ctsambiguousprovidercompat");
    static final String EXPECTED_MIME_TYPE_AMBIGUOUS = "got/theUnspecifiedMIME";
    static final Uri AMBIGUOUS_URI = Uri.parse("content://ctsambiguousprovider");

    static final Uri[] GRANTABLE = new Uri[] {
            Uri.withAppendedPath(PERM_URI_GRANTING, "foo"),
            Uri.withAppendedPath(PERM_URI_PATH, "foo"),
            Uri.withAppendedPath(PRIV_URI_GRANTING, "foo"),
    };

    static final Uri[] NOT_GRANTABLE = new Uri[] {
            Uri.withAppendedPath(PERM_URI, "bar"),
            Uri.withAppendedPath(PERM_URI_GRANTING, "bar"),
            Uri.withAppendedPath(PERM_URI_PATH, "bar"),
            Uri.withAppendedPath(PRIV_URI, "bar"),
            Uri.withAppendedPath(PRIV_URI_GRANTING, "bar"),
            Uri.withAppendedPath(AMBIGUOUS_URI, "bar"),
            CalendarContract.CONTENT_URI,
            ContactsContract.AUTHORITY_URI,
    };

    static final int[] GRANTABLE_MODES = new int[] {
            Intent.FLAG_GRANT_READ_URI_PERMISSION,
            Intent.FLAG_GRANT_WRITE_URI_PERMISSION,
    };

    static final int[] NOT_GRANTABLE_MODES = new int[] {
            Intent.FLAG_GRANT_READ_URI_PERMISSION,
            Intent.FLAG_GRANT_WRITE_URI_PERMISSION,
            Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION,
            Intent.FLAG_GRANT_WRITE_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION,
            Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION,
            Intent.FLAG_GRANT_WRITE_URI_PERMISSION | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION,
    };

    /**
     * Test that the ctspermissionwithsignature content provider cannot be read,
     * since this app lacks the required certs
     */
    @Test
    public void testReadProviderWithDiff() {
        assertReadingContentUriNotAllowed(PERM_URI, null);
    }

    /**
     * Test that the ctspermissionwithsignature content provider cannot be written,
     * since this app lacks the required certs
     */
    @Test
    public void testWriteProviderWithDiff() {
        assertWritingContentUriNotAllowed(PERM_URI, null);
    }

    /**
     * Test that the ctsprivateprovider content provider cannot be read,
     * since it is not exported from its app.
     */
    @Test
    public void testReadProviderWhenPrivate() {
        assertReadingContentUriNotAllowed(PRIV_URI, "shouldn't read private provider");
    }

    /**
     * Test that the ctsambiguousprovider content provider cannot be read,
     * since it doesn't have an "exported=" line.
     */
    @Test
    public void testReadProviderWhenAmbiguous() {
        assertReadingContentUriNotAllowed(AMBIGUOUS_URI, "shouldn't read ambiguous provider");
    }

    /**
     * Old App Compatibility Test
     *
     * Test that the ctsambiguousprovidercompat content provider can be read for older
     * API versions, because it didn't specify either exported=true or exported=false.
     */
    @Test
    public void testReadProviderWhenAmbiguousCompat() {
        assertReadingContentUriAllowed(AMBIGUOUS_URI_COMPAT);
    }

    /**
     * Old App Compatibility Test
     *
     * Test that the ctsambiguousprovidercompat content provider can be written for older
     * API versions, because it didn't specify either exported=true or exported=false.
     */
    @Test
    public void testWriteProviderWhenAmbiguousCompat() {
        assertWritingContentUriAllowed(AMBIGUOUS_URI_COMPAT);
    }

    /**
     * Test that the ctsprivateprovider content provider cannot be written,
     * since it is not exported from its app.
     */
    @Test
    public void testWriteProviderWhenPrivate() {
        assertWritingContentUriNotAllowed(PRIV_URI, "shouldn't write private provider");
    }

    /**
     * Test that the ctsambiguousprovider content provider cannot be written,
     * since it doesn't have an exported= line.
     */
    @Test
    public void testWriteProviderWhenAmbiguous() {
        assertWritingContentUriNotAllowed(AMBIGUOUS_URI, "shouldn't write ambiguous provider");
    }

    /**
     * Verify that we can access paths outside the {@code path-permission}
     * protections, which should only rely on {@code provider} permissions.
     */
    @Test
    public void testRestrictingProviderNoMatchingPath() {
        assertReadingContentUriAllowed(PERM_URI_PATH_RESTRICTING);
        assertWritingContentUriAllowed(PERM_URI_PATH_RESTRICTING);

        // allowed by no top-level permission
        final Uri test = PERM_URI_PATH_RESTRICTING.buildUpon().appendPath("fo").build();
        assertReadingContentUriAllowed(test);
        assertWritingContentUriAllowed(test);
    }

    /**
     * Verify that paths under {@code path-permission} restriction aren't
     * allowed, even though the {@code provider} requires no permissions.
     */
    @Test
    public void testRestrictingProviderMatchingPathDenied() {
        // rejected by "foo" prefix
        final Uri test1 = PERM_URI_PATH_RESTRICTING.buildUpon().appendPath("foo").build();
        assertReadingContentUriNotAllowed(test1, null);
        assertWritingContentUriNotAllowed(test1, null);

        // rejected by "foo" prefix
        final Uri test2 = PERM_URI_PATH_RESTRICTING.buildUpon()
                .appendPath("foo").appendPath("ba").build();
        assertReadingContentUriNotAllowed(test2, null);
        assertWritingContentUriNotAllowed(test2, null);
    }

    /**
     * Test that shady {@link Uri} are blocked by {@code path-permission}.
     */
    @Test
    public void testRestrictingProviderMatchingShadyPaths() {
        assertContentUriAllowed(
                Uri.parse("content://ctspermissionwithsignaturepathrestricting/"));
        assertContentUriAllowed(
                Uri.parse("content://ctspermissionwithsignaturepathrestricting//"));
        assertContentUriAllowed(
                Uri.parse("content://ctspermissionwithsignaturepathrestricting///"));
        assertContentUriNotAllowed(
                Uri.parse("content://ctspermissionwithsignaturepathrestricting/foo"), null);
        assertContentUriNotAllowed(
                Uri.parse("content://ctspermissionwithsignaturepathrestricting//foo"), null);
        assertContentUriNotAllowed(
                Uri.parse("content://ctspermissionwithsignaturepathrestricting///foo"), null);
        assertContentUriNotAllowed(
                Uri.parse("content://ctspermissionwithsignaturepathrestricting/foo//baz"), null);
    }

    /**
     * Verify that at least one {@code path-permission} rule will grant access,
     * even if the caller doesn't hold another matching {@code path-permission}.
     */
    @Test
    public void testRestrictingProviderMultipleMatchingPath() {
        // allowed by narrow "foo/bar" prefix
        final Uri test1 = PERM_URI_PATH_RESTRICTING.buildUpon()
                .appendPath("foo").appendPath("bar").build();
        assertReadingContentUriAllowed(test1);
        assertWritingContentUriAllowed(test1);

        // allowed by narrow "foo/bar" prefix
        final Uri test2 = PERM_URI_PATH_RESTRICTING.buildUpon()
                .appendPath("foo").appendPath("bar2").build();
        assertReadingContentUriAllowed(test2);
        assertWritingContentUriAllowed(test2);
    }

    @Test
    public void testGetMimeTypePermission() {
        // Precondition: no current access.
        assertReadingContentUriNotAllowed(PERM_URI, "shouldn't read when starting test");
        assertWritingContentUriNotAllowed(PERM_URI, "shouldn't write when starting test");
        
        // All apps should be able to get MIME type regardless of permission.
        assertEquals(getContext().getContentResolver().getType(PERM_URI), EXPECTED_MIME_TYPE);
    }

    @Test
    public void testGetMimeTypePrivate() {
        // Precondition: no current access.
        assertReadingContentUriNotAllowed(PRIV_URI, "shouldn't read when starting test");
        assertWritingContentUriNotAllowed(PRIV_URI, "shouldn't write when starting test");
        
        // All apps should be able to get MIME type even if provider is private.
        assertEquals(getContext().getContentResolver().getType(PRIV_URI), EXPECTED_MIME_TYPE);
    }

    @Test
    public void testGetMimeTypeAmbiguous() {
        // Precondition: no current access.
        assertReadingContentUriNotAllowed(AMBIGUOUS_URI, "shouldn't read when starting test");
        assertWritingContentUriNotAllowed(AMBIGUOUS_URI, "shouldn't write when starting test");

        // All apps should be able to get MIME type even if provider is private.
        assertEquals(getContext().getContentResolver().getType(AMBIGUOUS_URI), EXPECTED_MIME_TYPE);
    }

    /**
     * Old App Compatibility Test
     *
     * We should be able to access the mime type of a content provider of an older
     * application, even if that application didn't explicitly declare either
     * exported=true or exported=false
     */
    @Test
    public void testGetMimeTypeAmbiguousCompat() {
        // All apps should be able to get MIME type even if provider is private.
        assertEquals(EXPECTED_MIME_TYPE_AMBIGUOUS,
                getContext().getContentResolver().getType(AMBIGUOUS_URI_COMPAT));
    }
}
