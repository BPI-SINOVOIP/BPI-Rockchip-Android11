/*
 * Copyright (C) 2008 The Android Open Source Project
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

package android.content.pm.cts;

import static android.content.pm.ApplicationInfo.CATEGORY_MAPS;
import static android.content.pm.ApplicationInfo.CATEGORY_PRODUCTIVITY;
import static android.content.pm.ApplicationInfo.CATEGORY_UNDEFINED;
import static android.content.pm.ApplicationInfo.FLAG_MULTIARCH;
import static android.content.pm.ApplicationInfo.FLAG_SUPPORTS_RTL;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.cts.R;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Parcel;
import android.os.Process;
import android.os.UserHandle;
import android.platform.test.annotations.AppModeFull;
import android.util.StringBuilderPrinter;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test {@link ApplicationInfo}.
 */
@RunWith(AndroidJUnit4.class)
@AppModeFull // TODO(Instant) Figure out which APIs should work.
public class ApplicationInfoTest {
    private static final String SYNC_ACCOUNT_ACCESS_STUB_PACKAGE_NAME = "com.android.cts.stub";
    private static final String DIRECT_BOOT_UNAWARE_PACKAGE_NAME =
            "android.content.cts.directbootunaware";
    private static final String PARTIALLY_DIRECT_BOOT_AWARE_PACKAGE_NAME =
            "android.content.cts.partiallydirectbootaware";

    private ApplicationInfo mApplicationInfo;
    private String mPackageName;

    @Before
    public void setUp() throws Exception {
        mPackageName = getContext().getPackageName();
    }

    private Context getContext() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    @Test
    public void testConstructor() {
        ApplicationInfo info = new ApplicationInfo();
        // simple test to ensure packageName is copied by copy constructor
        // TODO: consider expanding to check all member variables
        info.packageName = mPackageName;
        ApplicationInfo copy = new ApplicationInfo(info);
        assertEquals(info.packageName, copy.packageName);
    }

    @Test
    public void testWriteToParcel() throws NameNotFoundException {
        mApplicationInfo = getContext().getPackageManager().getApplicationInfo(mPackageName, 0);

        Parcel p = Parcel.obtain();
        mApplicationInfo.writeToParcel(p, 0);

        p.setDataPosition(0);
        ApplicationInfo info = ApplicationInfo.CREATOR.createFromParcel(p);
        assertEquals(mApplicationInfo.taskAffinity, info.taskAffinity);
        assertEquals(mApplicationInfo.permission, info.permission);
        assertEquals(mApplicationInfo.processName, info.processName);
        assertEquals(mApplicationInfo.className, info.className);
        assertEquals(mApplicationInfo.theme, info.theme);
        assertEquals(mApplicationInfo.flags, info.flags);
        assertEquals(mApplicationInfo.sourceDir, info.sourceDir);
        assertEquals(mApplicationInfo.publicSourceDir, info.publicSourceDir);
        assertEquals(mApplicationInfo.sharedLibraryFiles, info.sharedLibraryFiles);
        assertEquals(mApplicationInfo.dataDir, info.dataDir);
        assertEquals(mApplicationInfo.uid, info.uid);
        assertEquals(mApplicationInfo.enabled, info.enabled);
        assertEquals(mApplicationInfo.manageSpaceActivityName, info.manageSpaceActivityName);
        assertEquals(mApplicationInfo.descriptionRes, info.descriptionRes);
    }

    @Test
    public void testToString() {
        mApplicationInfo = new ApplicationInfo();
        assertNotNull(mApplicationInfo.toString());
    }

    @Test
    public void testDescribeContents() throws NameNotFoundException {
       mApplicationInfo = getContext().getPackageManager().getApplicationInfo(mPackageName, 0);

        assertEquals(0, mApplicationInfo.describeContents());
    }

    @Test
    public void testDump() {
        mApplicationInfo = new ApplicationInfo();

        StringBuilder sb = new StringBuilder();
        assertEquals(0, sb.length());
        StringBuilderPrinter p = new StringBuilderPrinter(sb);

        String prefix = "";
        mApplicationInfo.dump(p, prefix);
        assertNotNull(sb.toString());
        assertTrue(sb.length() > 0);
    }

    @Test
    public void testLoadDescription() throws NameNotFoundException {
        mApplicationInfo = getContext().getPackageManager().getApplicationInfo(mPackageName, 0);

        assertNull(mApplicationInfo.loadDescription(getContext().getPackageManager()));

        mApplicationInfo.descriptionRes = R.string.hello_world;
        assertEquals(getContext().getResources().getString(R.string.hello_world),
                mApplicationInfo.loadDescription(getContext().getPackageManager()));
    }

    @Test
    public void verifyOwnInfo() throws NameNotFoundException {
        mApplicationInfo = getContext().getPackageManager().getApplicationInfo(mPackageName, 0);

        assertEquals("Android TestCase", mApplicationInfo.nonLocalizedLabel);
        assertEquals(R.drawable.size_48x48, mApplicationInfo.icon);
        assertEquals("android.content.cts.MockApplication", mApplicationInfo.name);
        int flags = FLAG_MULTIARCH | FLAG_SUPPORTS_RTL;
        assertEquals(flags, mApplicationInfo.flags & flags);
        assertEquals(CATEGORY_PRODUCTIVITY, mApplicationInfo.category);
    }

