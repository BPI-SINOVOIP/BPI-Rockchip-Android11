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

import org.junit.internal.runners.statements.RunAfters;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.MultipleFailureException;
import org.junit.runners.model.Statement;

import java.util.ArrayList;
import java.util.List;

/** @see RunAfters */
public class RunAftersWithInfo extends Statement {
    private final Statement next;

    private final TestInformation testInfo;

    private final List<FrameworkMethod> afters;

    public RunAftersWithInfo(
            Statement next, List<FrameworkMethod> afters, TestInformation testInfo) {
        this.next = next;
        this.afters = afters;
        this.testInfo = testInfo;
    }

    @Override
    public void evaluate() throws Throwable {
        List<Throwable> errors = new ArrayList<Throwable>();
        try {
            next.evaluate();
        } catch (Throwable e) {
            errors.add(e);
        } finally {
            for (FrameworkMethod each : afters) {
                try {
                    each.invokeExplosively(null, testInfo);
                } catch (Throwable e) {
                    errors.add(e);
                }
            }
        }
        MultipleFailureException.assertEmpty(errors);
    }
}
