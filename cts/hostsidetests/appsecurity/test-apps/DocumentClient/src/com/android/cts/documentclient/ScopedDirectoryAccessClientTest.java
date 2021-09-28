/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.cts.documentclient;

import static android.os.Environment.DIRECTORY_ALARMS;
import static android.os.Environment.DIRECTORY_DCIM;
import static android.os.Environment.DIRECTORY_DOCUMENTS;
import static android.os.Environment.DIRECTORY_DOWNLOADS;
import static android.os.Environment.DIRECTORY_MOVIES;
import static android.os.Environment.DIRECTORY_MUSIC;
import static android.os.Environment.DIRECTORY_NOTIFICATIONS;
import static android.os.Environment.DIRECTORY_PICTURES;
import static android.os.Environment.DIRECTORY_PODCASTS;
import static android.os.Environment.DIRECTORY_RINGTONES;

import android.content.Context;
import android.content.Intent;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;

import java.util.List;

/**
 * Set of tests that verify behavior of the deprecated Scoped Directory Access API.
 */
public class ScopedDirectoryAccessClientTest extends DocumentsClientTestCase {
    private static final String TAG = "ScopedDirectoryAccessClientTest";

    private static final String DIRECTORY_ROOT = null;

    private static final String[] STANDARD_DIRECTORIES = {
        DIRECTORY_MUSIC,
        DIRECTORY_PODCASTS,
        DIRECTORY_RINGTONES,
        DIRECTORY_ALARMS,
        DIRECTORY_NOTIFICATIONS,
        DIRECTORY_PICTURES,
        DIRECTORY_MOVIES,
        DIRECTORY_DOWNLOADS,
        DIRECTORY_DCIM,
        DIRECTORY_DOCUMENTS
    };

    @Override
    public void setUp() throws Exception {
        super.setUp();

        // DocumentsUI caches some info like whether a user rejects a request, so we need to clear
        // its data before each test.
        clearDocumentsUi();
    }

    public void testInvalidPath() {
        if (!supportedHardwareForScopedDirectoryAccess()) return;

        for (StorageVolume volume : getVolumes()) {
            openExternalDirectoryInvalidPath(volume, "");
            openExternalDirectoryInvalidPath(volume, "/dev/null");
            openExternalDirectoryInvalidPath(volume, "/../");
            openExternalDirectoryInvalidPath(volume, "/HiddenStuff");
        }
        openExternalDirectoryInvalidPath(getPrimaryVolume(), DIRECTORY_ROOT);
    }

    public void testActivityFailsForAllVolumesAndDirectories() {
        if (!supportedHardwareForScopedDirectoryAccess()) return;

        for (StorageVolume volume : getVolumes()) {
            // Tests user clicking DENY button, for all valid directories.
            for (String dir : STANDARD_DIRECTORIES) {
                sendOpenExternalDirectoryIntent(volume, dir);
                assertActivityFailed();
            }
            if (!volume.isPrimary()) {
                // Also test root
                sendOpenExternalDirectoryIntent(volume, DIRECTORY_ROOT);
                assertActivityFailed();
            }
        }
    }

    private void openExternalDirectoryInvalidPath(StorageVolume volume, String directoryName) {
        final Intent intent = volume.createAccessIntent(directoryName);
        assertNull("should not get intent for volume '" + volume + "' and directory '"
                + directoryName + "'", intent);
    }

    private void sendOpenExternalDirectoryIntent(StorageVolume volume, String directoryName) {
        final Intent intent = volume.createAccessIntent(directoryName);
        assertNotNull("no intent for '" + volume + "' and directory " + directoryName, intent);
        mActivity.startActivityForResult(intent, REQUEST_CODE);
        mDevice.waitForIdle();
    }

    private List<StorageVolume> getVolumes() {
        final StorageManager sm = (StorageManager)
                getInstrumentation().getTargetContext().getSystemService(Context.STORAGE_SERVICE);
        final List<StorageVolume> volumes = sm.getStorageVolumes();
        assertTrue("empty volumes", !volumes.isEmpty());
        return volumes;
    }

    private StorageVolume getPrimaryVolume() {
        final StorageManager sm = (StorageManager)
                getInstrumentation().getTargetContext().getSystemService(Context.STORAGE_SERVICE);
        return sm.getPrimaryStorageVolume();
    }
}
