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

package android.view.inputmethod.cts.util;

import android.app.KeyguardManager;
import android.content.Context;
import android.os.PowerManager;

import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

/**
 * {@link TestRule} class that will turn screen on and unlock insecure keyguard when necessary
 * before each test method running.
 */
public class UnlockScreenRule implements TestRule {
    @Override
    public Statement apply(Statement base, Description description) {
        final Context context = InstrumentationRegistry.getInstrumentation().getContext();
        final PowerManager pm = context.getSystemService(PowerManager.class);
        final KeyguardManager km = context.getSystemService(KeyguardManager.class);

        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                if (pm != null && !pm.isInteractive()) {
                    TestUtils.turnScreenOn();
                }
                if (km != null && km.isKeyguardLocked()) {
                    TestUtils.unlockScreen();
                }
                base.evaluate();
            }
        };
    }
}
