/**
 * Copyright 2017 The Android Open Source Project
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

#include "run_tflite.h"

#include "tensorflow/lite/nnapi/nnapi_implementation.h"

#include <jni.h>
#include <string>
#include <iomanip>
#include <sstream>
#include <fcntl.h>

#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <android/sharedmem.h>
#include <sys/mman.h>


extern "C"
JNIEXPORT jlong
JNICALL
Java_com_android_nn_benchmark_core_NNTestBase_initModel(
        JNIEnv *env,
        jobject /* this */,
        jstring _modelFileName,
        jboolean _useNnApi,
        jboolean _enableIntermediateTensorsDump,
        jstring _nnApiDeviceName) {
    const char *modelFileName = env->GetStringUTFChars(_modelFileName, NULL);
    const char *nnApiDeviceName =
        _nnApiDeviceName == NULL
            ? NULL
            : env->GetStringUTFChars(_nnApiDeviceName, NULL);
    void *handle =
        BenchmarkModel::create(modelFileName, _useNnApi,
                               _enableIntermediateTensorsDump, nnApiDeviceName);
    env->ReleaseStringUTFChars(_modelFileName, modelFileName);
    if (_nnApiDeviceName != NULL) {
        env->ReleaseStringUTFChars(_nnApiDeviceName, nnApiDeviceName);
    }

    return (jlong)(uintptr_t)handle;
}

extern "C"
JNIEXPORT void
JNICALL
Java_com_android_nn_benchmark_core_NNTestBase_destroyModel(
        JNIEnv *env,
        jobject /* this */,
        jlong _modelHandle) {
    BenchmarkModel* model = (BenchmarkModel *) _modelHandle;
    delete(model);
}

extern "C"
JNIEXPORT jboolean
JNICALL
Java_com_android_nn_benchmark_core_NNTestBase_resizeInputTensors(
        JNIEnv *env,
        jobject /* this */,
        jlong _modelHandle,
        jintArray _inputShape) {
    BenchmarkModel* model = (BenchmarkModel *) _modelHandle;
    jint* shapePtr = env->GetIntArrayElements(_inputShape, nullptr);
    jsize shapeLen = env->GetArrayLength(_inputShape);

    std::vector<int> shape(shapePtr, shapePtr + shapeLen);
    return model->resizeInputTensors(std::move(shape));
}

/** RAII container for a list of InferenceInOutSequence to handle JNI data release in destructor. */
class InferenceInOutSequenceList {
public:
    InferenceInOutSequenceList(JNIEnv *env,
                               const jobject& inOutDataList,
                               bool expectGoldenOutputs);
    ~InferenceInOutSequenceList();

    bool isValid() const { return mValid; }

    const std::vector<InferenceInOutSequence>& data() const { return mData; }

private:
    JNIEnv *mEnv;  // not owned.

    std::vector<InferenceInOutSequence> mData;
    std::vector<jbyteArray> mInputArrays;
    std::vector<jobjectArray> mOutputArrays;
    bool mValid;
};

