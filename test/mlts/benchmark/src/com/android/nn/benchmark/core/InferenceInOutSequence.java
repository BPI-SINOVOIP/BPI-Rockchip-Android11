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

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;

import android.content.res.AssetManager;
import android.util.Log;

import com.android.nn.benchmark.util.IOUtils;

import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;

/**
 * Input and expected output sequence pair for inference benchmark.
 *
 * Note that it's quite likely this class will need extension with new datasets,
 * it now supports imagenet-style files and labels only.
 */
public class InferenceInOutSequence {
    /** Sequence of input/output pairs */
    private List<InferenceInOut> mInputOutputs;
    private boolean mHasGoldenOutput;
    final public int mDatasize;

    public InferenceInOutSequence(int sequenceLength, boolean hasGoldenOutput, int datasize) {
        mInputOutputs = new ArrayList<>(sequenceLength);
        mHasGoldenOutput = hasGoldenOutput;
        mDatasize = datasize;
    }

    public int size() {
        return mInputOutputs.size();
    }

    public InferenceInOut get(int i) {
        return mInputOutputs.get(i);
    }

    public boolean hasGoldenOutput() {
        return mHasGoldenOutput;
    }

    /** Helper class, generates {@link InferenceInOut} from a pair of android asset files */
    public static class FromAssets {
        private String mInputAssetName;
        private String[] mOutputAssetsNames;
        private int mDataBytesSize;
        private int mInputSizeBytes;

        public FromAssets(String inputAssetName, String[] outputAssetsNames, int dataBytesSize,
                int inputSizeBytes) {
            this.mInputAssetName = inputAssetName;
            this.mOutputAssetsNames = outputAssetsNames;
            this.mDataBytesSize = dataBytesSize;
            this.mInputSizeBytes = inputSizeBytes;
        }

        public InferenceInOutSequence readAssets(AssetManager assetManager) throws IOException {
            byte[] inputs = IOUtils.readAsset(assetManager, mInputAssetName, mDataBytesSize);
            byte[][] outputs = new byte[mOutputAssetsNames.length][];
            int sequenceLength = inputs.length / mInputSizeBytes;

            for (int i = 0; i < mOutputAssetsNames.length; ++i) {
                outputs[i] = IOUtils.readAsset(assetManager, mOutputAssetsNames[i], mDataBytesSize);
                if (outputs[i].length % sequenceLength != 0) {
                    throw new IllegalArgumentException(
                            "Output data " + mOutputAssetsNames[i] + " size (in bytes): " +
                                    outputs[i].length + " is not a multiple of sequence length: " +
                                    sequenceLength);
                }
            }
            if (inputs.length % mInputSizeBytes != 0) {
                throw new IllegalArgumentException("Input data size (in bytes): " + inputs.length +
                        " is not a multiple of input size (in bytes): " + mInputSizeBytes);
            }
            InferenceInOutSequence sequence = new InferenceInOutSequence(
                    sequenceLength, true, mDataBytesSize);

            for (int i = 0; i < sequenceLength; ++i) {
                byte[][] outz = new byte[mOutputAssetsNames.length][];
                for (int j = 0; j < mOutputAssetsNames.length; ++j) {
                    int outputSizeBytes = outputs[j].length / sequenceLength;
                    outz[j] = Arrays.copyOfRange(outputs[j], outputSizeBytes * i,
                            outputSizeBytes * (i + 1));
                }

                sequence.mInputOutputs.add(new InferenceInOut(
                        Arrays.copyOfRange(inputs, mInputSizeBytes * i, mInputSizeBytes * (i + 1)),
                        outz,
                        -1));
            }
            return sequence;
        }
    }

    /**
     * Helper class, generates {@link InferenceInOut}[] from a directory with image files,
     * (optional) set of labels and an image preprocessor.
     *
     * The images and ground truth should look like imagenet: the images in the directory
     * must be name <prefix>-<number>.<extension>, where the number is used to find the
     * corresponding line in the ground truth labels.
     */
    public static class FromDataset {
        private String mInputPath;
        private String mLabelAssetName;
        private String mGroundTruthAssetName;
        private String mPreprocessorName;
        private int mDatasize;
        private float mQuantScale;
        private float mQuantZeroPoint;
        private int mImageDimension;

