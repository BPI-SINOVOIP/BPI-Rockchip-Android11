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

package android.dynamicmime.testapp;

import android.app.Instrumentation;
import android.content.Context;
import android.dynamicmime.testapp.assertions.MimeGroupAssertions;
import android.dynamicmime.testapp.commands.MimeGroupCommands;
import android.dynamicmime.testapp.util.MimeGroupOperations;

import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;

/**
 * Base class for dynamic MIME handlers feature test cases
 */
public abstract class BaseDynamicMimeTest extends MimeGroupOperations {
    public BaseDynamicMimeTest(MimeGroupCommands commands, MimeGroupAssertions assertions) {
        super(commands, assertions);
    }

    protected static Context context() {
        return instrumentation().getContext();
    }

    protected static Context targetContext() {
        return instrumentation().getTargetContext();
    }

    protected static Instrumentation instrumentation() {
        return InstrumentationRegistry.getInstrumentation();
    }

    @After
    public void tearDown() {
        clearGroups();
    }
}
