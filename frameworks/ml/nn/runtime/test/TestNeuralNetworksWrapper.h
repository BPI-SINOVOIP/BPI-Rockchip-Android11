/*
 * Copyright (C) 2017 The Android Open Source Project
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

// Provides C++ classes to more easily use the Neural Networks API.
// TODO(b/117845862): this should be auto generated from NeuralNetworksWrapper.h.

#ifndef ANDROID_FRAMEWORKS_ML_NN_RUNTIME_TEST_TEST_NEURAL_NETWORKS_WRAPPER_H
#define ANDROID_FRAMEWORKS_ML_NN_RUNTIME_TEST_TEST_NEURAL_NETWORKS_WRAPPER_H

#include <math.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "NeuralNetworks.h"
#include "NeuralNetworksWrapper.h"
#include "NeuralNetworksWrapperExtensions.h"

namespace android {
namespace nn {
namespace test_wrapper {

using wrapper::Event;
using wrapper::ExecutePreference;
using wrapper::ExecutePriority;
using wrapper::ExtensionModel;
using wrapper::ExtensionOperandParams;
using wrapper::ExtensionOperandType;
using wrapper::OperandType;
using wrapper::Result;
using wrapper::SymmPerChannelQuantParams;
using wrapper::Type;

class Memory {
   public:
    // Takes ownership of a ANeuralNetworksMemory
    Memory(ANeuralNetworksMemory* memory) : mMemory(memory) {}

    Memory(size_t size, int protect, int fd, size_t offset) {
        mValid = ANeuralNetworksMemory_createFromFd(size, protect, fd, offset, &mMemory) ==
                 ANEURALNETWORKS_NO_ERROR;
    }

    Memory(AHardwareBuffer* buffer) {
        mValid = ANeuralNetworksMemory_createFromAHardwareBuffer(buffer, &mMemory) ==
                 ANEURALNETWORKS_NO_ERROR;
    }

    virtual ~Memory() { ANeuralNetworksMemory_free(mMemory); }

    // Disallow copy semantics to ensure the runtime object can only be freed
    // once. Copy semantics could be enabled if some sort of reference counting
    // or deep-copy system for runtime objects is added later.
    Memory(const Memory&) = delete;
    Memory& operator=(const Memory&) = delete;

    // Move semantics to remove access to the runtime object from the wrapper
    // object that is being moved. This ensures the runtime object will be
    // freed only once.
    Memory(Memory&& other) { *this = std::move(other); }
    Memory& operator=(Memory&& other) {
        if (this != &other) {
            ANeuralNetworksMemory_free(mMemory);
            mMemory = other.mMemory;
            mValid = other.mValid;
            other.mMemory = nullptr;
            other.mValid = false;
        }
        return *this;
    }

    ANeuralNetworksMemory* get() const { return mMemory; }
    bool isValid() const { return mValid; }

   private:
    ANeuralNetworksMemory* mMemory = nullptr;
    bool mValid = true;
};

class Model {
   public:
    Model() {
        // TODO handle the value returned by this call
        ANeuralNetworksModel_create(&mModel);
    }
    ~Model() { ANeuralNetworksModel_free(mModel); }

    // Disallow copy semantics to ensure the runtime object can only be freed
    // once. Copy semantics could be enabled if some sort of reference counting
    // or deep-copy system for runtime objects is added later.
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    // Move semantics to remove access to the runtime object from the wrapper
    // object that is being moved. This ensures the runtime object will be
    // freed only once.
    Model(Model&& other) { *this = std::move(other); }
    Model& operator=(Model&& other) {
        if (this != &other) {
            ANeuralNetworksModel_free(mModel);
            mModel = other.mModel;
            mNextOperandId = other.mNextOperandId;
            mValid = other.mValid;
            mRelaxed = other.mRelaxed;
            mFinished = other.mFinished;
            other.mModel = nullptr;
            other.mNextOperandId = 0;
            other.mValid = false;
            other.mRelaxed = false;
            other.mFinished = false;
        }
        return *this;
    }

    Result finish() {
        if (mValid) {
            auto result = static_cast<Result>(ANeuralNetworksModel_finish(mModel));
            if (result != Result::NO_ERROR) {
                mValid = false;
            }
            mFinished = true;
            return result;
        } else {
            return Result::BAD_STATE;
        }
    }

    uint32_t addOperand(const OperandType* type) {
        if (ANeuralNetworksModel_addOperand(mModel, &(type->operandType)) !=
            ANEURALNETWORKS_NO_ERROR) {
            mValid = false;
        }
        if (type->channelQuant) {
            if (ANeuralNetworksModel_setOperandSymmPerChannelQuantParams(
                        mModel, mNextOperandId, &type->channelQuant.value().params) !=
                ANEURALNETWORKS_NO_ERROR) {
                mValid = false;
            }
        }
        return mNextOperandId++;
    }

    template <typename T>
    uint32_t addConstantOperand(const OperandType* type, const T& value) {
        static_assert(sizeof(T) <= ANEURALNETWORKS_MAX_SIZE_OF_IMMEDIATELY_COPIED_VALUES,
                      "Values larger than ANEURALNETWORKS_MAX_SIZE_OF_IMMEDIATELY_COPIED_VALUES "
                      "not supported");
        uint32_t index = addOperand(type);
        setOperandValue(index, &value);
        return index;
    }

    uint32_t addModelOperand(const Model* value) {
        OperandType operandType(Type::MODEL, {});
        uint32_t operand = addOperand(&operandType);
        setOperandValueFromModel(operand, value);
        return operand;
    }

    void setOperandValue(uint32_t index, const void* buffer, size_t length) {
        if (ANeuralNetworksModel_setOperandValue(mModel, index, buffer, length) !=
            ANEURALNETWORKS_NO_ERROR) {
            mValid = false;
        }
    }

    template <typename T>
    void setOperandValue(uint32_t index, const T* value) {
        static_assert(!std::is_pointer<T>(), "No operand may have a pointer as its value");
        return setOperandValue(index, value, sizeof(T));
    }

    void setOperandValueFromMemory(uint32_t index, const Memory* memory, uint32_t offset,
                                   size_t length) {
        if (ANeuralNetworksModel_setOperandValueFromMemory(mModel, index, memory->get(), offset,
                                                           length) != ANEURALNETWORKS_NO_ERROR) {
            mValid = false;
        }
    }

    void setOperandValueFromModel(uint32_t index, const Model* value) {
        if (ANeuralNetworksModel_setOperandValueFromModel(mModel, index, value->mModel) !=
            ANEURALNETWORKS_NO_ERROR) {
            mValid = false;
        }
    }

    void addOperation(ANeuralNetworksOperationType type, const std::vector<uint32_t>& inputs,
                      const std::vector<uint32_t>& outputs) {
        if (ANeuralNetworksModel_addOperation(mModel, type, static_cast<uint32_t>(inputs.size()),
                                              inputs.data(), static_cast<uint32_t>(outputs.size()),
                                              outputs.data()) != ANEURALNETWORKS_NO_ERROR) {
            mValid = false;
        }
    }
    void identifyInputsAndOutputs(const std::vector<uint32_t>& inputs,
                                  const std::vector<uint32_t>& outputs) {
        if (ANeuralNetworksModel_identifyInputsAndOutputs(
                    mModel, static_cast<uint32_t>(inputs.size()), inputs.data(),
                    static_cast<uint32_t>(outputs.size()),
                    outputs.data()) != ANEURALNETWORKS_NO_ERROR) {
            mValid = false;
        }
    }

    void relaxComputationFloat32toFloat16(bool isRelax) {
        if (ANeuralNetworksModel_relaxComputationFloat32toFloat16(mModel, isRelax) ==
            ANEURALNETWORKS_NO_ERROR) {
            mRelaxed = isRelax;
        }
    }

    ANeuralNetworksModel* getHandle() const { return mModel; }
    bool isValid() const { return mValid; }
    bool isRelaxed() const { return mRelaxed; }
    bool isFinished() const { return mFinished; }

   protected:
    ANeuralNetworksModel* mModel = nullptr;
    // We keep track of the operand ID as a convenience to the caller.
    uint32_t mNextOperandId = 0;
    bool mValid = true;
    bool mRelaxed = false;
    bool mFinished = false;
};

class Compilation {
   public:
    // On success, createForDevice(s) will return Result::NO_ERROR and the created compilation;
    // otherwise, it will return the error code and Compilation object wrapping a nullptr handle.
    static std::pair<Result, Compilation> createForDevice(const Model* model,
                                                          const ANeuralNetworksDevice* device) {
        return createForDevices(model, {device});
    }
    static std::pair<Result, Compilation> createForDevices(
            const Model* model, const std::vector<const ANeuralNetworksDevice*>& devices) {
        ANeuralNetworksCompilation* compilation = nullptr;
        const Result result = static_cast<Result>(ANeuralNetworksCompilation_createForDevices(
                model->getHandle(), devices.empty() ? nullptr : devices.data(), devices.size(),
                &compilation));
        return {result, Compilation(compilation)};
    }

    Compilation(const Model* model) {
        int result = ANeuralNetworksCompilation_create(model->getHandle(), &mCompilation);
        if (result != 0) {
            // TODO Handle the error
        }
    }

    Compilation() {}

    ~Compilation() { ANeuralNetworksCompilation_free(mCompilation); }

    // Disallow copy semantics to ensure the runtime object can only be freed
    // once. Copy semantics could be enabled if some sort of reference counting
    // or deep-copy system for runtime objects is added later.
    Compilation(const Compilation&) = delete;
    Compilation& operator=(const Compilation&) = delete;

    // Move semantics to remove access to the runtime object from the wrapper
    // object that is being moved. This ensures the runtime object will be
    // freed only once.
    Compilation(Compilation&& other) { *this = std::move(other); }
    Compilation& operator=(Compilation&& other) {
        if (this != &other) {
            ANeuralNetworksCompilation_free(mCompilation);
            mCompilation = other.mCompilation;
            other.mCompilation = nullptr;
        }
        return *this;
    }

    Result setPreference(ExecutePreference preference) {
        return static_cast<Result>(ANeuralNetworksCompilation_setPreference(
                mCompilation, static_cast<int32_t>(preference)));
    }

    Result setPriority(ExecutePriority priority) {
        return static_cast<Result>(ANeuralNetworksCompilation_setPriority(
                mCompilation, static_cast<int32_t>(priority)));
    }

    Result setCaching(const std::string& cacheDir, const std::vector<uint8_t>& token) {
        if (token.size() != ANEURALNETWORKS_BYTE_SIZE_OF_CACHE_TOKEN) {
            return Result::BAD_DATA;
        }
        return static_cast<Result>(ANeuralNetworksCompilation_setCaching(
                mCompilation, cacheDir.c_str(), token.data()));
    }

    Result finish() { return static_cast<Result>(ANeuralNetworksCompilation_finish(mCompilation)); }

    ANeuralNetworksCompilation* getHandle() const { return mCompilation; }

   protected:
    // Takes the ownership of ANeuralNetworksCompilation.
    Compilation(ANeuralNetworksCompilation* compilation) : mCompilation(compilation) {}

    ANeuralNetworksCompilation* mCompilation = nullptr;
};

class Execution {
   public:
    Execution(const Compilation* compilation) : mCompilation(compilation->getHandle()) {
        int result = ANeuralNetworksExecution_create(compilation->getHandle(), &mExecution);
        if (result != 0) {
            // TODO Handle the error
        }
    }

    ~Execution() { ANeuralNetworksExecution_free(mExecution); }

    // Disallow copy semantics to ensure the runtime object can only be freed
    // once. Copy semantics could be enabled if some sort of reference counting
    // or deep-copy system for runtime objects is added later.
    Execution(const Execution&) = delete;
    Execution& operator=(const Execution&) = delete;

    // Move semantics to remove access to the runtime object from the wrapper
    // object that is being moved. This ensures the runtime object will be
    // freed only once.
    Execution(Execution&& other) { *this = std::move(other); }
    Execution& operator=(Execution&& other) {
        if (this != &other) {
            ANeuralNetworksExecution_free(mExecution);
            mCompilation = other.mCompilation;
            other.mCompilation = nullptr;
            mExecution = other.mExecution;
            other.mExecution = nullptr;
        }
        return *this;
    }

    Result setInput(uint32_t index, const void* buffer, size_t length,
                    const ANeuralNetworksOperandType* type = nullptr) {
        return static_cast<Result>(
                ANeuralNetworksExecution_setInput(mExecution, index, type, buffer, length));
    }

    template <typename T>
    Result setInput(uint32_t index, const T* value,
                    const ANeuralNetworksOperandType* type = nullptr) {
        static_assert(!std::is_pointer<T>(), "No operand may have a pointer as its value");
        return setInput(index, value, sizeof(T), type);
    }

    Result setInputFromMemory(uint32_t index, const Memory* memory, uint32_t offset,
                              uint32_t length, const ANeuralNetworksOperandType* type = nullptr) {
        return static_cast<Result>(ANeuralNetworksExecution_setInputFromMemory(
                mExecution, index, type, memory->get(), offset, length));
    }

    Result setOutput(uint32_t index, void* buffer, size_t length,
                     const ANeuralNetworksOperandType* type = nullptr) {
        return static_cast<Result>(
                ANeuralNetworksExecution_setOutput(mExecution, index, type, buffer, length));
    }

    template <typename T>
    Result setOutput(uint32_t index, T* value, const ANeuralNetworksOperandType* type = nullptr) {
        static_assert(!std::is_pointer<T>(), "No operand may have a pointer as its value");
        return setOutput(index, value, sizeof(T), type);
    }

    Result setOutputFromMemory(uint32_t index, const Memory* memory, uint32_t offset,
                               uint32_t length, const ANeuralNetworksOperandType* type = nullptr) {
        return static_cast<Result>(ANeuralNetworksExecution_setOutputFromMemory(
                mExecution, index, type, memory->get(), offset, length));
    }

    Result setLoopTimeout(uint64_t duration) {
        return static_cast<Result>(ANeuralNetworksExecution_setLoopTimeout(mExecution, duration));
    }

    Result startCompute(Event* event) {
        ANeuralNetworksEvent* ev = nullptr;
        Result result = static_cast<Result>(ANeuralNetworksExecution_startCompute(mExecution, &ev));
        event->set(ev);
        return result;
    }

    Result startComputeWithDependencies(const std::vector<const Event*>& dependencies,
                                        uint64_t duration, Event* event) {
        std::vector<const ANeuralNetworksEvent*> deps(dependencies.size());
        std::transform(dependencies.begin(), dependencies.end(), deps.begin(),
                       [](const Event* e) { return e->getHandle(); });
        ANeuralNetworksEvent* ev = nullptr;
        Result result = static_cast<Result>(ANeuralNetworksExecution_startComputeWithDependencies(
                mExecution, deps.data(), deps.size(), duration, &ev));
        event->set(ev);
        return result;
    }

    Result compute() {
        switch (mComputeMode) {
            case ComputeMode::SYNC: {
                return static_cast<Result>(ANeuralNetworksExecution_compute(mExecution));
            }
            case ComputeMode::ASYNC: {
                ANeuralNetworksEvent* event = nullptr;
                Result result = static_cast<Result>(
                        ANeuralNetworksExecution_startCompute(mExecution, &event));
                if (result != Result::NO_ERROR) {
                    return result;
                }
                // TODO how to manage the lifetime of events when multiple waiters is not
                // clear.
                result = static_cast<Result>(ANeuralNetworksEvent_wait(event));
                ANeuralNetworksEvent_free(event);
                return result;
            }
            case ComputeMode::BURST: {
                ANeuralNetworksBurst* burst = nullptr;
                Result result =
                        static_cast<Result>(ANeuralNetworksBurst_create(mCompilation, &burst));
                if (result != Result::NO_ERROR) {
                    return result;
                }
                result = static_cast<Result>(
                        ANeuralNetworksExecution_burstCompute(mExecution, burst));
                ANeuralNetworksBurst_free(burst);
                return result;
            }
            case ComputeMode::FENCED: {
                ANeuralNetworksEvent* event = nullptr;
                Result result =
                        static_cast<Result>(ANeuralNetworksExecution_startComputeWithDependencies(
                                mExecution, nullptr, 0, 0, &event));
                if (result != Result::NO_ERROR) {
                    return result;
                }
                result = static_cast<Result>(ANeuralNetworksEvent_wait(event));
                ANeuralNetworksEvent_free(event);
                return result;
            }
        }
        return Result::BAD_DATA;
    }

    // By default, compute() uses the synchronous API. setComputeMode() can be
    // used to change the behavior of compute() to either:
    // - use the asynchronous API and then wait for computation to complete
    // or
    // - use the burst API
    // Returns the previous ComputeMode.
    enum class ComputeMode { SYNC, ASYNC, BURST, FENCED };
    static ComputeMode setComputeMode(ComputeMode mode) {
        ComputeMode oldComputeMode = mComputeMode;
        mComputeMode = mode;
        return oldComputeMode;
    }

    Result getOutputOperandDimensions(uint32_t index, std::vector<uint32_t>* dimensions) {
        uint32_t rank = 0;
        Result result = static_cast<Result>(
                ANeuralNetworksExecution_getOutputOperandRank(mExecution, index, &rank));
        dimensions->resize(rank);
        if ((result != Result::NO_ERROR && result != Result::OUTPUT_INSUFFICIENT_SIZE) ||
            rank == 0) {
            return result;
        }
        result = static_cast<Result>(ANeuralNetworksExecution_getOutputOperandDimensions(
                mExecution, index, dimensions->data()));
        return result;
    }

    ANeuralNetworksExecution* getHandle() { return mExecution; };

   private:
    ANeuralNetworksCompilation* mCompilation = nullptr;
    ANeuralNetworksExecution* mExecution = nullptr;

    // Initialized to ComputeMode::SYNC in TestNeuralNetworksWrapper.cpp.
    static ComputeMode mComputeMode;
};

}  // namespace test_wrapper
}  // namespace nn
}  // namespace android

#endif  // ANDROID_FRAMEWORKS_ML_NN_RUNTIME_TEST_TEST_NEURAL_NETWORKS_WRAPPER_H
