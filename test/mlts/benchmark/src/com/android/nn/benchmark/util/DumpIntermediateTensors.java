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

package com.android.nn.benchmark.util;

import android.util.Log;

import android.app.Activity;
import android.os.Bundle;
import com.android.nn.benchmark.core.NNTestBase;
import com.android.nn.benchmark.core.TestModels.TestModelEntry;
import com.android.nn.benchmark.core.TestModels;
import java.io.IOException;
import java.io.File;


/** Helper activity for dumping state of interference intermediate tensors.
 *
 * Example usage:
 * adb shell am start -n  com.android.nn.benchmark.app/com.android.nn.benchmark.\
 * util.DumpIntermediateTensors --es modelName mobilenet_v1_1.0_224_quant_topk_aosp,tts_float\
 * inputAssetIndex 0
 *
 * Assets will be then dumped into /data/data/com.android.nn.benchmark.app/files/intermediate
 * To fetch:
 * adb pull /data/data/com.android.nn.benchmark.app/files/intermediate
 *
 */
public class DumpIntermediateTensors extends Activity {
    protected static final String TAG = "VDEBUG";
    public static final String EXTRA_MODEL_NAME = "modelName";
    public static final String EXTRA_INPUT_ASSET_INDEX= "inputAssetIndex";
    public static final String EXTRA_INPUT_ASSET_SIZE= "inputAssetSize";
    public static final String DUMP_DIR = "intermediate";
    public static final String CPU_DIR = "cpu";
    public static final String NNAPI_DIR = "nnapi";
    // TODO(veralin): Update to use other models in vendor as well.
    // Due to recent change in NNScoringTest, the model names are moved to here.
    private static final String[] MODEL_NAMES = new String[]{
        "tts_float",
        "asr_float",
        "mobilenet_v1_1.0_224_quant_topk_aosp",
        "mobilenet_v1_1.0_224_topk_aosp",
        "mobilenet_v1_0.75_192_quant_topk_aosp",
        "mobilenet_v1_0.75_192_topk_aosp",
        "mobilenet_v1_0.5_160_quant_topk_aosp",
        "mobilenet_v1_0.5_160_topk_aosp",
        "mobilenet_v1_0.25_128_quant_topk_aosp",
        "mobilenet_v1_0.25_128_topk_aosp",
        "mobilenet_v2_0.35_128_topk_aosp",
        "mobilenet_v2_0.5_160_topk_aosp",
        "mobilenet_v2_0.75_192_topk_aosp",
        "mobilenet_v2_1.0_224_topk_aosp",
        "mobilenet_v2_1.0_224_quant_topk_aosp",
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Bundle extras = getIntent().getExtras();

        String userModelName = extras.getString(EXTRA_MODEL_NAME);
        int inputAssetIndex = extras.getInt(EXTRA_INPUT_ASSET_INDEX, 0);
        int inputAssetSize = extras.getInt(EXTRA_INPUT_ASSET_SIZE, 1);

        // Default to run all models in NNScoringTest
        String[] modelNames = userModelName == null? MODEL_NAMES: userModelName.split(",");

        try {
            File dumpDir = new File(getFilesDir(), DUMP_DIR);
            safeMkdir(dumpDir);

            for (String modelName : modelNames) {
                File modelDir = new File(getFilesDir() + "/" + DUMP_DIR, modelName);
                safeMkdir(modelDir);
                // Run in CPU and NNAPI mode
                for (final boolean useNNAPI : new boolean[] {false, true}) {
                    String useNNAPIDir = useNNAPI? NNAPI_DIR: CPU_DIR;
                    Log.i(TAG, "Running " + modelName + " in " + useNNAPIDir);
                    TestModelEntry modelEntry = TestModels.getModelByName(modelName);
                    try (NNTestBase testBase = modelEntry.createNNTestBase(
                            useNNAPI, true/*enableIntermediateTensorsDump*/)) {
                        testBase.setupModel(this);
                        File outputDir = new File(getFilesDir() + "/" + DUMP_DIR +
                            "/" + modelName, useNNAPIDir);
                        safeMkdir(outputDir);
                        testBase.dumpAllLayers(outputDir, inputAssetIndex, inputAssetSize);
                    }
                }
            }

        } catch (Exception e) {
            Log.e(TAG, "Failed to dump tensors", e);
            throw new IllegalStateException("Failed to dump tensors", e);
        }
        finish();
    }

    private void deleteRecursive(File fileOrDirectory) {
        if (fileOrDirectory.isDirectory()) {
            for (File child : fileOrDirectory.listFiles()) {
                deleteRecursive(child);
            }
        }
        fileOrDirectory.delete();
    }

    private void safeMkdir(File fileOrDirectory) {
        deleteRecursive(fileOrDirectory);
        fileOrDirectory.mkdir();
    }
}
