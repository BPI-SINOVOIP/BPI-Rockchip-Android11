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

// Class used to build a model through a succession of successive calls
// to the NN API.

#ifndef ANDROID_FRAMEWORKS_ML_NN_RUNTIME_MODEL_BUILDER_H
#define ANDROID_FRAMEWORKS_ML_NN_RUNTIME_MODEL_BUILDER_H

#include <memory>
#include <vector>

#include "HalInterfaces.h"
#include "Memory.h"
#include "NeuralNetworks.h"
#include "Utils.h"

namespace android {
namespace nn {

class CompilationBuilder;
class Device;
class ExecutionPlan;
class Memory;

class ModelBuilder {
   public:
    ModelBuilder() {}
    // Returns an operand/operation type corresponding to a given extension operand/operation type.
    int getExtensionType(const char* extensionName, uint16_t typeWithinExtension, int32_t* type);
    // Adds an operand to the model.
    int addOperand(const ANeuralNetworksOperandType& type);
    int setOperandValue(uint32_t index, const void* buffer, size_t length);
    int setOperandValueFromMemory(uint32_t index, const Memory* memory, uint32_t offset,
                                  size_t length);
    int setOperandValueFromModel(uint32_t index, const ModelBuilder* value);
    int setOperandSymmPerChannelQuantParams(
            uint32_t index, const ANeuralNetworksSymmPerChannelQuantParams& extraParams);
    int setOperandExtensionData(uint32_t index, const void* data, size_t length);

    int addOperation(ANeuralNetworksOperationType type, uint32_t inputCount, const uint32_t* inputs,
                     uint32_t outputCount, const uint32_t* outputs);
    int identifyInputsAndOutputs(uint32_t inputCount, const uint32_t* inputs, uint32_t outputCount,
                                 const uint32_t* outputs);
    int relaxComputationFloat32toFloat16(bool allow);
    bool isComputationFloat32RelaxedToFloat16() const { return mRelaxComputationFloat32toFloat16; }

    int finish();
    bool isFinished() const { return mCompletedModel; }
    bool isValid() const { return !mInvalidModel; }

    bool hasOEMOperation() const { return mHasOEMOperation; }
    bool hasExtensionOperation() const { return mHasExtensionOperation; }

    // explicitDeviceList is true if the list of devices was provided explicitly
    // via the ANeuralNetworksModel_createForDevices API (which has certain
    // special semantics) and false otherwise.
    int createCompilation(CompilationBuilder** compilation,
                          const std::vector<std::shared_ptr<Device>>& devices,
                          bool explicitDeviceList = false);

    hal::Model makeHidlModel() const;

    uint32_t operandCount() const {
        // We don't allow more than uint32_t worth of operands
        return static_cast<uint32_t>(mOperands.size());
    }
    uint32_t operationCount() const {
        // We don't allow more than uint32_t worth of operations
        return static_cast<uint32_t>(mOperations.size());
    }
    uint32_t inputCount() const { return static_cast<uint32_t>(mInputIndexes.size()); }
    uint32_t outputCount() const { return static_cast<uint32_t>(mOutputIndexes.size()); }
    uint32_t getInputOperandIndex(uint32_t i) const {
        CHECK_LT(i, mInputIndexes.size());
        return mInputIndexes[i];
    }
    const std::vector<uint32_t>& getInputOperandIndexes() const { return mInputIndexes; }
    const hal::Operand& getInputOperand(uint32_t i) const {
        uint32_t index = getInputOperandIndex(i);
        CHECK_LT(index, mOperands.size());
        return mOperands[index];
    }
    uint32_t getOutputOperandIndex(uint32_t i) const {
        CHECK_LT(i, mOutputIndexes.size());
        return mOutputIndexes[i];
    }
    const std::vector<uint32_t>& getOutputOperandIndexes() const { return mOutputIndexes; }
    const hal::Operand& getOutputOperand(uint32_t i) const {
        uint32_t index = getOutputOperandIndex(i);
        CHECK_LT(index, mOperands.size());
        return mOperands[index];
    }
    const hal::Operand& getOperand(uint32_t index) const { return mOperands[index]; }
    const hal::Operation& getOperation(uint32_t index) const { return mOperations[index]; }
    const MemoryTracker& getMemories() const { return mMemories; }
    const std::vector<hal::Operation>& getOperations() const { return mOperations; }
    const std::vector<uint32_t>& getSortedOperationMapping() const {
        return mSortedOperationIndexMap;
    }
    const uint8_t* getPointerToOperandValue(uint32_t offset) const {
        return mSmallOperandValues.data() + offset;
    }
    uint32_t referencedModelCount() const {
        return static_cast<uint32_t>(mReferencedModels.size());
    }
    const ModelBuilder* getReferencedModel(uint32_t i) const {
        CHECK_LT(i, mReferencedModels.size());
        return mReferencedModels[i];
    }
    const ModelBuilder* getReferencedModel(const hal::Operand& operand) const {
        CHECK(operand.lifetime == hal::OperandLifeTime::SUBGRAPH);
        return getReferencedModel(operand.location.offset);
    }