InferenceInOutSequenceList::InferenceInOutSequenceList(JNIEnv *env,
                                                       const jobject& inOutDataList,
                                                       bool expectGoldenOutputs)
    : mEnv(env), mValid(false) {

    jclass list_class = env->FindClass("java/util/List");
    if (list_class == nullptr) { return; }
    jmethodID list_size = env->GetMethodID(list_class, "size", "()I");
    if (list_size == nullptr) { return; }
    jmethodID list_get = env->GetMethodID(list_class, "get", "(I)Ljava/lang/Object;");
    if (list_get == nullptr) { return; }
    jmethodID list_add = env->GetMethodID(list_class, "add", "(Ljava/lang/Object;)Z");
    if (list_add == nullptr) { return; }

    jclass inOutSeq_class = env->FindClass("com/android/nn/benchmark/core/InferenceInOutSequence");
    if (inOutSeq_class == nullptr) { return; }
    jmethodID inOutSeq_size = env->GetMethodID(inOutSeq_class, "size", "()I");
    if (inOutSeq_size == nullptr) { return; }
    jmethodID inOutSeq_get = env->GetMethodID(inOutSeq_class, "get",
                                              "(I)Lcom/android/nn/benchmark/core/InferenceInOut;");
    if (inOutSeq_get == nullptr) { return; }

    jclass inout_class = env->FindClass("com/android/nn/benchmark/core/InferenceInOut");
    if (inout_class == nullptr) { return; }
    jfieldID inout_input = env->GetFieldID(inout_class, "mInput", "[B");
    if (inout_input == nullptr) { return; }
    jfieldID inout_expectedOutputs = env->GetFieldID(inout_class, "mExpectedOutputs", "[[B");
    if (inout_expectedOutputs == nullptr) { return; }
    jfieldID inout_inputCreator = env->GetFieldID(inout_class, "mInputCreator",
            "Lcom/android/nn/benchmark/core/InferenceInOut$InputCreatorInterface;");
    if (inout_inputCreator == nullptr) { return; }



    // Fetch input/output arrays
    size_t data_count = mEnv->CallIntMethod(inOutDataList, list_size);
    if (env->ExceptionCheck()) { return; }
    mData.reserve(data_count);

    jclass inputCreator_class = env->FindClass("com/android/nn/benchmark/core/InferenceInOut$InputCreatorInterface");
    if (inputCreator_class == nullptr) { return; }
    jmethodID createInput_method = env->GetMethodID(inputCreator_class, "createInput", "(Ljava/nio/ByteBuffer;)V");
    if (createInput_method == nullptr) { return; }

    for (int seq_index = 0; seq_index < data_count; ++seq_index) {
        jobject inOutSeq = mEnv->CallObjectMethod(inOutDataList, list_get, seq_index);
        if (mEnv->ExceptionCheck()) { return; }

        size_t seqLen = mEnv->CallIntMethod(inOutSeq, inOutSeq_size);
        if (mEnv->ExceptionCheck()) { return; }

        mData.push_back(InferenceInOutSequence{});
        auto& seq = mData.back();
        seq.reserve(seqLen);
        for (int i = 0; i < seqLen; ++i) {
            jobject inout = mEnv->CallObjectMethod(inOutSeq, inOutSeq_get, i);
            if (mEnv->ExceptionCheck()) { return; }

            uint8_t* input_data = nullptr;
            size_t input_len = 0;
            std::function<bool(uint8_t*, size_t)> inputCreator;
            jbyteArray input = static_cast<jbyteArray>(
                    mEnv->GetObjectField(inout, inout_input));
            mInputArrays.push_back(input);
            if (input != nullptr) {
                input_data = reinterpret_cast<uint8_t*>(
                        mEnv->GetByteArrayElements(input, NULL));
                input_len = mEnv->GetArrayLength(input);
            } else {
                inputCreator = [env, inout, inout_inputCreator, createInput_method](
                        uint8_t* buffer, size_t length) {
                    jobject byteBuffer = env->NewDirectByteBuffer(buffer, length);
                    if (byteBuffer == nullptr) { return false; }
                    jobject creator = env->GetObjectField(inout, inout_inputCreator);
                    if (creator == nullptr) { return false; }
                    env->CallVoidMethod(creator, createInput_method, byteBuffer);
                    env->DeleteLocalRef(byteBuffer);
                    if (env->ExceptionCheck()) { return false; }
                    return true;
                };
            }

            jobjectArray expectedOutputs = static_cast<jobjectArray>(
                    mEnv->GetObjectField(inout, inout_expectedOutputs));
            mOutputArrays.push_back(expectedOutputs);
            seq.push_back({input_data, input_len, {}, inputCreator});

            // Add expected output to sequence added above
            if (expectedOutputs != nullptr) {
                jsize expectedOutputsLength = mEnv->GetArrayLength(expectedOutputs);
                auto& outputs = seq.back().outputs;
                outputs.reserve(expectedOutputsLength);

                for (jsize j = 0;j < expectedOutputsLength; ++j) {
                    jbyteArray expectedOutput =
                            static_cast<jbyteArray>(mEnv->GetObjectArrayElement(expectedOutputs, j));
                    if (env->ExceptionCheck()) {
                        return;
                    }
                    if (expectedOutput == nullptr) {
                        jclass iaeClass = mEnv->FindClass("java/lang/IllegalArgumentException");
                        mEnv->ThrowNew(iaeClass, "Null expected output array");
                        return;
                    }

                    uint8_t *expectedOutput_data = reinterpret_cast<uint8_t*>(
                                        mEnv->GetByteArrayElements(expectedOutput, NULL));
                    size_t expectedOutput_len = mEnv->GetArrayLength(expectedOutput);
                    outputs.push_back({ expectedOutput_data, expectedOutput_len});
                }
            } else {
                if (expectGoldenOutputs) {
                    jclass iaeClass = mEnv->FindClass("java/lang/IllegalArgumentException");
                    mEnv->ThrowNew(iaeClass, "Expected golden output for every input");
                    return;
                }
            }
        }
    }
    mValid = true;
}

