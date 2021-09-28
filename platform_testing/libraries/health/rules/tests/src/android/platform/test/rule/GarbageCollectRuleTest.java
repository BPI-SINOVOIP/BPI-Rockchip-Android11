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

import static org.junit.Assert.fail;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import com.android.helpers.GarbageCollectionHelper;

import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.Statement;

/**
 * Unit test the logic for {@link GarbageCollectRule}
 */
@RunWith(JUnit4.class)
public class GarbageCollectRuleTest {
    private final GarbageCollectionHelper mGcHelper = mock(GarbageCollectionHelper.class);

    /**
     * Tests that this rule will fail to register if no apps are supplied.
     */
    @Test
    public void testNoAppToGcFails() {
        try {
            GarbageCollectRule rule = new GarbageCollectRule();
            fail("An initialization error should have been thrown, but wasn't.");
        } catch (InitializationError e) {
            return;
        }
    }

    /**
     * Tests that this rule will gc before the test if an app is supplied.
     */
    @Test
    public void testCallsGcBeforeTest() throws Throwable {
        GarbageCollectRule rule = new TestableGarbageCollectRule("package.name1");
        Statement testStatement = new Statement() {
            @Override
            public void evaluate() throws Throwable {
                // Assert that garbage collection was called before the test.
                verify(mGcHelper).setUp("package.name1");
                verify(mGcHelper).garbageCollect();
            }
        };

        rule.apply(testStatement,
                Description.createTestDescription("clzz", "mthd")).evaluate();
    }


    private class TestableGarbageCollectRule extends GarbageCollectRule {
        public TestableGarbageCollectRule(String app) {
            super(app);
        }

        @Override
        GarbageCollectionHelper initGcHelper() {
            return mGcHelper;
        }
    }
}
