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

#define LOG_TAG "ExecutionPlan"

#include "ExecutionPlan.h"

#include <android/sync.h>
#include <fcntl.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "BurstBuilder.h"
#include "Callbacks.h"
#include "CompilationBuilder.h"
#include "ControlFlow.h"
#include "CpuExecutor.h"
#include "ExecutionBuilder.h"
#include "ExecutionBurstController.h"
#include "GraphDump.h"
#include "Manager.h"
#include "MetaModel.h"
#include "ModelBuilder.h"
#include "OperationsUtils.h"
#include "TokenHasher.h"
#include "Tracing.h"
#include "TypeManager.h"
#include "Utils.h"

namespace android {
namespace nn {

namespace {

using namespace hal;

// The index of the main model in SourceModels.
constexpr uint32_t kMainModelInSourceModels = 0;

// Compiles the model on device.
// If compilation caching is available, depending on ExecutionPlan::mState, the token may only have
// been initialized by the user provided token (SIMPLE body), or is already re-hashed by the
// operation indices to be executed (COMPOUND body). The token will be re-hashed further by the
// device name, device version string, and the execution preference in this function.
int compile(const Device& device, const ModelBuilder& model, int executionPreference,
            int compilationPriority, const std::optional<Deadline>& deadline,
            const std::string& cacheDir, TokenHasher* token,
            std::shared_ptr<PreparedModel>* preparedModel) {
    CHECK(token != nullptr);
    CHECK(preparedModel != nullptr);
    *preparedModel = nullptr;

    std::optional<CacheToken> cacheToken;
    if (device.isCachingSupported() && token->ok() &&
        token->updateFromString(device.getName().c_str()) &&
        token->updateFromString(device.getVersionString().c_str()) &&
        token->update(&executionPreference, sizeof(executionPreference)) &&
        token->update(&compilationPriority, sizeof(compilationPriority)) && token->finish()) {
        cacheToken.emplace(token->getCacheToken());
    }

    const ModelFactory makeModel = [&model] { return model.makeHidlModel(); };
    const ExecutionPreference preference = static_cast<ExecutionPreference>(executionPreference);
    const Priority priority = convertToHalPriority(compilationPriority);
    const auto [n, returnedPreparedModel] =
            device.prepareModel(makeModel, preference, priority, deadline, cacheDir, cacheToken);
    *preparedModel = returnedPreparedModel;
    return n;
}

typedef std::function<void(uint32_t)> OperationReadyCallback;

int copyOperandExtraParams(ModelBuilder& model, uint32_t toOperandIndex,
                           const Operand& fromOperand) {
    if (fromOperand.type == OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL &&
        fromOperand.extraParams.getDiscriminator() ==
                OperandExtraParams::hidl_discriminator::channelQuant) {
        auto& fromChannelQuant = fromOperand.extraParams.channelQuant();
        ANeuralNetworksSymmPerChannelQuantParams toChannelQuant = {
                .channelDim = fromChannelQuant.channelDim,
                .scaleCount = static_cast<uint32_t>(fromChannelQuant.scales.size()),
                .scales = fromChannelQuant.scales.data(),
        };
        return model.setOperandSymmPerChannelQuantParams(toOperandIndex, toChannelQuant);
    } else if (isExtensionOperandType(fromOperand.type) &&
               fromOperand.extraParams.getDiscriminator() ==
                       OperandExtraParams::hidl_discriminator::extension) {
        hidl_vec<uint8_t> extensionData = fromOperand.extraParams.extension();
        return model.setOperandExtensionData(toOperandIndex, extensionData.data(),
                                             extensionData.size());
    } else if (fromOperand.extraParams.getDiscriminator() !=
                       OperandExtraParams::hidl_discriminator::none ||
               fromOperand.type == OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        LOG(ERROR) << "Type " << toString(fromOperand.type)
                   << " has an unexpected extraParams discriminator: "
                   << static_cast<int>(fromOperand.extraParams.getDiscriminator());
        return ANEURALNETWORKS_BAD_DATA;
    } else {
        return ANEURALNETWORKS_NO_ERROR;
    }
}

// This class tracks whether we know the value of an operand as operations
// are processed.
class OperandTracker {
   public:
    // Creates the tracker for this model. Figure out which operations can be
    // executed right away and cb for each one of them.
    OperandTracker(const ModelBuilder* model, OperationReadyCallback cb);
    // Mark the specified operation as having been processed. The output
    // of the operation now being known, this may make new operations to be
    // able to run.  Call cb for each one of them.
    void markProcessed(uint32_t operationIndex, OperationReadyCallback cb);

