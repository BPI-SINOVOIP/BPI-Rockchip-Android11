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

package android.os.storage.cts;

import static android.os.UserHandle.MIN_SECONDARY_USER_ID;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeTrue;

import android.Manifest;
import android.app.UiAutomation;
import android.app.usage.StorageStatsManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Process;
import android.os.SystemProperties;
import android.os.UserHandle;
import android.os.storage.CrateInfo;
import android.os.storage.StorageManager;
import android.text.TextUtils;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.google.common.truth.Correspondence;

import org.junit.After;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.junit.runner.RunWith;

import java.io.IOException;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.Collection;
import java.util.UUID;

@RunWith(AndroidJUnit4.class)
public class StorageStatsManagerTest {
    private static final String CRATES_ROOT = "crates";
    private final static boolean ENABLE_STORAGE_CRATES =
            SystemProperties.getBoolean("fw.storage_crates", false);

    private Context mContext;
    private StorageManager mStorageManager;
    private StorageStatsManager mStorageStatsManager;

    @Rule
    public TestName mTestName = new TestName();
    private Path mCratesPath;
    private Path mCrateDirPath;
    private UUID mUuid;
    private String mCrateId;

    private void cleanAllOfCrates() throws IOException {
        if (!mCratesPath.toFile().exists()) {
            return;
        }

        Files.walkFileTree(mCratesPath, new SimpleFileVisitor<Path>() {
            @Override
            public FileVisitResult visitFile(Path file, BasicFileAttributes attrs)
                    throws IOException {
                Files.deleteIfExists(file);
                return super.visitFile(file, attrs);
            }

            @Override
            public FileVisitResult postVisitDirectory(Path dir, IOException exc)
                    throws IOException {
                Files.deleteIfExists(dir);
                return super.postVisitDirectory(dir, exc);
            }
        });
        Files.deleteIfExists(mCratesPath);
    }

    /**
     * Setup the necessary member field used by test methods.
     */
    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mStorageManager = (StorageManager) mContext.getSystemService(Context.STORAGE_SERVICE);
        mStorageStatsManager =
            (StorageStatsManager) mContext.getSystemService(Context.STORAGE_STATS_SERVICE);

        mCratesPath = mContext.getDataDir().toPath().resolve(CRATES_ROOT);
        cleanAllOfCrates();

        mCrateId = mTestName.getMethodName();

