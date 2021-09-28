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

package android.os.cts;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;

import android.os.SystemConfigManager;
import android.text.TextUtils;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;
import java.util.Map;
import java.util.Set;

@RunWith(AndroidJUnit4.class)
// These tests are disabled for now because selinux doesn't allow CTS to access
// system_config_service. Re-enable once the appropriate changes are in.
public class SystemConfigTest {
    @Test
    @Ignore
    public void testGetDisabledUntilUsedPreinstalledCarrierApps() throws Exception {
        SystemConfigManager systemConfigManager =
                InstrumentationRegistry.getContext().getSystemService(SystemConfigManager.class);
        Set<String> carrierApps = SystemUtil.callWithShellPermissionIdentity(
                systemConfigManager::getDisabledUntilUsedPreinstalledCarrierApps);
        // We don't know which packages are going to be on the device, so just make sure it's not
        // null and any strings inside are nonempty.
        assertNotNull(carrierApps);
        for (String app : carrierApps) {
            assertFalse("pre-installed carrier apps contains an empty string",
                    TextUtils.isEmpty(app));
        }
    }

    @Test
    @Ignore
    public void testGetDisabledUntilUsedPreinstalledCarrierAssociatedApps() throws Exception {
        SystemConfigManager systemConfigManager =
                InstrumentationRegistry.getContext().getSystemService(SystemConfigManager.class);
        Map<String, List<String>> helperApps = SystemUtil.callWithShellPermissionIdentity(
                systemConfigManager::getDisabledUntilUsedPreinstalledCarrierAssociatedApps);
        // We don't know which packages are going to be on the device, so just make sure it's not
        // null and any strings inside are nonempty.
        assertNotNull(helperApps);
        for (String app : helperApps.keySet()) {
            assertFalse("pre-installed carrier apps contains an empty string",
                    TextUtils.isEmpty(app));
            List<String> helperAppsForApp = helperApps.get(app);
            assertNotNull(helperAppsForApp);
            for (String helperApp : helperAppsForApp) {
                assertFalse("pre-installed carrier helper apps contains an empty string",
                        TextUtils.isEmpty(helperApp));
            }
        }
    }
}
