/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.uibench.microbenchmark;

import static com.android.uibench.microbenchmark.UiBenchJankHelper.FIND_OBJECT_TIMEOUT;
import static com.android.uibench.microbenchmark.UiBenchJankHelper.SHORT_TIMEOUT;

import android.os.SystemClock;
import android.platform.helpers.HelperAccessor;
import android.platform.test.microbenchmark.Microbenchmark;
import android.platform.test.rule.NaturalOrientationRule;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.uiautomator.By;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject2;
import androidx.test.uiautomator.Until;

import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(Microbenchmark.class)
public class UiBenchWindowInsetsControllerMicrobenchmark {
    @ClassRule public static NaturalOrientationRule orientationRule = new NaturalOrientationRule();

    private static HelperAccessor<IUiBenchJankHelper> sHelper =
            new HelperAccessor<>(IUiBenchJankHelper.class);

    private static UiObject2 sEditText;
    private static UiDevice sDevice;

    @BeforeClass
    public static void openApp() {
        sHelper.get().openWindowInsetsController();
        sDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        sEditText = sDevice.wait(Until.findObject(By.text("WindowInsetsController")),
                FIND_OBJECT_TIMEOUT);
    }

    @Test
    public void testOpenIme() {
        sEditText.click();
        SystemClock.sleep(SHORT_TIMEOUT);
        sDevice.pressBack();
    }

    @AfterClass
    public static void closeApp() {
        sHelper.get().exit();
    }
}
