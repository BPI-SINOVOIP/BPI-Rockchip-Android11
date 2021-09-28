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
package com.android.tradefed.lite;

import org.junit.runner.Description;
import org.junit.runner.Result;
import org.junit.runner.Runner;
import org.junit.runner.notification.RunListener;
import org.junit.runner.notification.RunNotifier;

/**
 * Transparently dry runs tests instead of actually executing them.
 *
 * <p>This needs a Description object to know which tests to fake.
 */
public final class DryRunner extends Runner {
    private Description mDesc;

    /**
     * Constructs a new DryRunner using Description object
     *
     * @param desc
     */
    public DryRunner(Description desc) {
        mDesc = desc;
    }

    /** {@inheritDoc} */
    @Override
    public Description getDescription() {
        return mDesc;
    }

    /** {@inheritDoc} */
    @Override
    public void run(RunNotifier notifier) {
        Result res = new Result();
        notifier.fireTestRunStarted(mDesc);
        fakeExecution(notifier, res.createListener(), mDesc);
        notifier.fireTestRunFinished(res);
    }

    private void fakeExecution(RunNotifier notifier, RunListener list, Description desc) {
        if (desc.getMethodName() == null || !desc.getChildren().isEmpty()) {
            for (Description child : desc.getChildren()) {
                fakeExecution(notifier, list, child);
            }
        } else {
            notifier.fireTestStarted(desc);
            notifier.fireTestFinished(desc);
            try {
                list.testStarted(desc);
                list.testFinished(desc);
            } catch (Exception e) {
                // shouldn't be possible because this listener is just collecting
                // results.
                throw new RuntimeException(e);
            }
        }
    }
}
