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

package android.server.wm;

import android.graphics.Bitmap;
import android.util.Log;

import com.android.compatibility.common.util.BitmapUtils;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import java.io.File;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;

/**
 * A {@code TestRule} that allows dumping data on test failure.
 *
 * <p>Note: when using other {@code TestRule}s, make sure to use a {@code RuleChain} to ensure it
 * is applied outside of other rules that can fail a test (otherwise this rule may not know that the
 * test failed).
 *
 * <p>To capture the output of this rule, add the following to AndroidTest.xml:
 * <pre>
 *  <!-- Collect output of DumpOnFailure. -->
 *  <metrics_collector class="com.android.tradefed.device.metric.FilePullerLogCollector">
 *    <option name="directory-keys" value="/sdcard/DumpOnFailure" />
 *    <option name="collect-on-run-ended-only" value="true" />
 *  </metrics_collector>
 * </pre>
 * <p>And disable external storage isolation:
 * <pre>
 *  <uses-permission android:name="android.permission.MANAGE_EXTERNAL_STORAGE" />
 *  <application ... android:requestLegacyExternalStorage="true" ... >
 * </pre>
 */
public class DumpOnFailure implements TestRule {

    private static final String TAG = "DumpOnFailure";

    private final Map<String, Bitmap> mDumpOnFailureBitmaps = new HashMap<>();

    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                onTestSetup(description);
                try {
                    base.evaluate();
                } catch (Throwable t) {
                    onTestFailure(description, t);
                    throw t;
                } finally {
                    onTestTeardown(description);
                }
            }
        };
    }

    private void onTestSetup(Description description) {
        cleanDir(getDumpRoot(description).toFile());
        mDumpOnFailureBitmaps.clear();
    }

    private void onTestTeardown(Description description) {
        mDumpOnFailureBitmaps.clear();
    }

    private void onTestFailure(Description description, Throwable t) {
        Path root = getDumpRoot(description);
        File rootFile = root.toFile();
        if (!rootFile.exists() && !rootFile.mkdirs()) {
            throw new RuntimeException("Unable to create " + root);
        }

        for (Map.Entry<String, Bitmap> entry : mDumpOnFailureBitmaps.entrySet()) {
            String fileName = getFilename(description, entry.getKey(), "png");
            Log.i(TAG, "Dumping " + root + "/" + fileName);
            BitmapUtils.saveBitmap(entry.getValue(), root.toString(), fileName);
        }
    }

    private String getFilename(Description description, String name, String extension) {
        return description.getTestClass().getSimpleName() + "_" + description.getMethodName()
                + "__" + name + "." + extension;
    }

    private Path getDumpRoot(Description description) {
        return Paths.get("/sdcard/DumpOnFailure/", description.getClassName()
                + "_" + description.getMethodName());
    }

    private void cleanDir(File dir) {
        final File[] files = dir.listFiles();
        if (files == null) {
            return;
        }
        for (File file : files) {
            if (!file.isDirectory()) {
                if (!file.delete()) {
                    throw new RuntimeException("Unable to delete " + file);
                }
            }
        }
    }

    /**
     * Dumps the Bitmap if the test fails.
     */
    public void dumpOnFailure(String name, Bitmap bitmap) {
        if (mDumpOnFailureBitmaps.containsKey(name)) {
            int i = 1;
            while (mDumpOnFailureBitmaps.containsKey(name + "_" + i)) {
                ++i;
            }
            name += "_" + i;
        }
        mDumpOnFailureBitmaps.put(name, bitmap);
    }
}
