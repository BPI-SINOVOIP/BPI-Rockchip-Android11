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

import static android.theme.app.TestConfiguration.LAYOUTS;
import static android.theme.app.TestConfiguration.THEMES;

import android.animation.ValueAnimator;
import android.app.Activity;
import android.app.KeyguardManager;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.theme.app.modifiers.AbstractLayoutModifier;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager.LayoutParams;
import android.widget.DatePicker;

import java.io.File;

/**
 * A activity which display various UI elements with non-modifiable themes.
 */
public class ThemeDeviceActivity extends Activity {
    public static final String EXTRA_THEME = "theme";
    public static final String EXTRA_OUTPUT_DIR = "outputDir";

    private static final String TAG = "ThemeDeviceActivity";

    /**
     * Delay that allows the Holo-style CalendarView to settle to its final
     * position.
     */
    private static final long HOLO_CALENDAR_VIEW_ADJUSTMENT_DURATION = 540;

    private ThemeInfo mTheme;
    private ReferenceViewGroup mViewGroup;
    private File mOutputDir;
    private int mLayoutIndex;
    private boolean mIsRunning;

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);

        final Intent intent = getIntent();
        final int themeIndex = intent.getIntExtra(EXTRA_THEME, -1);
        if (themeIndex < 0) {
            Log.e(TAG, "No theme specified");
            finish();
        }

        final String outputDir = intent.getStringExtra(EXTRA_OUTPUT_DIR);
        if (outputDir == null) {
            Log.e(TAG, "No output directory specified");
            finish();
        }

        mOutputDir = new File(outputDir);
        mTheme = THEMES[themeIndex];

        // Disable animations.
        ValueAnimator.setDurationScale(0);

        // Force text scaling to 1.0 regardless of system default.
        Configuration config = new Configuration();
        config.fontScale = 1.0f;

        Context inflationContext = createConfigurationContext(config);
        inflationContext.setTheme(mTheme.id);

        LayoutInflater layoutInflater = LayoutInflater.from(inflationContext);
        setContentView(layoutInflater.inflate(R.layout.theme_test, null));

        mViewGroup = findViewById(R.id.reference_view_group);

        getWindow().addFlags(LayoutParams.FLAG_KEEP_SCREEN_ON);

        // The activity has been created, but we don't want to start image generation until various
        // asynchronous conditions are satisfied.
        new ConditionCheck(this, () -> setNextLayout(), message -> finish(message, false))
                .addCondition("Device is unlocked",
                        () -> !getSystemService(KeyguardManager.class).isDeviceLocked())
                .addCondition("Window is focused",
                        () -> hasWindowFocus())
                .addCondition("Activity is resumed",
                        () -> mIsRunning)
                .start();
    }

    @Override
    protected void onResume() {
        super.onResume();

        mIsRunning = true;
    }

    @Override
    protected void onPause() {
        mIsRunning = false;

        if (!isFinishing()) {
            // The activity paused for some reason, likely a system crash
            // dialog. Finish it so we can move to the next theme.
            Log.e(TAG, "onPause() called without a call to finish(), check system dialogs");
        }

        super.onPause();
    }

    @Override
    protected void onDestroy() {
        if (mLayoutIndex < LAYOUTS.length) {
            finish("Only rendered " + mLayoutIndex + "/" + LAYOUTS.length + " layouts", false);
        }

        super.onDestroy();
    }

    /**
     * Sets the next layout in the UI.
     */
    private void setNextLayout() {
        if (mLayoutIndex >= LAYOUTS.length) {
            finish("Rendered all layouts", true);
            return;
        }

        mViewGroup.removeAllViews();

        final LayoutInfo layout = LAYOUTS[mLayoutIndex++];
        final AbstractLayoutModifier modifier = layout.modifier;
        final String layoutName = String.format("%s_%s", mTheme.name, layout.name);
        final LayoutInflater layoutInflater = LayoutInflater.from(mViewGroup.getContext());
        final View view = layoutInflater.inflate(layout.id, mViewGroup, false);
        if (modifier != null) {
            modifier.modifyViewBeforeAdd(view);
        }
        view.setFocusable(false);

        mViewGroup.addView(view);

        if (modifier != null) {
            modifier.modifyViewAfterAdd(view);
        }

        Log.v(TAG, "Rendering layout " + layoutName
                + " (" + mLayoutIndex + "/" + LAYOUTS.length + ")");

        final Runnable generateBitmapRunnable = () ->
            new BitmapTask(view, layoutName).execute();

        if (view instanceof DatePicker && mTheme.spec == ThemeInfo.HOLO) {
            // The Holo-styled DatePicker uses a CalendarView that has a
            // non-configurable adjustment duration of 540ms.
            view.postDelayed(generateBitmapRunnable, HOLO_CALENDAR_VIEW_ADJUSTMENT_DURATION);
        } else {
            view.post(generateBitmapRunnable);
        }
    }

    private void finish(String reason, boolean success) {
        final Intent data = new Intent();
        data.putExtra(GenerateImagesActivity.EXTRA_REASON, reason);
        setResult(success ? RESULT_OK : RESULT_CANCELED, data);

        if (!isFinishing()) {
            finish();
        }
    }

    private class BitmapTask extends GenerateBitmapTask {

        public BitmapTask(View view, String name) {
            super(view, mOutputDir, name);
        }

        @Override
        protected void onPostExecute(Boolean success) {
            if (success && mIsRunning) {
                setNextLayout();
            } else {
                finish("Failed to render view to bitmap: " + mName + " (activity running? "
                        + mIsRunning + ")", false);
            }
        }
    }
}
