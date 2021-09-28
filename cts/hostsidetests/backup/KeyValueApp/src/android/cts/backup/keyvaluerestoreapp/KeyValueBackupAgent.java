/*
 * Copyright (C) 2017 The Android Open Source Project
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
 * limitations under the License
 */

package android.cts.backup.keyvaluerestoreapp;

import android.app.backup.BackupAgentHelper;
import android.app.backup.BackupDataInput;
import android.app.backup.BackupDataOutput;
import android.os.ParcelFileDescriptor;
import java.io.IOException;

public class KeyValueBackupAgent extends BackupAgentHelper {

    private static final String KEY_BACKUP_TEST_PREFS_PREFIX = "backup-test-prefs";
    private static final String KEY_BACKUP_TEST_FILES_PREFIX = "backup-test-files";

    @Override
    public void onCreate() {
        super.onCreate();
        addHelper(KEY_BACKUP_TEST_PREFS_PREFIX,
                KeyValueBackupRestoreTest.getSharedPreferencesBackupHelper(this));
        addHelper(KEY_BACKUP_TEST_FILES_PREFIX,
                KeyValueBackupRestoreTest.getFileBackupHelper(this));
    }

    @Override
    public void onBackup(ParcelFileDescriptor oldState, BackupDataOutput data,
        ParcelFileDescriptor newState) throws IOException {
        // Explicitly override and call super() to help go/android-api-coverage-dashboard pick up
        // the test coverage (b/113067697).
        super.onBackup(oldState, data, newState);
    }

    @Override
    public void onRestore(BackupDataInput data, int appVersionCode, ParcelFileDescriptor newState)
        throws IOException {
        // Explicitly override and call super() to help go/android-api-coverage-dashboard pick up
        // the test coverage (b/113067697).
        super.onRestore(data, appVersionCode, newState);
    }
}
