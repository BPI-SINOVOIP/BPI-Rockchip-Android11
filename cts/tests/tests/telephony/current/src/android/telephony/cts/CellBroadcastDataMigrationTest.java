/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.telephony.cts;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.provider.Telephony.CellBroadcasts;
import android.util.Log;
import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import androidx.test.InstrumentationRegistry;

public class CellBroadcastDataMigrationTest {
    private static final String TAG = CellBroadcastDataMigrationTest.class.getSimpleName();
    private static final String DEFAULT_LEGACY_DATA_MIGRATION_APP =
            "com.android.cellbroadcastreceiver";

    /**
     * To support data migration when upgrading from an older device, device need to define
     * legacy content provider. This tests verify that the AOSP legacy cellbroadcast contentprovider
     * only surfaces data for migration. The data should be protected by proper permissions and
     * it should be headless without any other activities, services or providers to handle alerts.
     */
    @Test
    public void testLegacyContentProvider() throws Exception {
        final ProviderInfo legacy = InstrumentationRegistry.getContext().getPackageManager()
                .resolveContentProvider(CellBroadcasts.AUTHORITY_LEGACY, 0);
        if (legacy == null) {
            Log.d(TAG, "Device does not support data migration");
            return;
        }

        // Verify that legacy provider is protected with certain permissions
        assertEquals("Legacy provider at MediaStore.AUTHORITY_LEGACY must protect its data",
                android.Manifest.permission.READ_CELL_BROADCASTS, legacy.readPermission);

        // Skip headless check for OEM defined data migration app. e.g, OEMs might use messaging
        // apps to store CBR data pre-R.
        if (!DEFAULT_LEGACY_DATA_MIGRATION_APP.equals(legacy.applicationInfo.packageName)) {
            Log.d(TAG, "Device support data migration from OEM apps");
            return;
        }

        // And finally verify that legacy provider is headless. We expect the legacy provider only
        // surface the old data for migration rather than handling emergency alerts.
        final PackageInfo legacyPackage = InstrumentationRegistry.getContext().getPackageManager()
                .getPackageInfo(legacy.packageName, PackageManager.GET_ACTIVITIES
                        | PackageManager.GET_PROVIDERS | PackageManager.GET_RECEIVERS
                        | PackageManager.GET_SERVICES);
        assertEmpty("Headless LegacyCellBroadcastContentProvider must have no activities",
                legacyPackage.activities);
        assertEquals("Headless LegacyCellBroadcastContentProvider must have exactly one provider",
                1, legacyPackage.providers.length);
        assertEmpty("Headless LegacyCellBroadcastContentProvider must have no receivers",
                legacyPackage.receivers);
        assertEmpty("Headless LegacyCellBroadcastContentProvider must have no services",
                legacyPackage.services);
    }

    private static <T> void assertEmpty(String message, T[] array) {
        if (array != null && array.length > 0) {
            fail(message);
        }
    }
}