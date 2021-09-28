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
package com.android.tradefed.testtype.junit4;

import com.android.tradefed.invoker.TestInformation;

import org.junit.internal.runners.statements.RunBefores;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.Statement;

import java.util.List;

/** @see RunBefores */
public class RunBeforesWithInfo extends Statement {
    private final Statement next;

    private final TestInformation testInfo;

    private final List<FrameworkMethod> befores;

    public RunBeforesWithInfo(
            Statement next, List<FrameworkMethod> befores, TestInformation testInfo) {
        this.next = next;
        this.befores = befores;
        this.testInfo = testInfo;
    }

    @Override
    public void evaluate() throws Throwable {
        for (FrameworkMethod before : befores) {
            before.invokeExplosively(null, testInfo);
        }
        next.evaluate();
    }
}
