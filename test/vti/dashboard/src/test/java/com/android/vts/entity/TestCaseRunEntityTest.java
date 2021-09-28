/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You may
 * obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

package com.android.vts.entity;

import com.android.vts.util.ObjectifyTestBase;
import org.junit.jupiter.api.Test;

import java.util.Arrays;
import java.util.List;

import static com.googlecode.objectify.ObjectifyService.factory;
import static org.junit.Assert.assertEquals;

public class TestCaseRunEntityTest extends ObjectifyTestBase {

    @Test
    public void saveTest() {

        factory().register(TestCaseRunEntity.class);

        List<Integer> results = Arrays.asList(1, 1, 1, 1, 1, 1, 1);
        List<String> testCaseNames =
                Arrays.asList(
                        "AudioEffectsFactoryTest.EnumerateEffects(default)_32bit",
                        "AudioEffectsFactoryTest.CreateEffect(default)_32bit",
                        "AudioEffectsFactoryTest.GetDescriptor(default)_32bit",
                        "AudioEffectsFactoryTest.DebugDumpArgument(default)_32bit",
                        "AudioEffectTest.Close(default)_32bit",
                        "AudioEffectTest.GetDescriptor(default)_32bit",
                        "AudioEffectTest.GetSetConfig(default)_32bit");

        TestCaseRunEntity testCaseRunEntity = new TestCaseRunEntity();
        for (int index = 0; index < results.size(); index++) {
            String testCaseName = testCaseNames.get(index);
            int result = results.get(index);
            testCaseRunEntity.addTestCase(testCaseName, result);
        }
        TestCaseRunEntity loadedTestCaseRunEntity = saveClearLoad(testCaseRunEntity);

        assertEquals(loadedTestCaseRunEntity.getTestCases().size(), results.size());
        assertEquals(
                (Integer) loadedTestCaseRunEntity.getTestCases().get(0).result, results.get(0));
        assertEquals(loadedTestCaseRunEntity.getTestCases().get(0).name, testCaseNames.get(0));
    }
}
