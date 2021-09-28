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

package android.permission.cts;

import static android.Manifest.permission.WRITE_EXTERNAL_STORAGE;
import static android.Manifest.permission.WRITE_MEDIA_STORAGE;

import android.app.UiAutomation;
import android.content.ContentValues;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.UserHandle;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.AppModeInstant;
import android.provider.CallLog;
import android.provider.Contacts;
import android.provider.ContactsContract;
import android.provider.Settings;
import android.provider.Telephony;
import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.MediumTest;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/**
 * Tests Permissions related to reading from and writing to providers
 */
@MediumTest
public class ProviderPermissionTest extends AndroidTestCase {

    private static final String TAG = ProviderPermissionTest.class.getSimpleName();

    private static final List<Uri> CONTACT_URIS = new ArrayList<Uri>() {{
        add(Contacts.People.CONTENT_URI); // Deprecated.
        add(ContactsContract.Contacts.CONTENT_FILTER_URI);
        add(ContactsContract.Contacts.CONTENT_GROUP_URI);
        add(ContactsContract.Contacts.CONTENT_LOOKUP_URI);
        add(ContactsContract.CommonDataKinds.Email.CONTENT_URI);
        add(ContactsContract.CommonDataKinds.Email.CONTENT_FILTER_URI);
        add(ContactsContract.Directory.CONTENT_URI);
        add(ContactsContract.Directory.ENTERPRISE_CONTENT_URI);
        add(ContactsContract.Profile.CONTENT_URI);
    }};

    /**
     * Verify that reading contacts requires permissions.
     * <p>Tests Permission:
     *   {@link android.Manifest.permission#READ_CONTACTS}
     */
    public void testReadContacts() {
        for (Uri uri : CONTACT_URIS) {
            Log.d(TAG, "Checking contacts URI " + uri);
            assertReadingContentUriRequiresPermission(uri,
                    android.Manifest.permission.READ_CONTACTS);
        }
    }

    /**
     * Verify that writing contacts requires permissions.
     * <p>Tests Permission:
     *   {@link android.Manifest.permission#WRITE_CONTACTS}
     */
    public void testWriteContacts() {
        assertWritingContentUriRequiresPermission(Contacts.People.CONTENT_URI,
                android.Manifest.permission.WRITE_CONTACTS);
    }

    /**
     * Verify that reading call logs requires permissions.
     * <p>Tests Permission:
     *   {@link android.Manifest.permission#READ_CALL_LOG}
     */
    @AppModeFull
    public void testReadCallLog() {
        assertReadingContentUriRequiresPermission(CallLog.CONTENT_URI,
                android.Manifest.permission.READ_CALL_LOG);
    }

    /**
     * Verify that writing call logs requires permissions.
     * <p>Tests Permission:
     *   {@link android.Manifest.permission#WRITE_CALL_LOG}
     */
    @AppModeFull
    public void testWriteCallLog() {
        assertWritingContentUriRequiresPermission(CallLog.CONTENT_URI,
                android.Manifest.permission.WRITE_CALL_LOG);
    }

    /**
     * Verify that reading from call-log (a content provider that is not accessible to instant apps)
     * returns null
     */
    @AppModeInstant
    public void testReadCallLogInstant() {
        assertNull(getContext().getContentResolver().query(CallLog.CONTENT_URI, null, null, null,
                null));
    }

    /**
     * Verify that writing to call-log (a content provider that is not accessible to instant apps)
     * yields an IAE.
     */
    @AppModeInstant
    public void testWriteCallLogInstant() {
        try {
            getContext().getContentResolver().insert(CallLog.CONTENT_URI, new ContentValues());
            fail("Expected IllegalArgumentException");
        } catch (IllegalArgumentException expected) {
        }
    }

    /**
     * Verify that reading already received SMS messages requires permissions.
     * <p>Tests Permission:
     *   {@link android.Manifest.permission#READ_SMS}
     *
     * <p>Note: The WRITE_SMS permission has been removed.
     */
    @AppModeFull
    public void testReadSms() {
        if (!mContext.getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_TELEPHONY)) {
            return;
        }