InferenceInOutSequenceList::~InferenceInOutSequenceList() {
    // Note that we may land here with a pending JNI exception so cannot call
    // java objects.
    int arrayIndex = 0;
    for (int seq_index = 0; seq_index < mData.size(); ++seq_index) {
        for (int i = 0; i < mData[seq_index].size(); ++i) {
            jbyteArray input = mInputArrays[arrayIndex];
            if (input != nullptr) {
                mEnv->ReleaseByteArrayElements(
                        input, reinterpret_cast<jbyte*>(mData[seq_index][i].input), JNI_ABORT);
            }
            jobjectArray expectedOutputs = mOutputArrays[arrayIndex];
            if (expectedOutputs != nullptr) {
                jsize expectedOutputsLength = mEnv->GetArrayLength(expectedOutputs);
                if (expectedOutputsLength != mData[seq_index][i].outputs.size()) {
                    // Should not happen? :)
                    jclass iaeClass = mEnv->FindClass("java/lang/IllegalStateException");
                    mEnv->ThrowNew(iaeClass, "Mismatch of the size of expected outputs jni array "
                                   "and internal array of its bufers");
                    return;
                }

                for (jsize j = 0;j < expectedOutputsLength; ++j) {
                    jbyteArray expectedOutput = static_cast<jbyteArray>(mEnv->GetObjectArrayElement(expectedOutputs, j));
                    mEnv->ReleaseByteArrayElements(
                        expectedOutput, reinterpret_cast<jbyte*>(mData[seq_index][i].outputs[j].ptr),
                        JNI_ABORT);
                }
            }
            arrayIndex++;
        }
    }
}