        mCrateDirPath = mCratesPath.resolve(mCrateId);
        mUuid = mStorageManager.getUuidForPath(mCratesPath.toFile());
    }

    /**
     * To clean all of crated folders to prevent from flaky.
     * @throws Exception happened when the tearDown tries to removed all of folders and files.
     */
    @After
    public void tearDown() throws Exception {
        cleanAllOfCrates();
    }

    @Test
    public void queryCratesForUid_noCratedFolder_shouldBeEmpty() throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        Collection<CrateInfo> collection = mStorageStatsManager.queryCratesForUid(mUuid,
                Process.myUid());

        assertThat(collection.size()).isEqualTo(0);
    }

    private Collection<CrateInfo> queryCratesForUser(boolean byShell, UUID uuid,
            UserHandle userHandle)
            throws PackageManager.NameNotFoundException, IOException {
        final UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation()
                .getUiAutomation();
        if (byShell) {
            uiAutomation.adoptShellPermissionIdentity(Manifest.permission.MANAGE_CRATES);
        }
        Collection<CrateInfo> crateInfos = mStorageStatsManager.queryCratesForUser(uuid,
                userHandle);
        if (byShell) {
            uiAutomation.dropShellPermissionIdentity();
        }
        return crateInfos;
    }
    @Test
    public void queryCratesForUser_noCratedFolder_shouldBeEmpty() throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        Collection<CrateInfo> collection = queryCratesForUser(true, mUuid,
                Process.myUserHandle());

        assertThat(collection.size()).isEqualTo(0);
    }

    @Test
    public void queryCratesForPackage_noCratedFolder_shouldBeEmpty() throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        Collection<CrateInfo> collection = mStorageStatsManager.queryCratesForPackage(mUuid,
                mContext.getOpPackageName(), Process.myUserHandle());

        assertThat(collection.size()).isEqualTo(0);
    }

    @Test
    public void queryCratesForUid_withOtherUid_shouldRiseSecurityIssueException() throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        int fakeUid = UserHandle.getUid(MIN_SECONDARY_USER_ID,
                UserHandle.getAppId(Process.myUid()));
        SecurityException securityException = null;

        try {
            mStorageStatsManager.queryCratesForUid(mUuid, fakeUid);
        } catch (SecurityException e) {
            securityException = e;
        }

        assertThat(securityException).isNotNull();
    }

    @Test
    public void queryCratesForUser_withOtherUid_shouldRiseSecurityIssueException()
            throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        UserHandle fakeUserHandle = UserHandle.of(MIN_SECONDARY_USER_ID);
        SecurityException securityException = null;

        try {
            mStorageStatsManager.queryCratesForUser(mUuid, fakeUserHandle);
        } catch (SecurityException e) {
            securityException = e;
        }

        assertThat(securityException).isNotNull();
    }

    @Test
    public void queryCratesForPackage_withOtherUid_shouldRiseSecurityIssueException()
            throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        UserHandle fakeUserHandle = UserHandle.of(MIN_SECONDARY_USER_ID);
        SecurityException securityException = null;

        try {
            mStorageStatsManager.queryCratesForPackage(mUuid,
                    mContext.getOpPackageName(), fakeUserHandle);
        } catch (SecurityException e) {
            securityException = e;
        }

        assertThat(securityException).isNotNull();
    }

    @Test
    public void queryCratesForUid_addOneDirectory_shouldIncreasingOneCrate()
            throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        Collection<CrateInfo> originalCollection = mStorageStatsManager.queryCratesForUid(
                mUuid, Process.myUid());

        mContext.getCrateDir(mCrateId);

        Collection<CrateInfo> newCollection = mStorageStatsManager.queryCratesForUid(
                mUuid, Process.myUid());
        assertThat(newCollection.size()).isEqualTo(originalCollection.size() + 1);
    }

    @Test
    public void queryCratesForUser_addOneDirectory_shouldIncreasingOneCrate()
            throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        Collection<CrateInfo> originalCollection = queryCratesForUser(true, mUuid,
                Process.myUserHandle());

        mContext.getCrateDir(mCrateId);

        Collection<CrateInfo> newCollection = queryCratesForUser(true,
                mUuid, Process.myUserHandle());
        assertThat(newCollection.size()).isEqualTo(originalCollection.size() + 1);
    }

    @Test
    public void queryCratesForPackage_addOneDirectory_shouldIncreasingOneCrate()
            throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        Collection<CrateInfo> originalCollection = mStorageStatsManager.queryCratesForPackage(
                mUuid, mContext.getOpPackageName(), Process.myUserHandle());

        mContext.getCrateDir(mCrateId);

        Collection<CrateInfo> newCollection = mStorageStatsManager.queryCratesForPackage(
                mUuid, mContext.getOpPackageName(), Process.myUserHandle());
        assertThat(newCollection.size()).isEqualTo(originalCollection.size() + 1);
    }

    @Test
    public void queryCratesForUid_withoutSetCrateInfo_labelShouldTheSameWithFolderName()
            throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        mContext.getCrateDir(mCrateId);

        Collection<CrateInfo> crateInfos = mStorageStatsManager.queryCratesForUid(
                mUuid, Process.myUid());

        assertThat(crateInfos.iterator().next().getLabel()).isEqualTo(mTestName.getMethodName());
    }

    @Test
    public void queryCratesForUser_withoutSetCrateInfo_labelShouldTheSameWithFolderName()
            throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        mContext.getCrateDir(mCrateId);

        Collection<CrateInfo> crateInfos =  queryCratesForUser(true, mUuid,
                Process.myUserHandle());

        assertThat(crateInfos.iterator().next().getLabel()).isEqualTo(mTestName.getMethodName());
    }

    @Test
    public void queryCratesForPackage_withoutSetCrateInfo_labelShouldTheSameWithFolderName()
            throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        mContext.getCrateDir(mCrateId);

        Collection<CrateInfo> crateInfos = mStorageStatsManager.queryCratesForPackage(
                mUuid, mContext.getOpPackageName(), Process.myUserHandle());

        assertThat(crateInfos.iterator().next().getLabel()).isEqualTo(mTestName.getMethodName());
    }

    @Test
    public void queryCratesForUid_withoutSetCrateInfo_expirationShouldBeZero()
            throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        mContext.getCrateDir(mCrateId);

        Collection<CrateInfo> crateInfos = mStorageStatsManager.queryCratesForUid(
                mUuid, Process.myUid());

        assertThat(crateInfos.iterator().next().getExpirationMillis()).isEqualTo(0);
    }

    @Test
    public void queryCratesForUser_withoutSetCrateInfo_expirationShouldBeZero()
            throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        mContext.getCrateDir(mCrateId);

        Collection<CrateInfo> crateInfos =  queryCratesForUser(true, mUuid,
                Process.myUserHandle());

        assertThat(crateInfos.iterator().next().getExpirationMillis()).isEqualTo(0);
    }

    @Test
    public void queryCratesForPackage_withoutSetCrateInfo_expirationShouldBeZero()
            throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        mContext.getCrateDir(mCrateId);

        Collection<CrateInfo> crateInfos = mStorageStatsManager.queryCratesForPackage(
                mUuid, mContext.getOpPackageName(), Process.myUserHandle());

        assertThat(crateInfos.iterator().next().getExpirationMillis()).isEqualTo(0);
    }

    @Test
    public void queryCratesForUid_removeCratedDir_shouldDecreaseTheNumberOfCrates()
            throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        for (int i = 0; i < 3; i++) {
            mContext.getCrateDir(mCrateId + "_" + i);
        }
        Collection<CrateInfo> oldCollection = mStorageStatsManager.queryCratesForUid(mUuid,
                Process.myUid());

        Files.deleteIfExists(mContext.getCrateDir(mCrateId + "_" + 1).toPath());

        Collection<CrateInfo> newCollection = mStorageStatsManager.queryCratesForUid(
                mUuid, Process.myUid());
        assertThat(newCollection.size()).isEqualTo(oldCollection.size() - 1);
    }

    @Test
    public void queryCratesForPackage_removeCratedDir_shouldDecreaseTheNumberOfCrates()
            throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        for (int i = 0; i < 3; i++) {
            mContext.getCrateDir(mCrateId + "_" + i);
        }
        Collection<CrateInfo> oldCollection = mStorageStatsManager.queryCratesForPackage(mUuid,
                mContext.getOpPackageName(), Process.myUserHandle());

        Files.deleteIfExists(mContext.getCrateDir(mCrateId + "_" + 1).toPath());

        Collection<CrateInfo> newCollection = mStorageStatsManager.queryCratesForPackage(mUuid,
                mContext.getOpPackageName(), Process.myUserHandle());
        assertThat(newCollection.size()).isEqualTo(oldCollection.size() - 1);
    }

    @Test
    public void queryCratesForUser_removeCratedDir_shouldDecreaseTheNumberOfCrates()
            throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        for (int i = 0; i < 3; i++) {
            mContext.getCrateDir(mCrateId + "_" + i);
        }
        Collection<CrateInfo> oldCollection = queryCratesForUser(true, mUuid,
                Process.myUserHandle());

        Files.deleteIfExists(mContext.getCrateDir(mCrateId + "_" + 1).toPath());

        Collection<CrateInfo> newCollection = queryCratesForUser(true, mUuid,
                Process.myUserHandle());
        assertThat(newCollection.size()).isEqualTo(oldCollection.size() - 1);
    }

    Correspondence<CrateInfo, String> mCorrespondenceByLabel = new Correspondence<>() {
        @Override
        public boolean compare(CrateInfo crateInfo, String expect) {
            return TextUtils.equals(crateInfo.getLabel(), expect);
        }

        @Override
        public String toString() {
            return "It should be the crated folder name";
        }
    };

    @Test
    public void queryCratesForUid_createDeepPath_shouldCreateOneCrate()
            throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        final Path threeLevelPath = mCrateDirPath.resolve("1").resolve("2").resolve("3");
        Files.createDirectories(threeLevelPath);

        Collection<CrateInfo> crateInfos = mStorageStatsManager.queryCratesForUid(
                mUuid, Process.myUid());

        assertThat(crateInfos).comparingElementsUsing(mCorrespondenceByLabel)
                .containsExactly(mCrateId);
    }

    @Test
    public void queryCratesForUser_createDeepPath_shouldCreateOneCrate()
            throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        final Path threeLevelPath = mCrateDirPath.resolve("1").resolve("2").resolve("3");
        Files.createDirectories(threeLevelPath);

        Collection<CrateInfo> crateInfos = queryCratesForUser(true, mUuid,
                Process.myUserHandle());

        assertThat(crateInfos).comparingElementsUsing(mCorrespondenceByLabel)
                .containsExactly(mCrateId);
    }

    @Test
    public void queryCratesForPackage_createDeepPath_shouldCreateOneCrate()
            throws Exception {
        assumeTrue("only test on the device with storage crates feature", ENABLE_STORAGE_CRATES);
        final Path threeLevelPath = mCrateDirPath.resolve("1").resolve("2").resolve("3");
        Files.createDirectories(threeLevelPath);

        Collection<CrateInfo> crateInfos = mStorageStatsManager.queryCratesForPackage(
                mUuid, mContext.getOpPackageName(), Process.myUserHandle());

        assertThat(crateInfos).comparingElementsUsing(mCorrespondenceByLabel)
                .containsExactly(mCrateId);
    }
}
