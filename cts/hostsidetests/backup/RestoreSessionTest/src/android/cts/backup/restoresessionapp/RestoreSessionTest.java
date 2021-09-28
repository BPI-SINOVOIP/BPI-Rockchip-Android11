/*
 * Copyright (C) 2019 Google Inc.
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

package android.cts.backup.restoresessionapp;

import static androidx.test.InstrumentationRegistry.getTargetContext;
import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertTrue;

import static org.junit.Assert.assertNotEquals;

import android.app.UiAutomation;
import android.app.backup.BackupManager;
import android.app.backup.BackupManagerMonitor;
import android.app.backup.RestoreObserver;
import android.app.backup.RestoreSession;
import android.app.backup.RestoreSet;
import android.content.Context;
import android.os.Bundle;

import androidx.annotation.Nullable;
import android.platform.test.annotations.AppModeFull;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Device side routines to be invoked by the host side RestoreSessionHostSideTest. These are not
 * designed to be called in any other way, as they rely on state set up by the host side test.
 */
@RunWith(AndroidJUnit4.class)
@AppModeFull
public class RestoreSessionTest {
    private static final String[] PACKAGES = new String[] {
        "android.cts.backup.restoresessionapp1",
        "android.cts.backup.restoresessionapp2",
        "android.cts.backup.restoresessionapp3"
    };

    private static final int PACKAGES_COUNT = 3;
    private static final int RESTORE_TIMEOUT_SECONDS = 10;

    private BackupManager mBackupManager;
    private Set<String> mRestorePackages;
    private Set<String> mNonRestorePackages;
    private CountDownLatch mRestoreSetsLatch;
    private CountDownLatch mRestoreObserverLatch;
    private RestoreSession mRestoreSession;
    private UiAutomation mUiAutomation;
    private long mRestoreToken;

    private final RestoreObserver mRestoreObserver =
            new RestoreObserver() {
                @Override
                public void restoreSetsAvailable(RestoreSet[] result) {
                    super.restoreSetsAvailable(result);

                    long token = 0L;

                    for (RestoreSet restoreSet : result) {
                        long restoreToken = restoreSet.token;
                        if (doesRestoreSetContainAllPackages(restoreToken, mRestorePackages)
                                && doesRestoreSetContainAllPackages(
                                        restoreToken, mNonRestorePackages)) {
                            token = restoreSet.token;
                            break;
                        }
                    }

                    mRestoreToken = token;

                    mRestoreSetsLatch.countDown();
                }

                @Override
                public void restoreStarting(int numPackages) {
                    super.restoreStarting(numPackages);

                    assertEquals(
                            "Wrong number of packages in the restore set",
                            mRestorePackages.size(),
                            numPackages);
                    mRestoreObserverLatch.countDown();
                }

                @Override
                public void onUpdate(int nowBeingRestored, String currentPackage) {
                    super.onUpdate(nowBeingRestored, currentPackage);

                    assertTrue(
                            "Restoring package that is not in mRestorePackages",
                            mRestorePackages.contains(currentPackage));
                    mRestoreObserverLatch.countDown();
                }

                @Override
                public void restoreFinished(int error) {
                    super.restoreFinished(error);

                    assertEquals(
                            "Restore finished with error: " + error, BackupManager.SUCCESS, error);
                    mRestoreObserverLatch.countDown();
                }
            };

    @Before
    public void setUp() throws InterruptedException {
        Context context = getTargetContext();
        mBackupManager = new BackupManager(context);

        mRestoreToken = 0L;

        mUiAutomation = getInstrumentation().getUiAutomation();
        mUiAutomation.adoptShellPermissionIdentity();
        mRestoreSession = mBackupManager.beginRestoreSession();
    }

    @After
    public void tearDown() {
        mUiAutomation.dropShellPermissionIdentity();
        mRestoreSession.endRestoreSession();
    }

    /**
     * Restore packages added to mRestorePackages and verify only those packages are restored. Use
     * {@link RestoreSession#restorePackage(String, RestoreObserver)}
     */

    @Test
    public void testRestorePackage() throws InterruptedException {
        initPackagesToRestore(/* packagesCount */ 1);
        testRestorePackagesInternal((BackupManagerMonitor monitor) -> {
            mRestoreSession.restorePackage(
                mRestorePackages.iterator().next(),
                mRestoreObserver);
        }, false);
    }

    /**
     * Restore packages added to mRestorePackages and verify only those packages are restored. Use
     * {@link RestoreSession#restorePackage(String, RestoreObserver, BackupManagerMonitor)}
     */
    @Test
    public void testRestorePackageWithMonitorParam() throws InterruptedException {
        initPackagesToRestore(/* packagesCount */ 1);
        testRestorePackagesInternal((BackupManagerMonitor monitor) -> {
            mRestoreSession.restorePackage(
                mRestorePackages.iterator().next(),
                mRestoreObserver,
                monitor);
        }, true);
    }

