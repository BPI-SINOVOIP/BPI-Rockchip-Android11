/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.print.cts;

import static org.junit.Assert.fail;

import android.platform.helpers.exceptions.TestHelperException;
import android.print.PrintManager;
import android.print.test.BasePrintTest;

import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Behavior of Android when a print service is installed
 */
@RunWith(AndroidJUnit4.class)
public class InstallBehavior extends BasePrintTest {
    private static String sPreviousEnabledServices;

    @BeforeClass
    public static void disableAllPrintServices() throws Exception {
        sPreviousEnabledServices = disablePrintServices(null);
    }

    @Before
    @After
    public void uninstallExternalPrintService() throws Exception {
        SystemUtil.runShellCommand(getInstrumentation(),
                "pm uninstall android.print.cts.externalservice");
    }

    @AfterClass
    public static void reenablePrintServices() throws Exception {
        enablePrintServices(sPreviousEnabledServices);
    }

    /**
     * Printers from a newly installed print service should show up immediately.
     *
     * <p>This tests:
     * <ul>
     *     <li>Print services are enabled by default</li>
     *     <li>Print services get added to already running printer discovery sessions</li>
     * </ul>
     */
    @Test
    public void installedServiceIsEnabled() throws Exception {
        getActivity().getSystemService(PrintManager.class).print("printjob",
                createDefaultPrintDocumentAdapter(1), null);

        waitForWriteAdapterCallback(1);

        // Printer should not be available
        try {
            selectPrinter("ExternalServicePrinter", 500);
            fail();
        } catch (TestHelperException expected) {
            // expected
        }

        SystemUtil.runShellCommand(getInstrumentation(),
                "pm install /data/local/tmp/cts/print/CtsExternalPrintService.apk");

        selectPrinter("ExternalServicePrinter", OPERATION_TIMEOUT_MILLIS);

        // Exit print preview and thereby end printing
        mPrintHelper.cancelPrinting();
    }
}
