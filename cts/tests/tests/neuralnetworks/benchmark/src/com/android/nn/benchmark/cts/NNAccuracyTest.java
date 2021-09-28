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

package com.android.nn.benchmark.cts;

import static junit.framework.TestCase.assertFalse;

import android.app.Activity;
import android.util.Pair;

import androidx.test.filters.LargeTest;
import androidx.test.rule.ActivityTestRule;

import com.android.nn.benchmark.core.BenchmarkException;
import com.android.nn.benchmark.core.BenchmarkResult;
import com.android.nn.benchmark.core.InferenceInOutSequence;
import com.android.nn.benchmark.core.InferenceResult;
import com.android.nn.benchmark.core.NNTestBase;
import com.android.nn.benchmark.core.TestModels;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameters;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Tests the accuracy of the model outputs.
 */
@RunWith(Parameterized.class)
public class NNAccuracyTest {

    @Rule
    public ActivityTestRule<NNAccuracyActivity> mActivityRule =
            new ActivityTestRule<>(NNAccuracyActivity.class);

    @Parameterized.Parameter(0)
    public TestModels.TestModelEntry mModel;

    private Activity mActivity;

    // TODO(vddang): Add mobilenet_v1_0.25_128_quant_topk_aosp
    private static final String[] MODEL_NAMES = new String[]{
            "tts_float",
            "asr_float",
            "mobilenet_v1_1.0_224_quant_topk_aosp",
            "mobilenet_v1_1.0_224_topk_aosp",
            "mobilenet_v1_0.75_192_quant_topk_aosp",
            "mobilenet_v1_0.75_192_topk_aosp",
            "mobilenet_v1_0.5_160_quant_topk_aosp",
            "mobilenet_v1_0.5_160_topk_aosp",
            "mobilenet_v1_0.25_128_topk_aosp",
            "mobilenet_v2_0.35_128_topk_aosp",
            "mobilenet_v2_0.5_160_topk_aosp",
            "mobilenet_v2_0.75_192_topk_aosp",
            "mobilenet_v2_1.0_224_quant_topk_aosp",
            "mobilenet_v2_1.0_224_topk_aosp",
    };

    @Parameters(name = "{0}")
    public static List<TestModels.TestModelEntry> modelsList() {
        List<TestModels.TestModelEntry> models = new ArrayList<>();
        for (String modelName : MODEL_NAMES) {
            models.add(TestModels.getModelByName(modelName));
        }
        return Collections.unmodifiableList(models);
    }

    @Before
    public void setUp() throws Exception {
        mActivity = mActivityRule.getActivity();
    }

    @Test
    @LargeTest
    public void testNNAPI() throws BenchmarkException, IOException {
        if (!NNTestBase.hasAccelerator()) {  // Skip.
            return;
        }

        try (NNTestBase test = mModel.createNNTestBase(/*useNNAPI=*/true,
                    /*enableIntermediateTensorsDump=*/false)) {
            test.setupModel(mActivity);
            Pair<List<InferenceInOutSequence>, List<InferenceResult>> inferenceResults =
                    test.runBenchmarkCompleteInputSet(/*setRepeat=*/1, /*timeoutSec=*/3600);
            BenchmarkResult benchmarkResult =
                    BenchmarkResult.fromInferenceResults(
                            mModel.mModelName,
                            BenchmarkResult.BACKEND_TFLITE_NNAPI,
                            inferenceResults.first,
                            inferenceResults.second,
                            test.getEvaluator());
            assertFalse(benchmarkResult.hasValidationErrors());
        }
    }
}
