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

package android.systemui.cts;

import android.util.Log;

import androidx.test.runner.screenshot.BasicScreenCaptureProcessor;
import androidx.test.runner.screenshot.ScreenCapture;
import androidx.test.runner.screenshot.Screenshot;

import org.junit.rules.TestWatcher;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import java.io.IOException;
import java.nio.file.FileSystems;

public class ScreenshotTestRule extends TestWatcher {
    private String mBasePath;
    private String mClassName;
    private String mTestMethod;
    private boolean mFinish;

    private static class ScreenCaptureProcessor extends BasicScreenCaptureProcessor {
        ScreenCaptureProcessor(String basePath) {
            super();
            try {
                mDefaultScreenshotPath = FileSystems.getDefault()
                        .getPath(basePath).toFile().getCanonicalFile();
            } catch (IOException e) {
                Log.e("ScreenshotTestRule", "Can't create directory.", e);
            }
        }
    }

    public ScreenshotTestRule(String basePath) {
        super();
        mBasePath = basePath;
    }

    @Override
    public Statement apply(Statement base, Description description) {
        mClassName = description.getTestClass().getSimpleName();
        mTestMethod = description.getMethodName();
        mFinish = false;
        return super.apply(base, description);
    }

    private void capture(String fileName) {
        ScreenCapture screenCapture = Screenshot.capture();
        screenCapture.setName(fileName);
        ScreenCaptureProcessor processors = new ScreenCaptureProcessor(mBasePath);

        try {
            processors.process(screenCapture);
        } catch (IOException e) {
            Log.e("ScreenshotTestRule", "Can't screenshot.", e);
        }
    }

    private void capture(Description description, String result) {
        String fileName = description.getTestClass().getSimpleName() + "#"
                + description.getMethodName();
        capture(fileName + "_" + result);
    }

    public void capture() {
        capture(mClassName + "#" + mTestMethod);
    }

    @Override
    protected void finished(Description description) {
        super.finished(description);
        mFinish = true;
    }

    @Override
    protected void failed(Throwable throwable, Description description) {
        super.failed(throwable, description);
        if (mFinish) {
            return;
        }
        mFinish = true;

        capture(description, "failed");
    }
}
