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

package android.cts.backup.backuptransportapp;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.app.backup.BackupTransport;
import android.platform.test.annotations.AppModeFull;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Device side routines to be invoked by the host side BackupTransportHostSideTest. These are not
 * designed to be called in any other way, as they rely on state set up by the host side test.
 */
@RunWith(AndroidJUnit4.class)
@AppModeFull
public class BackupTransportTest {
    private BackupTransport mBackupTransport;

    @Before
    public void setUp() {
        mBackupTransport = new BackupTransport();
    }

    @Test
    public void testName_throwsException() {
        assertThrows(UnsupportedOperationException.class, () -> mBackupTransport.name());
    }

    @Test
    public void testConfigurationIntent_returnsNull() throws Exception {
        assertThat(mBackupTransport.configurationIntent()).isNull();
    }

    @Test
    public void testCurrentDestinationString_throwsException() {
        assertThrows(
                UnsupportedOperationException.class,
                () -> mBackupTransport.currentDestinationString());
    }

    @Test
    public void testDataManagementIntent_returnsNull() {
        assertThat(mBackupTransport.dataManagementIntent()).isNull();
    }

    @Test
    public void testDataManagementIntentLabel_throwsException() {
        assertThrows(
                UnsupportedOperationException.class,
                () -> mBackupTransport.dataManagementIntentLabel());
    }

    @Test
    public void testTransportDirName_throwsException() {
        assertThrows(
                UnsupportedOperationException.class, () -> mBackupTransport.transportDirName());
    }

    @Test
    public void testInitializeDevice_returnsTransportError() {
        assertThat(mBackupTransport.initializeDevice()).isEqualTo(BackupTransport.TRANSPORT_ERROR);
    }

    @Test
    public void testClearBackupData_returnsTransportError() {
        assertThat(mBackupTransport.clearBackupData(null /* packageInfo */))
                .isEqualTo(BackupTransport.TRANSPORT_ERROR);
    }

    @Test
    public void testFinishBackup_returnsTransportError() {
        assertThat(mBackupTransport.finishBackup()).isEqualTo(BackupTransport.TRANSPORT_ERROR);
    }

    @Test
    public void testRequestBackupTime_returnsZero() {
        assertThat(mBackupTransport.requestBackupTime()).isEqualTo(0);
    }

    @Test
    public void testPerformBackupWithFlags_returnsTransportError() {
        assertThat(
                        mBackupTransport.performBackup(
                                null /* packageInfo */, null /* inFd */, 0 /* flags */))
                .isEqualTo(BackupTransport.TRANSPORT_ERROR);
    }

    @Test
    public void testPerformBackup_returnsTransportError() {
        assertThat(mBackupTransport.performBackup(null /* packageInfo */, null /* inFd */))
                .isEqualTo(BackupTransport.TRANSPORT_ERROR);
    }

    @Test
    public void testGetAvailableRestoreSets_returnsNull() {
        assertThat(mBackupTransport.getAvailableRestoreSets()).isNull();
    }

    @Test
    public void testGetCurrentRestoreSet_returnsZero() {
        assertThat(mBackupTransport.getCurrentRestoreSet()).isEqualTo(0);
    }

    @Test
    public void testStartRestore_returnsTransportError() {
        assertThat(mBackupTransport.startRestore(0 /* token */, null /* packages */))
                .isEqualTo(BackupTransport.TRANSPORT_ERROR);
    }

    @Test
    public void testNextRestorePackage_returnsNull() {
        assertThat(mBackupTransport.nextRestorePackage()).isNull();
    }

    @Test
    public void testGetRestoreData_returnsTransportError() {
        assertThat(mBackupTransport.getRestoreData(null /* outFd */))
                .isEqualTo(BackupTransport.TRANSPORT_ERROR);
    }

    @Test
    public void testFinishRestore_throwsException() {
        assertThrows(UnsupportedOperationException.class, () -> mBackupTransport.finishRestore());
    }

    @Test
    public void testRequestFullBackupTime_returnsZero() {
        assertThat(mBackupTransport.requestFullBackupTime()).isEqualTo(0);
    }

    @Test
    public void testPerformFullBackupWithFlags_returnsTransportPackageRejected() {
        assertThat(
                        mBackupTransport.performFullBackup(
                                null /* testPackage */, null /* socket */, 0 /* flags */))
                .isEqualTo(BackupTransport.TRANSPORT_PACKAGE_REJECTED);
    }

    @Test
    public void testPerformFullBackup_returnsTransportPackageRejected() {
        assertThat(mBackupTransport.performFullBackup(null /* targetPackage */, null /* socket */))
                .isEqualTo(BackupTransport.TRANSPORT_PACKAGE_REJECTED);
    }

    @Test
    public void testCheckFullBackupSize_whenZero_returnsTransportOk() {
        assertThat(mBackupTransport.checkFullBackupSize(0)).isEqualTo(BackupTransport.TRANSPORT_OK);
    }

    @Test
    public void testCheckFullBackupSize_whenMaxLong_returnsTransportOk() {
        assertThat(mBackupTransport.checkFullBackupSize(Long.MAX_VALUE))
                .isEqualTo(BackupTransport.TRANSPORT_OK);
    }

    @Test
    public void testSendBackupData_returnsTransportError() {
        assertThat(mBackupTransport.sendBackupData(0 /* numBytes */))
                .isEqualTo(BackupTransport.TRANSPORT_ERROR);
    }

    @Test
    public void testCancelFullBackup_throwsException() {
        assertThrows(
                UnsupportedOperationException.class, () -> mBackupTransport.cancelFullBackup());
    }

    @Test
    public void testIsAppEligibleForBackup_isFullBackup_returnsTrue() {
        assertThat(
                        mBackupTransport.isAppEligibleForBackup(
                                null /* targetPackage */, true /* isFullBackup */))
                .isTrue();
    }

    @Test
    public void testIsAppEligibleForBackup_isNotFullBackup_returnsTrue() {
        assertThat(
                        mBackupTransport.isAppEligibleForBackup(
                                null /* targetPackage */, false /* isFullBackup */))
                .isTrue();
    }

    @Test
    public void testGetBackupQuota_isFullBackup_returnsMaxValue() {
        assertThat(mBackupTransport.getBackupQuota(null /* packageName */, true /* isFullBackup */))
                .isEqualTo(Long.MAX_VALUE);
    }

    @Test
    public void testGetBackupQuota_isNotFullBackup_returnsMaxValue() {
        assertThat(
                        mBackupTransport.getBackupQuota(
                                null /* packageName */, false /* isFullBackup */))
                .isEqualTo(Long.MAX_VALUE);
    }

    @Test
    public void testGetNextFullRestoreDataChunk_returnsZero() {
        assertThat(mBackupTransport.getNextFullRestoreDataChunk(null /* socket */)).isEqualTo(0);
    }

    @Test
    public void testAbortFullRestore_returnsTransportOk() {
        assertThat(mBackupTransport.abortFullRestore()).isEqualTo(BackupTransport.TRANSPORT_OK);
    }

    @Test
    public void testGetTransportFlags_returnsZero() {
        assertThat(mBackupTransport.getTransportFlags()).isEqualTo(0);
    }
}