        assertReadingContentUriRequiresPermission(Telephony.Sms.CONTENT_URI,
                android.Manifest.permission.READ_SMS);
    }

    /**
     * Verify that reading from 'sms' (a content provider that is not accessible to instant apps)
     * returns null
     */
    @AppModeInstant
    public void testReadSmsInstant() {
        if (!mContext.getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_TELEPHONY)) {
            return;
        }

        assertNull(getContext().getContentResolver().query(Telephony.Sms.CONTENT_URI, null, null,
                null, null));
    }

    /**
     * Verify that write to settings requires permissions.
     * <p>Tests Permission:
     *   {@link android.Manifest.permission#WRITE_SETTINGS}
     */
    public void testWriteSettings() {
        final String permission = android.Manifest.permission.WRITE_SETTINGS;
        ContentValues value = new ContentValues();
        value.put(Settings.System.NAME, "name");
        value.put(Settings.System.VALUE, "value_insert");

        try {
            getContext().getContentResolver().insert(Settings.System.CONTENT_URI, value);
            fail("expected SecurityException requiring " + permission);
        } catch (SecurityException expected) {
            assertNotNull("security exception's error message.", expected.getMessage());
            assertTrue("error message should contain \"" + permission + "\". Got: \""
                    + expected.getMessage() + "\".",
                    expected.getMessage().contains(permission));
        }
    }

    /**
     * Verify that the {@link android.Manifest.permission#MANAGE_DOCUMENTS}
     * permission is only held by exactly one package: whoever handles the
     * {@link android.content.Intent#ACTION_OPEN_DOCUMENT} intent.
     * <p>
     * No other apps should <em>ever</em> attempt to acquire this permission,
     * since it would give those apps extremely broad access to all storage
     * providers on the device without user involvement in the arbitration
     * process. Apps should instead always rely on Uri permission grants for
     * access, using
     * {@link android.content.Intent#FLAG_GRANT_READ_URI_PERMISSION} and related
     * APIs.
     */
    public void testManageDocuments() {
        final PackageManager pm = getContext().getPackageManager();

        final Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        final ResolveInfo ri = pm.resolveActivity(intent, 0);
        final String validPkg = ri.activityInfo.packageName;

        final List<PackageInfo> holding = pm.getPackagesHoldingPermissions(new String[] {
                android.Manifest.permission.MANAGE_DOCUMENTS
        }, PackageManager.MATCH_UNINSTALLED_PACKAGES);
        for (PackageInfo pi : holding) {
            if (!Objects.equals(pi.packageName, validPkg)) {
                fail("Exactly one package (must be " + validPkg
                        + ") can request the MANAGE_DOCUMENTS permission; found package "
                        + pi.packageName + " which must be revoked for security reasons");
            }
        }
    }

    /**
     * The {@link android.Manifest.permission#WRITE_MEDIA_STORAGE} permission is
     * a very powerful permission that grants raw storage access to all devices,
     * and as such it's only appropriate to be granted to the media stack.
     * <p>
     * CDD now requires that all apps requesting this permission also hold the
     * "Storage" runtime permission, to give users visibility into the
     * capabilities of each app, and control over those capabilities.
     * <p>
     * If the end user revokes the "Storage" permission from an app, but that
     * app still has raw access to storage via {@code WRITE_MEDIA_STORAGE}, that
     * would be a CDD violation and a privacy incident.
     */
    public void testWriteMediaStorage() throws Exception {
        final UiAutomation ui = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        final PackageManager pm = getContext().getPackageManager();
        final List<PackageInfo> pkgs = pm.getInstalledPackages(
                PackageManager.MATCH_UNINSTALLED_PACKAGES | PackageManager.GET_PERMISSIONS);
        for (PackageInfo pkg : pkgs) {
            final boolean isSystem = pkg.applicationInfo.uid == android.os.Process.SYSTEM_UID;
            final boolean hasFrontDoor = pm.getLaunchIntentForPackage(pkg.packageName) != null;
            final boolean grantedMedia = pm.checkPermission(WRITE_MEDIA_STORAGE,
                    pkg.packageName) == PackageManager.PERMISSION_GRANTED;

            if (!isSystem && hasFrontDoor && grantedMedia) {
                final boolean requestsStorage = contains(pkg.requestedPermissions,
                        WRITE_EXTERNAL_STORAGE);
                if (!requestsStorage) {
                    fail("Found " + pkg.packageName + " holding WRITE_MEDIA_STORAGE permission "
                            + "without also requesting WRITE_EXTERNAL_STORAGE; these permissions "
                            + "must be requested together");
                }

                final boolean grantedStorage = pm.checkPermission(WRITE_EXTERNAL_STORAGE,
                        pkg.packageName) == PackageManager.PERMISSION_GRANTED;
                if (grantedStorage) {
                    final int flags;
                    ui.adoptShellPermissionIdentity("android.permission.GET_RUNTIME_PERMISSIONS");
                    try {
                        flags = pm.getPermissionFlags(WRITE_EXTERNAL_STORAGE, pkg.packageName,
                                android.os.Process.myUserHandle());
                    } finally {
                        ui.dropShellPermissionIdentity();
                    }

                    final boolean isFixed = (flags & (PackageManager.FLAG_PERMISSION_USER_FIXED
                            | PackageManager.FLAG_PERMISSION_POLICY_FIXED
                            | PackageManager.FLAG_PERMISSION_SYSTEM_FIXED)) != 0;
                    if (isFixed) {
                        fail("Found " + pkg.packageName + " holding WRITE_EXTERNAL_STORAGE in a "
                                + "fixed state; this permission must be revokable by the user");
                    }
                }
            }
        }
    }

    private static boolean contains(String[] haystack, String needle) {
        if (haystack != null) {
            for (String test : haystack) {
                if (Objects.equals(test, needle)) {
                    return true;
                }
            }
        }
        return false;
    }
}
