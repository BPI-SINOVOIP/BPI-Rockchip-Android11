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

package android.cts.backup;

import static org.junit.Assert.assertTrue;

import org.junit.Test;

/** Test verifying the default implementation of {@link BackupTransport} */
// TODO(b/135920714): Move this test to device-side once it's possible to use both system and test
// APIs in the same target.
public class BackupTransportHostSideTest extends BaseBackupHostSideTest {
    private static final String PACKAGE = "android.cts.backup.backuptransportapp";
    private static final String CLASS = PACKAGE + ".BackupTransportTest";
    private static final String APK = "CtsBackupTransportApp.apk";

    @Test
    public void testBackupTransport() throws Exception {
        installPackage(APK);
        assertTrue(runDeviceTests(PACKAGE, CLASS));
        uninstallPackage(PACKAGE);
    }
}