   private:
    const ModelBuilder* mModel;
    std::multimap<uint32_t, uint32_t> mOperandToOperations;
    std::vector<uint32_t> mUnknownInputCount;  // For each operation
};

OperandTracker::OperandTracker(const ModelBuilder* model, OperationReadyCallback cb)
    : mModel(model) {
    const auto& operations = mModel->getOperations();
    mUnknownInputCount.resize(operations.size());
    for (uint32_t operationIndex = 0; operationIndex < operations.size(); operationIndex++) {
        const Operation& operation = operations[operationIndex];
        uint32_t count = 0;
        for (uint32_t operandIndex : operation.inputs) {
            auto lifetime = mModel->getOperand(operandIndex).lifetime;
            if (lifetime == OperandLifeTime::TEMPORARY_VARIABLE ||
                lifetime == OperandLifeTime::SUBGRAPH_OUTPUT) {
                count++;
                mOperandToOperations.emplace(operandIndex, operationIndex);
            }
        }
        if (count == 0) {
            cb(operationIndex);
        }
        mUnknownInputCount[operationIndex] = count;
    }
}

void OperandTracker::markProcessed(uint32_t operationIndex, OperationReadyCallback cb) {
    // Mark all its outputs as known.
    const Operation& operation = mModel->getOperations()[operationIndex];
    for (uint32_t operandIndex : operation.outputs) {
        auto range = mOperandToOperations.equal_range(operandIndex);
        for (auto i = range.first; i != range.second; i++) {
            uint32_t& count = mUnknownInputCount[i->second];
            if (--count == 0) {
                cb(i->second);
            }
        }
    }
}

}  // namespace

ExecutionStep::ExecutionStep(ExecutionPlan* plan, uint32_t stepIndex, uint32_t sourceModelIndex,
                             std::shared_ptr<Device> device)
    : mPlan(plan),
      mIndex(stepIndex),
      mSourceModelIndex(sourceModelIndex),
      mStepModel(),
      mDevice(device),
      mToken(plan->getCacheToken()) {}

// Adds an operand if it has not been added already.
// Sets the index in the step model for the corresponding operand.
int ExecutionStep::addOperand(uint32_t sourceOperandIndex, uint32_t* stepOperandIndex,
                              OperandKind kind) {
    // Have we added this operand already?
    auto i = mOperandMap.find(sourceOperandIndex);
    if (i != mOperandMap.end()) {
        CHECK(kind == INPUT);
        *stepOperandIndex = i->second;
        return ANEURALNETWORKS_NO_ERROR;
    }

    // First time we add this operand.
    *stepOperandIndex = mStepModel.operandCount();
    mOperandMap.emplace(sourceOperandIndex, *stepOperandIndex);

    // Add the operand to the step model.
    const ModelBuilder& sourceModel = *getSourceModel();
    const Operand& operand = sourceModel.getOperand(sourceOperandIndex);
    ANeuralNetworksOperandType type = {
            .type = static_cast<int32_t>(operand.type),
            .dimensionCount = static_cast<uint32_t>(operand.dimensions.size()),
            .dimensions = operand.dimensions.size() > 0 ? operand.dimensions.data() : nullptr,
            .scale = operand.scale,
            .zeroPoint = operand.zeroPoint,
    };

    int n = mStepModel.addOperand(type);
    if (n != ANEURALNETWORKS_NO_ERROR) {
        LOG(ERROR) << "Previous error occurred when partitioning the graph";
        return n;
    }

    n = copyOperandExtraParams(mStepModel, *stepOperandIndex, operand);
    if (n != ANEURALNETWORKS_NO_ERROR) {
        LOG(ERROR) << "Error when copying extra parameters to the operand";
        return n;
    }

    // Sets its value.
    switch (operand.lifetime) {
        case OperandLifeTime::CONSTANT_COPY: {
            const uint8_t* data = sourceModel.getPointerToOperandValue(operand.location.offset);
            n = mStepModel.setOperandValue(*stepOperandIndex, data, operand.location.length);
            if (n != ANEURALNETWORKS_NO_ERROR) {
                LOG(ERROR) << "Previous error occurred when partitioning the graph";
                return n;
            }
        } break;
        case OperandLifeTime::CONSTANT_REFERENCE: {
            const Memory* memory = sourceModel.getMemories()[operand.location.poolIndex];
            n = mStepModel.setOperandValueFromMemory(
                    *stepOperandIndex, memory, operand.location.offset, operand.location.length);
            if (n != ANEURALNETWORKS_NO_ERROR) {
                LOG(ERROR) << "Previous error occurred when partitioning the graph";
                return n;
            }
        } break;
        case OperandLifeTime::NO_VALUE: {
            n = mStepModel.setOperandValue(*stepOperandIndex, nullptr, 0);
            if (n != ANEURALNETWORKS_NO_ERROR) {
                LOG(ERROR) << "Previous error occurred when partitioning the graph";
                return n;
            }
        } break;
        case OperandLifeTime::TEMPORARY_VARIABLE: {  // handled similarly to SUBGRAPH_OUTPUT
            if (kind == INPUT) {
                // The first time we've seen this operand is as an
                // input.  That means it must be defined by a
                // different partition, and is an input to this one.
                mTempsAsStepModelInputs.emplace_back(sourceOperandIndex, *stepOperandIndex);
            } else {
                // The first time we've seen this operand is as an
                // output.  It may be an input to a different
                // partition, so keep track of it.
                mPlan->recordTemporaryDef(SourceOperandIndex(mSourceModelIndex, sourceOperandIndex),
                                          mIndex);
            }
        } break;
        case OperandLifeTime::SUBGRAPH_INPUT: {
            mModelInputs.emplace_back(sourceOperandIndex, *stepOperandIndex);
        } break;
        case OperandLifeTime::SUBGRAPH_OUTPUT: {  // handled similarly to TEMPORARY_VARIABLE
            if (kind == INPUT) {
                // The first time we've seen this operand is as an
                // input.  That means it must be defined by a
                // different partition, and is an input to this one.
                mOutputsAsStepModelInputs.emplace_back(sourceOperandIndex, *stepOperandIndex);
            } else {
                // The first time we've seen this operand is as an
                // output.
                mModelOutputs.emplace_back(sourceOperandIndex, *stepOperandIndex);
            }
        } break;
        case OperandLifeTime::SUBGRAPH: {
            const ModelBuilder* model = sourceModel.getReferencedModel(operand);
            n = mStepModel.setOperandValueFromModel(*stepOperandIndex, model);
            if (n != ANEURALNETWORKS_NO_ERROR) {
                LOG(ERROR) << "Previous error occurred when partitioning the graph";
                return n;
            }
        } break;
        default: {
            CHECK(!"unexpected");
        } break;
    }

    return ANEURALNETWORKS_NO_ERROR;
}

int ExecutionStep::addOperation(int operationIndex) {
    const Operation& operation = getSourceModel()->getOperation(operationIndex);
    if (mToken.ok()) {
        mToken.update(&mSourceModelIndex, sizeof(mSourceModelIndex));
        mToken.update(&operationIndex, sizeof(operationIndex));
    }

    // Convert the input and output operand indexes.
    //
    // We expect operations to be added in topological order.  Therefore:
    //
    // - We may not have seen an input if it is a model input, a
    //   constant, or an operand written by a different partition.
    //
    // - We should not have seen any outputs.
    auto addOperands = [this](const hidl_vec<uint32_t>& sourceModelOperands,
                              std::vector<uint32_t>* stepModelOperands, OperandKind kind) -> int {
        const uint32_t operandCount = static_cast<uint32_t>(sourceModelOperands.size());
        for (uint32_t i = 0; i < operandCount; i++) {
            NN_RETURN_IF_ERROR(addOperand(sourceModelOperands[i], &stepModelOperands->at(i), kind));
        }
        return ANEURALNETWORKS_NO_ERROR;
    };

    const uint32_t inputCount = static_cast<uint32_t>(operation.inputs.size());
    const uint32_t outputCount = static_cast<uint32_t>(operation.outputs.size());
    std::vector<uint32_t> inputs(inputCount);
    std::vector<uint32_t> outputs(outputCount);
    NN_RETURN_IF_ERROR(addOperands(operation.inputs, &inputs, INPUT));
    NN_RETURN_IF_ERROR(addOperands(operation.outputs, &outputs, OUTPUT));
    return mStepModel.addOperation(static_cast<uint32_t>(operation.type), inputCount, inputs.data(),
                                   outputCount, outputs.data());
}

void ExecutionStep::mapInputsAndOutputs(
        std::shared_ptr<StepExecutor> executor, const Memory* temporaryMemory,
        const std::map<SourceOperandIndex, uint32_t>& sourceOperandToOffsetOfTemporary,
        const std::map<SourceOperandIndex, uint32_t>& sourceOperandToInputIndex,
        const std::map<SourceOperandIndex, uint32_t>& sourceOperandToOutputIndex,
        const std::map<SourceOperandIndex, ConstantReferenceLocation>&
                sourceOperandToConstantReference) const {
    auto mapInput = [&](uint32_t stepModelOperandIndex, uint32_t stepInputIndex) {
        SourceOperandIndex sourceOperandIndex(mSourceModelIndex, stepModelOperandIndex);
        if (auto it = sourceOperandToOffsetOfTemporary.find(sourceOperandIndex);
            it != sourceOperandToOffsetOfTemporary.end()) {
            executor->setInputFromMemory(stepInputIndex, temporaryMemory, it->second);
        } else if (auto it = sourceOperandToInputIndex.find(sourceOperandIndex);
                   it != sourceOperandToInputIndex.end()) {
            executor->mapInput(it->second, stepInputIndex);
        } else if (auto it = sourceOperandToOutputIndex.find(sourceOperandIndex);
                   it != sourceOperandToOutputIndex.end()) {
            executor->mapOutputToInput(it->second, stepInputIndex);
        } else if (auto it = sourceOperandToConstantReference.find(sourceOperandIndex);
                   it != sourceOperandToConstantReference.end()) {
            // Constant partition boundary operand. This could be an IF branch
            // model input or a WHILE variable initializer.
            executor->setInputFromMemory(stepInputIndex, it->second.memory, it->second.offset);
        } else {
            CHECK(false) << "Cannot map step input " << stepInputIndex << " from operand "
                         << toString(sourceOperandIndex);
        }
    };
    auto mapOutput = [&](uint32_t stepModelOperandIndex, uint32_t stepOutputIndex) {
        SourceOperandIndex sourceOperandIndex(mSourceModelIndex, stepModelOperandIndex);
        if (auto it = sourceOperandToOffsetOfTemporary.find(sourceOperandIndex);
            it != sourceOperandToOffsetOfTemporary.end()) {
            executor->setOutputFromMemory(stepOutputIndex, temporaryMemory, it->second);
        } else if (auto it = sourceOperandToOutputIndex.find(sourceOperandIndex);
                   it != sourceOperandToOutputIndex.end()) {
            executor->mapOutput(it->second, stepOutputIndex);
        } else {
            CHECK(false) << "Cannot map step output " << stepOutputIndex << " from operand "
                         << toString(sourceOperandIndex);
        }
    };
    for (uint32_t i = 0, n = mStepModelInputs.size(); i < n; ++i) {
        mapInput(mStepModelInputs[i].first, i);
    }
    for (uint32_t i = 0, n = mStepModelOutputs.size(); i < n; ++i) {
        mapOutput(mStepModelOutputs[i].first, i);
    }
}

void ExecutionPlan::CompoundBody::findTempsAsStepModelOutputs() {
    auto recordAsOutputIfTemporary = [this](const SourceOperandIndex& sourceOperandIndex) {
        const auto it = mTemporaryToDefiningExecutionStep.find(sourceOperandIndex);
        if (it == mTemporaryToDefiningExecutionStep.end()) {
            // The operand is not a temporary or is not defined by an
            // ExecutionStep (i.e. it's an output of an IF or a WHILE).
            // The latter case is handled by ExecutionPlan::makeController().
            return;
        }
        uint32_t stepIndex = it->second;
        CHECK_LT(stepIndex, mSteps.size());
        mSteps[stepIndex]->executionStep()->recordTempAsStepModelOutput(sourceOperandIndex.second);
    };
    for (const auto& logicalStep : mSteps) {
        if (const ExecutionStep* step = logicalStep->tryExecutionStep()) {
            for (const auto& input : step->getTempsAsStepModelInputs()) {
                SourceOperandIndex sourceOperandIndex(step->getSourceModelIndex(), input.first);
                recordAsOutputIfTemporary(sourceOperandIndex);
            }
        } else if (const IfStep* step = logicalStep->tryIfStep()) {
            recordAsOutputIfTemporary(step->conditionOperandIndex);
            for (const SourceOperandIndex& sourceOperandIndex : step->outerInputOperands) {
                recordAsOutputIfTemporary(sourceOperandIndex);
            }
        } else if (const WhileStep* step = logicalStep->tryWhileStep()) {
            for (const SourceOperandIndex& sourceOperandIndex : step->outerInputOperands) {
                recordAsOutputIfTemporary(sourceOperandIndex);
            }
        } else {
            CHECK(logicalStep->isGoto());
        }
    }
}

void ExecutionStep::recordTempAsStepModelOutput(uint32_t stepOperandIndex) {
    const auto it = mOperandMap.find(stepOperandIndex);
    CHECK(it != mOperandMap.end());
    mTempsAsStepModelOutputs.emplace(stepOperandIndex, it->second);
}

const ModelBuilder* ExecutionStep::getSourceModel() const {
    return mPlan->getSourceModels().getModel(mSourceModelIndex);
}

void ExecutionStep::logStepModel() const {
    VLOG(COMPILATION) << "ExecutionStep::finishStepModel, step " << mIndex;

    auto logRemapEntry = [](std::string& toLog, const std::pair<uint32_t, uint32_t>& e) {
        if (!toLog.empty()) {
            toLog += ", ";
        }
        toLog += toString(e.first);
        toLog += "->";
        toLog += toString(e.second);
    };

    auto logRemapVector = [&logRemapEntry](const char* name, const RemapVectorType& map) {
        std::string toLog;
        for (const auto& e : map) {
            logRemapEntry(toLog, e);
        }
        VLOG(COMPILATION) << name << ": " << toLog;
    };
    auto logRemapSet = [&logRemapEntry](const char* name, const StepModelOutputSetType& set) {
        std::string toLog;
        for (const auto& e : set) {
            logRemapEntry(toLog, e);
        }
        VLOG(COMPILATION) << name << ": " << toLog;
    };

    logRemapVector("step model inputs", mStepModelInputs);
    logRemapVector("step model outputs", mStepModelOutputs);
    logRemapVector("model inputs", mModelInputs);
    logRemapVector("model outputs", mModelOutputs);
    logRemapVector("temps as step model inputs", mTempsAsStepModelInputs);
    logRemapSet("temps as step model outputs", mTempsAsStepModelOutputs);
    logRemapVector("outputs as step model inputs", mOutputsAsStepModelInputs);
}

static bool hasUnknownSize(const Operand& operand) {
    if (operand.dimensions.size() == 0) {
        return TypeManager::get()->isTensorType(operand.type);
    }
    for (uint32_t dimension : operand.dimensions) {
        if (dimension == 0) {
            return true;
        }
    }
    return false;
}

int ExecutionStep::finishStepModel(const ModelBuilder* mainModel, bool* hasOutputOfUnknownSize,
                                   int32_t executionPreference, int32_t priority) {
    CHECK(mDevice != nullptr);

    for (const auto& stepModelOutput : mTempsAsStepModelOutputs) {
        const Operand& operand = mStepModel.getOperand(stepModelOutput.second);
        if (hasUnknownSize(operand)) {
            *hasOutputOfUnknownSize = true;
            VLOG(COMPILATION) << "StepModelOutput (operand#" << toString(stepModelOutput.first)
                              << " of source graph) has unknown size: " << toString(operand);
        }
    }

    mStepModel.relaxComputationFloat32toFloat16(mainModel->isComputationFloat32RelaxedToFloat16());

    mStepModelInputs.insert(mStepModelInputs.end(), mModelInputs.begin(), mModelInputs.end());
    mStepModelInputs.insert(mStepModelInputs.end(), mTempsAsStepModelInputs.begin(),
                            mTempsAsStepModelInputs.end());
    mStepModelInputs.insert(mStepModelInputs.end(), mOutputsAsStepModelInputs.begin(),
                            mOutputsAsStepModelInputs.end());

    mStepModelOutputs.insert(mStepModelOutputs.end(), mModelOutputs.begin(), mModelOutputs.end());
    mStepModelOutputs.insert(mStepModelOutputs.end(), mTempsAsStepModelOutputs.begin(),
                             mTempsAsStepModelOutputs.end());

    if (mSourceModelIndex == kMainModelInSourceModels) {
        std::map<uint32_t, uint32_t> mainModelOperandToInputIndex;
        for (uint32_t i = 0, n = mainModel->inputCount(); i < n; ++i) {
            mainModelOperandToInputIndex[mainModel->getInputOperandIndex(i)] = i;
        }
        std::map<uint32_t, uint32_t> mainModelOperandToOutputIndex;
        for (uint32_t i = 0, n = mainModel->outputCount(); i < n; ++i) {
            mainModelOperandToOutputIndex[mainModel->getOutputOperandIndex(i)] = i;
        }

        // mInputIndexStepModelToMainModel is ordered by step model input index and relies on
        // mModelInputs being the first inputs, as specified by mStepModelInputs.
        mInputIndexStepModelToMainModel.resize(mModelInputs.size());
        std::transform(mModelInputs.begin(), mModelInputs.end(),
                       mInputIndexStepModelToMainModel.begin(),
                       [&mainModelOperandToInputIndex](auto& e) {
                           uint32_t sourceOperandIndex = e.first;
                           return mainModelOperandToInputIndex[sourceOperandIndex];
                       });

        // mOutputIndexStepModelToMainModel is ordered by step model output index and relies on
        // mModelOutputs being the first outputs, as specified by mStepModelOutputs.
        mOutputIndexStepModelToMainModel.resize(mModelOutputs.size());
        std::transform(mModelOutputs.begin(), mModelOutputs.end(),
                       mOutputIndexStepModelToMainModel.begin(),
                       [&mainModelOperandToOutputIndex](auto& e) {
                           uint32_t sourceOperandIndex = e.first;
                           return mainModelOperandToOutputIndex[sourceOperandIndex];
                       });

        // mOutputsAsStepModelInputsIndexToMainModel is ordered by step model input index and relies
        // on mOutputsAsStepModelInputs being the first outputs.
        mOutputsAsStepModelInputsIndexToMainModel.resize(mOutputsAsStepModelInputs.size());
        std::transform(mOutputsAsStepModelInputs.begin(), mOutputsAsStepModelInputs.end(),
                       mOutputsAsStepModelInputsIndexToMainModel.begin(),
                       [&mainModelOperandToOutputIndex](auto& e) {
                           uint32_t sourceOperandIndex = e.first;
                           return mainModelOperandToOutputIndex[sourceOperandIndex];
                       });
    }

    if (VLOG_IS_ON(COMPILATION)) {
        logStepModel();
    }

    std::vector<uint32_t> inputs(mStepModelInputs.size());
    std::vector<uint32_t> outputs(mStepModelOutputs.size());
    std::transform(mStepModelInputs.begin(), mStepModelInputs.end(), inputs.begin(),
                   [](auto& e) { return e.second; });
    std::transform(mStepModelOutputs.begin(), mStepModelOutputs.end(), outputs.begin(),
                   [](auto& e) { return e.second; });
    NN_RETURN_IF_ERROR(mStepModel.identifyInputsAndOutputs(inputs.size(), inputs.data(),
                                                           outputs.size(), outputs.data()));
    // TODO: Model::finish() should use ValidationMode::RUNTIME when sending the
    // step model to CpuDevice. Right now, this is harmless because the only
    // difference in validation occurs with control flow operations and inputs
    // or outputs of unknown size and we never send control flow operations to
    // CpuDevice. We need to address this if this behavior changes (b/151634976).
    NN_RETURN_IF_ERROR(mStepModel.finish());

    // TODO: Move compilation elsewhere?
    VLOG(COMPILATION) << "ExecutionStep::finishStepModel, compilation on " << mDevice->getName();
    return compile(*mDevice, mStepModel, executionPreference, priority, {}, *mPlan->getCacheDir(),
                   &mToken, &mPreparedStepModel);
}

void ExecutionStep::dump() const {
    if (VLOG_IS_ON(COMPILATION)) {
        VLOG(COMPILATION) << "Step#" << mIndex << ": execute on " << mDevice->getName();
        logModelToInfo(mStepModel.makeHidlModel());
    }
}

std::string toString(const IfStep& step) {
    std::ostringstream oss;
    oss << "Step#" << step.index << ": if " << toString(step.conditionOperandIndex)
        << " then=" << step.thenStepIndex << " else=" << step.elseStepIndex;
    return oss.str();
}

std::string toString(const WhileStep& step) {
    std::ostringstream oss;
    oss << "Step#" << step.index << ": while cond=" << step.condStepIndex
        << " body=" << step.bodyStepIndex << " exit=" << step.exitStepIndex;
    return oss.str();
}

std::string toString(const GotoStep& step) {
    std::ostringstream oss;
    oss << "Step#" << step.index << ": goto " << step.gotoStepIndex;
    return oss.str();
}

void LogicalStep::dump() const {
    if (VLOG_IS_ON(COMPILATION)) {
        if (const IfStep* step = tryIfStep()) {
            VLOG(COMPILATION) << toString(*step);
        } else if (const WhileStep* step = tryWhileStep()) {
            VLOG(COMPILATION) << toString(*step);
        } else if (const GotoStep* step = tryGotoStep()) {
            VLOG(COMPILATION) << toString(*step);
        } else {
            executionStep()->dump();
        }
    }
}

int ExecutionPlan::CompoundBody::finish(const SourceModels* sourceModels,
                                        int32_t executionPreference, int32_t priority,
                                        const std::optional<Deadline>& deadline) {
    CHECK(!mSuccessfulFinish);
    CHECK(!deadline.has_value());
    const ModelBuilder* mainModel = sourceModels->getModel(kMainModelInSourceModels);

    auto containsUnknownSize = [sourceModels](const std::vector<SourceOperandIndex>& operands) {
        for (const auto& sourceOperandIndex : operands) {
            const ModelBuilder* sourceModel = sourceModels->getModel(sourceOperandIndex.first);
            const Operand& operand = sourceModel->getOperand(sourceOperandIndex.second);
            if (hasUnknownSize(operand)) {
                return true;
            }
        }
        return false;
    };

    findTempsAsStepModelOutputs();
    for (const auto& logicalStep : mSteps) {
        if (ExecutionStep* step = logicalStep->tryExecutionStep()) {
            int n = step->finishStepModel(mainModel, &mHasStepModelOutputOfUnknownSize,
                                          executionPreference, priority);
            if (n != ANEURALNETWORKS_NO_ERROR) {
                VLOG(COMPILATION)
                        << "ExecutionPlan::CompoundBody::finish -- finishStepModel failed";
                return n;
            }
        } else if (IfStep* step = logicalStep->tryIfStep()) {
            // The partitioner does not support dynamic temporaries (b/132458982).
            CHECK(!containsUnknownSize(step->outerInputOperands));
            CHECK(!containsUnknownSize(step->outerOutputOperands));
            // step->conditionOperandIndex has a static shape. See b/158557728.
            CHECK(!containsUnknownSize(step->thenBranchInputOperands));
            CHECK(!containsUnknownSize(step->thenBranchOutputOperands));
            CHECK(!containsUnknownSize(step->elseBranchInputOperands));
            CHECK(!containsUnknownSize(step->elseBranchOutputOperands));
        } else if (WhileStep* step = logicalStep->tryWhileStep()) {
            // The partitioner does not support dynamic temporaries (b/132458982).
            CHECK(!containsUnknownSize(step->outerInputOperands));
            CHECK(!containsUnknownSize(step->outerOutputOperands));
            CHECK(!containsUnknownSize(step->condInputOperands));
            // step->condOutputOperand has a static shape. See b/158557728.
            CHECK(!containsUnknownSize(step->bodyInputOperands));
            CHECK(!containsUnknownSize(step->bodyOutputOperands));
        } else {
            CHECK(logicalStep->isGoto());
        }
    }
    if (mHasStepModelOutputOfUnknownSize) {
        VLOG(COMPILATION)
                << "ExecutionPlan::CompoundBody::finish -- mHasStepModelOutputOfUnknownSize";
        return ANEURALNETWORKS_OP_FAILED;
    }

    for (uint32_t i = 0, n = mainModel->inputCount(); i < n; ++i) {
        SourceOperandIndex index(kMainModelInSourceModels, mainModel->getInputOperandIndex(i));
        mSourceOperandToInputIndex[index] = i;
    }
    for (uint32_t i = 0, n = mainModel->outputCount(); i < n; ++i) {
        SourceOperandIndex index(kMainModelInSourceModels, mainModel->getOutputOperandIndex(i));
        mSourceOperandToOutputIndex[index] = i;
    }

    findControlFlowBoundaryConstants(sourceModels);

    mSuccessfulFinish = true;
    return ANEURALNETWORKS_NO_ERROR;
}

void ExecutionPlan::CompoundBody::findControlFlowBoundaryConstants(
        const SourceModels* sourceModels) {
    auto handleBoundaryConstants = [this,
                                    sourceModels](const SourceOperandIndex& sourceOperandIndex) {
        const ModelBuilder* sourceModel = sourceModels->getModel(sourceOperandIndex.first);
        const Operand& operand = sourceModel->getOperand(sourceOperandIndex.second);
        const DataLocation& location = operand.location;
        if (operand.lifetime == OperandLifeTime::CONSTANT_COPY) {
            mSourceOperandToBoundaryConstantCopy[sourceOperandIndex] = {
                    .buffer = sourceModel->getPointerToOperandValue(location.offset),
                    .length = location.length,
            };
        } else if (operand.lifetime == OperandLifeTime::CONSTANT_REFERENCE) {
            mSourceOperandToBoundaryConstantReference[sourceOperandIndex] = {
                    .memory = sourceModel->getMemories()[location.poolIndex],
                    .offset = location.offset,
                    .length = location.length,
            };
        }
    };
    for (const auto& logicalStep : mSteps) {
        if (const IfStep* step = logicalStep->tryIfStep()) {
            handleBoundaryConstants(step->conditionOperandIndex);
            for (const auto& sourceOperandIndex : step->outerInputOperands) {
                handleBoundaryConstants(sourceOperandIndex);
            }
        } else if (const WhileStep* step = logicalStep->tryWhileStep()) {
            for (const auto& sourceOperandIndex : step->outerInputOperands) {
                handleBoundaryConstants(sourceOperandIndex);
            }
        }
    }
}

int ExecutionPlan::SimpleBody::finish(const SourceModels*, int32_t executionPreference,
                                      int32_t priority, const std::optional<Deadline>& deadline) {
    CHECK(!mSuccessfulFinish);
    CHECK(mDevice != nullptr);
    VLOG(COMPILATION) << "ExecutionPlan::SimpleBody::finish, compilation";
    const int n = compile(*mDevice, *mModel, executionPreference, priority, deadline, *mCacheDir,
                          &mToken, &mPreparedModel);
    mSuccessfulFinish = (n == ANEURALNETWORKS_NO_ERROR);
    return n;
}

int ExecutionPlan::finish(int32_t executionPreference, int32_t priority,
                          const std::optional<Deadline>& deadline) {
    CHECK(mBody != nullptr);
    return mBody->finish(&getSourceModels(), executionPreference, priority, deadline);
}

ExecutionPlan::Controller::Controller(const ExecutionPlan* plan, ExecutionBuilder* executionBuilder,
                                      const BurstBuilder* burstBuilder)
    : Controller(plan, executionBuilder, burstBuilder, 0, {}, {}, {}, {}, {}, {}) {}

ExecutionPlan::Controller::Controller(
        const ExecutionPlan* plan, ExecutionBuilder* executionBuilder,
        const BurstBuilder* burstBuilder, uint32_t totalSizeOfTemporaries,
        std::map<SourceOperandIndex, uint32_t> sourceOperandToOffsetOfTemporary,
        std::map<SourceOperandIndex, uint32_t> sourceOperandToOffsetOfTemporary2,
        std::map<SourceOperandIndex, uint32_t> sourceOperandToInputIndex,
        std::map<SourceOperandIndex, uint32_t> sourceOperandToOutputIndex,
        const std::map<SourceOperandIndex, ConstantCopyLocation>& sourceOperandToConstantCopy,
        std::map<SourceOperandIndex, ConstantReferenceLocation> sourceOperandToConstantReference)
    : mPlan(plan),
      mExecutionBuilder(executionBuilder),
      mBurstBuilder(burstBuilder),
      mSourceOperandToOffsetOfTemporary(std::move(sourceOperandToOffsetOfTemporary)),
      mSourceOperandToOffsetOfTemporary2(std::move(sourceOperandToOffsetOfTemporary2)),
      mSourceOperandToInputIndex(std::move(sourceOperandToInputIndex)),
      mSourceOperandToOutputIndex(std::move(sourceOperandToOutputIndex)),
      mSourceOperandToConstantReference(std::move(sourceOperandToConstantReference)),
      mNextStepIndex(0),
      mFallbackNextStepIndex(kBadStepIndex),
      mLastStepSyncFd(-1) {
    if (totalSizeOfTemporaries == 0) {
        return;
    }
    int n;
    std::tie(n, mTemporaries) = MemoryAshmem::create(totalSizeOfTemporaries);
    if (n != ANEURALNETWORKS_NO_ERROR) {
        LOG(ERROR) << "ExecutionPlan::Controller failed to allocate temporaries";
        mNextStepIndex = kBadStepIndex;
    }
    for (const auto& [sourceOperandIndex, location] : sourceOperandToConstantCopy) {
        memcpy(mTemporaries->getPointer() + mSourceOperandToOffsetOfTemporary[sourceOperandIndex],
               location.buffer, location.length);
    }
}

// Attempt to create a burst object for each PreparedModel/Partition. If the
// burst controller object cannot be made, return a nullptr in its place to
// indicate the regular execution path should be used. This can occur either
// because PreparedModel was nullptr (cpu was best choice), or because the
// IPreparedModel was of insufficient version or failed to configure the burst.
std::vector<std::shared_ptr<ExecutionBurstController>> ExecutionPlan::makeBursts(
        int preference) const {
    switch (mState) {
        // burst object for each partition in the compound case
        case COMPOUND: {
            std::vector<std::shared_ptr<ExecutionBurstController>> bursts;
            bursts.reserve(compound()->mSteps.size());
            for (const auto& logicalStep : compound()->mSteps) {
                if (!logicalStep->isExecution()) {
                    bursts.push_back(nullptr);
                    continue;
                }
                if (const auto preparedModel =
                            logicalStep->executionStep()->getPreparedStepModel()) {
                    const bool preferPowerOverLatency =
                            (preference == ANEURALNETWORKS_PREFER_LOW_POWER);
                    bursts.push_back(
                            preparedModel->configureExecutionBurst(preferPowerOverLatency));
                } else {
                    bursts.push_back(nullptr);
                }
            }
            return bursts;
        }
        // single burst object for the simple case
        case SIMPLE: {
            std::vector<std::shared_ptr<ExecutionBurstController>> burst;
            auto simpleBody = simple();
            if (const auto preparedModel = simpleBody->mPreparedModel) {
                const bool preferPowerOverLatency =
                        (preference == ANEURALNETWORKS_PREFER_LOW_POWER);
                burst.push_back(preparedModel->configureExecutionBurst(preferPowerOverLatency));
            } else {
                burst.push_back(nullptr);
            }
            return burst;
        }
        // no burst objects made
        default:
            return {};
    }
}

std::shared_ptr<ExecutionPlan::Controller> ExecutionPlan::makeController(
        ExecutionBuilder* executionBuilder, const BurstBuilder* burstBuilder) const {
    CHECK(isValid());
    if (mState == SIMPLE) {
        return std::shared_ptr<Controller>(new Controller(this, executionBuilder, burstBuilder));
    }
    // Create the layout for a Memory object big enough to hold
    // - every partition boundary TEMPORARY operand and
    // - buffers required by the control flow implementation.
    //
    // TODO: Rethink this approach for managing temporaries.  Some
    // alternatives:
    //
    // 1) Adopt a memory layout scheme analogous to stack allocation,
    // where objects of non-overlapping lifetime can occupy the same
    // storage.  We would still have a single Memory object in this
    // case.
    //
    // 2) Do something like what CpuExecutor does, and do allocations
    // and deallocations on the fly (during execution) before first
    // reference and after last reference, respectively.  This would
    // mean having one Memory object per TEMPORARY; or, in a more
    // complicated implementation, one Memory object per set of
    // temporaries that have the same lifetime.  Note that the Android
    // system limits the number of shared memory objects, which are
    // what our Memory objects represent.
    //
    uint32_t totalSizeOfTemporaries = 0;
    auto addTemporaryOfSize = [&totalSizeOfTemporaries](uint32_t size) {
        totalSizeOfTemporaries += alignBytesNeeded(totalSizeOfTemporaries, size);
        const uint32_t offset = totalSizeOfTemporaries;
        totalSizeOfTemporaries += size;
        return offset;
    };
    // This function has two modes of operation:
    // 1. When lifetime is TEMPORARY_VARIABLE, we allocate memory for
    //    TEMPORARY_VARIABLE source operands, skip SUBGRAPH_OUTPUT source
    //    operands, and panic if we see a source operand of another lifetime.
    // 2. When lifetime is SUBGRAPH_OUTPUT, we allocate memory for
    //    SUBGRAPH_OUTPUT source operands and panic if we see a source operand
    //    of another lifetime.
    auto mapTemporary =
            [executionBuilder, addTemporaryOfSize](
                    const SourceOperandIndex& sourceOperandIndex,
                    std::map<SourceOperandIndex, uint32_t>* sourceOperandToOffsetOfTemporary,
                    OperandLifeTime lifetime = OperandLifeTime::TEMPORARY_VARIABLE) {
                CHECK(lifetime == OperandLifeTime::TEMPORARY_VARIABLE ||
                      lifetime == OperandLifeTime::SUBGRAPH_OUTPUT);
                const Operand& sourceOperand =
                        executionBuilder->getSourceOperand(sourceOperandIndex);
                if (lifetime == OperandLifeTime::TEMPORARY_VARIABLE &&
                    sourceOperand.lifetime == OperandLifeTime::SUBGRAPH_OUTPUT) {
                    // See the caller for explanation.
                    return;
                }
                CHECK(sourceOperand.lifetime == lifetime);
                const uint32_t size = TypeManager::get()->getSizeOfData(sourceOperand);
                CHECK_NE(size, 0u);
                const uint32_t offset = addTemporaryOfSize(size);
                auto [_, isNew] =
                        sourceOperandToOffsetOfTemporary->emplace(sourceOperandIndex, offset);
                CHECK(isNew);
                VLOG(EXECUTION) << "temp: operand " << toString(sourceOperandIndex)
                                << " offset = " << offset;
            };
    std::map<SourceOperandIndex, uint32_t> sourceOperandToOffsetOfTemporary;
    std::map<SourceOperandIndex, uint32_t> sourceOperandToOffsetOfTemporary2;
    for (const auto& logicalStep : compound()->mSteps) {
        if (const ExecutionStep* step = logicalStep->tryExecutionStep()) {
            // Allocate memory for ExecutionStep temporary outputs that are
            // inputs to other steps, as determined by
            // ExecutionPlan::CompoundBody::findTempsAsStepModelOutputs().
            //
            // We don't allocate memory for step model output operands with
            // source operand lifetime SUBGRAPH_OUTPUT because they will be
            // - managed by the client (main model outputs),
            // - assigned a location of another operand (when this step model
            //   output is a branch model output of an IF; see
            //   ExecutionPlan::nextCompound(const IfStep*, ...)), or
            // - allocated by a WHILE (when this step model output
            //   is a condition or body model output of a WHILE; see the
            //   step->bodyOutputOperands and step->condOutputOperand handling
            //   below).
            for (const auto& output : step->getTempsAsStepModelOutputs()) {
                mapTemporary(SourceOperandIndex(step->getSourceModelIndex(), output.first),
                             &sourceOperandToOffsetOfTemporary);
            }
        } else if (const IfStep* step = logicalStep->tryIfStep()) {
            // Allocate memory for all temporary outputs of an IfStep because
            // they are going to be written to by a branch model. We don't
            // perform unused output operand optimisation for referenced models.
            //
            // We don't allocate memory for branch output operands because they
            // use the same location as the corresponding outer output operands,
            // as established in ExecutionPlan::nextCompound(const IfStep*, ...)
            //
            // We don't allocate memory for outer output operands with source
            // operand lifetime SUBGRAPH_OUTPUT because they will be
            // - managed by the client (main model outputs),
            // - assigned a location of another operand (when this IF outer
            //   output is a branch model output of another IF; see
            //   ExecutionPlan::nextCompound(const IfStep*, ...)), or
            // - allocated by a WHILE (when this IF outer output
            //   is a condition or body model output of a WHILE; see the
            //   step->bodyOutputOperands and step->condOutputOperand handling
            //   below).
            for (const auto& sourceOperandIndex : step->outerOutputOperands) {
                mapTemporary(sourceOperandIndex, &sourceOperandToOffsetOfTemporary);
            }
        } else if (const WhileStep* step = logicalStep->tryWhileStep()) {
            // Allocate memory for all temporary outputs of an WhileStep because
            // they are going to be written to by the WHILE loop.
            //
            // We don't allocate memory for outer output operands with source
            // operand lifetime SUBGRAPH_OUTPUT because they will be
            // - managed by the client (main model outputs),
            // - assigned a location of another operand (when this WHILE outer
            //   output is a branch model output of an IF; see
            //   ExecutionPlan::nextCompound(const IfStep*, ...)), or
            // - allocated by another WHILE (when this WHILE outer output
            //   is a condition or body model output of another WHILE; see the
            //   step->bodyOutputOperands and step->condOutputOperand handling
            //   below).
            for (const auto& sourceOperandIndex : step->outerOutputOperands) {
                mapTemporary(sourceOperandIndex, &sourceOperandToOffsetOfTemporary);
            }
            // Allocate memory for body model outputs. Note that we could use
            // the outer output operand memory instead but we currently don't do
            // so (b/148206073).
            for (const auto& sourceOperandIndex : step->bodyOutputOperands) {
                mapTemporary(sourceOperandIndex, &sourceOperandToOffsetOfTemporary,
                             OperandLifeTime::SUBGRAPH_OUTPUT);
                // Allocate another set of temporaries for double buffering.
                mapTemporary(sourceOperandIndex, &sourceOperandToOffsetOfTemporary2,
                             OperandLifeTime::SUBGRAPH_OUTPUT);
            }
            // Allocate memory for condition model output.
            // TODO: Share one condition output memory region between all loops.
            mapTemporary(step->condOutputOperand, &sourceOperandToOffsetOfTemporary,
                         OperandLifeTime::SUBGRAPH_OUTPUT);
        } else {
            CHECK(logicalStep->isGoto());
        }
    }
    // Allocate temporary memory for boundary CONSTANT_COPY operands.
    for (const auto& [sourceOperandIndex, location] :
         compound()->mSourceOperandToBoundaryConstantCopy) {
        const uint32_t offset = addTemporaryOfSize(location.length);
        sourceOperandToOffsetOfTemporary.emplace(sourceOperandIndex, offset);
        VLOG(EXECUTION) << "temp (boundary constant): operand " << toString(sourceOperandIndex)
                        << " offset = " << offset;
    }
    return std::shared_ptr<Controller>(new Controller(
            this, executionBuilder, burstBuilder, totalSizeOfTemporaries,
            std::move(sourceOperandToOffsetOfTemporary),
            std::move(sourceOperandToOffsetOfTemporary2), compound()->mSourceOperandToInputIndex,
            compound()->mSourceOperandToOutputIndex,
            compound()->mSourceOperandToBoundaryConstantCopy,
            compound()->mSourceOperandToBoundaryConstantReference));
}

// TODO: Find a better way to provide this functionality.
int ExecutionPlan::fallback(std::shared_ptr<Controller> controller,
                            std::shared_ptr<StepExecutor>* executor) const {
    *executor = nullptr;

    VLOG(EXECUTION) << "ExecutionPlan::fallback(" << SHOW_IF_DEBUG(controller << ", " << executor)
                    << "): mFallbackNextStepIndex = " << controller->mFallbackNextStepIndex;

    if (controller->mFallbackNextStepIndex == Controller::kBadStepIndex) {
        // We haven't called next().
        return ANEURALNETWORKS_OP_FAILED;
    }

    if (controller->mNextStepIndex == Controller::kBadStepIndex) {
        // The last call to next() did not produce an executor.
        return ANEURALNETWORKS_OP_FAILED;
    }

    controller->mNextStepIndex = controller->mFallbackNextStepIndex;
    return next(controller, executor);
}

ExecutionPlan::Buffer::Buffer(void* pointer, uint32_t size)
    : mInfo(RunTimePoolInfo::createFromExistingBuffer(reinterpret_cast<uint8_t*>(pointer), size)),
      mOffset(0) {}

ExecutionPlan::Buffer::Buffer(RunTimePoolInfo info, uint32_t offset)
    : mInfo(std::move(info)), mOffset(offset) {}

void* ExecutionPlan::Buffer::getPointer() const {
    return mInfo.getBuffer() + mOffset;
}

uint32_t ExecutionPlan::Buffer::getSize() const {
    return mInfo.getSize() - mOffset;
}

void ExecutionPlan::Buffer::flush() const {
    mInfo.flush();
}

std::optional<ExecutionPlan::Buffer> ExecutionPlan::getBufferFromModelArgumentInfo(
        const ModelArgumentInfo& info, const ExecutionBuilder* executionBuilder) const {
    switch (info.state()) {
        case ModelArgumentInfo::POINTER: {
            return Buffer(info.buffer(), info.length());
        } break;
        case ModelArgumentInfo::MEMORY: {
            if (std::optional<RunTimePoolInfo> poolInfo =
                        executionBuilder->getRunTimePoolInfo(info.locationAndLength().poolIndex)) {
                return Buffer(*poolInfo, info.locationAndLength().offset);
            } else {
                LOG(ERROR) << "Unable to map operand memory pool";
                return std::nullopt;
            }
        } break;
        case ModelArgumentInfo::HAS_NO_VALUE: {
            LOG(ERROR) << "Attempting to read an operand that has no value";
            return std::nullopt;
        } break;
        default: {
            LOG(ERROR) << "Unexpected operand memory state: " << static_cast<int>(info.state());
            return std::nullopt;
        } break;
    }
}

std::optional<ExecutionPlan::Buffer> ExecutionPlan::getBuffer(
        std::shared_ptr<Controller> controller, SourceOperandIndex operandIndex) const {
    const auto& sourceOperandToOffsetOfTemporary = controller->mSourceOperandToOffsetOfTemporary;
    const auto& sourceOperandToInputIndex = controller->mSourceOperandToInputIndex;
    const auto& sourceOperandToOutputIndex = controller->mSourceOperandToOutputIndex;
    const auto& sourceOperandToConstantReference = controller->mSourceOperandToConstantReference;
    if (auto it = sourceOperandToOffsetOfTemporary.find(operandIndex);
        it != sourceOperandToOffsetOfTemporary.end()) {
        const uint32_t offset = it->second;
        const std::unique_ptr<MemoryAshmem>& memory = controller->mTemporaries;
        return Buffer(memory->getPointer() + offset, memory->getSize() - offset);
    } else if (auto it = sourceOperandToInputIndex.find(operandIndex);
               it != sourceOperandToInputIndex.end()) {
        const ModelArgumentInfo& info = controller->mExecutionBuilder->getInputInfo(it->second);
        return getBufferFromModelArgumentInfo(info, controller->mExecutionBuilder);
    } else if (auto it = sourceOperandToOutputIndex.find(operandIndex);
               it != sourceOperandToOutputIndex.end()) {
        const ModelArgumentInfo& info = controller->mExecutionBuilder->getOutputInfo(it->second);
        return getBufferFromModelArgumentInfo(info, controller->mExecutionBuilder);
    } else if (auto it = sourceOperandToConstantReference.find(operandIndex);
               it != sourceOperandToConstantReference.end()) {
        const ConstantReferenceLocation& location = it->second;
        const std::optional<RunTimePoolInfo> info = location.memory->getRunTimePoolInfo();
        if (info == std::nullopt) {
            return std::nullopt;
        }
        return Buffer(info->getBuffer() + location.offset, location.length);
    }
    return std::nullopt;
}

int ExecutionPlan::readConditionValue(std::shared_ptr<Controller> controller,
                                      SourceOperandIndex operandIndex, bool* value) const {
    std::optional<ExecutionPlan::Buffer> buffer = getBuffer(controller, operandIndex);
    if (buffer == std::nullopt) {
        LOG(ERROR) << "Unable to read operand " << toString(operandIndex);
        return ANEURALNETWORKS_OP_FAILED;
    }
    CHECK_GE(buffer->getSize(), sizeof(bool8));
    bool8 value8 = *static_cast<bool8*>(buffer->getPointer());
    *value = static_cast<bool>(value8);
    VLOG(EXECUTION) << "readConditionValue: " << *value;
    return ANEURALNETWORKS_NO_ERROR;
}

int ExecutionPlan::next(std::shared_ptr<Controller> controller,
                        std::shared_ptr<StepExecutor>* executor,
                        std::shared_ptr<ExecutionBurstController>* burstController,
                        int syncFdOfLastStep) const {
    controller->mLastStepSyncFd = syncFdOfLastStep;
    *executor = nullptr;
    if (burstController != nullptr) {
        *burstController = nullptr;
    }

    VLOG(EXECUTION) << "ExecutionPlan::next(" << SHOW_IF_DEBUG(controller << ", " << executor)
                    << "): mNextStepIndex = " << controller->mNextStepIndex;

    if (controller->mNextStepIndex == Controller::kBadStepIndex) {
        return ANEURALNETWORKS_OP_FAILED;
    }

    if (mState == EMPTY) {
        CHECK_EQ(controller->mNextStepIndex, 0u);  // end
        controller->mNextStepIndex = Controller::kBadStepIndex;
        return ANEURALNETWORKS_NO_ERROR;
    }

    if (mState == SIMPLE) {
        if (controller->mNextStepIndex == 0) {
            // First (and only) step.
            auto simpleBody = simple();
            *executor = std::make_shared<StepExecutor>(controller->mExecutionBuilder,
                                                       simpleBody->mModel, simpleBody->mDevice,
                                                       simpleBody->mPreparedModel);
            (*executor)->mapInputsAndOutputsTrivially();
            if (burstController != nullptr && controller->mBurstBuilder != nullptr) {
                *burstController = controller->mBurstBuilder->getControllerAt(0);
            }
            controller->mFallbackNextStepIndex = 0;
            controller->mNextStepIndex = 1;
            return ANEURALNETWORKS_NO_ERROR;
        }

        CHECK_EQ(controller->mNextStepIndex, 1u);  // end
        controller->mNextStepIndex = Controller::kBadStepIndex;
        return ANEURALNETWORKS_NO_ERROR;
    }

    return nextCompound(controller, executor, burstController);
}

int ExecutionPlan::nextCompound(std::shared_ptr<Controller> controller,
                                std::shared_ptr<StepExecutor>* executor,
                                std::shared_ptr<ExecutionBurstController>* burstController) const {
    if (controller->mNextStepIndex == Controller::kBadStepIndex) {
        return ANEURALNETWORKS_OP_FAILED;
    }

    auto compoundBody = compound();
    if (controller->mNextStepIndex == compoundBody->mSteps.size()) {
        controller->mNextStepIndex = Controller::kBadStepIndex;  // end
        return ANEURALNETWORKS_NO_ERROR;
    }

    const auto& logicalStep = compoundBody->mSteps[controller->mNextStepIndex];
    if (const IfStep* step = logicalStep->tryIfStep()) {
        return nextCompound(step, controller, executor, burstController);
    } else if (const WhileStep* step = logicalStep->tryWhileStep()) {
        return nextCompound(step, controller, executor, burstController);
    } else if (const GotoStep* step = logicalStep->tryGotoStep()) {
        return nextCompound(step, controller, executor, burstController);
    } else if (const ExecutionStep* step = logicalStep->tryExecutionStep()) {
        return nextCompound(step, controller, executor, burstController);
    } else {
        CHECK(false) << "Unknown step variant";
        return ANEURALNETWORKS_BAD_STATE;
    }
}

int ExecutionPlan::nextCompound(const ExecutionStep* step, std::shared_ptr<Controller> controller,
                                std::shared_ptr<StepExecutor>* executor,
                                std::shared_ptr<ExecutionBurstController>* burstController) const {
    VLOG(EXECUTION) << "next: Step#" << controller->mNextStepIndex << ": execute on "
                    << step->getDevice()->getName();
    *executor =
            std::make_shared<StepExecutor>(controller->mExecutionBuilder, step->getStepModel(),
                                           step->getDevice(), step->getPreparedStepModel(), step);
    step->mapInputsAndOutputs(
            *executor, controller->mTemporaries.get(),
            controller->mSourceOperandToOffsetOfTemporary, controller->mSourceOperandToInputIndex,
            controller->mSourceOperandToOutputIndex, controller->mSourceOperandToConstantReference);
    if (burstController != nullptr && controller->mBurstBuilder != nullptr) {
        *burstController = controller->mBurstBuilder->getControllerAt(controller->mNextStepIndex);
    }

    controller->mFallbackNextStepIndex = controller->mNextStepIndex;
    controller->mNextStepIndex++;
    return ANEURALNETWORKS_NO_ERROR;
}

// The first argument is the "source" operand, the second operand is the "destination".
void ExecutionPlan::Controller::setInput(const SourceOperandIndex& outerOperand,
                                         const SourceOperandIndex& innerOperand) {
    VLOG(EXECUTION) << "mapping input " << toString(innerOperand) << " from "
                    << toString(outerOperand);
#ifdef NN_DEBUGGABLE
    CHECK_LE(mSourceOperandToOffsetOfTemporary.count(innerOperand) +
                     mSourceOperandToInputIndex.count(innerOperand) +
                     mSourceOperandToOutputIndex.count(innerOperand) +
                     mSourceOperandToConstantReference.count(innerOperand),
             1u);
#endif
    mSourceOperandToOffsetOfTemporary.erase(innerOperand);
    mSourceOperandToInputIndex.erase(innerOperand);
    mSourceOperandToOutputIndex.erase(innerOperand);
    mSourceOperandToConstantReference.erase(innerOperand);
    if (auto it = mSourceOperandToOffsetOfTemporary.find(outerOperand);
        it != mSourceOperandToOffsetOfTemporary.end()) {
        mSourceOperandToOffsetOfTemporary.emplace(innerOperand, it->second);
    } else if (auto it = mSourceOperandToInputIndex.find(outerOperand);
               it != mSourceOperandToInputIndex.end()) {
        mSourceOperandToInputIndex.emplace(innerOperand, it->second);
    } else if (auto it = mSourceOperandToOutputIndex.find(outerOperand);
               it != mSourceOperandToOutputIndex.end()) {
        mSourceOperandToOutputIndex.emplace(innerOperand, it->second);
    } else if (auto it = mSourceOperandToConstantReference.find(outerOperand);
               it != mSourceOperandToConstantReference.end()) {
        mSourceOperandToConstantReference.emplace(innerOperand, it->second);
    } else {
        CHECK(false) << "Cannot set step model input operand " << toString(innerOperand)
                     << " from operand " << toString(outerOperand);
    }
}

// The first argument is the "source" operand, the second operand is the "destination".
void ExecutionPlan::Controller::setOutput(const SourceOperandIndex& outerOperand,
                                          const SourceOperandIndex& innerOperand) {
    VLOG(EXECUTION) << "mapping output " << toString(innerOperand) << " from "
                    << toString(outerOperand);
#ifdef NN_DEBUGGABLE
    CHECK_LE(mSourceOperandToOffsetOfTemporary.count(innerOperand) +
                     mSourceOperandToOutputIndex.count(innerOperand),
             1u);
#endif
    mSourceOperandToOffsetOfTemporary.erase(innerOperand);
    mSourceOperandToOutputIndex.erase(innerOperand);
    if (auto it = mSourceOperandToOffsetOfTemporary.find(outerOperand);
        it != mSourceOperandToOffsetOfTemporary.end()) {
        mSourceOperandToOffsetOfTemporary.emplace(innerOperand, it->second);
    } else if (auto it = mSourceOperandToOutputIndex.find(outerOperand);
               it != mSourceOperandToOutputIndex.end()) {
        mSourceOperandToOutputIndex.emplace(innerOperand, it->second);
    } else {
        CHECK(false) << "Cannot set step model output operand " << toString(innerOperand)
                     << " from operand " << toString(outerOperand);
    }
}

int ExecutionPlan::Controller::waitForLastStepSyncFence() const {
    if (mLastStepSyncFd == -1) {
        return ANEURALNETWORKS_NO_ERROR;
    }
    VLOG(EXECUTION) << "wait for mLastStepSyncFd " << mLastStepSyncFd;
    auto r = syncWait(mLastStepSyncFd, -1);
    int n = ANEURALNETWORKS_NO_ERROR;
    if (r != FenceState::SIGNALED) {
        LOG(ERROR) << "syncWait failed, fd: " << mLastStepSyncFd;
        n = ANEURALNETWORKS_OP_FAILED;
    }
    return n;
}

int ExecutionPlan::nextCompound(const IfStep* step, std::shared_ptr<Controller> controller,
                                std::shared_ptr<StepExecutor>* executor,
                                std::shared_ptr<ExecutionBurstController>* burstController) const {
    VLOG(EXECUTION) << "next: " << toString(*step);
    // If the last step has a sync fence, wait for it to signal before reading the condition value.
    // This is safe because the steps are serialized when doing fenced compute.
    NN_RETURN_IF_ERROR(controller->waitForLastStepSyncFence());
    bool condValue;
    NN_RETURN_IF_ERROR(readConditionValue(controller, step->conditionOperandIndex, &condValue));
    controller->mNextStepIndex = condValue ? step->thenStepIndex : step->elseStepIndex;
    const std::vector<SourceOperandIndex>& branchInputOperands =
            condValue ? step->thenBranchInputOperands : step->elseBranchInputOperands;
    const std::vector<SourceOperandIndex>& branchOutputOperands =
            condValue ? step->thenBranchOutputOperands : step->elseBranchOutputOperands;
    CHECK_EQ(branchInputOperands.size(), step->outerInputOperands.size());
    CHECK_EQ(branchOutputOperands.size(), step->outerOutputOperands.size());
    for (uint32_t i = 0, n = step->outerInputOperands.size(); i < n; ++i) {
        // We have to do this assignment just before executing this step to
        // accommodate cases when the IF resides within a WHILE condition or
        // body model and for some j the i-th input of the IF branch model is
        // - an input of the WHILE condition model (whileStep->condInputOperands[j]),
        // - an input of the WHILE body model (whileStep->bodyInputOperands[j]), or
        // - an output of the WHILE body model (whileStep->bodyOutputOperands[j]).
        // In such cases, the WhileStep modifies the location of
        // step->outerInputOperands[i] to implement double buffering.
        controller->setInput(step->outerInputOperands[i], branchInputOperands[i]);
    }
    for (uint32_t i = 0, n = step->outerOutputOperands.size(); i < n; ++i) {
        // We have to do this assignment just before executing this step to
        // accommodate the case when the IF resides within a WHILE body
        // model and the i-th output of the IF branch model is an
        // output of the WHILE body model (whileStep->bodyOutputOperands[j] for
        // some j). In that case, the WhileStep modifies the location of
        // step->outerOutputOperands[i] to implement double buffering.
        controller->setOutput(step->outerOutputOperands[i], branchOutputOperands[i]);
    }
    return nextCompound(controller, executor, burstController);
}

int ExecutionPlan::nextCompound(const WhileStep* step, std::shared_ptr<Controller> controller,
                                std::shared_ptr<StepExecutor>* executor,
                                std::shared_ptr<ExecutionBurstController>* burstController) const {
    WhileState& state = controller->mWhileState[controller->mNextStepIndex];
    if (state.stage == WhileState::EVALUATE_CONDITION) {
        state.iteration = state.iteration == WhileState::kOutsideLoop ? 0 : state.iteration + 1;
        VLOG(EXECUTION) << "next: " << toString(*step) << ": iteration " << state.iteration
                        << ": evaluating condition";
        controller->mNextStepIndex = step->condStepIndex;

        if (state.iteration == 0) {
            state.startTime = std::chrono::steady_clock::now();
        }

        // iteration = 0   cond inputs = outer inputs
        // iteration = 1   cond inputs = body outputs
        // iteration = 2   cond inputs = body outputs
        // iteration = 3   cond inputs = ...
        uint32_t loopBodyOutputCount = step->bodyOutputOperands.size();
        CHECK_EQ(step->condInputOperands.size(), step->outerInputOperands.size());
        CHECK_GE(step->condInputOperands.size(), loopBodyOutputCount);
        for (uint32_t i = 0, n = step->condInputOperands.size(); i < n; ++i) {
            bool operandIsInputOnly = i >= loopBodyOutputCount;
            controller->setInput((state.iteration == 0 || operandIsInputOnly)
                                         ? step->outerInputOperands[i]
                                         : step->bodyOutputOperands[i],
                                 step->condInputOperands[i]);
        }

        state.stage = WhileState::EVALUATE_BODY;
        return nextCompound(controller, executor, burstController);
    }

    CHECK(state.stage == WhileState::EVALUATE_BODY);
    std::chrono::nanoseconds timeoutDuration(
            controller->mExecutionBuilder->getLoopTimeoutDuration());
    auto duration = std::chrono::steady_clock::now() - state.startTime;
    if (duration > timeoutDuration) {
        LOG(ERROR) << "WHILE loop timed out after "
                   << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
                   << " ms";
        return ANEURALNETWORKS_MISSED_DEADLINE_TRANSIENT;
    }

    // If the last step has a sync fence, wait for it to signal before reading the condition value.
    // This is safe because the steps are serialized when doing fenced compute.
    NN_RETURN_IF_ERROR(controller->waitForLastStepSyncFence());
    bool condValue;
    NN_RETURN_IF_ERROR(readConditionValue(controller, step->condOutputOperand, &condValue));
    if (condValue) {
        VLOG(EXECUTION) << "next: " << toString(*step) << ": iteration " << state.iteration
                        << ": evaluating body";
        controller->mNextStepIndex = step->bodyStepIndex;

        // iteration = 0   body inputs = cond inputs = outer inputs   body outputs = tmp1
        // iteration = 1   body inputs = cond inputs = tmp1           body outputs = tmp2
        // iteration = 2   body inputs = cond inputs = tmp2           body outputs = tmp1
        // iteration = 3   body inputs = cond inputs = ...            body outputs = ...
#ifdef NN_DEBUGGABLE
        CHECK_GE(step->bodyInputOperands.size(), step->bodyOutputOperands.size());
        CHECK_EQ(step->bodyInputOperands.size(), step->outerInputOperands.size());
        CHECK_EQ(step->bodyInputOperands.size(), step->condInputOperands.size());
        CHECK_GE(step->bodyOutputOperands.size(), step->outerOutputOperands.size());
#endif
        for (uint32_t i = 0, n = step->bodyInputOperands.size(); i < n; ++i) {
            controller->setInput(step->condInputOperands[i], step->bodyInputOperands[i]);
        }
        if (state.iteration != 0) {
            for (const SourceOperandIndex& outputOperand : step->bodyOutputOperands) {
#ifdef NN_DEBUGGABLE
                CHECK_EQ(controller->mSourceOperandToInputIndex.count(outputOperand), 0u);
                CHECK_EQ(controller->mSourceOperandToOutputIndex.count(outputOperand), 0u);
                CHECK_EQ(controller->mSourceOperandToOffsetOfTemporary.count(outputOperand), 1u);
                CHECK_EQ(controller->mSourceOperandToOffsetOfTemporary2.count(outputOperand), 1u);
#endif
                std::swap(controller->mSourceOperandToOffsetOfTemporary[outputOperand],
                          controller->mSourceOperandToOffsetOfTemporary2[outputOperand]);
            }
        }
    } else {
        VLOG(EXECUTION) << "next: " << toString(*step) << ": iteration " << state.iteration
                        << ": exiting loop";
        controller->mNextStepIndex = step->exitStepIndex;

        // Copy body outputs to outer outputs.
        // TODO: Use outer outputs instead of tmp2 to avoid copying?
        CHECK_LE(step->outerOutputOperands.size(), step->bodyOutputOperands.size());
        for (uint32_t i = 0, n = step->outerOutputOperands.size(); i < n; ++i) {
            // condInputOperands[i] points to a body output operand from the
            // last iteration if we've executed at least one iteration and to a
            // WHILE operation input operand otherwise.
            const SourceOperandIndex& innerOperand = step->condInputOperands[i];
            const SourceOperandIndex& outerOperand = step->outerOutputOperands[i];
            std::optional<Buffer> outerBuffer = getBuffer(controller, outerOperand);
            if (outerBuffer == std::nullopt) {
                // This should never happen.
                LOG(ERROR) << "Unable to get outerBuffer for operand " << toString(outerOperand);
                return ANEURALNETWORKS_OP_FAILED;
            }
            const Operand& sourceOperand =
                    controller->mExecutionBuilder->getSourceOperand(outerOperand);
            const uint32_t size = TypeManager::get()->getSizeOfData(sourceOperand);
            CHECK_NE(size, 0u);
            std::optional<Buffer> innerBuffer = getBuffer(controller, innerOperand);
            if (innerBuffer == std::nullopt) {
                // This should never happen.
                LOG(ERROR) << "Unable to get innerBuffer for operand " << toString(innerOperand);
                return ANEURALNETWORKS_OP_FAILED;
            }
            CHECK_LE(size, innerBuffer->getSize());
            CHECK_LE(size, outerBuffer->getSize());
            memcpy(outerBuffer->getPointer(), innerBuffer->getPointer(), size);
            outerBuffer->flush();
        }
        state.iteration = WhileState::kOutsideLoop;
    }

    state.stage = WhileState::EVALUATE_CONDITION;
    return nextCompound(controller, executor, burstController);
}

int ExecutionPlan::nextCompound(const GotoStep* step, std::shared_ptr<Controller> controller,
                                std::shared_ptr<StepExecutor>* executor,
                                std::shared_ptr<ExecutionBurstController>* burstController) const {
    VLOG(EXECUTION) << "next: " << toString(*step);
    controller->mNextStepIndex = step->gotoStepIndex;
    return nextCompound(controller, executor, burstController);
}

void ExecutionPlan::becomeCompoundIfEmpty() {
    CHECK(mState != SIMPLE);
    if (mState == EMPTY) {
        mBody = new CompoundBody();
        mState = COMPOUND;
    }
}

ExecutionStep* ExecutionPlan::createNewExecutionStep(uint32_t sourceModelIndex,
                                                     const std::shared_ptr<Device> device) {
    becomeCompoundIfEmpty();
    auto step = std::make_shared<LogicalStep>(std::in_place_type<ExecutionStep>, this,
                                              compound()->mSteps.size(), sourceModelIndex, device);
    compound()->mSteps.push_back(step);
    return step->executionStep();
}

IfStep* ExecutionPlan::createNewIfStep() {
    becomeCompoundIfEmpty();
    auto step = std::make_shared<LogicalStep>(std::in_place_type<IfStep>);
    step->ifStep()->index = compound()->mSteps.size();
    compound()->mSteps.push_back(step);
    return step->ifStep();
}

WhileStep* ExecutionPlan::createNewWhileStep() {
    becomeCompoundIfEmpty();
    auto step = std::make_shared<LogicalStep>(std::in_place_type<WhileStep>);
    step->whileStep()->index = compound()->mSteps.size();
    compound()->mSteps.push_back(step);
    return step->whileStep();
}

GotoStep* ExecutionPlan::createNewGotoStep() {
    becomeCompoundIfEmpty();
    auto step = std::make_shared<LogicalStep>(std::in_place_type<GotoStep>);
    step->gotoStep()->index = compound()->mSteps.size();
    compound()->mSteps.push_back(step);
    return step->gotoStep();
}

void ExecutionPlan::becomeSingleStep(const std::shared_ptr<Device> device,
                                     const ModelBuilder* model) {
    CHECK(mState == EMPTY);
    mBody = new SimpleBody(device, model, mCacheDir, mToken);
    mState = SIMPLE;
}

void ExecutionPlan::recordTemporaryDef(SourceOperandIndex sourceOperandIndex, uint32_t stepIndex) {
    auto [it, isNew] =
            compound()->mTemporaryToDefiningExecutionStep.emplace(sourceOperandIndex, stepIndex);
    CHECK(isNew) << "Step " << stepIndex << " redefines temporary operand "
                 << toString(sourceOperandIndex) << " already defined by step " << it->second;
}

void ExecutionPlan::dump() const {
    if (mBody) {
        mBody->dump();
    } else {
        VLOG(COMPILATION) << "EMPTY";
    }
}

void ExecutionPlan::reset() {
    if (mBody) {
        delete mBody;
        mBody = nullptr;
    }
    mState = EMPTY;
}

bool ExecutionPlan::isSimpleCpu() const {
    return isSimple() && simple()->mDevice == DeviceManager::getCpuDevice();
}

ExecutionPlan::Kind ExecutionPlan::forTest_getKind() const {
    switch (mState) {
        case EMPTY:
            return Kind::EMPTY;
        case SIMPLE:
            nnAssert(mBody);
            return mBody->mSuccessfulFinish ? Kind::SIMPLE : Kind::ERROR;
        case COMPOUND:
            nnAssert(mBody);
            return mBody->mSuccessfulFinish ? Kind::COMPOUND : Kind::ERROR;
        default:
            nnAssert(!"unexpected state");
            return Kind::ERROR;
    }
}

std::shared_ptr<const Device> ExecutionPlan::forTest_simpleGetDevice() const {
    return simple()->mDevice;
}

const std::vector<std::shared_ptr<LogicalStep>>& ExecutionPlan::forTest_compoundGetSteps() const {
    return compound()->mSteps;
}

bool ExecutionPlan::forTest_hasStepModelOutputsOfUnknownSize() const {
    return mBody->hasStepModelOutputsOfUnknownSize();
}

const uint8_t* ExecutionPlan::forTest_simpleGetCacheToken() const {
    return simple()->mToken.getCacheToken();
}

void ExecutionPlan::SimpleBody::dump() const {
    VLOG(COMPILATION) << "SIMPLE for " << mDevice->getName();
}

void ExecutionPlan::CompoundBody::dump() const {
    for (const auto& step : mSteps) {
        step->dump();
    }
}

void ExecutionPlan::SimpleBody::forEachStepRoleOfInput(uint32_t index,
                                                       const StepRoleCallback& callback) const {
    callback(mPreparedModel.get(), IOType::INPUT, index);
}

void ExecutionPlan::SimpleBody::forEachStepRoleOfOutput(uint32_t index,
                                                        const StepRoleCallback& callback) const {
    callback(mPreparedModel.get(), IOType::OUTPUT, index);
}

// Map an input role of the main model to the input/output roles in the step models:
// - An input role of the main model may be used as an input of multiple step models.
// - An input role of the main model should not be used as an output of any step model.
void ExecutionPlan::CompoundBody::forEachStepRoleOfInput(uint32_t index,
                                                         const StepRoleCallback& callback) const {
    for (const auto& logicalStep : mSteps) {
        if (const ExecutionStep* step = logicalStep->tryExecutionStep()) {
            // Model input as step model input.
            const auto& inputMapping = step->getInputIndexStepModelToMainModel();
            for (uint32_t i = 0; i < inputMapping.size(); i++) {
                if (inputMapping[i] == index) {
                    callback(step->getPreparedStepModel().get(), IOType::INPUT, i);
                }
            }
        }
    }
}

// Map an output role of the main model to the input/output roles in the step models:
// - An output role of the main model may only be used as one output of one single step model.
// - An output role of the main model may be used as an input of multiple step models.
void ExecutionPlan::CompoundBody::forEachStepRoleOfOutput(uint32_t index,
                                                          const StepRoleCallback& callback) const {
    bool found = false;
    for (const auto& logicalStep : mSteps) {
        if (const ExecutionStep* step = logicalStep->tryExecutionStep()) {
            // Model output as step model output.
            if (!found) {
                const auto& outputMapping = step->getOutputIndexStepModelToMainModel();
                for (uint32_t i = 0; i < outputMapping.size(); i++) {
                    if (outputMapping[i] == index) {
                        callback(step->getPreparedStepModel().get(), IOType::OUTPUT, i);
                        found = true;
                        break;
                    }
                }
            }
            // Model output as step model input.
            const auto& inputToOutputMapping = step->getOutputsAsStepModelInputsIndexToMainModel();
            for (uint32_t i = 0; i < inputToOutputMapping.size(); i++) {
                if (inputToOutputMapping[i] == index) {
                    callback(step->getPreparedStepModel().get(), IOType::INPUT, i);
                }
            }
        }
    }
}

int ModelBuilder::partitionTheWork(const std::vector<std::shared_ptr<Device>>& devices,
                                   uint32_t preference, uint32_t priority,
                                   const std::optional<Deadline>& deadline,
                                   ExecutionPlan* plan) const {
    uint32_t sourceModelIndex = plan->getSourceModels().addModel(this);
    NN_RETURN_IF_ERROR(partitionTheWorkInternal(sourceModelIndex, devices, preference, priority,
                                                deadline, plan));
    int n = plan->finish(preference, priority, deadline);
    if (VLOG_IS_ON(COMPILATION)) {
        VLOG(COMPILATION) << "ModelBuilder::partitionTheWork: source model: ";
        logModelToInfo(makeHidlModel());
        plan->dump();
    }
    return n;
}

int ModelBuilder::partitionTheWorkInternal(uint32_t sourceModelIndex,
                                           const std::vector<std::shared_ptr<Device>>& devices,
                                           uint32_t preference, uint32_t priority,
                                           const std::optional<Deadline>& deadline,
                                           ExecutionPlan* plan) const {
    // This function uses a heuristic approach to partitioning the graph.
    // It should be good enough for the first release.

    SourceModels* sourceModels = &plan->getSourceModels();
    const size_t deviceCount = devices.size();
    const size_t operationCount = mOperations.size();

    VLOG(COMPILATION) << "ModelBuilder::partitionTheWork: "
                      << "sourceModelIndex = " << sourceModelIndex << ", "
                      << "deviceCount = " << deviceCount << ", "
                      << "operationCount = " << operationCount;

    // Figure out where each operation will best execute.
    // The value of the vector is the index in the devices vector.
    std::vector<int> bestDeviceForOperation(operationCount);
    NN_RETURN_IF_ERROR(
            findBestDeviceForEachOperation(preference, devices, &bestDeviceForOperation));

    // A special value produced by findBestDeviceForEachOperation meaning that
    // this is a control flow operation scheduled for interpreted execution
    // (see LogicalStep).
    const int kControlFlowInterpreter = deviceCount;

    // If one device will run all the operations, we don't need to split the
    // work. This shortcut does not apply when recursively partitioning
    // referenced models because our plan representation is flat.
    if (sourceModelIndex == kMainModelInSourceModels &&
        std::adjacent_find(bestDeviceForOperation.begin(), bestDeviceForOperation.end(),
                           std::not_equal_to<int>()) == bestDeviceForOperation.end()) {
        const int bestDeviceIndex = bestDeviceForOperation[0];
        // Bypass the partitioning process unless the only operation is a
        // control flow operation scheduled for interpreted execution.
        if (bestDeviceIndex != kControlFlowInterpreter) {
            VLOG(COMPILATION) << "ModelBuilder::partitionTheWork: only one best device: "
                              << bestDeviceIndex << " = " << devices[bestDeviceIndex]->getName();
            plan->becomeSingleStep(devices[bestDeviceIndex], this);
            return ANEURALNETWORKS_NO_ERROR;
        }
    }

    // No easy solution, we need to split the work.

    // We keep track of the operations that are ready to run for each device.
    // perDeviceQueue[deviceCount] is for interpreted execution of control flow
    // (see LogicalStep).
    std::vector<std::queue<uint32_t>> perDeviceQueue(deviceCount + 1);

    // This helper function enqueues the operation on the appropriate queue.
    auto enqueueOnAppropriateDevice = [&](uint32_t operationIndex) {
        int deviceIndex = bestDeviceForOperation[operationIndex];
        perDeviceQueue[deviceIndex].push(operationIndex);
        VLOG(COMPILATION) << "enqueueOnAppropriateDevice " << operationIndex << " onto "
                          << deviceIndex;
    };

    // This helper function finds a device that has operations ready to process.
    // We start by looking at the control flow queue, and then look at the
    // devices in reverse order (i.e., starting at the end of the devices
    // vector). Earlier devices have a chance to prepare more of the inputs
    // required by other devices. This function returns -1 if all queues are
    // empty.
    auto findNextDeviceToProcess = [&]() -> int {
        for (int i = perDeviceQueue.size() - 1; i >= 0; i--) {
            if (!perDeviceQueue[i].empty()) {
                return i;
            }
        }
        return -1;
    };

    OperandTracker tracker(this, enqueueOnAppropriateDevice);
    // For each iteration of this loop, we'll create an execution step.
    while (true) {
        // Find the device we'll do this step for.
        int deviceIndex = findNextDeviceToProcess();
        VLOG(COMPILATION) << "findNextDeviceToProcess: " << deviceIndex;
        if (deviceIndex < 0) {
            break;
        }

        // Assign as much as possible to this device.
        auto& queue = perDeviceQueue[deviceIndex];
        if (deviceIndex != kControlFlowInterpreter) {
            ExecutionStep* step =
                    plan->createNewExecutionStep(sourceModelIndex, devices[deviceIndex]);
            while (!queue.empty()) {
                uint32_t operationIndex = queue.front();
                queue.pop();
                int n = step->addOperation(operationIndex);
                if (n != ANEURALNETWORKS_NO_ERROR) {
                    LOG(ERROR) << "failed to add operation " << operationIndex << " to step";
                    return n;
                }
                tracker.markProcessed(operationIndex, enqueueOnAppropriateDevice);
            }
        } else {
            while (!queue.empty()) {
                uint32_t operationIndex = queue.front();
                queue.pop();
                const Operation& operation = getOperation(operationIndex);
                if (operation.type == OperationType::IF) {
                    namespace op = operation_if;
                    const Operand& thenOperand =
                            getOperand(operation.inputs[op::kThenModelOperand]);
                    const Operand& elseOperand =
                            getOperand(operation.inputs[op::kElseModelOperand]);
                    const ModelBuilder* thenModel = getReferencedModel(thenOperand);
                    const ModelBuilder* elseModel = getReferencedModel(elseOperand);
                    uint32_t thenModelIndex = sourceModels->addModel(thenModel);
                    uint32_t elseModelIndex = sourceModels->addModel(elseModel);

                    // Emits the following:
                    // Index  Step
                    //   i    if then=(i + 1) else=(j + 1)
                    //  ...   (then model steps)
                    //   j    goto k
                    //  ...   (else model steps)
                    //   k    (steps after the IF)
                    IfStep* ifStep = plan->createNewIfStep();
                    ifStep->conditionOperandIndex = SourceOperandIndex(
                            sourceModelIndex, operation.inputs[op::kCondBoolOperand]);
                    ifStep->thenStepIndex = plan->getNextStepIndex();
                    NN_RETURN_IF_ERROR(thenModel->partitionTheWorkInternal(
                            thenModelIndex, devices, preference, priority, deadline, plan));
                    GotoStep* afterThenBranch = plan->createNewGotoStep();
                    ifStep->elseStepIndex = plan->getNextStepIndex();
                    NN_RETURN_IF_ERROR(elseModel->partitionTheWorkInternal(
                            elseModelIndex, devices, preference, priority, deadline, plan));
                    afterThenBranch->gotoStepIndex = plan->getNextStepIndex();

                    // Outer model operands.
                    for (uint32_t i = op::kFirstInput, n = operation.inputs.size(); i < n; ++i) {
                        ifStep->outerInputOperands.emplace_back(sourceModelIndex,
                                                                operation.inputs[i]);
                    }
                    for (uint32_t i = 0, n = operation.outputs.size(); i < n; ++i) {
                        ifStep->outerOutputOperands.emplace_back(sourceModelIndex,
                                                                 operation.outputs[i]);
                    }
                    // Then model operands.
                    for (uint32_t i = 0, n = thenModel->inputCount(); i < n; ++i) {
                        ifStep->thenBranchInputOperands.emplace_back(
                                thenModelIndex, thenModel->getInputOperandIndex(i));
                    }
                    for (uint32_t i = 0, n = thenModel->outputCount(); i < n; ++i) {
                        ifStep->thenBranchOutputOperands.emplace_back(
                                thenModelIndex, thenModel->getOutputOperandIndex(i));
                    }
                    // Else model operands.
                    for (uint32_t i = 0, n = elseModel->inputCount(); i < n; ++i) {
                        ifStep->elseBranchInputOperands.emplace_back(
                                elseModelIndex, elseModel->getInputOperandIndex(i));
                    }
                    for (uint32_t i = 0, n = elseModel->outputCount(); i < n; ++i) {
                        ifStep->elseBranchOutputOperands.emplace_back(
                                elseModelIndex, elseModel->getOutputOperandIndex(i));
                    }
                } else if (operation.type == OperationType::WHILE) {
                    namespace op = operation_while;
                    const Operand& condOperand =
                            getOperand(operation.inputs[op::kCondModelOperand]);
                    const Operand& bodyOperand =
                            getOperand(operation.inputs[op::kBodyModelOperand]);
                    const ModelBuilder* condModel = getReferencedModel(condOperand);
                    const ModelBuilder* bodyModel = getReferencedModel(bodyOperand);
                    uint32_t condModelIndex = sourceModels->addModel(condModel);
                    uint32_t bodyModelIndex = sourceModels->addModel(bodyModel);

                    // Emits the following:
                    // Index  Step
                    //   i    while cond=(i + 1) body=(j + 1) exit=(k + 1)
                    //  ...   (cond model steps)
                    //   j    goto i
                    //  ...   (body model steps)
                    //   k    goto i
                    //  ...   (steps after the WHILE)
                    //
                    //  Note that WhileStep has WhileState associated with it.
                    WhileStep* whileStep = plan->createNewWhileStep();
                    whileStep->condStepIndex = plan->getNextStepIndex();
                    NN_RETURN_IF_ERROR(condModel->partitionTheWorkInternal(
                            condModelIndex, devices, preference, priority, deadline, plan));
                    GotoStep* afterCond = plan->createNewGotoStep();
                    afterCond->gotoStepIndex = whileStep->index;
                    whileStep->bodyStepIndex = plan->getNextStepIndex();
                    NN_RETURN_IF_ERROR(bodyModel->partitionTheWorkInternal(
                            bodyModelIndex, devices, preference, priority, deadline, plan));
                    GotoStep* afterBody = plan->createNewGotoStep();
                    afterBody->gotoStepIndex = whileStep->index;
                    whileStep->exitStepIndex = plan->getNextStepIndex();

                    // Outer model operands.
                    for (uint32_t i = op::kFirstInput, n = operation.inputs.size(); i < n; ++i) {
                        whileStep->outerInputOperands.emplace_back(sourceModelIndex,
                                                                   operation.inputs[i]);
                    }
                    for (uint32_t i = 0, n = operation.outputs.size(); i < n; ++i) {
                        whileStep->outerOutputOperands.emplace_back(sourceModelIndex,
                                                                    operation.outputs[i]);
                    }
                    // Cond model operands.
                    for (uint32_t i = 0, n = condModel->inputCount(); i < n; ++i) {
                        whileStep->condInputOperands.emplace_back(
                                condModelIndex, condModel->getInputOperandIndex(i));
                    }
                    whileStep->condOutputOperand =
                            SourceOperandIndex(condModelIndex, condModel->getOutputOperandIndex(0));
                    // Body model operands.
                    for (uint32_t i = 0, n = bodyModel->inputCount(); i < n; ++i) {
                        whileStep->bodyInputOperands.emplace_back(
                                bodyModelIndex, bodyModel->getInputOperandIndex(i));
                    }
                    for (uint32_t i = 0, n = bodyModel->outputCount(); i < n; ++i) {
                        whileStep->bodyOutputOperands.emplace_back(
                                bodyModelIndex, bodyModel->getOutputOperandIndex(i));
                    }
                } else {
                    CHECK(false) << toString(operation.type) << " is not a control flow operation";
                }
                tracker.markProcessed(operationIndex, enqueueOnAppropriateDevice);
            }
        }
    }
    return ANEURALNETWORKS_NO_ERROR;
}

float ModelBuilder::getPerformance(uint32_t preference,
                                   const std::shared_ptr<Device> device) const {
    // Note that we will call this method multiple times per compilation with
    // the same arguments if there are nested control flow operations and we
    // decide to execute the outer operation on the ExecutionPlan::next()
    // interpreter.
    //
    // This is a potential compilation performance problem. To work around it,
    // the performance value could be cached for the duration of a compilation.
    float perf = 0;
    const size_t operationCount = mOperations.size();
    for (size_t operationIndex = 0; operationIndex < operationCount; operationIndex++) {
        perf += getPerformance(preference, device, operationIndex);
    }
    return perf;
}

float ModelBuilder::getPerformance(uint32_t preference, const std::shared_ptr<Device> device,
                                   uint32_t operationIndex) const {
    auto applyPreference = [preference](const PerformanceInfo& perf) {
        return preference == ANEURALNETWORKS_PREFER_LOW_POWER ? perf.powerUsage : perf.execTime;
    };

    const Operation& operation = getOperation(operationIndex);

    if (operation.type == OperationType::IF) {
        namespace op = operation_if;
        const Operand& thenOperand = getOperand(operation.inputs[op::kThenModelOperand]);
        const Operand& elseOperand = getOperand(operation.inputs[op::kElseModelOperand]);
        const ModelBuilder* thenModel = getReferencedModel(thenOperand);
        const ModelBuilder* elseModel = getReferencedModel(elseOperand);
        return applyPreference(device->getIfPerformance()) +
               0.5 * (thenModel->getPerformance(preference, device) +
                      elseModel->getPerformance(preference, device));
    }

    if (operation.type == OperationType::WHILE) {
        namespace op = operation_while;
        const Operand& condOperand = getOperand(operation.inputs[op::kCondModelOperand]);
        const Operand& bodyOperand = getOperand(operation.inputs[op::kBodyModelOperand]);
        const ModelBuilder* condModel = getReferencedModel(condOperand);
        const ModelBuilder* bodyModel = getReferencedModel(bodyOperand);
        return applyPreference(device->getWhilePerformance()) +
               condModel->getPerformance(preference, device) +
               bodyModel->getPerformance(preference, device);
    }

    // TODO This assumes that the type is dictated by the first operand. This is
    // currently the case but is not a safe assumption to make in the long term.
    const uint32_t operandIndex = operation.inputs[0];
    const OperandType operandType = mOperands[operandIndex].type;
    switch (operandType) {
        case OperandType::FLOAT32:
            if (mRelaxComputationFloat32toFloat16) {
                return applyPreference(device->getRelaxedFloat32toFloat16PerformanceScalar());
            }
            break;
        case OperandType::TENSOR_FLOAT32:
            if (mRelaxComputationFloat32toFloat16) {
                return applyPreference(device->getRelaxedFloat32toFloat16PerformanceTensor());
            }
            break;
        default:
            break;
    }

    return applyPreference(device->getPerformance(operandType));
}

bool ModelBuilder::isControlFlowOperationWithOperandOfUnknownSize(uint32_t operationIndex) const {
    auto containsUnknownSize = [](const ModelBuilder* model,
                                  const std::vector<uint32_t>& operandIndexes) {
        for (uint32_t operandIndex : operandIndexes) {
            if (hasUnknownSize(model->getOperand(operandIndex))) {
                return true;
            }
        }
        return false;
    };

    const Operation& operation = getOperation(operationIndex);

    if (operation.type == OperationType::IF) {
        namespace op = operation_if;
        const Operand& thenOperand = getOperand(operation.inputs[op::kThenModelOperand]);
        const Operand& elseOperand = getOperand(operation.inputs[op::kElseModelOperand]);
        const ModelBuilder* thenModel = getReferencedModel(thenOperand);
        const ModelBuilder* elseModel = getReferencedModel(elseOperand);
        return containsUnknownSize(this, operation.inputs) ||
               containsUnknownSize(this, operation.outputs) ||
               containsUnknownSize(thenModel, thenModel->getInputOperandIndexes()) ||
               containsUnknownSize(thenModel, thenModel->getOutputOperandIndexes()) ||
               containsUnknownSize(elseModel, elseModel->getInputOperandIndexes()) ||
               containsUnknownSize(elseModel, elseModel->getOutputOperandIndexes());
    }

    if (operation.type == OperationType::WHILE) {
        namespace op = operation_while;
        const Operand& condOperand = getOperand(operation.inputs[op::kCondModelOperand]);
        const Operand& bodyOperand = getOperand(operation.inputs[op::kBodyModelOperand]);
        const ModelBuilder* condModel = getReferencedModel(condOperand);
        const ModelBuilder* bodyModel = getReferencedModel(bodyOperand);
        return containsUnknownSize(this, operation.inputs) ||
               containsUnknownSize(this, operation.outputs) ||
               containsUnknownSize(condModel, condModel->getInputOperandIndexes()) ||
               containsUnknownSize(condModel, condModel->getOutputOperandIndexes()) ||
               containsUnknownSize(bodyModel, bodyModel->getInputOperandIndexes()) ||
               containsUnknownSize(bodyModel, bodyModel->getOutputOperandIndexes());
    }

    // Not a control flow operation.
    return false;
}

bool ModelBuilder::supportedByControlFlowInterpreter(uint32_t operationIndex) const {
    const Operation& operation = getOperation(operationIndex);
    return (operation.type == OperationType::IF || operation.type == OperationType::WHILE) &&
           // The partitioner does not support dynamic temporaries (b/132458982).
           !isControlFlowOperationWithOperandOfUnknownSize(operationIndex);
}

namespace {

// This class determines whether a given device can execute a given operation
class CanDo {
   public:
    CanDo() {}

