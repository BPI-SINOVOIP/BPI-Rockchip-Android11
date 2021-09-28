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

package com.android.nn.benchmark.app;

import android.test.suitebuilder.annotation.LargeTest;
import android.util.Log;
import com.android.nn.benchmark.core.InferenceInOutSequence;
import com.android.nn.benchmark.core.TestModels;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.concurrent.TimeUnit;
import java.util.List;
import java.util.stream.Collectors;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.Stopwatch;
import org.junit.runners.Parameterized.Parameters;

/**
 * Tests that ensure stability of NNAPI by loading models for a prolonged
 * period of time.
 */
public class NNModelLoadingStressTest extends BenchmarkTestBase {
    private static final String TAG = NNModelLoadingStressTest.class.getSimpleName();

    private static final float WARMUP_SECONDS = 0; // No warmup.
    private static final float INFERENCE_SECONDS = 0; // No inference.
    private static final float RUNTIME_SECONDS = 30 * 60;

    @Rule public Stopwatch stopwatch = new Stopwatch() {};

    public NNModelLoadingStressTest(TestModels.TestModelEntry model) {
        super(model);
    }

    @Parameters(name = "{0}")
    public static List<TestModels.TestModelEntry> modelsList() {
        return TestModels.modelsList().stream()
                .map(model ->
                        new TestModels.TestModelEntry(
                                model.mModelName,
                                model.mBaselineSec,
                                model.mInputShape,
                                new InferenceInOutSequence.FromAssets[0], // No inputs for inference.
                                null,
                                model.mTestName,
                                model.mModelFile,
                                null, // Disable evaluation.
                                model.mMinSdkVersion))
                .collect(Collectors.collectingAndThen(
                        Collectors.toList(),
                        Collections::unmodifiableList));
    }

    @Test
    @LargeTest
    public void stressTestNNAPI() throws IOException {
        waitUntilCharged();
        setUseNNApi(true);
        setCompleteInputSet(true);
        float endTime = stopwatch.runtime(TimeUnit.SECONDS) + RUNTIME_SECONDS;
        TestAction ta = new TestAction(mModel, WARMUP_SECONDS, INFERENCE_SECONDS);
        while (stopwatch.runtime(TimeUnit.SECONDS) < endTime) {
            runTest(ta, mModel.getTestName());
        }
    }
}
