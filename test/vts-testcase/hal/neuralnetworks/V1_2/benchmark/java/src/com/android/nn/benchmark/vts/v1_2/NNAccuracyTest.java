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

package com.android.nn.benchmark.vts.v1_2;

import static junit.framework.TestCase.assertFalse;
import static junit.framework.TestCase.assertTrue;

import android.app.Activity;
import android.hidl.manager.V1_2.IServiceManager;
import android.os.RemoteException;
import android.util.Pair;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.rule.ActivityTestRule;
import com.android.nn.benchmark.core.BenchmarkException;
import com.android.nn.benchmark.core.BenchmarkResult;
import com.android.nn.benchmark.core.InferenceInOutSequence;
import com.android.nn.benchmark.core.InferenceResult;
import com.android.nn.benchmark.core.NNTestBase;
import com.android.nn.benchmark.core.TestModels;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import org.junit.AssumptionViolatedException;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameters;

/**
 * Tests the accuracy of the model outputs.
 */
@RunWith(Parameterized.class)
public class NNAccuracyTest {
    private static final String HAL_SERVICE_TYPE = "android.hardware.neuralnetworks@1.2::IDevice";

    @Rule
    public ActivityTestRule<NNAccuracyActivity> mActivityRule =
            new ActivityTestRule<>(NNAccuracyActivity.class);

    private static class InstanceModel {
        final String mInstance;
        final TestModels.TestModelEntry mEntry;

        InstanceModel(String instance, TestModels.TestModelEntry entry) {
            mInstance = instance;
            mEntry = entry;
        }

        @Override
        public String toString() {
            return mInstance + ":" + mEntry.mModelName;
        }
    }
    @Parameterized.Parameter(0) public InstanceModel mModel;

    private Activity mActivity;

    // TODO(vddang): Add mobilenet_v1_0.25_128_quant_topk_aosp
    private static final String[] MODEL_NAMES = new String[] {
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
    public static Iterable<InstanceModel> parameterList() throws RemoteException {
        List<String> serviceInstances =
                IServiceManager.getService().listManifestByInterface(HAL_SERVICE_TYPE);
        List<InstanceModel> parameters = new ArrayList<>();
        for (String instance : serviceInstances) {
            for (String modelName : MODEL_NAMES) {
                TestModels.TestModelEntry entry = TestModels.getModelByName(modelName);
                if (entry == null) {
                    throw new RuntimeException("Failed to find model of name " + modelName);
                }
                parameters.add(new InstanceModel(instance, entry));
            }
        }

        return Collections.unmodifiableList(parameters);
    }

    @Before
    public void setUp() throws Exception {
        mActivity = mActivityRule.getActivity();
    }

    @Test
    @LargeTest
    public void testDriver() throws BenchmarkException, IOException {
        try (NNTestBase test = mModel.mEntry.createNNTestBase()) {
            test.useNNApi();
            test.setNNApiDeviceName(mModel.mInstance);
            if (!test.setupModel(mActivity)) {
                throw new AssumptionViolatedException("The driver rejected the model.");
            }
            Pair<List<InferenceInOutSequence>, List<InferenceResult>> inferenceResults =
                    test.runBenchmarkCompleteInputSet(/*setRepeat=*/1, /*timeoutSec=*/3600);
            BenchmarkResult benchmarkResult = BenchmarkResult.fromInferenceResults(
                    mModel.mEntry.mModelName, BenchmarkResult.BACKEND_TFLITE_NNAPI,
                    inferenceResults.first, inferenceResults.second, test.getEvaluator());
            assertFalse(benchmarkResult.hasValidationErrors());
        }
    }
}
