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

package android.security;

import android.platform.test.annotations.AppModeInstant;
import android.test.AndroidTestCase;
import com.android.compatibility.common.util.PropertyUtil;
import java.io.IOException;


/**
 * Verify the selinux domain for apps running in instant mode
 */
@AppModeInstant
public class SELinuxTargetSdkTest extends SELinuxTargetSdkTestBase
{

    @Override
    public void setUp() throws Exception {
        super.setUp();
        assertTrue("This class is annotated with @AppModeInstant and must never "
                   + "be run as a normal CTS test",
            getContext().getPackageManager().isInstantApp());
    }

    public void testCanNotExecuteFromHomeDir() throws Exception {
        assertFalse(canExecuteFromHomeDir());
    }

    /**
     * Verify the selinux domain.
     */
    public void testAppDomainContext() throws IOException {
        String context = "u:r:ephemeral_app:s0:c[0-9]+,c[0-9]+,c[0-9]+,c[0-9]+";
        String msg = "Instant apps must run in the ephemeral_app selinux domain " +
            "and use the levelFrom=all selector in SELinux seapp_contexts which adds " +
            "four category types to the app's selinux context.\n" +
            "Example expected value: u:r:ephemeral_app:s0:c89,c256,c512,c768\n" +
            "Actual value: ";
        appDomainContext(context, msg);
    }

    /**
     * Verify the selinux context of the app data type.
     */
    public void testAppDataContext() throws Exception {
        String context = "u:object_r:app_data_file:s0:c[0-9]+,c[0-9]+,c[0-9]+,c[0-9]+";
        String msg = "Instant apps must use the app_data_file selinux context and use the " +
            "levelFrom=all selector in SELinux seapp_contexts which adds four category types " +
            "to the app_data_file context.\n" +
            "Example expected value: u:object_r:app_data_file:s0:c89,c256,c512,c768\n" +
            "Actual value: ";
        appDataContext(context, msg);
    }

    public void testDex2oat() throws Exception {
        /*
         * Apps with a vendor image older than Q may access the dex2oat executable through
         * selinux policy on the vendor partition because the permission was granted in public
         * policy for appdomain.
         */
        if (PropertyUtil.isVendorApiLevelNewerThan(28)) {
            checkDex2oatAccess(false);
        }
    }

    public void testNoNetlinkRouteGetlink() throws IOException {
        checkNetlinkRouteGetlink(false);
    }

    public void testNoNetlinkRouteBind() throws IOException {
        checkNetlinkRouteBind(false);
    }
}