        public FromDataset(String inputPath, String labelAssetName, String groundTruthAssetName,
                String preprocessorName, int datasize,
                float quantScale, float quantZeroPoint,
                int imageDimension) {
            mInputPath = inputPath;
            if (mInputPath.endsWith("/")) {
                mInputPath = mInputPath.substring(0, mInputPath.length() - 1);
            }
            mLabelAssetName = labelAssetName;
            mGroundTruthAssetName = groundTruthAssetName;
            mPreprocessorName = preprocessorName;
            mDatasize = datasize;
            mQuantScale = quantScale;
            mQuantZeroPoint = quantZeroPoint;
            mImageDimension = imageDimension;
        }

        private boolean isImageFile(String fileName) {
            String lower = fileName.toLowerCase();
            return (lower.endsWith(".jpeg") || lower.endsWith(".jpg"));
        }

        private ImageProcessorInterface createImageProcessor() {
            try {
                Class<?> clazz = Class.forName(
                        "com.android.nn.benchmark.imageprocessors." + mPreprocessorName);
                return (ImageProcessorInterface) clazz.getConstructor().newInstance();
            } catch (Exception e) {
                throw new IllegalArgumentException(
                        "Can not create image processors named '" + mPreprocessorName + "'",
                        e);
            }
        }

        private static Integer getIndexFromFilename(String filename) {
            String index = filename.split("-")[1].split("\\.")[0];
            return Integer.valueOf(index, 10);
        }

        public ArrayList<InferenceInOutSequence> readDataset(
                final AssetManager assetManager, final File cacheDir) throws IOException {
            String[] allFileNames = assetManager.list(mInputPath);
            ArrayList<String> imageFileNames = new ArrayList<String>();
            for (String fileName : allFileNames) {
                if (isImageFile(fileName)) {
                    imageFileNames.add(fileName);
                }
            }
            Collections.sort(imageFileNames, new Comparator<String>() {
                @Override
                public int compare(String o1, String o2) {
                    Integer index1 = getIndexFromFilename(o1);
                    Integer index2 = getIndexFromFilename(o2);
                    return index1.compareTo(index2);
                }
            });

            Integer[] expectedClasses = null;
            HashMap<String, Integer> labelMap = null;
            if (mLabelAssetName != null) {
                labelMap = new HashMap<String, Integer>();
                InputStream labelStream = assetManager.open(mLabelAssetName);
                BufferedReader labelReader = new BufferedReader(
                        new InputStreamReader(labelStream, "UTF-8"));
                String line;
                int index = 0;
                while ((line = labelReader.readLine()) != null) {
                    labelMap.put(line, new Integer(index));
                    index++;
                }
            }
            if (mGroundTruthAssetName != null) {
                expectedClasses = new Integer[imageFileNames.size()];
                InputStream truthStream = assetManager.open(mGroundTruthAssetName);
                BufferedReader truthReader = new BufferedReader(
                        new InputStreamReader(truthStream, "UTF-8"));
                String line;
                int index = 0;
                while ((line = truthReader.readLine()) != null) {
                    if (labelMap != null) {
                        expectedClasses[index] = labelMap.get(line);
                    } else {
                        expectedClasses[index] = Integer.parseInt(line, 10);
                    }
                    index++;
                }
            }

            ArrayList<InferenceInOutSequence> ret = new ArrayList<InferenceInOutSequence>();
            final ImageProcessorInterface imageProcessor = createImageProcessor();

            for (int i = 0; i < imageFileNames.size(); i++) {
                final String fileName = mInputPath + '/' + imageFileNames.get(i);
                int expectedClass = -1;
                if (expectedClasses != null) {
                    expectedClass = expectedClasses[i];
                }
                InferenceInOut.InputCreatorInterface creator =
                        new InferenceInOut.InputCreatorInterface() {
                            @Override
                            public void createInput(ByteBuffer buffer) {
                                try {
                                    imageProcessor.preprocess(mDatasize,
                                            mQuantScale, mQuantZeroPoint, mImageDimension,
                                            assetManager, fileName, cacheDir, buffer);
                                } catch (Throwable t) {
                                    throw new Error("Failed to create image input", t);
                                }
                            }
                        };
                InferenceInOutSequence sequence = new InferenceInOutSequence(
                        1, false, mDatasize);
                sequence.mInputOutputs.add(new InferenceInOut(creator, null,
                        expectedClass));
                ret.add(sequence);
            }
            return ret;
        }
    }
}
