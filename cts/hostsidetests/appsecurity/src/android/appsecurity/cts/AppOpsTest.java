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

package android.appsecurity.cts;

import android.platform.test.annotations.AppModeFull;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test for app ops behaviors.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
@AppModeFull(reason = "No instant specific behaviors")
public final class AppOpsTest extends BaseHostJUnit4Test {

    @Test
    public void testBadConfigCannotCauseBootLoopEnabled() throws Exception {
        try {
            createBadHistoricalFile();
            setAppOpHistoryParameters("mode=HISTORICAL_MODE_ENABLED_ACTIVE,baseIntervalMillis=1000"
                    + ",intervalMultiplier=10");
            getDevice().reboot();
        } finally {
            setAppOpHistoryParameters(null);
            deleteHistoricalFiles();
        }
    }

    @Test
    public void testBadConfigCannotCauseBootLoopDisabled() throws Exception {
        try {
            createBadHistoricalFile();
            setAppOpHistoryParameters("mode=HISTORICAL_MODE_DISABLED,baseIntervalMillis=1000"
                    + ",intervalMultiplier=10");
            getDevice().reboot();
        } finally {
            setAppOpHistoryParameters(null);
            deleteHistoricalFiles();
        }
    }

    private void setAppOpHistoryParameters(String value) throws Exception {
        getDevice().executeShellCommand("settings put global appop_history_parameters " + value);
    }

    private void createBadHistoricalFile() throws Exception {
        getDevice().executeShellCommand("touch data/system/appops/history/1000.xml");
    }

    private void deleteHistoricalFiles() throws Exception {
        getDevice().executeShellCommand("rm -rf data/system/appops/history");
    }
}
