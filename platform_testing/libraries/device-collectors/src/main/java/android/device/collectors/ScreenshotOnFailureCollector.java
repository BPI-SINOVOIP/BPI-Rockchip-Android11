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
 * limitations under the License.
 */
package android.device.collectors;

import android.device.collectors.annotations.OptionClass;
import android.graphics.Bitmap;
import android.os.Bundle;
import androidx.annotation.VisibleForTesting;
import android.support.test.uiautomator.UiDevice;
import android.util.Log;

import org.junit.runner.Description;
import org.junit.runner.notification.Failure;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.Map;
import java.util.HashMap;


/**
 * A {@link BaseMetricListener} that captures screenshots when a test fails.
 *
 * <p>This class needs external storage permission. See {@link BaseMetricListener} how to grant
 * external storage permission, especially at install time.
 *
 * <p>Options: -e screenshot-quality [0-100]: set screenshot image quality. Default is 75. -e
 * include-ui-xml [true, false]: include the UI XML on failure too, if true.
 */
@OptionClass(alias = "screenshot-failure-collector")
public class ScreenshotOnFailureCollector extends BaseMetricListener {

    public static final String DEFAULT_DIR = "run_listeners/screenshots";
    public static final String KEY_INCLUDE_XML = "include-ui-xml";
    public static final String KEY_QUALITY = "screenshot-quality";
    public static final int DEFAULT_QUALITY = 75;
    private boolean mIncludeUiXml = false;
    private int mQuality = DEFAULT_QUALITY;

    private File mDestDir;
    private UiDevice mDevice;

    // Tracks the test iterations to ensure that each failure gets unique filenames.
    // Key: test description; value: number of iterations.
    private Map<String, Integer> mTestIterations = new HashMap<String, Integer>();

    public ScreenshotOnFailureCollector() {
        super();
    }

    /**
     * Constructor to simulate receiving the instrumentation arguments. Should not be used except
     * for testing.
     */
    @VisibleForTesting
    ScreenshotOnFailureCollector(Bundle args) {
        super(args);
    }

    @Override
    public void onTestRunStart(DataRecord runData, Description description) {
        Bundle args = getArgsBundle();
        if (args.containsKey(KEY_QUALITY)) {
            try {
                int quality = Integer.parseInt(args.getString(KEY_QUALITY));
                if (quality >= 0 && quality <= 100) {
                    mQuality = quality;
                } else {
                    Log.e(getTag(), String.format("Invalid screenshot quality: %d.", quality));
                }
            } catch (Exception e) {
                Log.e(getTag(), "Failed to parse screenshot quality", e);
            }
        }

        if (args.containsKey(KEY_INCLUDE_XML)) {
            mIncludeUiXml = Boolean.parseBoolean(args.getString(KEY_INCLUDE_XML));
        }

        String dir = DEFAULT_DIR;
        mDestDir = createAndEmptyDirectory(dir);
    }

    @Override
    public void onTestStart(DataRecord testData, Description description) {
        // Track the number of iteration for this test.
        String testName = description.getDisplayName();
        mTestIterations.computeIfPresent(testName, (name, iterations) -> iterations + 1);
        mTestIterations.computeIfAbsent(testName, name -> 1);
    }

    @Override
    public void onTestFail(DataRecord testData, Description description, Failure failure) {
        if (mDestDir == null) {
            return;
        }
        final String fileNameBase =
                String.format("%s.%s", description.getClassName(), description.getMethodName());
        // Omit the iteration number for the first iteration.
        int iteration = mTestIterations.get(description.getDisplayName());
        final String fileName =
                iteration == 1
                        ? fileNameBase
                        : String.join("-", fileNameBase, String.valueOf(iteration));
        // Capture the screenshot first.
        final String pngFileName = String.format("%s-screenshot-on-failure.png", fileName);
        File img = takeScreenshot(pngFileName);
        if (img != null) {
            testData.addFileMetric(String.format("%s_%s", getTag(), img.getName()), img);
        }
        // Capture the UI XML second.
        if (mIncludeUiXml) {
            File uixFile = collectUiXml(fileName);
            if (uixFile != null) {
                testData.addFileMetric(
                        String.format("%s_%s", getTag(), uixFile.getName()), uixFile);
            }
        }
    }

    /** Public so that Mockito can alter its behavior. */
    @VisibleForTesting
    public File takeScreenshot(String fileName) {
        File img = new File(mDestDir, fileName);
        if (img.exists()) {
            Log.w(getTag(), String.format("File exists: %s", img.getAbsolutePath()));
            img.delete();
        }
        try (
                OutputStream out = new BufferedOutputStream(new FileOutputStream(img))
        ){
            screenshotToStream(out);
            out.flush();
            return img;
        } catch (Exception e) {
            Log.e(getTag(), "Unable to save screenshot", e);
            img.delete();
            return null;
        }
    }

    /**
     * Public so that Mockito can alter its behavior.
     */
    @VisibleForTesting
    public void screenshotToStream(OutputStream out) {
        getInstrumentation().getUiAutomation()
                .takeScreenshot().compress(Bitmap.CompressFormat.PNG, mQuality, out);
    }

    /** Public so that Mockito can alter its behavior. */
    @VisibleForTesting
    public File collectUiXml(String fileName) {
        File uixFile = new File(mDestDir, String.format("%s.uix", fileName));
        if (uixFile.exists()) {
            Log.w(getTag(), String.format("File exists: %s.", uixFile.getAbsolutePath()));
            uixFile.delete();
        }
        try {
            getDevice().dumpWindowHierarchy(uixFile);
            return uixFile;
        } catch (IOException e) {
            Log.e(getTag(), "Failed to collect UI XML on failure.");
        }
        return null;
    }

    private UiDevice getDevice() {
        if (mDevice == null) {
            mDevice = UiDevice.getInstance(getInstrumentation());
        }
        return mDevice;
    }
}
