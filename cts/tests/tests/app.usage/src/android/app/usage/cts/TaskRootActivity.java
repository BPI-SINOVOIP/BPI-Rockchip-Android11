/**
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */
package android.app.usage.cts;

import android.annotation.Nullable;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.Until;
import android.view.WindowManager;

import androidx.test.InstrumentationRegistry;

public class TaskRootActivity extends Activity  {
    public static final String TEST_APP_PKG = "android.app.usage.cts.test1";
    public static final String TEST_APP_CLASS = "android.app.usage.cts.test1.SomeActivity";
    private static final long TIMEOUT = 5000;
    private UiDevice mUiDevice;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    @Override
    protected void onResume() {
        super.onResume();
        Intent intent = new Intent();
        intent.setClassName(TEST_APP_PKG, TEST_APP_CLASS);
        startActivity(intent);
        mUiDevice.wait(Until.hasObject(By.clazz(TEST_APP_PKG, TEST_APP_CLASS)), TIMEOUT);
    }
}