    /**
     * Restore packages added to mRestorePackages and verify only those packages are restored. Use
     * {@link RestoreSession#restorePackages(long, RestoreObserver, Set)}
     */
    @Test
    public void testRestorePackages() throws InterruptedException {
        initPackagesToRestore(/* packagesCount */ 2);
        testRestorePackagesInternal((BackupManagerMonitor monitor) -> {
            mRestoreSession.restorePackages(
                mRestoreToken,
                mRestoreObserver,
                mRestorePackages);
        }, false);
    }

    /**
     * Restore packages added to mRestorePackages and verify only those packages are restored. Use
     * {@link RestoreSession#restorePackages(long, RestoreObserver, Set, BackupManagerMonitor)}
     */
    @Test
    public void testRestorePackagesWithMonitorParam() throws InterruptedException {
        initPackagesToRestore(/* packagesCount */ 2);
        testRestorePackagesInternal((BackupManagerMonitor monitor) -> {
            mRestoreSession.restorePackages(
                mRestoreToken,
                mRestoreObserver,
                mRestorePackages,
                monitor);
        }, true);
    }

    private void testRestorePackagesInternal(RestoreRunner restoreRunner, boolean useMonitorParam)
            throws InterruptedException {
        CountDownLatch backupMonitorLatch = null;
        if (useMonitorParam) {
            // Wait for the callbacks from BackupManagerMonitor: one for each package.
            backupMonitorLatch = new CountDownLatch(mRestorePackages.size());
            BackupManagerMonitor backupMonitor = new TestBackupMonitor(backupMonitorLatch);

            loadAvailableRestoreSets(backupMonitor);

            restoreRunner.runRestore(backupMonitor);
        } else {
            loadAvailableRestoreSets(null);
            restoreRunner.runRestore(null);
        }

        awaitResultAndAssertSuccess(mRestoreObserverLatch);
        if (backupMonitorLatch != null) {
            awaitResultAndAssertSuccess(backupMonitorLatch);
        }
    }

    private void loadAvailableRestoreSets(@Nullable BackupManagerMonitor monitor)
            throws InterruptedException {
        mRestoreSetsLatch = new CountDownLatch(1);
        assertEquals(
                BackupManager.SUCCESS, monitor == null
                ? mRestoreSession.getAvailableRestoreSets(mRestoreObserver)
                : mRestoreSession.getAvailableRestoreSets(mRestoreObserver, monitor));
        awaitResultAndAssertSuccess(mRestoreSetsLatch);

        assertNotEquals("Restore set not found", 0L, mRestoreToken);
    }

    private boolean doesRestoreSetContainAllPackages(long restoreToken, Set<String> packages) {
        for (String restorePackage : packages) {
            if (mBackupManager.getAvailableRestoreToken(restorePackage) != restoreToken) {
                return false;
            }
        }
        return true;
    }

    private void awaitResultAndAssertSuccess(CountDownLatch latch) throws InterruptedException {
        boolean waitResult = latch.await(RESTORE_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        assertTrue("Restore timed out", waitResult);
    }

    private void initPackagesToRestore(int packagesCount) {
        mRestorePackages = new HashSet<>();
        mNonRestorePackages = new HashSet<>();

        for (int i = 0; i < PACKAGES_COUNT; i++) {
            if (i < packagesCount) {
                mRestorePackages.add(PACKAGES[i]);
            } else {
                mNonRestorePackages.add(PACKAGES[i]);
            }
        }

        // Wait for the callbacks from RestoreObserver: one for each package from
        // mRestorePackages plus restoreStarting and restoreFinished.
        mRestoreObserverLatch = new CountDownLatch(mRestorePackages.size() + 2);
    }

    private static class TestBackupMonitor extends BackupManagerMonitor {
        private final CountDownLatch mLatch;

        TestBackupMonitor(CountDownLatch latch) {
            mLatch = latch;
        }

        @Override
        public void onEvent(Bundle event) {
            super.onEvent(event);

            int eventType = event.getInt(BackupManagerMonitor.EXTRA_LOG_EVENT_ID);
            assertEquals(
                    "Unexpected event from BackupManagerMonitor: " + eventType,
                    BackupManagerMonitor.LOG_EVENT_ID_VERSIONS_MATCH,
                    eventType);
            mLatch.countDown();
        }
    }

    @FunctionalInterface
    private interface RestoreRunner {
        void runRestore(BackupManagerMonitor monitor) throws InterruptedException;
    }
}