extern "C"
JNIEXPORT jboolean
JNICALL
Java_com_android_nn_benchmark_core_NNTestBase_runBenchmark(
        JNIEnv *env,
        jobject /* this */,
        jlong _modelHandle,
        jobject inOutDataList,
        jobject resultList,
        jint inferencesSeqMaxCount,
        jfloat timeoutSec,
        jint flags) {

    BenchmarkModel* model = reinterpret_cast<BenchmarkModel*>(_modelHandle);

    jclass list_class = env->FindClass("java/util/List");
    if (list_class == nullptr) { return false; }
    jmethodID list_add = env->GetMethodID(list_class, "add", "(Ljava/lang/Object;)Z");
    if (list_add == nullptr) { return false; }

    jclass result_class = env->FindClass("com/android/nn/benchmark/core/InferenceResult");
    if (result_class == nullptr) { return false; }
    jmethodID result_ctor = env->GetMethodID(result_class, "<init>", "(F[F[F[[BII)V");
    if (result_ctor == nullptr) { return false; }

    std::vector<InferenceResult> result;

    const bool expectGoldenOutputs = (flags & FLAG_IGNORE_GOLDEN_OUTPUT) == 0;
    InferenceInOutSequenceList data(env, inOutDataList, expectGoldenOutputs);
    if (!data.isValid()) {
        return false;
    }

    // TODO: Remove success boolean from this method and throw an exception in case of problems
    bool success = model->benchmark(data.data(), inferencesSeqMaxCount, timeoutSec, flags, &result);

    // Generate results
    if (success) {
        for (const InferenceResult &rentry : result) {
            jobjectArray inferenceOutputs = nullptr;
            jfloatArray meanSquareErrorArray = nullptr;
            jfloatArray maxSingleErrorArray = nullptr;

            if ((flags & FLAG_IGNORE_GOLDEN_OUTPUT) == 0) {
                meanSquareErrorArray = env->NewFloatArray(rentry.meanSquareErrors.size());
                if (env->ExceptionCheck()) { return false; }
                maxSingleErrorArray = env->NewFloatArray(rentry.maxSingleErrors.size());
                if (env->ExceptionCheck()) { return false; }
                {
                    jfloat *bytes = env->GetFloatArrayElements(meanSquareErrorArray, nullptr);
                    memcpy(bytes,
                           &rentry.meanSquareErrors[0],
                           rentry.meanSquareErrors.size() * sizeof(float));
                    env->ReleaseFloatArrayElements(meanSquareErrorArray, bytes, 0);
                }
                {
                    jfloat *bytes = env->GetFloatArrayElements(maxSingleErrorArray, nullptr);
                    memcpy(bytes,
                           &rentry.maxSingleErrors[0],
                           rentry.maxSingleErrors.size() * sizeof(float));
                    env->ReleaseFloatArrayElements(maxSingleErrorArray, bytes, 0);
                }
            }

            if ((flags & FLAG_DISCARD_INFERENCE_OUTPUT) == 0) {
                jclass byteArrayClass = env->FindClass("[B");

                inferenceOutputs = env->NewObjectArray(
                    rentry.inferenceOutputs.size(),
                    byteArrayClass, nullptr);

                for (int i = 0;i < rentry.inferenceOutputs.size();++i) {
                    jbyteArray inferenceOutput = nullptr;
                    inferenceOutput = env->NewByteArray(rentry.inferenceOutputs[i].size());
                    if (env->ExceptionCheck()) { return false; }
                    jbyte *bytes = env->GetByteArrayElements(inferenceOutput, nullptr);
                    memcpy(bytes, &rentry.inferenceOutputs[i][0], rentry.inferenceOutputs[i].size());
                    env->ReleaseByteArrayElements(inferenceOutput, bytes, 0);
                    env->SetObjectArrayElement(inferenceOutputs, i, inferenceOutput);
                }
            }

            jobject object = env->NewObject(
                result_class, result_ctor, rentry.computeTimeSec,
                meanSquareErrorArray, maxSingleErrorArray, inferenceOutputs,
                rentry.inputOutputSequenceIndex, rentry.inputOutputIndex);
            if (env->ExceptionCheck() || object == NULL) { return false; }

            env->CallBooleanMethod(resultList, list_add, object);
            if (env->ExceptionCheck()) { return false; }
        }
    }

    return success;
}

extern "C"
JNIEXPORT void
JNICALL
Java_com_android_nn_benchmark_core_NNTestBase_dumpAllLayers(
        JNIEnv *env,
        jobject /* this */,
        jlong _modelHandle,
        jstring dumpPath,
        jobject inOutDataList) {

    BenchmarkModel* model = reinterpret_cast<BenchmarkModel*>(_modelHandle);

    InferenceInOutSequenceList data(env, inOutDataList, /*expectGoldenOutputs=*/false);
    if (!data.isValid()) {
        return;
    }

    const char *dumpPathStr = env->GetStringUTFChars(dumpPath, JNI_FALSE);
    model->dumpAllLayers(dumpPathStr, data.data());
    env->ReleaseStringUTFChars(dumpPath, dumpPathStr);
}

extern "C"
JNIEXPORT jboolean
JNICALL
Java_com_android_nn_benchmark_core_NNTestBase_hasAccelerator() {
  uint32_t device_count = 0;
  NnApiImplementation()->ANeuralNetworks_getDeviceCount(&device_count);
  // We only consider a real device, not 'nnapi-reference'.
  return device_count > 1;
}