    int partitionTheWork(const std::vector<std::shared_ptr<Device>>& devices, uint32_t preference,
                         uint32_t priority, const std::optional<Deadline>& deadline,
                         ExecutionPlan* plan) const;

   private:
    // TODO(b/132322449): move partitionTheWork, findBestDeviceForEachOperation,
    // getPerformance, supportedByControlFlowInterpreter,
    // isControlFlowOperationWithOperandOfUnknownSize, partitionTheWorkInternal,
    // sortIntoRunOrder to CompilationBuilder?

    // Populates bestDeviceForOperation
    //
    // For 0 <= i < operationCount(), produces
    //
    //     0 <= (*bestDeviceForOperation)[i] <= devices.size()
    //
    // (*bestDeviceForOperation)[i] == devices.size() is a special value meaning
    // that this is a control flow operation scheduled for interpreted execution
    // (see LogicalStep).
    int findBestDeviceForEachOperation(uint32_t preference,
                                       const std::vector<std::shared_ptr<Device>>& devices,
                                       std::vector<int>* bestDeviceForOperation) const;
    float getPerformance(uint32_t preference, const std::shared_ptr<Device> device) const;
    float getPerformance(uint32_t preference, const std::shared_ptr<Device> device,
                         uint32_t operationIndex) const;
    bool supportedByControlFlowInterpreter(uint32_t operationIndex) const;

    // Returns true if the operation is IF or WHILE and has an inner or outer
    // input or output of unknown size.
    bool isControlFlowOperationWithOperandOfUnknownSize(uint32_t operationIndex) const;

    int partitionTheWorkInternal(uint32_t sourceModelIndex,
                                 const std::vector<std::shared_ptr<Device>>& devices,
                                 uint32_t preference, uint32_t priority,
                                 const std::optional<Deadline>& deadline,
                                 ExecutionPlan* plan) const;

    // Return true if either mCompleteModel or mInvalidModel is true.
    bool badState(const char* name);

    // Removes some trailing operation inputs that are set to default values.
    //
    // Some drivers reject operations based on the argument count even when the
    // optional arguments are set to default values. This transformation enables
    // more drivers to execute the model. See http://b/147105700.
    void removeTrailingArgumentsWithDefaultValues();
    uint32_t getNumTrailingArgumentsToRemove(const hal::Operation& operation) const;

    // Sorts the operations to be in the correct order for single threaded
    // node-at-a-time execution.
    bool sortIntoRunOrder();

    // Copies the large values to a shared memory, if we have any.
    int copyLargeValuesToSharedMemory();

    // The operations of the graph.
    std::vector<hal::Operation> mOperations;
    // The mapping from sorted index to the original index of operations in mOperations.
    // mSortedOperationIndexMap is empty before sortIntoRunOrder() is called.
    std::vector<uint32_t> mSortedOperationIndexMap;
    // Is at least one of those operations an OEM_OPERATION?
    bool mHasOEMOperation = false;
    // Is at least one of those operations an extension operation?
    bool mHasExtensionOperation = false;
    // The description of the operands of the graph.
    std::vector<hal::Operand> mOperands;
    // Is at least one of those operands an OEM operand?
    bool mHasOEMOperand = false;
    // The indexes of input operands of the model.
    std::vector<uint32_t> mInputIndexes;
    // The indexes of output operands of the model.
    std::vector<uint32_t> mOutputIndexes;

    MemoryTracker mMemories;

    // The value of the small operands that are defined at model
    // creation time.
    std::vector<uint8_t> mSmallOperandValues;

    struct LargeValue {
        uint32_t operandIndex;
        const void* buffer;
    };
    // Operand index and buffer pointer for all the large operand values of this model.
    std::vector<LargeValue> mLargeOperandValues;
    // The shared memory region that will contain the large values.
    std::unique_ptr<MemoryAshmem> mLargeValueMemory;

    // Once the model has been finished, we should not allow further
    // modifications to the model.
    bool mCompletedModel = false;

    // Any invalid manipulation of the model will mark the model invalid.
    // No further modifications are allowed to the model.
    bool mInvalidModel = false;

    // 'true' indicates TENSOR_FLOAT32 may be calculated with range and/or
    // precision as low as that of the IEEE 754 16-bit floating-point format.
    // 'false' indicates TENSOR_FLOAT32 must be calculated using at least the
    // range and precision of the IEEE 754 32-bit floating-point format.
    bool mRelaxComputationFloat32toFloat16 = false;

    // Models referenced by operands in this model.
    std::vector<const ModelBuilder*> mReferencedModels;

    class HidlModelMaker;
};

}  // namespace nn
}  // namespace android

#endif  // ANDROID_FRAMEWORKS_ML_NN_RUNTIME_MODEL_BUILDER_H
