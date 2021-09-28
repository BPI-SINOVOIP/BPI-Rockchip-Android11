/*
 * Copyright 2020 The Android Open Source Project
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
package com.android.cts.blob;

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner;

import org.junit.rules.RunRules;
import org.junit.rules.TestRule;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.Statement;

import java.util.List;

/**
 * Custom runner to allow dumping logs after a test failure before the @After methods get to run.
 */
public class BlobStoreTestRunner extends AndroidJUnit4ClassRunner {
    private TestRule mDumpOnFailureRule = new DumpOnFailureRule();

    public BlobStoreTestRunner(Class<?> klass) throws InitializationError {
        super(klass);
    }

    @Override
    public Statement methodInvoker(FrameworkMethod method, Object test) {
        return new RunRules(super.methodInvoker(method, test), List.of(mDumpOnFailureRule),
                describeChild(method));
    }
}
