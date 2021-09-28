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

package com.android.nn.benchmark.core;

import android.content.res.AssetManager;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;

/** Helper class to register test model definitions from assets data */
public class TestModelsListLoader {

    /**
     * Parse list of models in form of json data.
     *
     * Example input:
     * { "models" : [
     * {"name" : "modelName",
     * "testName" : "testName",
     * "baselineSec" : 0.03,
     * "evaluator": "TopK",
     * "inputSize" : [1,2,3,4],
     * "dataSize" : 4,
     * "inputOutputs" : [ {"input": "input1", "output": "output2"} ]
     * }
     * ]}
     */
    static public void parseJSONModelsList(String jsonStringInput) throws JSONException {
        JSONObject jsonRootObject = new JSONObject(jsonStringInput);
        JSONArray jsonModelsArray = jsonRootObject.getJSONArray("models");

        for (int i = 0; i < jsonModelsArray.length(); i++) {
            JSONObject jsonTestModelEntry = jsonModelsArray.getJSONObject(i);

            String name = jsonTestModelEntry.getString("name");
            String testName = name;
            if (jsonTestModelEntry.has("testName")) {
                testName = jsonTestModelEntry.getString("testName");
            }
            String modelFile = name;
            if (jsonTestModelEntry.has("modelFile")) {
                modelFile = jsonTestModelEntry.getString("modelFile");
            }
            double baseline = jsonTestModelEntry.getDouble("baselineSec");
            int minSdkVersion = 0;
            if (jsonTestModelEntry.has("minSdkVersion")) {
                minSdkVersion = jsonTestModelEntry.getInt("minSdkVersion");
            }
            EvaluatorConfig evaluator = null;
            if (jsonTestModelEntry.has("evaluator")) {
                JSONObject evaluatorJson = jsonTestModelEntry.getJSONObject("evaluator");
                evaluator = new EvaluatorConfig(evaluatorJson.getString("className"),
                        evaluatorJson.has("outputMeanStdDev")
                                ? evaluatorJson.getString("outputMeanStdDev")
                                : null,
                        evaluatorJson.has("expectedTop1")
                                ? evaluatorJson.getDouble("expectedTop1")
                                : null);
            }

            int dataSize = jsonTestModelEntry.getInt("dataSize");
            JSONArray jsonInputSize = jsonTestModelEntry.getJSONArray("inputSize");
            int[] inputSize = new int[jsonInputSize.length()];
            int inputSizeBytes = dataSize;
            for (int k = 0; k < jsonInputSize.length(); ++k) {
                inputSize[k] = jsonInputSize.getInt(k);
                inputSizeBytes *= inputSize[k];
            }

            InferenceInOutSequence.FromAssets[] inputOutputs = null;
            if (jsonTestModelEntry.has("inputOutputs")) {
                JSONArray jsonInputOutputs = jsonTestModelEntry.getJSONArray("inputOutputs");
                inputOutputs =
                        new InferenceInOutSequence.FromAssets[jsonInputOutputs.length()];

                for (int j = 0; j < jsonInputOutputs.length(); j++) {
                    JSONObject jsonInputOutput = jsonInputOutputs.getJSONObject(j);
                    String input = jsonInputOutput.getString("input");
                    String[] outputs = null;
                    String output = jsonInputOutput.optString("output", null);
                    if (output != null) {
                        outputs = new String[]{output};
                    } else {
                        JSONArray outputArray = jsonInputOutput.getJSONArray("outputs");
                        if (outputArray != null) {
                            outputs = new String[outputArray.length()];
                            for (int k = 0; k < outputArray.length(); ++k) {
                                outputs[k] = outputArray.getString(k);
                            }
                        }
                    }

                    inputOutputs[j] = new InferenceInOutSequence.FromAssets(input, outputs,
                            dataSize,
                            inputSizeBytes);
                }
            }
            InferenceInOutSequence.FromDataset[] datasets = null;
            if (jsonTestModelEntry.has("dataset")) {
                JSONObject jsonDataset = jsonTestModelEntry.getJSONObject("dataset");
                String inputPath = jsonDataset.getString("inputPath");
                String groundTruth = jsonDataset.getString("groundTruth");
                String labels = jsonDataset.getString("labels");
                String preprocessor = jsonDataset.getString("preprocessor");
                if (inputSize.length != 4 || inputSize[0] != 1 || inputSize[1] != inputSize[2] ||
                        inputSize[3] != 3) {
                    throw new IllegalArgumentException("Datasets only support square images," +
                            "input size [1, D, D, 3], given " + inputSize[0] +
                            ", " + inputSize[1] + ", " + inputSize[2] + ", " + inputSize[3]);
                }
                float quantScale = 0.f;
                float quantZeroPoint = 0.f;
                if (dataSize == 1) {
                    if (!jsonTestModelEntry.has("inputScale") ||
                            !jsonTestModelEntry.has("inputZeroPoint")) {
                        throw new IllegalArgumentException("Quantized test model must include " +
                                "inputScale and inputZeroPoint for reading a dataset");
                    }
                    quantScale = (float) jsonTestModelEntry.getDouble("inputScale");
                    quantZeroPoint = (float) jsonTestModelEntry.getDouble("inputZeroPoint");
                }
                datasets = new InferenceInOutSequence.FromDataset[]{
                        new InferenceInOutSequence.FromDataset(inputPath, labels, groundTruth,
                                preprocessor, dataSize, quantScale, quantZeroPoint, inputSize[1])
                };
            }

            TestModels.registerModel(
                    new TestModels.TestModelEntry(name, (float) baseline, inputSize,
                            inputOutputs, datasets, testName, modelFile, evaluator, minSdkVersion));
        }
    }

    static String readAssetsFileAsString(InputStream inputStream) throws IOException {
        Reader reader = new InputStreamReader(inputStream);
        StringBuilder sb = new StringBuilder();
        char buffer[] = new char[16384];
        int len;
        while ((len = reader.read(buffer)) > 0) {
            sb.append(buffer, 0, len);
        }
        reader.close();
        return sb.toString();
    }

    /** Parse all ".json" files in root assets directory */
    private static final String MODELS_LIST_ROOT = "models_list";

    static public void parseFromAssets(AssetManager assetManager) throws IOException {
        for (String file : assetManager.list(MODELS_LIST_ROOT)) {
            if (!file.endsWith(".json")) {
                continue;
            }
            try {
                parseJSONModelsList(readAssetsFileAsString(
                        assetManager.open(MODELS_LIST_ROOT + "/" + file)));
            } catch (JSONException e) {
                throw new IOException("JSON error in " + file, e);
            } catch (Exception e) {
                // Wrap exception to add a filename to it
                throw new IOException("Error while parsing " + file, e);
            }

        }
    }
}
