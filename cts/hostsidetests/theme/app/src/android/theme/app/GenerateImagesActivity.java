/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.theme.app;

import static android.theme.app.TestConfiguration.THEMES;

import android.app.Activity;
import android.content.Intent;
import android.os.Build.VERSION;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.WindowManager.LayoutParams;

import java.io.File;
import java.io.IOException;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Generates images by iterating through all themes and launching instances of
 * {@link ThemeDeviceActivity}.
 */
public class GenerateImagesActivity extends Activity {
    private static final String TAG = "GenerateImagesActivity";

    private static final String OUT_DIR = "cts-theme-assets";
    private static final int REQUEST_CODE = 1;

    public static final String EXTRA_REASON = "reason";

    private final CountDownLatch mLatch = new CountDownLatch(1);

    private File mOutputDir;
    private File mOutputZip;

    private int mCurrentTheme;
    private String mFinishReason;
    private boolean mFinishSuccess;

    class CompressOutputThread extends Thread {
        public void run() {
            compressOutput();
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Useful for local testing. Not required for CTS harness.
        getWindow().addFlags(LayoutParams.FLAG_KEEP_SCREEN_ON);

        // Make sure the device has reasonable assets.
        String assetDensityFailureMsg = checkAssetDensity();
        if (assetDensityFailureMsg != null) {
            finish("Failed to verify device assets: "+ assetDensityFailureMsg, false);
        } else {
            mOutputDir = setupOutputDirectory();
            if (mOutputDir == null) {
                finish("Failed to create output directory: " + OUT_DIR, false);
            } else {
                generateNextImage();
            }
        }
    }

    private String checkAssetDensity() {
        AssetBucketVerifier.Result result = AssetBucketVerifier.verifyAssetBucket(this);

        String message;
        if (result.foundAtDensity.contains(result.expectedAtDensity)) {
            message = null;
        } else if (result.foundAtDensity.isEmpty()) {
            message = "Failed to find expected device assets at any density";
        } else {
            StringBuilder foundAtDensityStr = new StringBuilder(result.foundAtDensity.get(0));
            for (int i = 1; i < result.foundAtDensity.size(); i++) {
                foundAtDensityStr.append(", ");
                foundAtDensityStr.append(result.foundAtDensity.get(i));
            }
            message = "Failed to find device assets at expected density ("
                    + result.expectedAtDensity + "), but found at " + foundAtDensityStr;
        }

        return message;
    }

    private File setupOutputDirectory() {
        mOutputDir = new File(Environment.getExternalStorageDirectory(), OUT_DIR);
        ThemeTestUtils.deleteDirectory(mOutputDir);
        mOutputDir.mkdirs();

        if (mOutputDir.exists()) {
            return mOutputDir;
        }
        return null;
    }

    /**
     * @return whether the test finished successfully
     */
    public boolean isFinishSuccess() {
        return mFinishSuccess;
    }

    /**
     * @return user-visible string explaining why the test finished, may be {@code null} if the test
     *         finished unexpectedly
     */
    public String getFinishReason() {
        return mFinishReason;
    }

    /**
     * Starts the activity to generate the next image.
     */
    private void generateNextImage() {
        // Keep trying themes until one works.
        boolean success = false;
        while (mCurrentTheme < THEMES.length && !success) {
            success = launchThemeDeviceActivity();
            mCurrentTheme++;
        }

        // If we ran out of themes, we're done.
        if (!success) {
            CompressOutputThread compressOutputThread = new CompressOutputThread();
            compressOutputThread.start();
        }
    }

    private boolean launchThemeDeviceActivity() {
        final ThemeInfo theme = THEMES[mCurrentTheme];
        if (theme.apiLevel > VERSION.SDK_INT) {
            Log.v(TAG, "Skipping theme \"" + theme.name
                    + "\" (requires API " + theme.apiLevel + ")");
            return false;
        }

        Log.v(TAG, "Generating images for theme \"" + theme.name + "\"...");

        final Intent intent = new Intent(this, ThemeDeviceActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        intent.putExtra(ThemeDeviceActivity.EXTRA_THEME, mCurrentTheme);
        intent.putExtra(ThemeDeviceActivity.EXTRA_OUTPUT_DIR, mOutputDir.getAbsolutePath());
        startActivityForResult(intent, REQUEST_CODE);
        return true;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (resultCode != RESULT_OK) {
            finish("Failed to generate images for theme " + mCurrentTheme + " ("
                    + data.getStringExtra(EXTRA_REASON) + ")", false);
            return;
        }

        generateNextImage();
    }

    private void compressOutput() {
        mOutputZip = new File(mOutputDir.getParentFile(), mOutputDir.getName() + ".zip");

        if (mOutputZip.exists()) {
            // Remove any old test results.
            mOutputZip.delete();
        }

        try {
            ThemeTestUtils.compressDirectory(mOutputDir, mOutputZip);
            ThemeTestUtils.deleteDirectory(mOutputDir);
        } catch (IOException e) {
            e.printStackTrace();
        }
        runOnUiThread(() -> {
            finish("Image generation complete!", true);
        });
    }

    private void finish(String reason, boolean success) {
        mFinishSuccess = success;
        mFinishReason = reason;

        finish();
    }

    @Override
    public void finish() {
        mLatch.countDown();

        super.finish();
    }

    public File getOutputZip() {
        return mOutputZip;
    }

    public boolean waitForCompletion(long timeoutMillis) throws InterruptedException {
        return mLatch.await(timeoutMillis, TimeUnit.MILLISECONDS);
    }
}