    @Test
    public void verifyDefaultValues() throws NameNotFoundException {
        // The application "com.android.cts.stub" does not have any attributes set
        mApplicationInfo = getContext().getPackageManager().getApplicationInfo(
                SYNC_ACCOUNT_ACCESS_STUB_PACKAGE_NAME, 0);
        int currentUserId = Process.myUserHandle().getIdentifier();

        assertNull(mApplicationInfo.className);
        assertNull(mApplicationInfo.permission);
        assertEquals(SYNC_ACCOUNT_ACCESS_STUB_PACKAGE_NAME, mApplicationInfo.packageName);
        assertEquals(SYNC_ACCOUNT_ACCESS_STUB_PACKAGE_NAME, mApplicationInfo.processName);
        assertEquals(SYNC_ACCOUNT_ACCESS_STUB_PACKAGE_NAME, mApplicationInfo.taskAffinity);
        assertTrue(UserHandle.isApp(mApplicationInfo.uid));
        assertEquals(0, mApplicationInfo.theme);
        assertEquals(0, mApplicationInfo.requiresSmallestWidthDp);
        assertEquals(0, mApplicationInfo.compatibleWidthLimitDp);
        assertEquals(0, mApplicationInfo.largestWidthLimitDp);
        assertNotNull(mApplicationInfo.sourceDir);
        assertEquals(mApplicationInfo.sourceDir, mApplicationInfo.publicSourceDir);
        assertNull(mApplicationInfo.splitSourceDirs);
        assertArrayEquals(mApplicationInfo.splitSourceDirs, mApplicationInfo.splitPublicSourceDirs);
        assertEquals("/data/user/" + currentUserId + "/" + SYNC_ACCOUNT_ACCESS_STUB_PACKAGE_NAME,
                mApplicationInfo.dataDir);
        assertEquals("/data/user_de/" + currentUserId + "/" + SYNC_ACCOUNT_ACCESS_STUB_PACKAGE_NAME,
                mApplicationInfo.deviceProtectedDataDir);
        assertEquals("/data/user/" + currentUserId + "/" + SYNC_ACCOUNT_ACCESS_STUB_PACKAGE_NAME,
                mApplicationInfo.credentialProtectedDataDir);
        assertNull(mApplicationInfo.sharedLibraryFiles);
        assertTrue(mApplicationInfo.enabled);
        assertNull(mApplicationInfo.manageSpaceActivityName);
        assertEquals(0, mApplicationInfo.descriptionRes);
        assertEquals(0, mApplicationInfo.uiOptions);
        assertEquals(CATEGORY_UNDEFINED, mApplicationInfo.category);
    }

    @Test(expected=IllegalArgumentException.class)
    public void setOwnAppCategory() throws Exception {
        getContext().getPackageManager().setApplicationCategoryHint(getContext().getPackageName(),
                CATEGORY_MAPS);
    }

    @Test(expected=IllegalArgumentException.class)
    public void setAppCategoryByNotInstaller() throws Exception {
        getContext().getPackageManager().setApplicationCategoryHint(
                SYNC_ACCOUNT_ACCESS_STUB_PACKAGE_NAME, CATEGORY_MAPS);
    }

    @Test
    public void testDirectBootUnawareAppIsNotEncryptionAware() throws Exception {
        ApplicationInfo applicationInfo = getContext().getPackageManager().getApplicationInfo(
                DIRECT_BOOT_UNAWARE_PACKAGE_NAME, 0);
        assertFalse(applicationInfo.isEncryptionAware());
    }

    @Test
    public void testPartiallyDirectBootAwareAppIsEncryptionAware() throws Exception {
        ApplicationInfo applicationInfo = getContext().getPackageManager().getApplicationInfo(
                PARTIALLY_DIRECT_BOOT_AWARE_PACKAGE_NAME, 0);
        assertTrue(applicationInfo.isEncryptionAware());
    }

    @Test
    public void testWriteToParcelDontSquash() throws Exception {
        // Make sure ApplicationInfo.writeToParcel() doesn't do the "squashing",
        // because Parcel.allowSquashing() isn't called.

        mApplicationInfo = getContext().getPackageManager().getApplicationInfo(mPackageName, 0);

        final Parcel p = Parcel.obtain();
        mApplicationInfo.writeToParcel(p, 0);
        mApplicationInfo.writeToParcel(p, 0);

        // Don't call Parcel.allowSquashing()

        p.setDataPosition(0);
        final ApplicationInfo copy1 = ApplicationInfo.CREATOR.createFromParcel(p);
        final ApplicationInfo copy2 = ApplicationInfo.CREATOR.createFromParcel(p);

        p.recycle();

        assertNotSame(mApplicationInfo, copy1);

        // writeToParcel() doesn't do the squashing, so copy1 and copy2 will be different.
        assertNotSame(copy1, copy2);

        // Check several fields to make sure they're properly copied.
        assertEquals(mApplicationInfo.packageName, copy2.packageName);
        assertEquals(copy1.packageName, copy2.packageName);

        assertEquals(mApplicationInfo.flags, copy2.flags);
        assertEquals(copy1.flags, copy2.flags);
    }

    @Test
    public void testWriteToParcelSquash() throws Exception {
        // Make sure ApplicationInfo.writeToParcel() does the "squashing", after
        // Parcel.allowSquashing() is called.

        mApplicationInfo = getContext().getPackageManager().getApplicationInfo(mPackageName, 0);

        final Parcel p = Parcel.obtain();

        final boolean prevSquashing = p.allowSquashing(); // Allow squashing.

        mApplicationInfo.writeToParcel(p, 0);
        mApplicationInfo.writeToParcel(p, 0);

        p.setDataPosition(0);
        final ApplicationInfo copy1 = ApplicationInfo.CREATOR.createFromParcel(p);
        final ApplicationInfo copy2 = ApplicationInfo.CREATOR.createFromParcel(p);

        p.recycle();

        assertNotSame(mApplicationInfo, copy1);
        assertSame(copy1, copy2); //

        p.restoreAllowSquashing(prevSquashing);
    }
}
