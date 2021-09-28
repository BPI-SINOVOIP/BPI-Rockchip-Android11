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
package com.android.cts.net.hostside;

import static com.android.cts.net.hostside.AbstractRestrictBackgroundNetworkTestCase.TAG;
import static com.android.cts.net.hostside.AbstractRestrictBackgroundNetworkTestCase.TEST_APP2_PKG;
import static com.android.cts.net.hostside.AbstractRestrictBackgroundNetworkTestCase.TEST_PKG;

import android.os.Environment;
import android.os.FileUtils;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import com.android.compatibility.common.util.OnFailureRule;

import org.junit.AssumptionViolatedException;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

import androidx.test.platform.app.InstrumentationRegistry;

public class DumpOnFailureRule extends OnFailureRule {
    private File mDumpDir = new File(Environment.getExternalStorageDirectory(),
            "CtsHostsideNetworkTests");

    @Override
    public void onTestFailure(Statement base, Description description, Throwable throwable) {
        final String testName = description.getClassName() + "_" + description.getMethodName();

        if (throwable instanceof AssumptionViolatedException) {
            Log.d(TAG, "Skipping test " + testName + ": " + throwable);
            return;
        }

        prepareDumpRootDir();
        final File dumpFile = new File(mDumpDir, "dump-" + testName);
        Log.i(TAG, "Dumping debug info for " + description + ": " + dumpFile.getPath());
        try (FileOutputStream out = new FileOutputStream(dumpFile)) {
            for (String cmd : new String[] {
                    "dumpsys netpolicy",
                    "dumpsys network_management",
                    "dumpsys usagestats " + TEST_PKG + " " + TEST_APP2_PKG,
                    "dumpsys usagestats appstandby",
            }) {
                dumpCommandOutput(out, cmd);
            }
        } catch (FileNotFoundException e) {
            Log.e(TAG, "Error opening file: " + dumpFile, e);
        } catch (IOException e) {
            Log.e(TAG, "Error closing file: " + dumpFile, e);
        }
    }

    void dumpCommandOutput(FileOutputStream out, String cmd) {
        final ParcelFileDescriptor pfd = InstrumentationRegistry.getInstrumentation()
                .getUiAutomation().executeShellCommand(cmd);
        try (FileInputStream in = new ParcelFileDescriptor.AutoCloseInputStream(pfd)) {
            out.write(("Output of '" + cmd + "':\n").getBytes(StandardCharsets.UTF_8));
            FileUtils.copy(in, out);
            out.write("\n\n=================================================================\n\n"
                    .getBytes(StandardCharsets.UTF_8));
        } catch (IOException e) {
            Log.e(TAG, "Error dumping '" + cmd + "'", e);
        }
    }

    void prepareDumpRootDir() {
        if (!mDumpDir.exists() && !mDumpDir.mkdir()) {
            Log.e(TAG, "Error creating " + mDumpDir);
        }
    }
}