    void initialize(const MetaModel& metaModel, std::shared_ptr<Device> device) {
        mSupportsOperationByIndex = device->getSupportedOperations(metaModel);
    }

    bool check(size_t operationIndex) const { return mSupportsOperationByIndex[operationIndex]; }

   private:
    std::vector<bool> mSupportsOperationByIndex;
};

}  // anonymous namespace

int ModelBuilder::findBestDeviceForEachOperation(
        uint32_t preference, const std::vector<std::shared_ptr<Device>>& devices,
        std::vector<int>* bestDeviceForOperation) const {
    const MetaModel metaModel(makeHidlModel(), DeviceManager::get()->strictSlicing());

    const size_t deviceCount = devices.size();
    std::vector<CanDo> canDo(deviceCount);
    for (size_t deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++) {
        canDo[deviceIndex].initialize(metaModel, devices[deviceIndex]);
    }

    // Figure out the best driver for each operation.
    const size_t operationCount = mOperations.size();
    for (size_t operationIndex = 0; operationIndex < operationCount; operationIndex++) {
        const Operation& operation = getOperation(operationIndex);
        // Find which device, including CPU fallback, gives the best performance for this operation.
        int bestChoice = -1;

        if (isControlFlowOperationWithOperandOfUnknownSize(operationIndex)) {
            // Do not schedule control flow operations with unknown size to
            // non-CPU devices because this is not supported by the 1.3 HAL.
            // See http://b/159076604#comment5.
            auto cpuDeviceIterator =
                    std::find(devices.begin(), devices.end(), DeviceManager::getCpuDevice());
            if (cpuDeviceIterator != devices.end()) {
                int cpuDeviceIndex = cpuDeviceIterator - devices.begin();
                if (canDo[cpuDeviceIndex].check(operationIndex)) {
                    bestChoice = cpuDeviceIndex;
                }
            }
        } else {
            float bestPerfVal = 0.0;  // Do not check bestPerfVal if bestChoice < 0.
            for (size_t deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++) {
                const auto& device = devices[deviceIndex];
                if (canDo[deviceIndex].check(operationIndex)) {
                    const float perfVal = getPerformance(preference, device, operationIndex);
                    if (bestChoice < 0 || perfVal < bestPerfVal ||
                        (perfVal == bestPerfVal && device == DeviceManager::getCpuDevice())) {
                        bestChoice = deviceIndex;
                        bestPerfVal = perfVal;
                    }
                } else {
                    // Somewhat noisy logging, but only place where the user of NNAPI can get
                    // feedback on why an operation was not run on a specific device.
                    //
                    // Logs O(operationCount * deviceCount) times, but typically deviceCount is
                    // very small.
                    VLOG(COMPILATION) << "Device " << device->getName() << " can't do operation "
                                      << toString(operation.type);
                }
            }
        }

        if (bestChoice < 0) {
            LOG(ERROR) << "No driver can do operation " << toString(operation.type);
            return ANEURALNETWORKS_BAD_DATA;
        } else if (devices[bestChoice] == DeviceManager::getCpuDevice() &&
                   supportedByControlFlowInterpreter(operationIndex)) {
            // Run control flow on the ExecutionPlan::next() interpreter and try
            // to delegate referenced models.
            const int kControlFlowInterpreter = deviceCount;
            (*bestDeviceForOperation)[operationIndex] = kControlFlowInterpreter;
            VLOG(COMPILATION) << "ModelBuilder::findBestDeviceForEachOperation("
                              << toString(operation.type) << ") = -1"
                              << " (NNAPI)";
        } else {
            (*bestDeviceForOperation)[operationIndex] = bestChoice;
            VLOG(COMPILATION) << "ModelBuilder::findBestDeviceForEachOperation("
                              << toString(operation.type) << ") = " << bestChoice << " ("
                              << devices[bestChoice]->getName() << ")";
        }
    }
    return ANEURALNETWORKS_NO_ERROR;
}

}  // namespace nn
}  // namespace android
