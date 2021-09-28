/*
 * Copyright (C) 2018 The Android Open Source Project
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
package android.platform.test.rule;

import android.os.RemoteException;

import org.junit.runner.Description;

/**
 * This rule will lock orientation before running a test class and unlock after.
 */
public class NaturalOrientationRule extends TestWatcher {
    @Override
    protected void starting(Description description) {
        try {
            getUiDevice().setOrientationNatural();
        } catch (RemoteException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    protected void finished(Description description) {
        try {
            getUiDevice().unfreezeRotation();
        } catch (RemoteException e) {
            throw new RuntimeException(e);
        }
    }
}
