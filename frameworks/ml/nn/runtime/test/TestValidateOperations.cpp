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

#include <gmock/gmock.h>
#include <gtest/gtest-death-test.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "NeuralNetworks.h"
#include "NeuralNetworksOEM.h"
#include "NeuralNetworksWrapper.h"

using namespace android::nn::wrapper;

namespace {

static const int32_t kAvailableOperandCodes[] = {ANEURALNETWORKS_FLOAT32,
                                                 ANEURALNETWORKS_INT32,
                                                 ANEURALNETWORKS_UINT32,
                                                 ANEURALNETWORKS_TENSOR_FLOAT32,
                                                 ANEURALNETWORKS_TENSOR_INT32,
                                                 ANEURALNETWORKS_TENSOR_QUANT8_ASYMM,
                                                 ANEURALNETWORKS_BOOL,
                                                 ANEURALNETWORKS_TENSOR_QUANT16_SYMM,
                                                 ANEURALNETWORKS_TENSOR_FLOAT16,
                                                 ANEURALNETWORKS_TENSOR_BOOL8,
                                                 ANEURALNETWORKS_FLOAT16,
                                                 ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL,
                                                 ANEURALNETWORKS_TENSOR_QUANT16_ASYMM,
                                                 ANEURALNETWORKS_TENSOR_OEM_BYTE};

ANeuralNetworksOperandType getOpType(int32_t opcode, uint32_t dimCount = 0,
                                     const uint32_t* dim = nullptr) {
    ANeuralNetworksOperandType opType = {.type = opcode,
                                         .dimensionCount = dimCount,
                                         .dimensions = dim,
                                         .scale = 0.0,
                                         .zeroPoint = 0};
    if (opcode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM ||
        opcode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED ||
        opcode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM ||
        opcode == ANEURALNETWORKS_TENSOR_QUANT16_ASYMM ||
        opcode == ANEURALNETWORKS_TENSOR_QUANT16_SYMM) {
        opType.scale = 1. / 256.;
    }
    return opType;
}

struct OperandTypeWithExtraParams {
    OperandTypeWithExtraParams(const ANeuralNetworksOperandType& operandType)
        : operandType(operandType), channelQuant(std::nullopt), valueModel(std::nullopt) {}

    ANeuralNetworksOperandType operandType;
    std::optional<ANeuralNetworksSymmPerChannelQuantParams> channelQuant;
    std::optional<const ANeuralNetworksModel*> valueModel;

    bool operator==(const OperandTypeWithExtraParams& that) const {
        if (operandType.type != that.operandType.type ||
            operandType.scale != that.operandType.scale ||
            operandType.zeroPoint != that.operandType.zeroPoint ||
            operandType.dimensionCount != that.operandType.dimensionCount) {
            return false;
        }

        if (channelQuant.has_value() != that.channelQuant.has_value() ||
            (channelQuant.has_value() &&
             (channelQuant->channelDim != that.channelQuant->channelDim ||
              channelQuant->scaleCount != that.channelQuant->scaleCount))) {
            return false;
        }

        if (valueModel != that.valueModel) {
            return false;
        }

        if (operandType.dimensions) {
            if (!that.operandType.dimensions) {
                return false;
            }
            if (!std::equal(operandType.dimensions,
                            operandType.dimensions + operandType.dimensionCount,
                            that.operandType.dimensions)) {
                return false;
            }
        } else {
            if (that.operandType.dimensions) {
                return false;
            }
        }

        if (channelQuant.has_value()) {
            if (channelQuant->scales) {
                return that.channelQuant->scales &&
                       std::equal(channelQuant->scales,
                                  channelQuant->scales + channelQuant->scaleCount,
                                  that.channelQuant->scales);
            } else {
                return that.channelQuant->scales == nullptr;
            }
        }
        return true;
    }

    bool operator!=(const OperandTypeWithExtraParams& that) const { return !(*this == that); }

    bool operator<(const OperandTypeWithExtraParams& that) const {
        if (operandType.type < that.operandType.type) return true;
        if (operandType.dimensionCount < that.operandType.dimensionCount) return true;
        return false;
    }
};  // namespace

// Generates valid and invalid mutations of given OperandTypeWithParams
// instances.
// It is also responsible of freeing the memory allocated when creating
// mutations.
// Mutations shouldn't outlive the generating TensorRankConstraint instance.
class TensorRankConstraint {
   public:
    TensorRankConstraint(const TensorRankConstraint& copyFrom) {
        // ignoring the array of allocated dimension
        this->mRangeMax = copyFrom.mRangeMax;
        this->mRangeMin = copyFrom.mRangeMin;
    }

    TensorRankConstraint& operator=(const TensorRankConstraint& copyFrom) {
        // ignoring the array of allocated dimension
        this->mRangeMax = copyFrom.mRangeMax;
        this->mRangeMin = copyFrom.mRangeMin;
        return *this;
    }

    static TensorRankConstraint Exactly(uint32_t rank) {
        return TensorRankConstraint(std::optional(rank), std::optional(rank));
    }

    static TensorRankConstraint AtLeast(uint32_t min) {
        return TensorRankConstraint(std::optional(min), std::nullopt);
    }

    static TensorRankConstraint UpTo(uint32_t max) {
        return TensorRankConstraint(std::nullopt, std::optional(max));
    }

    static TensorRankConstraint Between(uint32_t min, uint32_t max) {
        if (min == 0) {
            return UpTo(max);
        }
        return TensorRankConstraint(std::optional(min), std::optional(max));
    }

    std::set<std::vector<OperandTypeWithExtraParams>> MutationsWithValidRank(
            const std::vector<OperandTypeWithExtraParams>& operandsTypeWithParams) {
        // can't be both nullopt
        if (!mRangeMin) {
            return {ModifyForRank(operandsTypeWithParams, 1),
                    ModifyForRank(operandsTypeWithParams, *mRangeMax)};
        } else if (!mRangeMax) {
            return {ModifyForRank(operandsTypeWithParams, *mRangeMin),
                    ModifyForRank(operandsTypeWithParams, *mRangeMin + 1)};
        } else if (mRangeMax == mRangeMin) {
            std::for_each(operandsTypeWithParams.begin(), operandsTypeWithParams.end(),
                          [this](const OperandTypeWithExtraParams& op) {
                              assert(op.operandType.dimensionCount == *mRangeMin);
                          });
            return {operandsTypeWithParams};
        } else {
            return {ModifyForRank(operandsTypeWithParams, *mRangeMin),
                    ModifyForRank(operandsTypeWithParams, *mRangeMax)};
        }
    }

    std::set<std::vector<OperandTypeWithExtraParams>> MutationsWithInvalidRank(
            const std::vector<OperandTypeWithExtraParams>& operandsTypeWithParams) {
        std::set<std::vector<OperandTypeWithExtraParams>> result;
        if (mRangeMax) {
            result.insert(ModifyForRank(operandsTypeWithParams, *mRangeMax + 1));
        }
        if (mRangeMin.value_or(0) > 1) {
            result.insert(ModifyForRank(operandsTypeWithParams, *mRangeMin - 1));
        }
        return result;
    }

   private:
    std::vector<OperandTypeWithExtraParams> ModifyForRank(
            const std::vector<OperandTypeWithExtraParams>& operandsTypeWithParams, uint_t newRank) {
        std::vector<OperandTypeWithExtraParams> result;
        std::transform(operandsTypeWithParams.cbegin(), operandsTypeWithParams.cend(),
                       std::back_inserter(result),
                       [this, newRank](const OperandTypeWithExtraParams& operandTypeWithParams) {
                           return ModifyForRank(operandTypeWithParams, newRank);
                       });
        return result;
    }

    OperandTypeWithExtraParams ModifyForRank(
            const OperandTypeWithExtraParams& operandTypeWithParams, uint_t newRank) {
        if (operandTypeWithParams.operandType.dimensionCount == newRank) {
            return operandTypeWithParams;
        }

        uint32_t* resultDimensions = nullptr;
        if (newRank != 0) {
            std::unique_ptr<uint32_t[]> dimensions = std::make_unique<uint32_t[]>(newRank);
            resultDimensions = dimensions.get();
            mAllocatedDimensions.insert(std::move(dimensions));
            std::fill(resultDimensions, resultDimensions + newRank, 1);
            const auto originDims = operandTypeWithParams.operandType.dimensions;
            if (originDims != nullptr) {
                const int dimsToCopy =
                        std::min(operandTypeWithParams.operandType.dimensionCount, newRank);
                std::copy(originDims, originDims + dimsToCopy, resultDimensions);
            }
        }

        OperandTypeWithExtraParams result = operandTypeWithParams;
        result.operandType = {
                .type = operandTypeWithParams.operandType.type,
                .dimensionCount = newRank,
                .dimensions = resultDimensions,
                .scale = operandTypeWithParams.operandType.scale,
                .zeroPoint = operandTypeWithParams.operandType.zeroPoint,
        };

        return result;
    }

    TensorRankConstraint(const std::optional<uint32_t>& min, const std::optional<uint32_t>& max)
        : mRangeMin(min), mRangeMax(max) {
        if (mRangeMax.has_value()) {
            assert(*mRangeMax >= mRangeMin.value_or(0));
        }

        assert(mRangeMax.has_value() || mRangeMin.has_value());
    }

    std::optional<uint32_t> mRangeMin;
    std::optional<uint32_t> mRangeMax;
    std::set<std::unique_ptr<uint32_t[]>> mAllocatedDimensions;
};

// Mutates a set of inputs applying the same rank constraint.
class TensorRankMutator {
   public:
    TensorRankMutator(const TensorRankConstraint& constraint,
                      const std::set<uint32_t>& applyToIndexes = {0})
        : mApplyToIndexes(applyToIndexes.begin(), applyToIndexes.end()), mConstraint(constraint) {}

    std::set<std::vector<OperandTypeWithExtraParams>> ValidInputsMutations(
            const std::vector<OperandTypeWithExtraParams>& validInputs) {
        return InputsMutations(
                validInputs, [this](const std::vector<OperandTypeWithExtraParams>& inputsToMutate) {
                    return mConstraint.MutationsWithValidRank(inputsToMutate);
                });
    }

    std::set<std::vector<OperandTypeWithExtraParams>> InvalidInputsMutations(
            const std::vector<OperandTypeWithExtraParams>& validInputs) {
        return InputsMutations(
                validInputs, [this](const std::vector<OperandTypeWithExtraParams>& inputsToMutate) {
                    return mConstraint.MutationsWithInvalidRank(inputsToMutate);
                });
    }

   private:
    std::set<std::vector<OperandTypeWithExtraParams>> InputsMutations(
            const std::vector<OperandTypeWithExtraParams>& validInputs,
            std::function<std::set<std::vector<OperandTypeWithExtraParams>>(
                    const std::vector<OperandTypeWithExtraParams>&)>
                    operandMutator) {
        std::for_each(mApplyToIndexes.begin(), mApplyToIndexes.end(),
                      [&validInputs](uint32_t index) { assert(index < validInputs.size()); });

        std::vector<OperandTypeWithExtraParams> toMutate;
        std::transform(mApplyToIndexes.begin(), mApplyToIndexes.end(), std::back_inserter(toMutate),
                       [&validInputs](int input_index) { return validInputs[input_index]; });

        // Get a series of mutation for the operands in toMutate
        std::set<std::vector<OperandTypeWithExtraParams>> mutatedOps = operandMutator(toMutate);

        // Generate a set of mutation by replacing the mutated ops in validInputs
        // with all the mutations in mutatedOps
        std::set<std::vector<OperandTypeWithExtraParams>> mutatedValidInputs;
        std::transform(
                mutatedOps.cbegin(), mutatedOps.cend(),
                std::inserter(mutatedValidInputs, mutatedValidInputs.begin()),
                [this, &validInputs](const std::vector<OperandTypeWithExtraParams>& opsMutation) {
                    std::vector<OperandTypeWithExtraParams> currInputMutation(validInputs.begin(),
                                                                              validInputs.end());
                    for (size_t i = 0; i < mApplyToIndexes.size(); i++) {
                        currInputMutation[mApplyToIndexes[i]] = opsMutation[i];
                    }

                    return currInputMutation;
                });

        return mutatedValidInputs;
    }

    std::vector<uint32_t> mApplyToIndexes;
    TensorRankConstraint mConstraint;
};

class OperationTestBase {
   public:
    OperationTestBase(ANeuralNetworksOperationType opCode,
                      const std::vector<ANeuralNetworksOperandType>& validInputs,
                      const std::vector<ANeuralNetworksOperandType>& validOutputs,
                      const std::vector<TensorRankMutator>& inputRankMutators = {})
        : mOpCode(opCode), mValidInputs(), mValidOutputs(), mInputRankMutators(inputRankMutators) {
        for (ANeuralNetworksOperandType input : validInputs) {
            mValidInputs.push_back(input);
        }
        for (ANeuralNetworksOperandType output : validOutputs) {
            mValidOutputs.push_back(output);
        }
    }

    void setInputSymmPerChannelQuantParams(
            int32_t index, const ANeuralNetworksSymmPerChannelQuantParams& channelQuant) {
        mValidInputs[index].channelQuant = channelQuant;
    }

    void setOutputSymmPerChannelQuantParams(
            int32_t index, const ANeuralNetworksSymmPerChannelQuantParams& channelQuant) {
        mValidOutputs[index].channelQuant = channelQuant;
    }

    void setInputOperandValueFromModel(int32_t index, const ANeuralNetworksModel* valueModel) {
        mValidInputs[index].valueModel = valueModel;
    }

    // Add each operand separately and add the operation using these operands.
    // This function does not cover the cases that an operand is used mutiple times.
    int32_t addOperation(const std::vector<OperandTypeWithExtraParams>& inputs,
                         const std::vector<OperandTypeWithExtraParams>& outputs) {
        ANeuralNetworksModel* model = nullptr;
        ANeuralNetworksModel_create(&model);

        uint32_t opIdx = 0;
        std::vector<uint32_t> inputIds;
        std::vector<uint32_t> outputIds;
        for (uint32_t i = 0; i < inputs.size(); i++) {
            ANeuralNetworksModel_addOperand(model, &inputs[i].operandType);
            if (inputs[i].channelQuant) {
                ANeuralNetworksModel_setOperandSymmPerChannelQuantParams(
                        model, opIdx, &inputs[i].channelQuant.value());
            }
            if (inputs[i].valueModel) {
                ANeuralNetworksModel_setOperandValueFromModel(model, opIdx,
                                                              inputs[i].valueModel.value());
            }
            inputIds.push_back(opIdx++);
        }
        for (uint32_t i = 0; i < outputs.size(); i++) {
            ANeuralNetworksModel_addOperand(model, &outputs[i].operandType);
            if (outputs[i].channelQuant) {
                ANeuralNetworksModel_setOperandSymmPerChannelQuantParams(
                        model, opIdx, &outputs[i].channelQuant.value());
            }
            outputIds.push_back(opIdx++);
        }

        int32_t result = ANeuralNetworksModel_addOperation(
                model, mOpCode, static_cast<uint32_t>(inputIds.size()), inputIds.data(),
                static_cast<uint32_t>(outputIds.size()), outputIds.data());
        ANeuralNetworksModel_free(model);
        return result;
    }

    void testOpsValidations() {
        EXPECT_TRUE(testSuccess());
        EXPECT_TRUE(testMutatingInputOperandCode());
        EXPECT_TRUE(testMutatingInputOperandCounts());
        EXPECT_TRUE(testMutatingOutputOperandCode());
        EXPECT_TRUE(testMutatingOutputOperandCounts());
        EXPECT_TRUE(testMutatingInputRanks());
    }

    void testFailure(int32_t expectedResult) {
        int32_t result = addOperation(mValidInputs, mValidOutputs);
        EXPECT_TRUE(expectedResult == result);
    }

    bool testSuccess() {
        int32_t result = addOperation(mValidInputs, mValidOutputs);
        return ANEURALNETWORKS_NO_ERROR == result;
    }

    bool testMutatingInputOperandCode() {
        for (uint32_t i = 0; i < mValidInputs.size(); i++) {
            // LSH_PROJECTION's second argument is allowed to have any type.
            // This is the only operation that currently has a type that can be
            // anything independent from any other type. Changing the operand
            // type to any other type will result in a valid model for
            // LSH_PROJECTION. If this is the case, skip the test.
            if (mOpCode == ANEURALNETWORKS_LSH_PROJECTION && i == 1) {
                continue;
            }
            // RANK can have input of any type.
            if (mOpCode == ANEURALNETWORKS_RANK) {
                continue;
            }
            OperandTypeWithExtraParams newType = mValidInputs[i];
            int32_t originalOperandCode = mValidInputs[i].operandType.type;
            std::set<int32_t> operandTypesToSkip;
            // Transposed conv can have either fully quantized or per-channel
            // quantized filter for the quantized version of the op.
            if ((mOpCode == ANEURALNETWORKS_TRANSPOSE_CONV_2D ||
                 mOpCode == ANEURALNETWORKS_DEPTHWISE_CONV_2D) &&
                i == 1) {
                if (originalOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM ||
                    originalOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED ||
                    originalOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
                    operandTypesToSkip.insert(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
                    operandTypesToSkip.insert(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
                    operandTypesToSkip.insert(ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL);
                }
            }
            // CAST accepts any of supported types for any of output types
            if (mOpCode == ANEURALNETWORKS_CAST && i == 0) {
                operandTypesToSkip.insert(ANEURALNETWORKS_TENSOR_FLOAT16);
                operandTypesToSkip.insert(ANEURALNETWORKS_TENSOR_FLOAT32);
                operandTypesToSkip.insert(ANEURALNETWORKS_TENSOR_INT32);
                operandTypesToSkip.insert(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
                operandTypesToSkip.insert(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
            }
            // RANDOM_MULTINOMIAL's first input can be either of float16 or
            // float32 type while everything else has the same types.
            if (mOpCode == ANEURALNETWORKS_RANDOM_MULTINOMIAL && i == 0) {
                if (originalOperandCode == ANEURALNETWORKS_TENSOR_FLOAT16) {
                    operandTypesToSkip.insert(ANEURALNETWORKS_TENSOR_FLOAT32);
                } else if (originalOperandCode == ANEURALNETWORKS_TENSOR_FLOAT32) {
                    operandTypesToSkip.insert(ANEURALNETWORKS_TENSOR_FLOAT16);
                }
            }
            // DEQUANTIZE supports any of the inputs types below for any of the
            // output types.
            if (mOpCode == ANEURALNETWORKS_DEQUANTIZE && i == 0) {
                operandTypesToSkip.insert(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
                operandTypesToSkip.insert(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
                operandTypesToSkip.insert(ANEURALNETWORKS_TENSOR_QUANT8_SYMM);
                operandTypesToSkip.insert(ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL);
            }
            // AXIS_ALIGNED_BBOX_TRANSFORM's second input cab be either QUANT8_ASYMM or
            // QUANT8_ASYMM_SIGNED
            if (mOpCode == ANEURALNETWORKS_AXIS_ALIGNED_BBOX_TRANSFORM && i == 1) {
                operandTypesToSkip.insert(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
                operandTypesToSkip.insert(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
            }

            for (int32_t newOperandCode : kAvailableOperandCodes) {
                if (newOperandCode == originalOperandCode ||
                    operandTypesToSkip.find(newOperandCode) != operandTypesToSkip.end()) {
                    continue;
                }
                // Switch input 7 from bool to int for 10-input CONV_2d
                // switch between valid "implicit padding with layout param"
                // and valid "explicit padding without layout param"
                if (mOpCode == ANEURALNETWORKS_CONV_2D && i == 7 && mValidInputs.size() == 10) {
                    if ((newOperandCode == ANEURALNETWORKS_INT32 &&
                         originalOperandCode == ANEURALNETWORKS_BOOL) ||
                        (newOperandCode == ANEURALNETWORKS_BOOL &&
                         originalOperandCode == ANEURALNETWORKS_INT32)) {
                        continue;
                    }
                }
                // QUANTIZE supports both types below and its output type does
                // not depend on the input type.
                if (mOpCode == ANEURALNETWORKS_QUANTIZE && i == 0 &&
                    (newOperandCode == ANEURALNETWORKS_TENSOR_FLOAT16 ||
                     newOperandCode == ANEURALNETWORKS_TENSOR_FLOAT32)) {
                    continue;
                }

                // ARGMIN/MAX supports four input types and has a fixed output type.
                if ((mOpCode == ANEURALNETWORKS_ARGMIN || mOpCode == ANEURALNETWORKS_ARGMAX) &&
                    i == 0 &&
                    (newOperandCode == ANEURALNETWORKS_TENSOR_FLOAT16 ||
                     newOperandCode == ANEURALNETWORKS_TENSOR_FLOAT32 ||
                     newOperandCode == ANEURALNETWORKS_TENSOR_INT32 ||
                     newOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM ||
                     newOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED)) {
                    continue;
                }

                // Switch input 8 from bool to int for 11-input DEPTHWISE_CONV_2D
                // switch between valid "implicit padding with layout param"
                // and valid "explicit padding without layout param"
                if (mOpCode == ANEURALNETWORKS_DEPTHWISE_CONV_2D && i == 8 &&
                    mValidInputs.size() == 11) {
                    if ((newOperandCode == ANEURALNETWORKS_INT32 &&
                         originalOperandCode == ANEURALNETWORKS_BOOL) ||
                        (newOperandCode == ANEURALNETWORKS_BOOL &&
                         originalOperandCode == ANEURALNETWORKS_INT32)) {
                        continue;
                    }
                }

                newType.operandType.type = newOperandCode;
                std::vector<OperandTypeWithExtraParams> inputs = mValidInputs;
                inputs[i] = newType;
                int32_t result = addOperation(inputs, mValidOutputs);
                if (ANEURALNETWORKS_NO_ERROR == result) {
                    return false;
                }
            }
        }
        return true;
    }

    bool testMutatingOutputOperandCode() {
        for (uint32_t i = 0; i < mValidOutputs.size(); i++) {
            // LSH_PROJECTION's second argument is allowed to have any type.
            // This is the only operation that currently has a type that can be
            // anything independent from any other type. Changing the operand
            // type to any other type will result in a valid model for
            // LSH_PROJECTION. If this is the case, skip the test.
            if (mOpCode == ANEURALNETWORKS_LSH_PROJECTION && i == 1) {
                continue;
            }
            OperandTypeWithExtraParams newType = mValidOutputs[i].operandType;
            int32_t originalOperandCode = mValidOutputs[i].operandType.type;
            for (int32_t newOperandCode : kAvailableOperandCodes) {
                if (newOperandCode == originalOperandCode) {
                    continue;
                }
                // DEQUANTIZE's output can be either TENSOR_FLOAT16 or TENSOR_FLOAT32.
                if (mOpCode == ANEURALNETWORKS_DEQUANTIZE &&
                    (newOperandCode == ANEURALNETWORKS_TENSOR_FLOAT16 ||
                     newOperandCode == ANEURALNETWORKS_TENSOR_FLOAT32)) {
                    continue;
                }

                // QUANTIZE's output can be either TENSOR_QUANT8_ASYMM or
                // TENSOR_QUANT8_ASYMM_SIGNED.
                if (mOpCode == ANEURALNETWORKS_QUANTIZE && i == 0 &&
                    (newOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM ||
                     newOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED)) {
                    continue;
                }

                // CAST accepts any of supported types for any of input types
                if (mOpCode == ANEURALNETWORKS_CAST && i == 0 &&
                    (newOperandCode == ANEURALNETWORKS_TENSOR_FLOAT16 ||
                     newOperandCode == ANEURALNETWORKS_TENSOR_FLOAT32 ||
                     newOperandCode == ANEURALNETWORKS_TENSOR_INT32 ||
                     newOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM)) {
                    continue;
                }
                newType.operandType.type = newOperandCode;
                std::vector<OperandTypeWithExtraParams> outputs = mValidOutputs;
                outputs[i] = newType;
                int32_t result = addOperation(mValidInputs, outputs);
                if (ANEURALNETWORKS_NO_ERROR == result) {
                    return false;
                }
            }
        }
        return true;
    }

    bool testMutatingInputOperandCounts() {
        uint32_t numToAdd = 5;
        // LSTM since API 29 supports 23 and 27 outputs.
        if (mOpCode == ANEURALNETWORKS_LSTM) {
            numToAdd = 3;
        }
        std::vector<OperandTypeWithExtraParams> inputs = mValidInputs;
        for (uint32_t i = 0; i < numToAdd; i++) {
            inputs.push_back(inputs[0]);
            if (ANEURALNETWORKS_NO_ERROR == addOperation(inputs, mValidOutputs)) {
                return false;
            }
        }
        return true;
    }

    bool testMutatingOutputOperandCounts() {
        // SPLIT's number of outputs depends on a value of one of its inputs and
        // are not checked during validation.
        if (mOpCode == ANEURALNETWORKS_SPLIT) {
            return true;
        }
        std::vector<OperandTypeWithExtraParams> outputs = mValidOutputs;
        for (int i = 0; i < 6; i++) {
            outputs.push_back(outputs[0]);
            if (ANEURALNETWORKS_NO_ERROR == addOperation(mValidInputs, outputs)) {
                if (mOpCode == ANEURALNETWORKS_UNIDIRECTIONAL_SEQUENCE_RNN && i < 1) {
                    continue;
                }
                if (mOpCode == ANEURALNETWORKS_UNIDIRECTIONAL_SEQUENCE_LSTM && i < 3) {
                    continue;
                }
                if (mOpCode == ANEURALNETWORKS_BIDIRECTIONAL_SEQUENCE_RNN && i < 3) {
                    continue;
                }
                if (mOpCode == ANEURALNETWORKS_BIDIRECTIONAL_SEQUENCE_LSTM && i < 5) {
                    continue;
                }
                return false;
            }
        }
        return true;
    }

    bool testMutatingInputRanks() {
        for (auto& rankMutator : mInputRankMutators) {
            for (const auto& validMutation : rankMutator.ValidInputsMutations(mValidInputs)) {
                int32_t result = addOperation(validMutation, mValidOutputs);
                if (ANEURALNETWORKS_NO_ERROR != result) {
                    return false;
                }
            }

            for (const auto& invalidMutation : rankMutator.InvalidInputsMutations(mValidInputs)) {
                int32_t result = addOperation(invalidMutation, mValidOutputs);
                if (ANEURALNETWORKS_NO_ERROR == result) {
                    return false;
                }
            }
        }

        return true;
    }

   private:
    ANeuralNetworksOperationType mOpCode;
    // The dimensions in the ANeuralNetworksOperandType must outlive the test object.
    std::vector<OperandTypeWithExtraParams> mValidInputs;
    std::vector<OperandTypeWithExtraParams> mValidOutputs;

    std::vector<TensorRankMutator> mInputRankMutators;
};

std::ostream& operator<<(std::ostream& os, const OperandTypeWithExtraParams& operand) {
    const auto& operandType = operand.operandType;
    os << "{ operand_type: { type: " << operandType.type << ", "
       << "dimensionCount: " << operandType.dimensionCount << ", dimensions: [";
    std::for_each(operandType.dimensions, operandType.dimensions + operandType.dimensionCount,
                  [&os](uint32_t dimension) { os << dimension << ", "; });
    os << "], scale: " << operandType.scale << ", zeroPoint: " << operandType.zeroPoint << " }";

    const auto& channelQuant = operand.channelQuant;
    if (channelQuant.has_value()) {
        os << ", channelQuant { channelDim: " << channelQuant->channelDim
           << ", scaleCount: " << channelQuant->scaleCount << ", scales: [";
        std::for_each(channelQuant->scales, channelQuant->scales + channelQuant->scaleCount,
                      [&os](float scale) { os << scale << ", "; });
        os << "] }";
    } else {
        os << ", channelQuant: nullopt";
    }

    if (operand.valueModel.has_value()) {
        os << ", valueModel: " << operand.valueModel.value();
    } else {
        os << ", valueModel: nullopt";
    }
    os << "}";
    return os;
}

inline OperandTypeWithExtraParams MutationWithDimensions(
        const OperandTypeWithExtraParams& origin, const std::vector<uint32_t>& expectedDims) {
    OperandTypeWithExtraParams expected = origin;
    expected.operandType.dimensionCount = expectedDims.size();
    if (expectedDims.size() == 0) {
        expected.operandType.dimensions = nullptr;
    } else {
        expected.operandType.dimensions = expectedDims.data();
    }
    return expected;
}
std::string DescribeMutationWithDimensions(const OperandTypeWithExtraParams& origin,
                                           const std::vector<uint32_t>& expectedDims) {
    std::ostringstream osstream;
    osstream << MutationWithDimensions(origin, expectedDims);
    return osstream.str();
}

MATCHER_P2(IsMutationWithDimensions, origin, expectedDims,
           DescribeMutationWithDimensions(origin, expectedDims)) {
    return arg == MutationWithDimensions(origin, expectedDims);
}

TEST(TensorRankConstraint, ExactlyWillReturnSameInputAsValidMutation) {
    uint32_t opDimensions[3] = {2, 2, 2};
    OperandTypeWithExtraParams operand{ANeuralNetworksOperandType{
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 3,
            .dimensions = opDimensions,
    }};

    auto constraint = TensorRankConstraint::Exactly(3);
    auto validMutationSet = constraint.MutationsWithValidRank({operand});
    ASSERT_EQ(validMutationSet.size(), 1u);
    auto validMutations = *validMutationSet.begin();
    ASSERT_EQ(validMutations.size(), 1u);
    EXPECT_THAT(validMutations[0],
                IsMutationWithDimensions(operand, std::vector<uint32_t>({2, 2, 2})));
};

TEST(TensorRankConstraint, ExactlyWillFailIfValidInputHasInvalidSize) {
    uint32_t opDimensions[2] = {2, 2};
    OperandTypeWithExtraParams operand{ANeuralNetworksOperandType{
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 2,
            .dimensions = opDimensions,
    }};
    EXPECT_DEATH(TensorRankConstraint::Exactly(3).MutationsWithValidRank({operand}),
                 ".*assertion.+failed.*");
};

TEST(TensorRankConstraint, ExactlyWillReturnTwoInvalidMutationsWithLowerAndHigherRank) {
    uint32_t opDimensions[3] = {2, 2, 2};
    OperandTypeWithExtraParams operand{ANeuralNetworksOperandType{
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 3,
            .dimensions = opDimensions,
    }};

    auto constraint = TensorRankConstraint::Exactly(3);
    auto invalidMutations = constraint.MutationsWithInvalidRank({operand});
    ASSERT_EQ(invalidMutations.size(), 2u);
    std::for_each(invalidMutations.begin(), invalidMutations.end(),
                  [&operand](const std::vector<OperandTypeWithExtraParams>& mutations) {
                      EXPECT_EQ(mutations.size(), 1u);
                      if (mutations.size() == 1) {
                          EXPECT_THAT(
                                  mutations[0],
                                  ::testing::AnyOf(
                                          IsMutationWithDimensions(operand,
                                                                   std::vector<uint32_t>({2, 2})),
                                          IsMutationWithDimensions(
                                                  operand, std::vector<uint32_t>({2, 2, 2, 1}))));
                      }
                  });
};

TEST(TensorRankConstraint, AtLeastWillReturnTwoValidMutationsAboveThreshold) {
    uint32_t opDimensions[3] = {2, 2, 2};
    OperandTypeWithExtraParams operand{ANeuralNetworksOperandType{
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 2,
            .dimensions = opDimensions,
    }};

    auto constraint = TensorRankConstraint::AtLeast(1);
    auto invalidMutations =
            constraint.MutationsWithValidRank({(OperandTypeWithExtraParams)operand});
    ASSERT_EQ(invalidMutations.size(), 2u);
    std::for_each(
            invalidMutations.begin(), invalidMutations.end(),
            [&operand](const std::vector<OperandTypeWithExtraParams>& mutations) {
                EXPECT_EQ(mutations.size(), 1u);
                if (mutations.size() == 1) {
                    EXPECT_THAT(mutations[0],
                                ::testing::AnyOf(IsMutationWithDimensions(
                                                         operand, std::vector<uint32_t>({2})),
                                                 IsMutationWithDimensions(
                                                         operand, std::vector<uint32_t>({2, 2}))));
                }
            });
}

TEST(TensorRankConstraint, AtLeastWillReturnOneInvalidMutationsBelowThreshold) {
    uint32_t opDimensions[2] = {2, 2};
    OperandTypeWithExtraParams operand{ANeuralNetworksOperandType{
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 2,
            .dimensions = opDimensions,
    }};

    auto constraint = TensorRankConstraint::AtLeast(2);
    auto invalidMutations =
            constraint.MutationsWithInvalidRank({(OperandTypeWithExtraParams)operand});
    ASSERT_EQ(invalidMutations.size(), 1u);
    auto invalidMutationVector = *invalidMutations.begin();
    ASSERT_EQ(invalidMutationVector.size(), 1u);
    ASSERT_THAT(invalidMutationVector[0],
                IsMutationWithDimensions(operand, std::vector<uint32_t>({2})));
}

TEST(TensorRankConstraint, AtLeastWillReturnNoInvalidMutationsIfThresholdIs1) {
    uint32_t opDimensions[1] = {2};
    OperandTypeWithExtraParams operand{ANeuralNetworksOperandType{
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 1,
            .dimensions = opDimensions,
    }};

    auto constraint = TensorRankConstraint::AtLeast(1);
    auto invalidMutations =
            constraint.MutationsWithInvalidRank({(OperandTypeWithExtraParams)operand});
    ASSERT_EQ(invalidMutations.size(), 0u);
}

TEST(TensorRankConstraint, UpToWillReturnUpToTwoValidMutationsBelowThreshold) {
    uint32_t opDimensions[3] = {2, 2, 2};
    OperandTypeWithExtraParams operand{ANeuralNetworksOperandType{
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 2,
            .dimensions = opDimensions,
    }};

    auto constraint = TensorRankConstraint::UpTo(3);
    auto invalidMutations =
            constraint.MutationsWithValidRank({(OperandTypeWithExtraParams)operand});

    auto expected = std::vector<uint32_t>({7, 7});
    ASSERT_EQ(invalidMutations.size(), 2u);
    std::for_each(invalidMutations.begin(), invalidMutations.end(),
                  [&operand](const std::vector<OperandTypeWithExtraParams>& mutations) {
                      EXPECT_EQ(mutations.size(), 1u);
                      if (mutations.size() == 1) {
                          EXPECT_THAT(mutations[0],
                                      ::testing::AnyOf(
                                              IsMutationWithDimensions(operand,
                                                                       std::vector<uint32_t>({2})),
                                              IsMutationWithDimensions(
                                                      operand, std::vector<uint32_t>({2, 2, 1}))));
                      }
                  });
}

TEST(TensorRankConstraint, UpToWillReturnOneInvalidMutationsAboveThreshold) {
    uint32_t opDimensions[3] = {2, 2, 2};
    OperandTypeWithExtraParams operand{ANeuralNetworksOperandType{
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 3,
            .dimensions = opDimensions,
    }};

    auto constraint = TensorRankConstraint::UpTo(3);
    auto invalidMutations =
            constraint.MutationsWithInvalidRank({(OperandTypeWithExtraParams)operand});
    ASSERT_EQ(invalidMutations.size(), 1u);
    auto invalidMutationVector = *invalidMutations.begin();
    ASSERT_EQ(invalidMutationVector.size(), 1u);
    ASSERT_THAT(invalidMutationVector[0],
                IsMutationWithDimensions(operand, std::vector<uint32_t>({2, 2, 2, 1})));
}

TEST(TensorRankConstraint, BetweenWillReturnTwoValidMutationsOnRangeBoundaries) {
    uint32_t opDimensions[3] = {2, 2, 2};
    OperandTypeWithExtraParams operand{ANeuralNetworksOperandType{
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 3,
            .dimensions = opDimensions,
    }};

    auto constraint = TensorRankConstraint::Between(2, 4);
    auto validMutations = constraint.MutationsWithValidRank({(OperandTypeWithExtraParams)operand});
    ASSERT_EQ(validMutations.size(), 2u);
    std::for_each(validMutations.begin(), validMutations.end(),
                  [&operand](const std::vector<OperandTypeWithExtraParams>& mutations) {
                      EXPECT_EQ(mutations.size(), 1u);
                      if (mutations.size() == 1) {
                          EXPECT_THAT(
                                  mutations[0],
                                  ::testing::AnyOf(
                                          IsMutationWithDimensions(operand,
                                                                   std::vector<uint32_t>({2, 2})),
                                          IsMutationWithDimensions(
                                                  operand, std::vector<uint32_t>({2, 2, 2, 1}))));
                      }
                  });
}

TEST(TensorRankConstraint, BetweenWillReturnTwoInvValidMutationsAdjacentToRangeBoundaries) {
    uint32_t opDimensions[3] = {2, 2, 2};
    OperandTypeWithExtraParams operand{ANeuralNetworksOperandType{
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 3,
            .dimensions = opDimensions,
    }};

    auto constraint = TensorRankConstraint::Between(2, 4);
    auto validMutations =
            constraint.MutationsWithInvalidRank({(OperandTypeWithExtraParams)operand});
    ASSERT_EQ(validMutations.size(), 2u);
    std::for_each(
            validMutations.begin(), validMutations.end(),
            [&operand](const std::vector<OperandTypeWithExtraParams>& mutations) {
                EXPECT_EQ(mutations.size(), 1u);
                if (mutations.size() == 1) {
                    EXPECT_THAT(
                            mutations[0],
                            ::testing::AnyOf(
                                    IsMutationWithDimensions(operand, std::vector<uint32_t>({2})),
                                    IsMutationWithDimensions(
                                            operand, std::vector<uint32_t>({2, 2, 2, 1, 1}))));
                }
            });
}

TEST(TensorRankConstraint, BetweenWillReturnOneInvalidMutationsOnlyIfLowerBoundIs1) {
    uint32_t opDimensions[3] = {2, 2, 2};
    OperandTypeWithExtraParams operand{ANeuralNetworksOperandType{
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 3,
            .dimensions = opDimensions,
    }};

    auto constraint = TensorRankConstraint::Between(1, 4);
    auto invalidMutations =
            constraint.MutationsWithInvalidRank({(OperandTypeWithExtraParams)operand});
    ASSERT_EQ(invalidMutations.size(), 1u);
    auto invalidMutationVector = *invalidMutations.begin();
    ASSERT_EQ(invalidMutationVector.size(), 1u);
    ASSERT_THAT(invalidMutationVector[0],
                IsMutationWithDimensions(operand, std::vector<uint32_t>({2, 2, 2, 1, 1})));
}

TEST(TensorRankMutator, AppliesConstraintToInputsAtGivenInputsToGenerateValidMutations) {
    uint32_t opDimensions0[2] = {0, 0};
    OperandTypeWithExtraParams operand0{ANeuralNetworksOperandType{
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 2,
            .dimensions = opDimensions0,
    }};
    uint32_t opDimensions1[1] = {1};
    OperandTypeWithExtraParams operand1{ANeuralNetworksOperandType{
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 1,
            .dimensions = opDimensions1,
    }};
    uint32_t opDimensions2[2] = {2, 2};
    OperandTypeWithExtraParams operand2{ANeuralNetworksOperandType{
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 2,
            .dimensions = opDimensions2,
    }};
    TensorRankMutator mutator{TensorRankConstraint::AtLeast(2), {0, 2}};

    const auto mutationSet = mutator.ValidInputsMutations({operand0, operand1, operand2});
    ASSERT_EQ(mutationSet.size(), 2u);
    std::for_each(mutationSet.begin(), mutationSet.end(),
                  [&](const std::vector<OperandTypeWithExtraParams>& mutatedInputs) {
                      EXPECT_EQ(mutatedInputs.size(), 3u);
                      if (mutatedInputs.size() == 3) {
                          EXPECT_EQ(mutatedInputs[0].operandType.dimensionCount,
                                    mutatedInputs[2].operandType.dimensionCount);
                          EXPECT_THAT(mutatedInputs[0],
                                      ::testing::AnyOf(
                                              IsMutationWithDimensions(
                                                      operand0, std::vector<uint32_t>({0, 0})),
                                              IsMutationWithDimensions(
                                                      operand0, std::vector<uint32_t>({0, 0, 1}))));

                          EXPECT_EQ(mutatedInputs[1], operand1);

                          EXPECT_THAT(mutatedInputs[2],
                                      ::testing::AnyOf(
                                              IsMutationWithDimensions(
                                                      operand2, std::vector<uint32_t>({2, 2})),
                                              IsMutationWithDimensions(
                                                      operand2, std::vector<uint32_t>({2, 2, 1}))));
                      }
                  });
}

TEST(TensorRankMutator, AppliesConstraintToInputsAtGivenInputsToGenerateInvalidMutations) {
    uint32_t opDimensions0[2] = {0, 0};
    OperandTypeWithExtraParams operand0{ANeuralNetworksOperandType{
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 2,
            .dimensions = opDimensions0,
    }};
    uint32_t opDimensions1[1] = {1};
    OperandTypeWithExtraParams operand1{ANeuralNetworksOperandType{
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 1,
            .dimensions = opDimensions1,
    }};
    uint32_t opDimensions2[2] = {2, 2};
    OperandTypeWithExtraParams operand2{ANeuralNetworksOperandType{
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 2,
            .dimensions = opDimensions2,
    }};
    TensorRankMutator mutator{TensorRankConstraint::AtLeast(2), {0, 2}};

    const auto mutationSet = mutator.InvalidInputsMutations({operand0, operand1, operand2});
    ASSERT_EQ(mutationSet.size(), 1u);
    std::for_each(
            mutationSet.begin(), mutationSet.end(),
            [&](const std::vector<OperandTypeWithExtraParams>& mutatedInputs) {
                EXPECT_EQ(mutatedInputs.size(), 3u);
                if (mutatedInputs.size() == 3) {
                    EXPECT_THAT(mutatedInputs[0],
                                IsMutationWithDimensions(operand0, std::vector<uint32_t>({0})));

                    EXPECT_EQ(mutatedInputs[1], operand1);

                    EXPECT_THAT(mutatedInputs[2],
                                IsMutationWithDimensions(operand2, std::vector<uint32_t>({2})));
                }
            });
}

void argMinMaxTest(ANeuralNetworksOperationType operationCode, int32_t inputOperandType) {
    SCOPED_TRACE(inputOperandType);
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input0 = getOpType(inputOperandType, 4, inputDimensions);
    ANeuralNetworksOperandType axis = {
            .type = ANEURALNETWORKS_INT32,
            .dimensionCount = 0,
            .dimensions = nullptr,
    };
    uint32_t outputDimensions[3] = {2, 2, 2};
    ANeuralNetworksOperandType output = {
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 3,
            .dimensions = outputDimensions,
    };
    OperationTestBase test(operationCode, {input0, axis}, {output});
    test.testOpsValidations();
}

TEST(OperationValidationTest, ARGMIN) {
    argMinMaxTest(ANEURALNETWORKS_ARGMIN, ANEURALNETWORKS_TENSOR_FLOAT16);
    argMinMaxTest(ANEURALNETWORKS_ARGMIN, ANEURALNETWORKS_TENSOR_FLOAT32);
    argMinMaxTest(ANEURALNETWORKS_ARGMIN, ANEURALNETWORKS_TENSOR_INT32);
    argMinMaxTest(ANEURALNETWORKS_ARGMIN, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    argMinMaxTest(ANEURALNETWORKS_ARGMIN, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, ARGMAX) {
    argMinMaxTest(ANEURALNETWORKS_ARGMAX, ANEURALNETWORKS_TENSOR_FLOAT16);
    argMinMaxTest(ANEURALNETWORKS_ARGMAX, ANEURALNETWORKS_TENSOR_FLOAT32);
    argMinMaxTest(ANEURALNETWORKS_ARGMAX, ANEURALNETWORKS_TENSOR_INT32);
    argMinMaxTest(ANEURALNETWORKS_ARGMAX, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    argMinMaxTest(ANEURALNETWORKS_ARGMAX, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void dequantizeOpTest(int32_t inputOperandType, int32_t outputOperandType) {
    SCOPED_TRACE(testing::Message()
                 << "inputType: " << inputOperandType << ", outputType: " << outputOperandType);
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input = getOpType(inputOperandType, 4, inputDimensions);
    ANeuralNetworksOperandType output = getOpType(outputOperandType, 4, inputDimensions);
    OperationTestBase dequantizeTest(ANEURALNETWORKS_DEQUANTIZE, {input}, {output},
                                     {{TensorRankConstraint::UpTo(4)}});
    dequantizeTest.testOpsValidations();
}

TEST(OperationValidationTest, DEQUANTIZE) {
    dequantizeOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, ANEURALNETWORKS_TENSOR_FLOAT16);
    dequantizeOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, ANEURALNETWORKS_TENSOR_FLOAT32);
    dequantizeOpTest(ANEURALNETWORKS_TENSOR_QUANT8_SYMM, ANEURALNETWORKS_TENSOR_FLOAT16);
    dequantizeOpTest(ANEURALNETWORKS_TENSOR_QUANT8_SYMM, ANEURALNETWORKS_TENSOR_FLOAT32);
    dequantizeOpTest(ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL,
                     ANEURALNETWORKS_TENSOR_FLOAT16);
    dequantizeOpTest(ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL,
                     ANEURALNETWORKS_TENSOR_FLOAT32);
    dequantizeOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED, ANEURALNETWORKS_TENSOR_FLOAT16);
    dequantizeOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED, ANEURALNETWORKS_TENSOR_FLOAT32);
}

void expandDimsTest(int32_t inputOperandType) {
    SCOPED_TRACE(inputOperandType);
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input0 = getOpType(inputOperandType, 4, inputDimensions);
    ANeuralNetworksOperandType axis = {
            .type = ANEURALNETWORKS_INT32,
            .dimensionCount = 0,
            .dimensions = nullptr,
    };
    uint32_t outputDimensions[5] = {2, 2, 2, 2, 2};
    ANeuralNetworksOperandType output = getOpType(inputOperandType, 5, outputDimensions);
    OperationTestBase test(ANEURALNETWORKS_EXPAND_DIMS, {input0, axis}, {output});
    test.testOpsValidations();
}

TEST(OperationValidationTest, EXPAND_DIMS) {
    expandDimsTest(ANEURALNETWORKS_TENSOR_FLOAT16);
    expandDimsTest(ANEURALNETWORKS_TENSOR_FLOAT32);
    expandDimsTest(ANEURALNETWORKS_TENSOR_INT32);
    expandDimsTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    expandDimsTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void gatherTest(int32_t inputOperandType) {
    SCOPED_TRACE(inputOperandType);
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input0 = getOpType(inputOperandType, 4, inputDimensions);
    ANeuralNetworksOperandType axis = {
            .type = ANEURALNETWORKS_INT32,
            .dimensionCount = 0,
            .dimensions = nullptr,
    };
    ANeuralNetworksOperandType input2 = {
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 4,
            .dimensions = inputDimensions,
    };
    uint32_t outputDimensions[7] = {2, 2, 2, 2, 2, 2, 2};
    ANeuralNetworksOperandType output = getOpType(inputOperandType, 7, outputDimensions);
    OperationTestBase test(ANEURALNETWORKS_GATHER, {input0, axis, input2}, {output});
    test.testOpsValidations();
}

TEST(OperationValidationTest, GATHER) {
    gatherTest(ANEURALNETWORKS_TENSOR_FLOAT16);
    gatherTest(ANEURALNETWORKS_TENSOR_FLOAT32);
    gatherTest(ANEURALNETWORKS_TENSOR_INT32);
    gatherTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    gatherTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void quantizeOpTest(int32_t inputOperandCode, int32_t outputOperandCode) {
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input = {
            .type = inputOperandCode, .dimensionCount = 4, .dimensions = inputDimensions};
    ANeuralNetworksOperandType output = {.type = outputOperandCode,
                                         .dimensionCount = 4,
                                         .dimensions = inputDimensions,
                                         .scale = 1.0f,
                                         .zeroPoint = 0};
    OperationTestBase test(ANEURALNETWORKS_QUANTIZE, {input}, {output});
    test.testOpsValidations();
}

TEST(OperationValidationTest, QUANTIZE_float16) {
    quantizeOpTest(ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    quantizeOpTest(ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, QUANTIZE_float32) {
    quantizeOpTest(ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    quantizeOpTest(ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, QUANTIZED_16BIT_LSTM) {
    uint32_t oneDimensional[1] = {5};
    uint32_t twoDimensional[2] = {5, 5};

    ANeuralNetworksOperandType int32Tensor1D = {
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 1,
            .dimensions = oneDimensional,
            .scale = 0.0000318,
            .zeroPoint = 0,
    };
    ANeuralNetworksOperandType quant8Tensor2D = {
            .type = ANEURALNETWORKS_TENSOR_QUANT8_ASYMM,
            .dimensionCount = 2,
            .dimensions = twoDimensional,
            .scale = 0.00408021,
            .zeroPoint = 100,
    };
    ANeuralNetworksOperandType quant16Tensor2D = {
            .type = ANEURALNETWORKS_TENSOR_QUANT16_SYMM,
            .dimensionCount = 2,
            .dimensions = twoDimensional,
            .scale = 1.0 / 2048,
            .zeroPoint = 0,
    };

    ANeuralNetworksOperandType input = quant8Tensor2D;
    ANeuralNetworksOperandType input_to_input_weights = quant8Tensor2D;
    ANeuralNetworksOperandType input_to_forget_weights = quant8Tensor2D;
    ANeuralNetworksOperandType input_to_cell_weights = quant8Tensor2D;
    ANeuralNetworksOperandType input_to_output_weights = quant8Tensor2D;
    ANeuralNetworksOperandType recurrent_to_input_weights = quant8Tensor2D;
    ANeuralNetworksOperandType recurrent_to_forget_weights = quant8Tensor2D;
    ANeuralNetworksOperandType recurrent_to_cell_weights = quant8Tensor2D;
    ANeuralNetworksOperandType recurrent_to_output_weights = quant8Tensor2D;
    ANeuralNetworksOperandType input_gate_bias = int32Tensor1D;
    ANeuralNetworksOperandType forget_gate_bias = int32Tensor1D;
    ANeuralNetworksOperandType cell_gate_bias = int32Tensor1D;
    ANeuralNetworksOperandType output_gate_bias = int32Tensor1D;
    ANeuralNetworksOperandType prev_cell_state = quant16Tensor2D;
    ANeuralNetworksOperandType prev_output = quant8Tensor2D;

    ANeuralNetworksOperandType cell_state_out = quant16Tensor2D;
    ANeuralNetworksOperandType output = quant8Tensor2D;

    OperationTestBase test(
            ANEURALNETWORKS_QUANTIZED_16BIT_LSTM,
            {input, input_to_input_weights, input_to_forget_weights, input_to_cell_weights,
             input_to_output_weights, recurrent_to_input_weights, recurrent_to_forget_weights,
             recurrent_to_cell_weights, recurrent_to_output_weights, input_gate_bias,
             forget_gate_bias, cell_gate_bias, output_gate_bias, prev_cell_state, prev_output},
            {cell_state_out, output});
    test.testOpsValidations();
}

void splitTest(int32_t inputOperandType) {
    SCOPED_TRACE(inputOperandType);
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input0 = getOpType(inputOperandType, 4, inputDimensions);
    ANeuralNetworksOperandType axis = {
            .type = ANEURALNETWORKS_INT32,
            .dimensionCount = 0,
            .dimensions = nullptr,
    };
    ANeuralNetworksOperandType count = {
            .type = ANEURALNETWORKS_INT32,
            .dimensionCount = 0,
            .dimensions = nullptr,
    };
    uint32_t outputDimensions[2] = {2, 2};
    ANeuralNetworksOperandType output0 = getOpType(inputOperandType, 2, outputDimensions);
    ANeuralNetworksOperandType output1 = getOpType(inputOperandType, 2, outputDimensions);
    OperationTestBase test(ANEURALNETWORKS_SPLIT, {input0, axis, count}, {output0, output1});
    test.testOpsValidations();
}

TEST(OperationValidationTest, SPLIT) {
    splitTest(ANEURALNETWORKS_TENSOR_FLOAT16);
    splitTest(ANEURALNETWORKS_TENSOR_FLOAT32);
    splitTest(ANEURALNETWORKS_TENSOR_INT32);
    splitTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    splitTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void tileTest(int32_t inputOperandType) {
    SCOPED_TRACE(inputOperandType);
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input0 = getOpType(inputOperandType, 4, inputDimensions);
    uint32_t multiplesDimensions[1] = {4};
    ANeuralNetworksOperandType multiples = {
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 1,
            .dimensions = multiplesDimensions,
    };
    uint32_t outputDimensions[8] = {2, 2, 2, 2, 2, 2, 2, 2};
    ANeuralNetworksOperandType output0 = getOpType(inputOperandType, 8, outputDimensions);
    OperationTestBase test(ANEURALNETWORKS_TILE, {input0, multiples}, {output0});
    test.testOpsValidations();
}

TEST(OperationValidationTest, TILE) {
    tileTest(ANEURALNETWORKS_TENSOR_FLOAT16);
    tileTest(ANEURALNETWORKS_TENSOR_FLOAT32);
    tileTest(ANEURALNETWORKS_TENSOR_INT32);
    tileTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    tileTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void topkV2Test(int32_t inputOperandType) {
    SCOPED_TRACE(inputOperandType);
    uint32_t inputDimensions[4] = {4, 5, 6, 7};
    ANeuralNetworksOperandType input = getOpType(inputOperandType, 4, inputDimensions);
    ANeuralNetworksOperandType k = getOpType(ANEURALNETWORKS_INT32);
    uint32_t outputDimensions[4] = {4, 5, 6, 3};
    ANeuralNetworksOperandType outputValues = getOpType(inputOperandType, 4, outputDimensions);
    ANeuralNetworksOperandType outputIndices =
            getOpType(ANEURALNETWORKS_TENSOR_INT32, 4, outputDimensions);
    OperationTestBase test(ANEURALNETWORKS_TOPK_V2, {input, k}, {outputValues, outputIndices});
    test.testOpsValidations();
}

TEST(OperationValidationTest, TOPK_V2) {
    topkV2Test(ANEURALNETWORKS_TENSOR_FLOAT16);
    topkV2Test(ANEURALNETWORKS_TENSOR_FLOAT32);
    topkV2Test(ANEURALNETWORKS_TENSOR_INT32);
    topkV2Test(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    topkV2Test(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void simpleMathOpTest(ANeuralNetworksOperationType operationCode, int32_t operandCode) {
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input1 = getOpType(operandCode, 4, inputDimensions);

    ANeuralNetworksOperandType input2 = input1;
    ANeuralNetworksOperandType output = input1;
    ANeuralNetworksOperandType activation = {.type = ANEURALNETWORKS_INT32,
                                             .dimensionCount = 0,
                                             .dimensions = nullptr,
                                             .scale = 0.0f,
                                             .zeroPoint = 0};

    OperationTestBase simpleMathTest(
            operationCode, {input1, input2, activation}, {output},
            {{TensorRankConstraint::UpTo(4), {0}}, {TensorRankConstraint::UpTo(4), {1}}});
    simpleMathTest.testOpsValidations();
}

TEST(OperationValidationTest, ADD_float16) {
    simpleMathOpTest(ANEURALNETWORKS_ADD, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, ADD_float32) {
    simpleMathOpTest(ANEURALNETWORKS_ADD, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, ADD_quant8) {
    simpleMathOpTest(ANEURALNETWORKS_ADD, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, ADD_quant8_signed) {
    simpleMathOpTest(ANEURALNETWORKS_ADD, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, ADD_int32) {
    simpleMathOpTest(ANEURALNETWORKS_ADD, ANEURALNETWORKS_TENSOR_INT32);
}

TEST(OperationValidationTest, MUL_float16) {
    simpleMathOpTest(ANEURALNETWORKS_MUL, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, MUL_float32) {
    simpleMathOpTest(ANEURALNETWORKS_MUL, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, MUL_quant8) {
    simpleMathOpTest(ANEURALNETWORKS_MUL, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, MUL_quant8_signed) {
    simpleMathOpTest(ANEURALNETWORKS_MUL, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, MUL_int32) {
    simpleMathOpTest(ANEURALNETWORKS_MUL, ANEURALNETWORKS_TENSOR_INT32);
}

TEST(OperationValidationTest, SUB_float16) {
    simpleMathOpTest(ANEURALNETWORKS_SUB, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, SUB_float32) {
    simpleMathOpTest(ANEURALNETWORKS_SUB, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, SUB_quant8) {
    simpleMathOpTest(ANEURALNETWORKS_SUB, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, SUB_quant8_signed) {
    simpleMathOpTest(ANEURALNETWORKS_SUB, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, SUB_int32) {
    simpleMathOpTest(ANEURALNETWORKS_SUB, ANEURALNETWORKS_TENSOR_INT32);
}

TEST(OperationValidationTest, DIV_float16) {
    simpleMathOpTest(ANEURALNETWORKS_DIV, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, DIV_float32) {
    simpleMathOpTest(ANEURALNETWORKS_DIV, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, DIV_int32) {
    simpleMathOpTest(ANEURALNETWORKS_DIV, ANEURALNETWORKS_TENSOR_INT32);
}

TEST(OperationValidationTest, MUL_quant8_bad_output_scale) {
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input1 =
            getOpType(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, 4, inputDimensions);
    ANeuralNetworksOperandType input2 = input1;
    ANeuralNetworksOperandType output = input1;
    input1.scale = 1.0f;
    input2.scale = 1.0f;
    output.scale = 0.5f;
    ANeuralNetworksOperandType activation = {.type = ANEURALNETWORKS_INT32,
                                             .dimensionCount = 0,
                                             .dimensions = nullptr,
                                             .scale = 0.0f,
                                             .zeroPoint = 0};

    OperationTestBase mulTest(ANEURALNETWORKS_MUL, {input1, input2, activation}, {output});
    mulTest.testFailure(ANEURALNETWORKS_BAD_DATA);
}

void binaryOpTest(ANeuralNetworksOperationType operationCode, int32_t operandCode) {
    uint32_t inputDimensions[] = {2, 2, 2, 2, 2};
    ANeuralNetworksOperandType input1 = getOpType(operandCode, 5, inputDimensions);

    ANeuralNetworksOperandType input2 = input1;
    ANeuralNetworksOperandType output = input1;

    OperationTestBase test(operationCode, {input1, input2}, {output});
    test.testOpsValidations();
}

TEST(OperationValidationTest, MAXIMUM_float16) {
    binaryOpTest(ANEURALNETWORKS_MAXIMUM, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, MAXIMUM_float32) {
    binaryOpTest(ANEURALNETWORKS_MAXIMUM, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, MAXIMUM_int32) {
    binaryOpTest(ANEURALNETWORKS_MAXIMUM, ANEURALNETWORKS_TENSOR_INT32);
}

TEST(OperationValidationTest, MAXIMUM_quant8) {
    binaryOpTest(ANEURALNETWORKS_MAXIMUM, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, MAXIMUM_quant8signed) {
    binaryOpTest(ANEURALNETWORKS_MAXIMUM, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, MINIMUM_float16) {
    binaryOpTest(ANEURALNETWORKS_MINIMUM, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, MINIMUM_float32) {
    binaryOpTest(ANEURALNETWORKS_MINIMUM, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, MINIMUM_int32) {
    binaryOpTest(ANEURALNETWORKS_MINIMUM, ANEURALNETWORKS_TENSOR_INT32);
}

TEST(OperationValidationTest, MINIMUM_quant8) {
    binaryOpTest(ANEURALNETWORKS_MINIMUM, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, MINIMUM_quant8signed) {
    binaryOpTest(ANEURALNETWORKS_MINIMUM, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void activationOpTest(ANeuralNetworksOperationType operationCode, int32_t operandCode) {
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input = getOpType(operandCode, 4, inputDimensions);

    ANeuralNetworksOperandType output = input;
    std::vector<TensorRankMutator> inputRankMutators;
    if (operationCode == ANEURALNETWORKS_FLOOR || operationCode == ANEURALNETWORKS_LOGISTIC ||
        operationCode == ANEURALNETWORKS_RELU || operationCode == ANEURALNETWORKS_RELU1 ||
        operationCode == ANEURALNETWORKS_RELU6 || operationCode == ANEURALNETWORKS_TANH) {
        inputRankMutators.push_back({TensorRankConstraint::UpTo(4)});
    }
    OperationTestBase test(operationCode, {input}, {output}, inputRankMutators);
    test.testOpsValidations();
}

TEST(OperationValidationTest, ABS_float16) {
    activationOpTest(ANEURALNETWORKS_ABS, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, ABS_float32) {
    activationOpTest(ANEURALNETWORKS_ABS, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, ABS_int32) {
    activationOpTest(ANEURALNETWORKS_ABS, ANEURALNETWORKS_TENSOR_INT32);
}

TEST(OperationValidationTest, EXP_float16) {
    activationOpTest(ANEURALNETWORKS_EXP, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, EXP_float32) {
    activationOpTest(ANEURALNETWORKS_EXP, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, LOG_float16) {
    activationOpTest(ANEURALNETWORKS_LOG, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, LOG_float32) {
    activationOpTest(ANEURALNETWORKS_LOG, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, RSQRT_float16) {
    activationOpTest(ANEURALNETWORKS_RSQRT, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, RSQRT_float32) {
    activationOpTest(ANEURALNETWORKS_RSQRT, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, SIN_float16) {
    activationOpTest(ANEURALNETWORKS_SIN, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, SIN_float32) {
    activationOpTest(ANEURALNETWORKS_SIN, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, SQRT_float16) {
    activationOpTest(ANEURALNETWORKS_SQRT, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, SQRT_float32) {
    activationOpTest(ANEURALNETWORKS_SQRT, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, NEG_float16) {
    activationOpTest(ANEURALNETWORKS_NEG, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, NEG_float32) {
    activationOpTest(ANEURALNETWORKS_NEG, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, NEG_int32) {
    activationOpTest(ANEURALNETWORKS_NEG, ANEURALNETWORKS_TENSOR_INT32);
}

TEST(OperationValidationTest, FLOOR_float16) {
    activationOpTest(ANEURALNETWORKS_FLOOR, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, FLOOR_float32) {
    activationOpTest(ANEURALNETWORKS_FLOOR, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, LOGICAL_NOT_bool) {
    activationOpTest(ANEURALNETWORKS_LOGICAL_NOT, ANEURALNETWORKS_TENSOR_BOOL8);
}

TEST(OperationValidationTest, TANH_float16) {
    activationOpTest(ANEURALNETWORKS_TANH, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, TANH_float32) {
    activationOpTest(ANEURALNETWORKS_TANH, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, TANH_quant8) {
    activationOpTest(ANEURALNETWORKS_TANH, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, TANH_quant8_signed) {
    activationOpTest(ANEURALNETWORKS_TANH, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, RELU_float16) {
    activationOpTest(ANEURALNETWORKS_RELU, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, RELU1_float16) {
    activationOpTest(ANEURALNETWORKS_RELU, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, RELU6_float16) {
    activationOpTest(ANEURALNETWORKS_RELU, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, RELU_float32) {
    activationOpTest(ANEURALNETWORKS_RELU, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, RELU1_float32) {
    activationOpTest(ANEURALNETWORKS_RELU, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, RELU6_float32) {
    activationOpTest(ANEURALNETWORKS_RELU, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, RELU_quant8) {
    activationOpTest(ANEURALNETWORKS_RELU, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, RELU1_quant8) {
    activationOpTest(ANEURALNETWORKS_RELU, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, RELU6_quant8) {
    activationOpTest(ANEURALNETWORKS_RELU, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, RELU_quant8_signed) {
    activationOpTest(ANEURALNETWORKS_RELU, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, RELU1_quant8_signed) {
    activationOpTest(ANEURALNETWORKS_RELU, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, RELU6_quant8_signed) {
    activationOpTest(ANEURALNETWORKS_RELU, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, LOGISTIC_float16) {
    activationOpTest(ANEURALNETWORKS_LOGISTIC, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, LOGISTIC_float32) {
    activationOpTest(ANEURALNETWORKS_LOGISTIC, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, LOGISTIC_quant8) {
    activationOpTest(ANEURALNETWORKS_LOGISTIC, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, LOGISTIC_quant8_signed) {
    activationOpTest(ANEURALNETWORKS_LOGISTIC, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, HARD_SWISH_float16) {
    activationOpTest(ANEURALNETWORKS_HARD_SWISH, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, HARD_SWISH_float32) {
    activationOpTest(ANEURALNETWORKS_HARD_SWISH, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, HARD_SWISH_quant8) {
    activationOpTest(ANEURALNETWORKS_HARD_SWISH, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, HARD_SWISH_quant8_signed) {
    activationOpTest(ANEURALNETWORKS_HARD_SWISH, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void eluOpTest(int32_t operandCode) {
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input = getOpType(operandCode, 4, inputDimensions);
    ANeuralNetworksOperandType alpha = (operandCode == ANEURALNETWORKS_TENSOR_FLOAT32)
                                               ? getOpType(ANEURALNETWORKS_FLOAT32)
                                               : getOpType(ANEURALNETWORKS_FLOAT16);

    ANeuralNetworksOperandType output = input;
    OperationTestBase test(ANEURALNETWORKS_ELU, {input, alpha}, {output});
    test.testOpsValidations();
}

TEST(OperationValidationTest, ELU_float16) {
    eluOpTest(ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, ELU_float32) {
    eluOpTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

void reshapeOpTest(int32_t inputOperandCode) {
    SCOPED_TRACE(inputOperandCode);
    uint32_t inputDimensions[3] = {2, 3, 4};
    ANeuralNetworksOperandType input = getOpType(inputOperandCode, 3, inputDimensions);
    uint32_t shapeDims[1] = {2};
    ANeuralNetworksOperandType shape = getOpType(ANEURALNETWORKS_TENSOR_INT32, 1, shapeDims);
    uint32_t outputDimensions[2] = {4, 6};
    ANeuralNetworksOperandType output = getOpType(inputOperandCode, 2, outputDimensions);
    OperationTestBase test(ANEURALNETWORKS_RESHAPE, {input, shape}, {output},
                           {{TensorRankConstraint::UpTo(4)}});
    test.testOpsValidations();
}

TEST(OperationValidationTest, RESHAPE) {
    reshapeOpTest(ANEURALNETWORKS_TENSOR_FLOAT16);
    reshapeOpTest(ANEURALNETWORKS_TENSOR_FLOAT32);
    reshapeOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    reshapeOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void logSoftmaxOpTest(int32_t inputOperandCode) {
    uint32_t inputDimensions[3] = {2, 2, 2};
    ANeuralNetworksOperandType input = {.type = inputOperandCode,
                                        .dimensionCount = 3,
                                        .dimensions = inputDimensions,
                                        .scale = 0.0f,
                                        .zeroPoint = 0};
    ANeuralNetworksOperandType beta = {.type = (inputOperandCode == ANEURALNETWORKS_TENSOR_FLOAT32)
                                                       ? ANEURALNETWORKS_FLOAT32
                                                       : ANEURALNETWORKS_FLOAT16,
                                       .dimensionCount = 0,
                                       .dimensions = nullptr,
                                       .scale = 0.0f,
                                       .zeroPoint = 0};
    ANeuralNetworksOperandType axis = {.type = ANEURALNETWORKS_INT32,
                                       .dimensionCount = 0,
                                       .dimensions = nullptr,
                                       .scale = 0.0f,
                                       .zeroPoint = 0};

    ANeuralNetworksOperandType output = {.type = inputOperandCode,
                                         .dimensionCount = 3,
                                         .dimensions = inputDimensions,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};

    OperationTestBase test(ANEURALNETWORKS_LOG_SOFTMAX, {input, beta, axis}, {output});
    test.testOpsValidations();
}

TEST(OperationValidationTest, LOG_SOFTMAX_float16) {
    logSoftmaxOpTest(ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, LOG_SOFTMAX_float32) {
    logSoftmaxOpTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

void meanOpTest(int32_t inputOperandCode) {
    uint32_t inputDimensions[3] = {2, 2, 2};
    ANeuralNetworksOperandType input = getOpType(inputOperandCode, 3, inputDimensions);
    ANeuralNetworksOperandType dims = getOpType(ANEURALNETWORKS_TENSOR_INT32, 1, inputDimensions);
    ANeuralNetworksOperandType keepDims = getOpType(ANEURALNETWORKS_INT32);
    ANeuralNetworksOperandType output = getOpType(inputOperandCode, 3, inputDimensions);

    OperationTestBase test(ANEURALNETWORKS_MEAN, {input, dims, keepDims}, {output},
                           {{TensorRankConstraint::UpTo(4)}});
    test.testOpsValidations();
}

TEST(OperationValidationTest, MEAN_float16) {
    meanOpTest(ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, MEAN_float32) {
    meanOpTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, MEAN_quant8) {
    meanOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, MEAN_quant8_signed) {
    meanOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void padOpTest(int32_t inputOperandCode) {
    SCOPED_TRACE(inputOperandCode);
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input = getOpType(inputOperandCode, 4, inputDimensions);
    uint32_t padSizeDimensions[1] = {4};
    ANeuralNetworksOperandType padSize =
            getOpType(ANEURALNETWORKS_TENSOR_INT32, 1, padSizeDimensions);
    uint32_t outputDimensions[4] = {4, 3, 4, 3};
    ANeuralNetworksOperandType output = getOpType(inputOperandCode, 4, outputDimensions);
    OperationTestBase test(ANEURALNETWORKS_PAD, {input, padSize}, {output},
                           {{TensorRankConstraint::UpTo(4)}});
    test.testOpsValidations();
}

TEST(OperationValidationTest, PAD) {
    padOpTest(ANEURALNETWORKS_TENSOR_FLOAT16);
    padOpTest(ANEURALNETWORKS_TENSOR_FLOAT32);
    padOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    padOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void padV2OpTest(int32_t inputOperandCode) {
    SCOPED_TRACE(inputOperandCode);
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input = getOpType(inputOperandCode, 4, inputDimensions);
    uint32_t padSizeDimensions[1] = {4};
    ANeuralNetworksOperandType padSize =
            getOpType(ANEURALNETWORKS_TENSOR_INT32, 1, padSizeDimensions);
    ANeuralNetworksOperandType padValue = getOpType(ANEURALNETWORKS_FLOAT32);
    if (inputOperandCode == ANEURALNETWORKS_TENSOR_FLOAT16) {
        padValue = getOpType(ANEURALNETWORKS_FLOAT16);
    } else if (inputOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM ||
               inputOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED) {
        padValue = getOpType(ANEURALNETWORKS_INT32);
    }
    uint32_t outputDimensions[4] = {4, 3, 4, 3};
    ANeuralNetworksOperandType output = getOpType(inputOperandCode, 4, outputDimensions);
    OperationTestBase test(ANEURALNETWORKS_PAD_V2, {input, padSize, padValue}, {output},
                           {{TensorRankConstraint::UpTo(4)}});
    test.testOpsValidations();
}

TEST(OperationValidationTest, PAD_V2) {
    padV2OpTest(ANEURALNETWORKS_TENSOR_FLOAT16);
    padV2OpTest(ANEURALNETWORKS_TENSOR_FLOAT32);
    padV2OpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    padV2OpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void softmaxOpTest(int32_t operandCode) {
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input = getOpType(operandCode, 4, inputDimensions);

    ANeuralNetworksOperandType output = input;
    ANeuralNetworksOperandType beta = getOpType(ANEURALNETWORKS_FLOAT32);
    if (operandCode == ANEURALNETWORKS_TENSOR_FLOAT16) {
        beta = getOpType(ANEURALNETWORKS_FLOAT16);
    }

    OperationTestBase softmaxTest(ANEURALNETWORKS_SOFTMAX, {input, beta}, {output},
                                  {{TensorRankConstraint::UpTo(4)}});
    softmaxTest.testOpsValidations();

    ANeuralNetworksOperandType axis = getOpType(ANEURALNETWORKS_INT32);
    OperationTestBase softmaxAxisTest(ANEURALNETWORKS_SOFTMAX, {input, beta, axis}, {output},
                                      {{TensorRankConstraint::UpTo(4)}});
    softmaxAxisTest.testOpsValidations();
}

TEST(OperationValidationTest, SOFTMAX_float16) {
    softmaxOpTest(ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, SOFTMAX_float32) {
    softmaxOpTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, SOFTMAX_quant8) {
    softmaxOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, SOFTMAX_quant8_signed) {
    softmaxOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void poolingOpTest(ANeuralNetworksOperationType operationCode, int32_t operandCode) {
    uint32_t inputDimensions[4] = {2, 4, 4, 2};
    ANeuralNetworksOperandType input = getOpType(operandCode, 4, inputDimensions);
    ANeuralNetworksOperandType output = input;

    ANeuralNetworksOperandType scalar = {.type = ANEURALNETWORKS_INT32,
                                         .dimensionCount = 0,
                                         .dimensions = nullptr,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};
    ANeuralNetworksOperandType padLeft = scalar;
    ANeuralNetworksOperandType padRight = scalar;
    ANeuralNetworksOperandType padTop = scalar;
    ANeuralNetworksOperandType padBottom = scalar;
    ANeuralNetworksOperandType strideWidth = scalar;
    ANeuralNetworksOperandType strideHeight = scalar;
    ANeuralNetworksOperandType filterWidth = scalar;
    ANeuralNetworksOperandType filterHeight = scalar;
    ANeuralNetworksOperandType activation = scalar;

    OperationTestBase explicitPoolingTest(operationCode,
                                          {input, padLeft, padRight, padTop, padBottom, strideWidth,
                                           strideHeight, filterWidth, filterHeight, activation},
                                          {output});
    explicitPoolingTest.testOpsValidations();

    ANeuralNetworksOperandType padImplicit = scalar;
    OperationTestBase implicitPoolingTest(
            operationCode,
            {input, padImplicit, strideWidth, strideHeight, filterWidth, filterHeight, activation},
            {output});
    implicitPoolingTest.testOpsValidations();

    ANeuralNetworksOperandType layout = {.type = ANEURALNETWORKS_BOOL,
                                         .dimensionCount = 0,
                                         .dimensions = nullptr,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};

    OperationTestBase explicitNchwPoolingTest(
            operationCode,
            {input, padLeft, padRight, padTop, padBottom, strideWidth, strideHeight, filterWidth,
             filterHeight, activation, layout},
            {output});
    explicitNchwPoolingTest.testOpsValidations();

    OperationTestBase implicitNchwPoolingTest(operationCode,
                                              {input, padImplicit, strideWidth, strideHeight,
                                               filterWidth, filterHeight, activation, layout},
                                              {output});
    implicitNchwPoolingTest.testOpsValidations();
}

TEST(OperationValidationTest, AVERAGE_POOL_2D_float16) {
    poolingOpTest(ANEURALNETWORKS_AVERAGE_POOL_2D, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, AVERAGE_POOL_2D_float32) {
    poolingOpTest(ANEURALNETWORKS_AVERAGE_POOL_2D, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, AVERAGE_POOL_2D_quant8) {
    poolingOpTest(ANEURALNETWORKS_AVERAGE_POOL_2D, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, AVERAGE_POOL_2D_quant8_signed) {
    poolingOpTest(ANEURALNETWORKS_AVERAGE_POOL_2D, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, MAX_POOL_2D_float32) {
    poolingOpTest(ANEURALNETWORKS_MAX_POOL_2D, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, MAX_POOL_2D_float16) {
    poolingOpTest(ANEURALNETWORKS_MAX_POOL_2D, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, MAX_POOL_2D_quant8) {
    poolingOpTest(ANEURALNETWORKS_MAX_POOL_2D, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, MAX_POOL_2D_quant8_signed) {
    poolingOpTest(ANEURALNETWORKS_MAX_POOL_2D, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, L2_POOL_2D_float16) {
    poolingOpTest(ANEURALNETWORKS_L2_POOL_2D, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, L2_POOL_2D_float32) {
    poolingOpTest(ANEURALNETWORKS_L2_POOL_2D, ANEURALNETWORKS_TENSOR_FLOAT32);
}

void spaceDepthOpTest(ANeuralNetworksOperationType operationCode, int32_t operandCode) {
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input = getOpType(operandCode, 4, inputDimensions);

    ANeuralNetworksOperandType block_size = {.type = ANEURALNETWORKS_INT32,
                                             .dimensionCount = 0,
                                             .dimensions = nullptr,
                                             .scale = 0.0f,
                                             .zeroPoint = 0};
    ANeuralNetworksOperandType output = input;

    OperationTestBase spaceDepthTest(operationCode, {input, block_size}, {output});
    spaceDepthTest.testOpsValidations();

    ANeuralNetworksOperandType layout = {.type = ANEURALNETWORKS_BOOL,
                                         .dimensionCount = 0,
                                         .dimensions = nullptr,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};
    OperationTestBase spaceDepthNchwTest(operationCode, {input, block_size, layout}, {output});
    spaceDepthNchwTest.testOpsValidations();
}

TEST(OperationValidationTest, SPACE_TO_DEPTH_float16) {
    spaceDepthOpTest(ANEURALNETWORKS_SPACE_TO_DEPTH, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, DEPTH_TO_SPACE_float16) {
    spaceDepthOpTest(ANEURALNETWORKS_DEPTH_TO_SPACE, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, SPACE_TO_DEPTH_float32) {
    spaceDepthOpTest(ANEURALNETWORKS_SPACE_TO_DEPTH, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, DEPTH_TO_SPACE_float32) {
    spaceDepthOpTest(ANEURALNETWORKS_DEPTH_TO_SPACE, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, SPACE_TO_DEPTH_quant8) {
    spaceDepthOpTest(ANEURALNETWORKS_SPACE_TO_DEPTH, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, DEPTH_TO_SPACE_quant8) {
    spaceDepthOpTest(ANEURALNETWORKS_DEPTH_TO_SPACE, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, SPACE_TO_DEPTH_quant8signed) {
    spaceDepthOpTest(ANEURALNETWORKS_SPACE_TO_DEPTH, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, DEPTH_TO_SPACE_quant8signed) {
    spaceDepthOpTest(ANEURALNETWORKS_DEPTH_TO_SPACE, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void spaceBatchOpTest(ANeuralNetworksOperationType operationCode, int32_t operandCode) {
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input = getOpType(operandCode, 4, inputDimensions);

    uint32_t blockDimensions[1] = {2};
    ANeuralNetworksOperandType blockShape = {.type = ANEURALNETWORKS_TENSOR_INT32,
                                             .dimensionCount = 1,
                                             .dimensions = blockDimensions,
                                             .scale = 0.0f,
                                             .zeroPoint = 0};
    ANeuralNetworksOperandType layout = {.type = ANEURALNETWORKS_BOOL,
                                         .dimensionCount = 0,
                                         .dimensions = nullptr,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};

    ANeuralNetworksOperandType padding = blockShape;
    ANeuralNetworksOperandType output = input;
    if (operationCode == ANEURALNETWORKS_SPACE_TO_BATCH_ND) {
        OperationTestBase spaceBatchTest(operationCode, {input, blockShape, padding}, {output});
        spaceBatchTest.testOpsValidations();

        OperationTestBase spaceBatchNchwTest(operationCode, {input, blockShape, padding, layout},
                                             {output});
        spaceBatchNchwTest.testOpsValidations();
    } else {
        OperationTestBase spaceBatchTest(operationCode, {input, blockShape}, {output});
        spaceBatchTest.testOpsValidations();

        OperationTestBase spaceBatchNchwTest(operationCode, {input, blockShape, layout}, {output});
        spaceBatchNchwTest.testOpsValidations();
    }
}

TEST(OperationValidationTest, SPACE_TO_BATCH_ND_float16) {
    spaceBatchOpTest(ANEURALNETWORKS_SPACE_TO_BATCH_ND, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, BATCH_TO_SPACE_ND_float16) {
    spaceBatchOpTest(ANEURALNETWORKS_BATCH_TO_SPACE_ND, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, SPACE_TO_BATCH_ND_float32) {
    spaceBatchOpTest(ANEURALNETWORKS_SPACE_TO_BATCH_ND, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, BATCH_TO_SPACE_ND_float32) {
    spaceBatchOpTest(ANEURALNETWORKS_BATCH_TO_SPACE_ND, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, SPACE_TO_BATCH_ND_quant8) {
    spaceBatchOpTest(ANEURALNETWORKS_SPACE_TO_BATCH_ND, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, BATCH_TO_SPACE_ND_quant8) {
    spaceBatchOpTest(ANEURALNETWORKS_BATCH_TO_SPACE_ND, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, SPACE_TO_BATCH_ND_quant8signed) {
    spaceBatchOpTest(ANEURALNETWORKS_SPACE_TO_BATCH_ND, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, BATCH_TO_SPACE_ND_quant8signed) {
    spaceBatchOpTest(ANEURALNETWORKS_BATCH_TO_SPACE_ND, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void transposeAndSqueezeOpTest(ANeuralNetworksOperationType operationCode, int32_t operandCode) {
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input = getOpType(operandCode, 4, inputDimensions);

    uint32_t blockDimensions[1] = {4};
    ANeuralNetworksOperandType dims = {.type = ANEURALNETWORKS_TENSOR_INT32,
                                       .dimensionCount = 1,
                                       .dimensions = blockDimensions,
                                       .scale = 0.0f,
                                       .zeroPoint = 0};

    ANeuralNetworksOperandType output = input;
    OperationTestBase transposeAndSqueezeTest(operationCode, {input, dims}, {output},
                                              {{TensorRankConstraint::UpTo(4)}});
    transposeAndSqueezeTest.testOpsValidations();
}

TEST(OperationValidationTest, TRANSPOSE_float16) {
    transposeAndSqueezeOpTest(ANEURALNETWORKS_TRANSPOSE, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, SQUEEZE_float16) {
    transposeAndSqueezeOpTest(ANEURALNETWORKS_SQUEEZE, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, TRANSPOSE_float32) {
    transposeAndSqueezeOpTest(ANEURALNETWORKS_TRANSPOSE, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, SQUEEZE_float32) {
    transposeAndSqueezeOpTest(ANEURALNETWORKS_SQUEEZE, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, TRANSPOSE_quant8) {
    transposeAndSqueezeOpTest(ANEURALNETWORKS_TRANSPOSE, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, TRANSPOSE_quant8signed) {
    transposeAndSqueezeOpTest(ANEURALNETWORKS_TRANSPOSE,
                              ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, SQUEEZE_quant8) {
    transposeAndSqueezeOpTest(ANEURALNETWORKS_SQUEEZE, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, SQUEEZE_quant8_signed) {
    transposeAndSqueezeOpTest(ANEURALNETWORKS_SQUEEZE, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void convOpTest(int32_t inputOperandCode, int32_t filterOperandCode) {
    uint32_t inputDimensions[4] = {2, 4, 4, 2};
    ANeuralNetworksOperandType input = getOpType(inputOperandCode, 4, inputDimensions);
    ANeuralNetworksOperandType output = input;

    float filterScales[2] = {0.5f, 1.0f};
    ANeuralNetworksOperandType filter = getOpType(filterOperandCode, 4, inputDimensions);
    ANeuralNetworksSymmPerChannelQuantParams filterChannelQuantParams = {
            .channelDim = 0,
            .scaleCount = 2,
            .scales = filterScales,
    };

    uint32_t biasDimensions[1] = {2};
    ANeuralNetworksOperandType bias = {.type = inputOperandCode,
                                       .dimensionCount = 1,
                                       .dimensions = biasDimensions,
                                       .scale = 0.0f,
                                       .zeroPoint = 0};
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM) {
        bias.type = ANEURALNETWORKS_TENSOR_INT32;
        bias.scale = 0.25f;
    }
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED) {
        bias.type = ANEURALNETWORKS_TENSOR_INT32;
        bias.scale = 0.25f;
    }
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        bias.type = ANEURALNETWORKS_TENSOR_INT32;
        bias.scale = 0.0f;
    }

    ANeuralNetworksOperandType scalar = {.type = ANEURALNETWORKS_INT32,
                                         .dimensionCount = 0,
                                         .dimensions = nullptr,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};
    ANeuralNetworksOperandType padLeft = scalar;
    ANeuralNetworksOperandType padRight = scalar;
    ANeuralNetworksOperandType padTop = scalar;
    ANeuralNetworksOperandType padBottom = scalar;
    ANeuralNetworksOperandType strideWidth = scalar;
    ANeuralNetworksOperandType strideHeight = scalar;
    ANeuralNetworksOperandType dilationHeightFactor = scalar;
    ANeuralNetworksOperandType dilationWidthFactor = scalar;
    ANeuralNetworksOperandType activation = scalar;

    OperationTestBase explicitConvTest(ANEURALNETWORKS_CONV_2D,
                                       {input, filter, bias, padLeft, padRight, padTop, padBottom,
                                        strideWidth, strideHeight, activation},
                                       {output});
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        explicitConvTest.setInputSymmPerChannelQuantParams(1, filterChannelQuantParams);
    }
    explicitConvTest.testOpsValidations();

    ANeuralNetworksOperandType padImplicit = scalar;
    OperationTestBase implicitConvTest(
            ANEURALNETWORKS_CONV_2D,
            {input, filter, bias, padImplicit, strideWidth, strideHeight, activation}, {output},
            {{TensorRankConstraint::Exactly(4), {0, 1}}});
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        implicitConvTest.setInputSymmPerChannelQuantParams(1, filterChannelQuantParams);
    }
    implicitConvTest.testOpsValidations();

    ANeuralNetworksOperandType layout = {.type = ANEURALNETWORKS_BOOL,
                                         .dimensionCount = 0,
                                         .dimensions = nullptr,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};

    OperationTestBase explicitNchwConvTest(
            ANEURALNETWORKS_CONV_2D,
            {input, filter, bias, padLeft, padRight, padTop, padBottom, strideWidth, strideHeight,
             activation, layout},
            {output});
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        explicitNchwConvTest.setInputSymmPerChannelQuantParams(1, filterChannelQuantParams);
    }
    explicitNchwConvTest.testOpsValidations();

    OperationTestBase implicitNchwConvTest(
            ANEURALNETWORKS_CONV_2D,
            {input, filter, bias, padImplicit, strideWidth, strideHeight, activation, layout},
            {output});
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        implicitNchwConvTest.setInputSymmPerChannelQuantParams(1, filterChannelQuantParams);
    }
    implicitNchwConvTest.testOpsValidations();

    OperationTestBase explicitDilateConvTest(
            ANEURALNETWORKS_CONV_2D,
            {input, filter, bias, padLeft, padRight, padTop, padBottom, strideWidth, strideHeight,
             activation, layout, dilationWidthFactor, dilationHeightFactor},
            {output});
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        explicitDilateConvTest.setInputSymmPerChannelQuantParams(1, filterChannelQuantParams);
    }
    explicitDilateConvTest.testOpsValidations();

    OperationTestBase implicitDilateConvTest(
            ANEURALNETWORKS_CONV_2D,
            {input, filter, bias, padImplicit, strideWidth, strideHeight, activation, layout,
             dilationWidthFactor, dilationHeightFactor},
            {output});
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        implicitDilateConvTest.setInputSymmPerChannelQuantParams(1, filterChannelQuantParams);
    }
    implicitDilateConvTest.testOpsValidations();
}

TEST(OperationValidationTest, CONV_2D_float16) {
    convOpTest(ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, CONV_2D_float32) {
    convOpTest(ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, CONV_2D_quant8) {
    convOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, CONV_2D_quant8_per_channel) {
    convOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL);
}

TEST(OperationValidationTest, CONV_2D_quant8_signed) {
    convOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED,
               ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, CONV_2D_quant8_signed_per_channel) {
    convOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED,
               ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL);
}

void depthwiseConvOpTest(int32_t inputOperandCode, int32_t filterOperandCode) {
    uint32_t inputDimensions[4] = {1, 2, 2, 2};
    ANeuralNetworksOperandType input = getOpType(inputOperandCode, 4, inputDimensions);
    ANeuralNetworksOperandType output = input;

    float filterScales[2] = {0.5f, 1.0f};
    ANeuralNetworksOperandType filter = getOpType(filterOperandCode, 4, inputDimensions);
    ANeuralNetworksSymmPerChannelQuantParams filterChannelQuantParams = {
            .channelDim = 3,
            .scaleCount = 2,
            .scales = filterScales,
    };

    uint32_t biasDimensions[1] = {2};
    ANeuralNetworksOperandType bias = {.type = inputOperandCode,
                                       .dimensionCount = 1,
                                       .dimensions = biasDimensions,
                                       .scale = 0.0f,
                                       .zeroPoint = 0};
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM ||
        filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED) {
        bias.type = ANEURALNETWORKS_TENSOR_INT32;
        bias.scale = 0.25f;
    }
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        bias.type = ANEURALNETWORKS_TENSOR_INT32;
        bias.scale = 0.0f;
    }

    ANeuralNetworksOperandType scalar = {.type = ANEURALNETWORKS_INT32,
                                         .dimensionCount = 0,
                                         .dimensions = nullptr,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};
    ANeuralNetworksOperandType padLeft = scalar;
    ANeuralNetworksOperandType padRight = scalar;
    ANeuralNetworksOperandType padTop = scalar;
    ANeuralNetworksOperandType padBottom = scalar;
    ANeuralNetworksOperandType strideWidth = scalar;
    ANeuralNetworksOperandType strideHeight = scalar;
    ANeuralNetworksOperandType multiplier = scalar;
    ANeuralNetworksOperandType activation = scalar;

    OperationTestBase explicitDepthwiseConvTest(
            ANEURALNETWORKS_DEPTHWISE_CONV_2D,
            {input, filter, bias, padLeft, padRight, padTop, padBottom, strideWidth, strideHeight,
             multiplier, activation},
            {output});
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        explicitDepthwiseConvTest.setInputSymmPerChannelQuantParams(1, filterChannelQuantParams);
    }
    explicitDepthwiseConvTest.testOpsValidations();

    ANeuralNetworksOperandType padImplicit = scalar;
    OperationTestBase implicitDepthwiseConvTest(
            ANEURALNETWORKS_DEPTHWISE_CONV_2D,
            {input, filter, bias, padImplicit, strideWidth, strideHeight, multiplier, activation},
            {output});
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        implicitDepthwiseConvTest.setInputSymmPerChannelQuantParams(1, filterChannelQuantParams);
    }
    implicitDepthwiseConvTest.testOpsValidations();

    ANeuralNetworksOperandType layout = {.type = ANEURALNETWORKS_BOOL,
                                         .dimensionCount = 0,
                                         .dimensions = nullptr,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};

    OperationTestBase explicitNchwDepthwiseConvTest(
            ANEURALNETWORKS_DEPTHWISE_CONV_2D,
            {input, filter, bias, padLeft, padRight, padTop, padBottom, strideWidth, strideHeight,
             multiplier, activation, layout},
            {output});
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        explicitNchwDepthwiseConvTest.setInputSymmPerChannelQuantParams(1,
                                                                        filterChannelQuantParams);
    }
    explicitNchwDepthwiseConvTest.testOpsValidations();

    OperationTestBase implicitNchwDepthwiseConvTest(ANEURALNETWORKS_DEPTHWISE_CONV_2D,
                                                    {input, filter, bias, padImplicit, strideWidth,
                                                     strideHeight, multiplier, activation, layout},
                                                    {output});
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        implicitNchwDepthwiseConvTest.setInputSymmPerChannelQuantParams(1,
                                                                        filterChannelQuantParams);
    }
    implicitNchwDepthwiseConvTest.testOpsValidations();

    ANeuralNetworksOperandType dilationHeightFactor = scalar;
    ANeuralNetworksOperandType dilationWidthFactor = scalar;

    OperationTestBase explicitDilationDepthwiseConvTest(
            ANEURALNETWORKS_DEPTHWISE_CONV_2D,
            {input, filter, bias, padLeft, padRight, padTop, padBottom, strideWidth, strideHeight,
             multiplier, activation, layout, dilationWidthFactor, dilationHeightFactor},
            {output});
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        explicitDilationDepthwiseConvTest.setInputSymmPerChannelQuantParams(
                1, filterChannelQuantParams);
    }
    explicitDilationDepthwiseConvTest.testOpsValidations();

    OperationTestBase implicitDilationDepthwiseConvTest(
            ANEURALNETWORKS_DEPTHWISE_CONV_2D,
            {input, filter, bias, padImplicit, strideWidth, strideHeight, multiplier, activation,
             layout, dilationWidthFactor, dilationHeightFactor},
            {output});
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        implicitDilationDepthwiseConvTest.setInputSymmPerChannelQuantParams(
                1, filterChannelQuantParams);
    }
    implicitDilationDepthwiseConvTest.testOpsValidations();
}

TEST(OperationValidationTest, DEPTHWISE_CONV_2D_float32) {
    depthwiseConvOpTest(ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, DEPTHWISE_CONV_2D_float16) {
    depthwiseConvOpTest(ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, DEPTHWISE_CONV_2D_quant8) {
    depthwiseConvOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, DEPTHWISE_CONV_2D_quant8_per_channel) {
    depthwiseConvOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM,
                        ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL);
}

TEST(OperationValidationTest, DEPTHWISE_CONV_2D_quant8_signed) {
    depthwiseConvOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED,
                        ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, DEPTHWISE_CONV_2D_quant8_signed_per_channel) {
    depthwiseConvOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED,
                        ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL);
}

void fullyConnectedOpTest(int32_t operandCode) {
    uint32_t inputDimensions[2] = {5, 5};
    ANeuralNetworksOperandType input = getOpType(operandCode, 2, inputDimensions);

    ANeuralNetworksOperandType weights = input;
    ANeuralNetworksOperandType output = input;

    uint32_t biasDimensions[1] = {5};
    ANeuralNetworksOperandType bias = {.type = operandCode,
                                       .dimensionCount = 1,
                                       .dimensions = biasDimensions,
                                       .scale = 0.0f,
                                       .zeroPoint = 0};
    if (operandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM ||
        operandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED) {
        bias.type = ANEURALNETWORKS_TENSOR_INT32;
        bias.scale = 0.25f;
    }

    ANeuralNetworksOperandType activation = {.type = ANEURALNETWORKS_INT32,
                                             .dimensionCount = 0,
                                             .dimensions = nullptr,
                                             .scale = 0.0f,
                                             .zeroPoint = 0};

    OperationTestBase fullyConnectedTest(ANEURALNETWORKS_FULLY_CONNECTED,
                                         {input, weights, bias, activation}, {output},
                                         {{TensorRankConstraint::Between(2, 4), {0}},
                                          {TensorRankConstraint::Exactly(2), {1}},
                                          {TensorRankConstraint::Exactly(1), {2}}});
    fullyConnectedTest.testOpsValidations();
}

TEST(OperationValidationTest, FULLY_CONNECTED_float16) {
    fullyConnectedOpTest(ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, FULLY_CONNECTED_float32) {
    fullyConnectedOpTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, FULLY_CONNECTED_quant8) {
    fullyConnectedOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, FULLY_CONNECTED_quant8_signed) {
    fullyConnectedOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void concatenationTest(int32_t operandCode) {
    uint32_t inputDimensions[2] = {5, 5};
    ANeuralNetworksOperandType input1 = getOpType(operandCode, 2, inputDimensions);
    ANeuralNetworksOperandType input2 = input1;
    ANeuralNetworksOperandType output = input1;

    ANeuralNetworksOperandType activation = {.type = ANEURALNETWORKS_INT32,
                                             .dimensionCount = 0,
                                             .dimensions = nullptr,
                                             .scale = 0.0f,
                                             .zeroPoint = 0};

    OperationTestBase concat2Test(ANEURALNETWORKS_CONCATENATION, {input1, input2, activation},
                                  {output}, {{TensorRankConstraint::UpTo(4), {0, 1}}});
    concat2Test.testOpsValidations();

    OperationTestBase concat1Test(ANEURALNETWORKS_CONCATENATION, {input1, activation}, {output},
                                  {{TensorRankConstraint::UpTo(4)}});
    concat1Test.testOpsValidations();
}

TEST(OperationValidationTest, CONCATENATION_float16) {
    concatenationTest(ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, CONCATENATION_float32) {
    concatenationTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, CONCATENATION_quant8) {
    concatenationTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, CONCATENATION_quant8_signed) {
    concatenationTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void resizeBilinearOpTest(int32_t inputOperandCode, int32_t scalarOperandCode) {
    SCOPED_TRACE(inputOperandCode);
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input = getOpType(inputOperandCode, 4, inputDimensions);
    ANeuralNetworksOperandType height = getOpType(scalarOperandCode);
    ANeuralNetworksOperandType width = height;
    ANeuralNetworksOperandType output = input;

    OperationTestBase resizeTest(ANEURALNETWORKS_RESIZE_BILINEAR, {input, height, width}, {output});
    resizeTest.testOpsValidations();

    ANeuralNetworksOperandType layout = getOpType(ANEURALNETWORKS_BOOL);
    OperationTestBase resizeNchwTest(ANEURALNETWORKS_RESIZE_BILINEAR,
                                     {input, height, width, layout}, {output});
    resizeNchwTest.testOpsValidations();
}

TEST(OperationValidationTest, RESIZE_BILINEAR) {
    resizeBilinearOpTest(ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_INT32);
    resizeBilinearOpTest(ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_INT32);
    resizeBilinearOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, ANEURALNETWORKS_INT32);
    resizeBilinearOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED, ANEURALNETWORKS_INT32);
    resizeBilinearOpTest(ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_FLOAT16);
    resizeBilinearOpTest(ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_FLOAT32);
    resizeBilinearOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, ANEURALNETWORKS_FLOAT32);
    resizeBilinearOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED, ANEURALNETWORKS_FLOAT32);
}

void embeddingLookupTest(int32_t operandCode) {
    uint32_t lookupDimensions[1] = {5};
    ANeuralNetworksOperandType lookup = {.type = ANEURALNETWORKS_TENSOR_INT32,
                                         .dimensionCount = 1,
                                         .dimensions = lookupDimensions,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};

    uint32_t inputDimensions[2] = {5, 5};
    ANeuralNetworksOperandType input = getOpType(operandCode, 2, inputDimensions);
    ANeuralNetworksOperandType output = input;

    OperationTestBase embedLookupTest(ANEURALNETWORKS_EMBEDDING_LOOKUP, {lookup, input}, {output});
    embedLookupTest.testOpsValidations();
}

TEST(OperationValidationTest, EMBEDDING_LOOKUP_float32) {
    embeddingLookupTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, EMBEDDING_LOOKUP_int32) {
    embeddingLookupTest(ANEURALNETWORKS_TENSOR_INT32);
}

TEST(OperationValidationTest, EMBEDDING_LOOKUP_quant8) {
    embeddingLookupTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, EMBEDDING_LOOKUP_quant8_signed) {
    embeddingLookupTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void hashtableLookupTest(int32_t operandCode) {
    uint32_t lookupDimensions[1] = {5};
    ANeuralNetworksOperandType lookup = {.type = ANEURALNETWORKS_TENSOR_INT32,
                                         .dimensionCount = 1,
                                         .dimensions = lookupDimensions,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};
    ANeuralNetworksOperandType keys = lookup;

    uint32_t valuesDimensions[2] = {5, 5};
    ANeuralNetworksOperandType values = getOpType(operandCode, 2, valuesDimensions);
    ANeuralNetworksOperandType output = values;

    ANeuralNetworksOperandType hits = lookup;
    hits.type = ANEURALNETWORKS_TENSOR_QUANT8_ASYMM;
    hits.scale = 1.0f;

    OperationTestBase hashLookupTest(ANEURALNETWORKS_HASHTABLE_LOOKUP, {lookup, keys, values},
                                     {output, hits});
    hashLookupTest.testOpsValidations();
}

TEST(OperationValidationTest, HASHTABLE_LOOKUP_float32) {
    hashtableLookupTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, HASHTABLE_LOOKUP_int32) {
    hashtableLookupTest(ANEURALNETWORKS_TENSOR_INT32);
}

TEST(OperationValidationTest, HASHTABLE_LOOKUP_quant8) {
    hashtableLookupTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

void lshProjectionTest(int32_t operandCode, int32_t hashAndWeightOperandCode) {
    uint32_t inputDimensions[2] = {5, 5};
    ANeuralNetworksOperandType hash = getOpType(hashAndWeightOperandCode, 2, inputDimensions);
    ANeuralNetworksOperandType input = getOpType(operandCode, 2, inputDimensions);

    uint32_t weightDimensions[1] = {5};
    ANeuralNetworksOperandType weight = getOpType(hashAndWeightOperandCode, 1, weightDimensions);

    ANeuralNetworksOperandType type = {.type = ANEURALNETWORKS_INT32,
                                       .dimensionCount = 0,
                                       .dimensions = nullptr,
                                       .scale = 0.0f,
                                       .zeroPoint = 0};

    ANeuralNetworksOperandType output = weight;
    output.type = ANEURALNETWORKS_TENSOR_INT32;

    OperationTestBase lshProjTest(ANEURALNETWORKS_LSH_PROJECTION, {hash, input, weight, type},
                                  {output});
    lshProjTest.testOpsValidations();
}

TEST(OperationValidationTest, LSH_PROJECTION_float16) {
    lshProjectionTest(ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_TENSOR_FLOAT32);
    lshProjectionTest(ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, LSH_PROJECTION_float32) {
    lshProjectionTest(ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_TENSOR_FLOAT32);
    lshProjectionTest(ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, LSH_PROJECTION_quant8) {
    lshProjectionTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, ANEURALNETWORKS_TENSOR_FLOAT32);
    lshProjectionTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, LSH_PROJECTION_int32) {
    lshProjectionTest(ANEURALNETWORKS_TENSOR_INT32, ANEURALNETWORKS_TENSOR_FLOAT32);
    lshProjectionTest(ANEURALNETWORKS_TENSOR_INT32, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, LSTM_float32) {
    uint32_t oneDimensional[1] = {5};
    uint32_t twoDimensional[2] = {5, 5};
    ANeuralNetworksOperandType floatTensor1D = {.type = ANEURALNETWORKS_TENSOR_FLOAT32,
                                                .dimensionCount = 1,
                                                .dimensions = oneDimensional,
                                                .scale = 0.0f,
                                                .zeroPoint = 0};
    ANeuralNetworksOperandType floatTensor2D = {.type = ANEURALNETWORKS_TENSOR_FLOAT32,
                                                .dimensionCount = 2,
                                                .dimensions = twoDimensional,
                                                .scale = 0.0f,
                                                .zeroPoint = 0};
    ANeuralNetworksOperandType intScalar = {.type = ANEURALNETWORKS_INT32,
                                            .dimensionCount = 0,
                                            .dimensions = nullptr,
                                            .scale = 0.0f,
                                            .zeroPoint = 0};
    ANeuralNetworksOperandType floatScalar = {.type = ANEURALNETWORKS_FLOAT32,
                                              .dimensionCount = 0,
                                              .dimensions = nullptr,
                                              .scale = 0.0f,
                                              .zeroPoint = 0};

    ANeuralNetworksOperandType input = floatTensor2D;
    ANeuralNetworksOperandType inputToInput = floatTensor2D;
    ANeuralNetworksOperandType inputToForget = floatTensor2D;
    ANeuralNetworksOperandType inputToCell = floatTensor2D;
    ANeuralNetworksOperandType inputToOutput = floatTensor2D;
    ANeuralNetworksOperandType recurrentToInput = floatTensor2D;
    ANeuralNetworksOperandType recurrentToForget = floatTensor2D;
    ANeuralNetworksOperandType recurrentToCell = floatTensor2D;
    ANeuralNetworksOperandType recurrentToOutput = floatTensor2D;
    ANeuralNetworksOperandType cellToInput = floatTensor1D;
    ANeuralNetworksOperandType cellToForget = floatTensor1D;
    ANeuralNetworksOperandType cellToOutput = floatTensor1D;
    ANeuralNetworksOperandType inputGateBias = floatTensor1D;
    ANeuralNetworksOperandType forgetGateBias = floatTensor1D;
    ANeuralNetworksOperandType cellBias = floatTensor1D;
    ANeuralNetworksOperandType outputGateBias = floatTensor1D;
    ANeuralNetworksOperandType projWeights = floatTensor2D;
    ANeuralNetworksOperandType projBias = floatTensor1D;
    ANeuralNetworksOperandType outputStateIn = floatTensor2D;
    ANeuralNetworksOperandType cellStateIn = floatTensor2D;
    ANeuralNetworksOperandType activation = intScalar;
    ANeuralNetworksOperandType clipCellState = floatScalar;
    ANeuralNetworksOperandType clipProjLayer = floatScalar;

    ANeuralNetworksOperandType scratch = floatTensor2D;
    ANeuralNetworksOperandType outputStateOut = floatTensor2D;
    ANeuralNetworksOperandType cellStateOut = floatTensor2D;
    ANeuralNetworksOperandType output = floatTensor2D;

    OperationTestBase lstmTest(ANEURALNETWORKS_LSTM,
                               {input,
                                inputToInput,
                                inputToForget,
                                inputToCell,
                                inputToOutput,
                                recurrentToInput,
                                recurrentToForget,
                                recurrentToCell,
                                recurrentToOutput,
                                cellToInput,
                                cellToForget,
                                cellToOutput,
                                inputGateBias,
                                forgetGateBias,
                                cellBias,
                                outputGateBias,
                                projWeights,
                                projBias,
                                outputStateIn,
                                cellStateIn,
                                activation,
                                clipCellState,
                                clipProjLayer},
                               {scratch, outputStateOut, cellStateOut, output});
    lstmTest.testOpsValidations();
}

void lstmTestV1_2(int32_t operandCode) {
    SCOPED_TRACE(operandCode);
    uint32_t oneDimensional[1] = {5};
    uint32_t twoDimensional[2] = {5, 5};
    ANeuralNetworksOperandType floatTensor1D = {.type = operandCode,
                                                .dimensionCount = 1,
                                                .dimensions = oneDimensional,
                                                .scale = 0.0f,
                                                .zeroPoint = 0};
    ANeuralNetworksOperandType floatTensor2D = {.type = operandCode,
                                                .dimensionCount = 2,
                                                .dimensions = twoDimensional,
                                                .scale = 0.0f,
                                                .zeroPoint = 0};
    ANeuralNetworksOperandType intScalar = {.type = ANEURALNETWORKS_INT32,
                                            .dimensionCount = 0,
                                            .dimensions = nullptr,
                                            .scale = 0.0f,
                                            .zeroPoint = 0};
    ANeuralNetworksOperandType floatScalar = {
            .type = (operandCode == ANEURALNETWORKS_TENSOR_FLOAT32) ? ANEURALNETWORKS_FLOAT32
                                                                    : ANEURALNETWORKS_FLOAT16,
            .dimensionCount = 0,
            .dimensions = nullptr,
            .scale = 0.0f,
            .zeroPoint = 0};

    ANeuralNetworksOperandType input = floatTensor2D;
    ANeuralNetworksOperandType inputToInput = floatTensor2D;
    ANeuralNetworksOperandType inputToForget = floatTensor2D;
    ANeuralNetworksOperandType inputToCell = floatTensor2D;
    ANeuralNetworksOperandType inputToOutput = floatTensor2D;
    ANeuralNetworksOperandType recurrentToInput = floatTensor2D;
    ANeuralNetworksOperandType recurrentToForget = floatTensor2D;
    ANeuralNetworksOperandType recurrentToCell = floatTensor2D;
    ANeuralNetworksOperandType recurrentToOutput = floatTensor2D;
    ANeuralNetworksOperandType cellToInput = floatTensor1D;
    ANeuralNetworksOperandType cellToForget = floatTensor1D;
    ANeuralNetworksOperandType cellToOutput = floatTensor1D;
    ANeuralNetworksOperandType inputGateBias = floatTensor1D;
    ANeuralNetworksOperandType forgetGateBias = floatTensor1D;
    ANeuralNetworksOperandType cellBias = floatTensor1D;
    ANeuralNetworksOperandType outputGateBias = floatTensor1D;
    ANeuralNetworksOperandType projWeights = floatTensor2D;
    ANeuralNetworksOperandType projBias = floatTensor1D;
    ANeuralNetworksOperandType outputStateIn = floatTensor2D;
    ANeuralNetworksOperandType cellStateIn = floatTensor2D;
    ANeuralNetworksOperandType activation = intScalar;
    ANeuralNetworksOperandType clipCellState = floatScalar;
    ANeuralNetworksOperandType clipProjLayer = floatScalar;
    ANeuralNetworksOperandType inputLayerNormWeights = floatTensor1D;
    ANeuralNetworksOperandType forgetLayerNormWeights = floatTensor1D;
    ANeuralNetworksOperandType cellLayerNormWeights = floatTensor1D;
    ANeuralNetworksOperandType outputLayerNormWeights = floatTensor1D;

    ANeuralNetworksOperandType scratch = floatTensor2D;
    ANeuralNetworksOperandType outputStateOut = floatTensor2D;
    ANeuralNetworksOperandType cellStateOut = floatTensor2D;
    ANeuralNetworksOperandType output = floatTensor2D;

    OperationTestBase lstmTest(ANEURALNETWORKS_LSTM,
                               {input,
                                inputToInput,
                                inputToForget,
                                inputToCell,
                                inputToOutput,
                                recurrentToInput,
                                recurrentToForget,
                                recurrentToCell,
                                recurrentToOutput,
                                cellToInput,
                                cellToForget,
                                cellToOutput,
                                inputGateBias,
                                forgetGateBias,
                                cellBias,
                                outputGateBias,
                                projWeights,
                                projBias,
                                outputStateIn,
                                cellStateIn,
                                activation,
                                clipCellState,
                                clipProjLayer,
                                inputLayerNormWeights,
                                forgetLayerNormWeights,
                                cellLayerNormWeights,
                                outputLayerNormWeights},
                               {scratch, outputStateOut, cellStateOut, output});
    lstmTest.testOpsValidations();
}

TEST(OperationValidationTest, LSTM_V1_2) {
    lstmTestV1_2(ANEURALNETWORKS_TENSOR_FLOAT32);
    lstmTestV1_2(ANEURALNETWORKS_TENSOR_FLOAT16);
}

void lstmBidirectionalSequence(int32_t operandCode) {
    uint32_t oneDimensional[1] = {5};
    uint32_t twoDimensional[2] = {5, 5};
    uint32_t threeDimensional[3] = {5, 5, 5};
    ANeuralNetworksOperandType floatTensor1D = {
            .type = operandCode,
            .dimensionCount = 1,
            .dimensions = oneDimensional,
            .scale = 0.0f,
            .zeroPoint = 0,
    };
    ANeuralNetworksOperandType floatTensor2D = {
            .type = operandCode,
            .dimensionCount = 2,
            .dimensions = twoDimensional,
            .scale = 0.0f,
            .zeroPoint = 0,
    };
    ANeuralNetworksOperandType floatTensor3D = {
            .type = operandCode,
            .dimensionCount = 3,
            .dimensions = threeDimensional,
            .scale = 0.0f,
            .zeroPoint = 0,
    };
    ANeuralNetworksOperandType intScalar = {
            .type = ANEURALNETWORKS_INT32,
            .dimensionCount = 0,
            .dimensions = nullptr,
            .scale = 0.0f,
            .zeroPoint = 0,
    };
    ANeuralNetworksOperandType floatScalar = {
            .type = operandCode == ANEURALNETWORKS_TENSOR_FLOAT32 ? ANEURALNETWORKS_FLOAT32
                                                                  : ANEURALNETWORKS_FLOAT16,
            .dimensionCount = 0,
            .dimensions = nullptr,
            .scale = 0.0f,
            .zeroPoint = 0,
    };
    ANeuralNetworksOperandType boolScalar = {.type = ANEURALNETWORKS_BOOL,
                                             .dimensionCount = 0,
                                             .dimensions = nullptr,
                                             .scale = 0.0f,
                                             .zeroPoint = 0};

    ANeuralNetworksOperandType input = floatTensor3D;
    ANeuralNetworksOperandType inputToInputFw = floatTensor2D;
    ANeuralNetworksOperandType inputToForgetFw = floatTensor2D;
    ANeuralNetworksOperandType inputToCellFw = floatTensor2D;
    ANeuralNetworksOperandType inputToOutputFw = floatTensor2D;
    ANeuralNetworksOperandType recurrentToInputFw = floatTensor2D;
    ANeuralNetworksOperandType recurrentToForgetFw = floatTensor2D;
    ANeuralNetworksOperandType recurrentToCellFw = floatTensor2D;
    ANeuralNetworksOperandType recurrentToOutputFw = floatTensor2D;
    ANeuralNetworksOperandType cellToInputFw = floatTensor1D;
    ANeuralNetworksOperandType cellToForgetFw = floatTensor1D;
    ANeuralNetworksOperandType cellToOutputFw = floatTensor1D;
    ANeuralNetworksOperandType inputGateBiasFw = floatTensor1D;
    ANeuralNetworksOperandType forgetGateBiasFw = floatTensor1D;
    ANeuralNetworksOperandType cellBiasFw = floatTensor1D;
    ANeuralNetworksOperandType outputGateBiasFw = floatTensor1D;
    ANeuralNetworksOperandType projWeightsFw = floatTensor2D;
    ANeuralNetworksOperandType projBiasFw = floatTensor1D;
    ANeuralNetworksOperandType outputStateInFw = floatTensor2D;
    ANeuralNetworksOperandType cellStateInFw = floatTensor2D;
    ANeuralNetworksOperandType inputToInputBw = floatTensor2D;
    ANeuralNetworksOperandType inputToForgetBw = floatTensor2D;
    ANeuralNetworksOperandType inputToCellBw = floatTensor2D;
    ANeuralNetworksOperandType inputToOutputBw = floatTensor2D;
    ANeuralNetworksOperandType recurrentToInputBw = floatTensor2D;
    ANeuralNetworksOperandType recurrentToForgetBw = floatTensor2D;
    ANeuralNetworksOperandType recurrentToCellBw = floatTensor2D;
    ANeuralNetworksOperandType recurrentToOutputBw = floatTensor2D;
    ANeuralNetworksOperandType cellToInputBw = floatTensor1D;
    ANeuralNetworksOperandType cellToForgetBw = floatTensor1D;
    ANeuralNetworksOperandType cellToOutputBw = floatTensor1D;
    ANeuralNetworksOperandType inputGateBiasBw = floatTensor1D;
    ANeuralNetworksOperandType forgetGateBiasBw = floatTensor1D;
    ANeuralNetworksOperandType cellBiasBw = floatTensor1D;
    ANeuralNetworksOperandType outputGateBiasBw = floatTensor1D;
    ANeuralNetworksOperandType projWeightsBw = floatTensor2D;
    ANeuralNetworksOperandType projBiasBw = floatTensor1D;
    ANeuralNetworksOperandType outputStateInBw = floatTensor2D;
    ANeuralNetworksOperandType cellStateInBw = floatTensor2D;
    ANeuralNetworksOperandType auxInput = floatTensor3D;
    ANeuralNetworksOperandType auxInputToInputFw = floatTensor2D;
    ANeuralNetworksOperandType auxInputToForgetFw = floatTensor2D;
    ANeuralNetworksOperandType auxInputToCellFw = floatTensor2D;
    ANeuralNetworksOperandType auxInputToOutputFw = floatTensor2D;
    ANeuralNetworksOperandType auxInputToInputBw = floatTensor2D;
    ANeuralNetworksOperandType auxInputToForgetBw = floatTensor2D;
    ANeuralNetworksOperandType auxInputToCellBw = floatTensor2D;
    ANeuralNetworksOperandType auxInputToOutputBw = floatTensor2D;
    ANeuralNetworksOperandType activation = intScalar;
    ANeuralNetworksOperandType clipCellState = floatScalar;
    ANeuralNetworksOperandType clipProjLayer = floatScalar;
    ANeuralNetworksOperandType mergeOutputs = boolScalar;
    ANeuralNetworksOperandType timeMajor = boolScalar;
    ANeuralNetworksOperandType inputLayerNormWeightsFw = floatTensor1D;
    ANeuralNetworksOperandType forgetLayerNormWeightsFw = floatTensor1D;
    ANeuralNetworksOperandType cellLayerNormWeightsFw = floatTensor1D;
    ANeuralNetworksOperandType outputLayerNormWeightsFw = floatTensor1D;
    ANeuralNetworksOperandType inputLayerNormWeightsBw = floatTensor1D;
    ANeuralNetworksOperandType forgetLayerNormWeightsBw = floatTensor1D;
    ANeuralNetworksOperandType cellLayerNormWeightsBw = floatTensor1D;
    ANeuralNetworksOperandType outputLayerNormWeightsBw = floatTensor1D;

    ANeuralNetworksOperandType outputFw = floatTensor2D;
    ANeuralNetworksOperandType outputBw = floatTensor2D;

    OperationTestBase lstmTest(ANEURALNETWORKS_BIDIRECTIONAL_SEQUENCE_LSTM,
                               {
                                       input,
                                       inputToInputFw,
                                       inputToForgetFw,
                                       inputToCellFw,
                                       inputToOutputFw,
                                       recurrentToInputFw,
                                       recurrentToForgetFw,
                                       recurrentToCellFw,
                                       recurrentToOutputFw,
                                       cellToInputFw,
                                       cellToForgetFw,
                                       cellToOutputFw,
                                       inputGateBiasFw,
                                       forgetGateBiasFw,
                                       cellBiasFw,
                                       outputGateBiasFw,
                                       projWeightsFw,
                                       projBiasFw,
                                       outputStateInFw,
                                       cellStateInFw,
                                       inputToInputBw,
                                       inputToForgetBw,
                                       inputToCellBw,
                                       inputToOutputBw,
                                       recurrentToInputBw,
                                       recurrentToForgetBw,
                                       recurrentToCellBw,
                                       recurrentToOutputBw,
                                       cellToInputBw,
                                       cellToForgetBw,
                                       cellToOutputBw,
                                       inputGateBiasBw,
                                       forgetGateBiasBw,
                                       cellBiasBw,
                                       outputGateBiasBw,
                                       projWeightsBw,
                                       projBiasBw,
                                       outputStateInBw,
                                       cellStateInBw,
                                       auxInput,
                                       auxInputToInputFw,
                                       auxInputToForgetFw,
                                       auxInputToCellFw,
                                       auxInputToOutputFw,
                                       auxInputToInputBw,
                                       auxInputToForgetBw,
                                       auxInputToCellBw,
                                       auxInputToOutputBw,
                                       activation,
                                       clipCellState,
                                       clipProjLayer,
                                       mergeOutputs,
                                       timeMajor,
                                       inputLayerNormWeightsFw,
                                       forgetLayerNormWeightsFw,
                                       cellLayerNormWeightsFw,
                                       outputLayerNormWeightsFw,
                                       inputLayerNormWeightsBw,
                                       forgetLayerNormWeightsBw,
                                       cellLayerNormWeightsBw,
                                       outputLayerNormWeightsBw,
                               },
                               {
                                       outputFw,
                                       outputBw,
                               });

    lstmTest.testOpsValidations();
}

TEST(OperationValidationTest, LSTM_BIDIRECTIONAL_SEQUENCE) {
    lstmBidirectionalSequence(ANEURALNETWORKS_TENSOR_FLOAT32);
    lstmBidirectionalSequence(ANEURALNETWORKS_TENSOR_FLOAT16);
}

void randomMultinomialOpTest(int32_t operandCode) {
    uint32_t inputDims[2] = {5, 5};
    ANeuralNetworksOperandType input = {.type = operandCode,
                                        .dimensionCount = 2,
                                        .dimensions = inputDims,
                                        .scale = 0.0f,
                                        .zeroPoint = 0};
    ANeuralNetworksOperandType sample_count = {.type = ANEURALNETWORKS_INT32,
                                               .dimensionCount = 0,
                                               .dimensions = nullptr,
                                               .scale = 0.0f,
                                               .zeroPoint = 0};
    uint32_t seedDims[1] = {2};
    ANeuralNetworksOperandType seed = {.type = ANEURALNETWORKS_TENSOR_INT32,
                                       .dimensionCount = 1,
                                       .dimensions = seedDims,
                                       .scale = 0.0f,
                                       .zeroPoint = 0};
    uint32_t outputDims[2] = {5, 7};
    ANeuralNetworksOperandType output = {.type = ANEURALNETWORKS_TENSOR_INT32,
                                         .dimensionCount = 2,
                                         .dimensions = outputDims,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};

    OperationTestBase multinomialTest(ANEURALNETWORKS_RANDOM_MULTINOMIAL,
                                      {input, sample_count, seed}, {output});
    multinomialTest.testOpsValidations();
}

TEST(OperationValidationTest, RANDOM_MULTINOMIAL_float16) {
    randomMultinomialOpTest(ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, RANDOM_MULTINOMIAL_float32) {
    randomMultinomialOpTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, RNN_float16) {
    uint32_t oneDimensional[1] = {5};
    uint32_t twoDimensional[2] = {5, 5};
    ANeuralNetworksOperandType floatTensor1D = {.type = ANEURALNETWORKS_TENSOR_FLOAT16,
                                                .dimensionCount = 1,
                                                .dimensions = oneDimensional,
                                                .scale = 0.0f,
                                                .zeroPoint = 0};
    ANeuralNetworksOperandType floatTensor2D = {.type = ANEURALNETWORKS_TENSOR_FLOAT16,
                                                .dimensionCount = 2,
                                                .dimensions = twoDimensional,
                                                .scale = 0.0f,
                                                .zeroPoint = 0};
    ANeuralNetworksOperandType intScalar = {.type = ANEURALNETWORKS_INT32,
                                            .dimensionCount = 0,
                                            .dimensions = nullptr,
                                            .scale = 0.0f,
                                            .zeroPoint = 0};

    ANeuralNetworksOperandType input = floatTensor2D;
    ANeuralNetworksOperandType weights = floatTensor2D;
    ANeuralNetworksOperandType recurrentWeights = floatTensor2D;
    ANeuralNetworksOperandType bias = floatTensor1D;
    ANeuralNetworksOperandType hiddenStateIn = floatTensor2D;
    ANeuralNetworksOperandType activation = intScalar;

    ANeuralNetworksOperandType hiddenStateOut = floatTensor2D;
    ANeuralNetworksOperandType output = floatTensor2D;

    OperationTestBase rnnTest(ANEURALNETWORKS_RNN,
                              {input, weights, recurrentWeights, bias, hiddenStateIn, activation},
                              {hiddenStateOut, output});
    rnnTest.testOpsValidations();
}

TEST(OperationValidationTest, RNN_float32) {
    uint32_t oneDimensional[1] = {5};
    uint32_t twoDimensional[2] = {5, 5};
    ANeuralNetworksOperandType floatTensor1D = {.type = ANEURALNETWORKS_TENSOR_FLOAT32,
                                                .dimensionCount = 1,
                                                .dimensions = oneDimensional,
                                                .scale = 0.0f,
                                                .zeroPoint = 0};
    ANeuralNetworksOperandType floatTensor2D = {.type = ANEURALNETWORKS_TENSOR_FLOAT32,
                                                .dimensionCount = 2,
                                                .dimensions = twoDimensional,
                                                .scale = 0.0f,
                                                .zeroPoint = 0};
    ANeuralNetworksOperandType intScalar = {.type = ANEURALNETWORKS_INT32,
                                            .dimensionCount = 0,
                                            .dimensions = nullptr,
                                            .scale = 0.0f,
                                            .zeroPoint = 0};

    ANeuralNetworksOperandType input = floatTensor2D;
    ANeuralNetworksOperandType weights = floatTensor2D;
    ANeuralNetworksOperandType recurrentWeights = floatTensor2D;
    ANeuralNetworksOperandType bias = floatTensor1D;
    ANeuralNetworksOperandType hiddenStateIn = floatTensor2D;
    ANeuralNetworksOperandType activation = intScalar;

    ANeuralNetworksOperandType hiddenStateOut = floatTensor2D;
    ANeuralNetworksOperandType output = floatTensor2D;

    OperationTestBase rnnTest(ANEURALNETWORKS_RNN,
                              {input, weights, recurrentWeights, bias, hiddenStateIn, activation},
                              {hiddenStateOut, output});
    rnnTest.testOpsValidations();
}

TEST(OperationValidationTest, SVDF_float32) {
    uint32_t oneDimensional[1] = {5};
    uint32_t twoDimensional[2] = {5, 5};
    ANeuralNetworksOperandType floatTensor1D = {.type = ANEURALNETWORKS_TENSOR_FLOAT32,
                                                .dimensionCount = 1,
                                                .dimensions = oneDimensional,
                                                .scale = 0.0f,
                                                .zeroPoint = 0};
    ANeuralNetworksOperandType floatTensor2D = {.type = ANEURALNETWORKS_TENSOR_FLOAT32,
                                                .dimensionCount = 2,
                                                .dimensions = twoDimensional,
                                                .scale = 0.0f,
                                                .zeroPoint = 0};
    ANeuralNetworksOperandType intScalar = {.type = ANEURALNETWORKS_INT32,
                                            .dimensionCount = 0,
                                            .dimensions = nullptr,
                                            .scale = 0.0f,
                                            .zeroPoint = 0};

    ANeuralNetworksOperandType input = floatTensor2D;
    ANeuralNetworksOperandType weightsFeature = floatTensor2D;
    ANeuralNetworksOperandType weightsTime = floatTensor2D;
    ANeuralNetworksOperandType bias = floatTensor1D;
    ANeuralNetworksOperandType stateIn = floatTensor2D;
    ANeuralNetworksOperandType rank = intScalar;
    ANeuralNetworksOperandType activation = intScalar;

    ANeuralNetworksOperandType stateOut = floatTensor2D;
    ANeuralNetworksOperandType output = floatTensor2D;

    OperationTestBase svdfTest(
            ANEURALNETWORKS_SVDF,
            {input, weightsFeature, weightsTime, bias, stateIn, rank, activation},
            {stateOut, output});
    svdfTest.testOpsValidations();
}

TEST(OperationValidationTest, SVDF_float16) {
    uint32_t oneDimensional[1] = {5};
    uint32_t twoDimensional[2] = {5, 5};
    ANeuralNetworksOperandType floatTensor1D = {.type = ANEURALNETWORKS_TENSOR_FLOAT16,
                                                .dimensionCount = 1,
                                                .dimensions = oneDimensional,
                                                .scale = 0.0f,
                                                .zeroPoint = 0};
    ANeuralNetworksOperandType floatTensor2D = {.type = ANEURALNETWORKS_TENSOR_FLOAT16,
                                                .dimensionCount = 2,
                                                .dimensions = twoDimensional,
                                                .scale = 0.0f,
                                                .zeroPoint = 0};
    ANeuralNetworksOperandType intScalar = {.type = ANEURALNETWORKS_INT32,
                                            .dimensionCount = 0,
                                            .dimensions = nullptr,
                                            .scale = 0.0f,
                                            .zeroPoint = 0};

    ANeuralNetworksOperandType input = floatTensor2D;
    ANeuralNetworksOperandType weightsFeature = floatTensor2D;
    ANeuralNetworksOperandType weightsTime = floatTensor2D;
    ANeuralNetworksOperandType bias = floatTensor1D;
    ANeuralNetworksOperandType stateIn = floatTensor2D;
    ANeuralNetworksOperandType rank = intScalar;
    ANeuralNetworksOperandType activation = intScalar;

    ANeuralNetworksOperandType stateOut = floatTensor2D;
    ANeuralNetworksOperandType output = floatTensor2D;

    OperationTestBase svdfTest(
            ANEURALNETWORKS_SVDF,
            {input, weightsFeature, weightsTime, bias, stateIn, rank, activation},
            {stateOut, output});
    svdfTest.testOpsValidations();
}

void stridedSliceOpTest(int32_t operandCode) {
    uint32_t inputDimensions[2] = {5, 5};
    ANeuralNetworksOperandType input = getOpType(operandCode, 2, inputDimensions);
    ANeuralNetworksOperandType output = input;

    uint32_t beginsDimensions[1] = {2};
    ANeuralNetworksOperandType begins = {.type = ANEURALNETWORKS_TENSOR_INT32,
                                         .dimensionCount = 1,
                                         .dimensions = beginsDimensions,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};

    ANeuralNetworksOperandType ends = begins;
    ANeuralNetworksOperandType strides = begins;

    ANeuralNetworksOperandType beginMask = {.type = ANEURALNETWORKS_INT32,
                                            .dimensionCount = 0,
                                            .dimensions = nullptr,
                                            .scale = 0.0f,
                                            .zeroPoint = 0};
    ANeuralNetworksOperandType endMask = beginMask;
    ANeuralNetworksOperandType shrinkAxisMask = beginMask;

    OperationTestBase stridedSliceTest(
            ANEURALNETWORKS_STRIDED_SLICE,
            {input, begins, ends, strides, beginMask, endMask, shrinkAxisMask}, {output},
            {{TensorRankConstraint::UpTo(4)}});
    stridedSliceTest.testOpsValidations();
}

TEST(OperationValidationTest, STRIDED_SLICE_float32) {
    stridedSliceOpTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, STRIDED_SLICE_float16) {
    stridedSliceOpTest(ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, STRIDED_SLICE_quant8) {
    stridedSliceOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, STRIDED_SLICE_quant8_signed) {
    stridedSliceOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void roiAlignOpTest(int32_t inputOperandCode, int32_t roiOperandCode, int32_t scalarOperandCode) {
    uint32_t inDim[] = {1, 4, 4, 1}, roiDim[] = {4, 4}, batchSplitDim[] = {1};
    uint32_t outDim[] = {4, 2, 2, 1};
    OperationTestBase roiAlignTest(
            ANEURALNETWORKS_ROI_ALIGN,
            {getOpType(inputOperandCode, 4, inDim), getOpType(roiOperandCode, 2, roiDim),
             getOpType(ANEURALNETWORKS_TENSOR_INT32, 1, batchSplitDim),
             getOpType(ANEURALNETWORKS_INT32), getOpType(ANEURALNETWORKS_INT32),
             getOpType(scalarOperandCode), getOpType(scalarOperandCode),
             getOpType(ANEURALNETWORKS_INT32), getOpType(ANEURALNETWORKS_INT32),
             getOpType(ANEURALNETWORKS_BOOL)},
            {getOpType(inputOperandCode, 4, outDim)});
    roiAlignTest.testOpsValidations();
}

TEST(OperationValidationTest, ROI_ALIGN_float16) {
    roiAlignOpTest(ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_TENSOR_FLOAT16,
                   ANEURALNETWORKS_FLOAT16);
}

TEST(OperationValidationTest, ROI_ALIGN_float32) {
    roiAlignOpTest(ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_TENSOR_FLOAT32,
                   ANEURALNETWORKS_FLOAT32);
}

TEST(OperationValidationTest, ROI_ALIGN_quant8) {
    roiAlignOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, ANEURALNETWORKS_TENSOR_QUANT16_ASYMM,
                   ANEURALNETWORKS_FLOAT32);
}

TEST(OperationValidationTest, ROI_ALIGN_quant8signed) {
    roiAlignOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED, ANEURALNETWORKS_TENSOR_QUANT16_ASYMM,
                   ANEURALNETWORKS_FLOAT32);
}

void roiPoolingOpTest(int32_t inputOperandCode, int32_t roiOperandCode, int32_t scalarOperandCode) {
    uint32_t inDim[] = {1, 4, 4, 1}, roiDim[] = {4, 4}, batchSplitDim[] = {1};
    uint32_t outDim[] = {4, 2, 2, 1};
    OperationTestBase roiPoolingTest(
            ANEURALNETWORKS_ROI_POOLING,
            {getOpType(inputOperandCode, 4, inDim), getOpType(roiOperandCode, 2, roiDim),
             getOpType(ANEURALNETWORKS_TENSOR_INT32, 1, batchSplitDim),
             getOpType(ANEURALNETWORKS_INT32), getOpType(ANEURALNETWORKS_INT32),
             getOpType(scalarOperandCode), getOpType(scalarOperandCode),
             getOpType(ANEURALNETWORKS_BOOL)},
            {getOpType(inputOperandCode, 4, outDim)});
    roiPoolingTest.testOpsValidations();
}

TEST(OperationValidationTest, ROI_POOLING_float16) {
    roiPoolingOpTest(ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_TENSOR_FLOAT16,
                     ANEURALNETWORKS_FLOAT16);
}

TEST(OperationValidationTest, ROI_POOLING_float32) {
    roiPoolingOpTest(ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_TENSOR_FLOAT32,
                     ANEURALNETWORKS_FLOAT32);
}

TEST(OperationValidationTest, ROI_POOLING_quant8) {
    roiPoolingOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, ANEURALNETWORKS_TENSOR_QUANT16_ASYMM,
                     ANEURALNETWORKS_FLOAT32);
}

TEST(OperationValidationTest, ROI_POOLING_quant8signed) {
    roiPoolingOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED,
                     ANEURALNETWORKS_TENSOR_QUANT16_ASYMM, ANEURALNETWORKS_FLOAT32);
}

void heatmapMaxKeypointOpTest(int32_t heatmapOperandCode, int32_t roiOperandCode) {
    uint32_t heatmapDim[] = {6, 4, 4, 1}, boxDim[] = {6, 4}, outScoreDim[] = {6, 1},
             outKeypointDim[] = {6, 1, 2};
    OperationTestBase heatmapMaxKeypointTest(
            ANEURALNETWORKS_HEATMAP_MAX_KEYPOINT,
            {getOpType(heatmapOperandCode, 4, heatmapDim), getOpType(roiOperandCode, 2, boxDim),
             getOpType(ANEURALNETWORKS_BOOL)},
            {getOpType(heatmapOperandCode, 2, outScoreDim),
             getOpType(roiOperandCode, 3, outKeypointDim)});
    heatmapMaxKeypointTest.testOpsValidations();
}

TEST(OperationValidationTest, HEATMAP_MAX_KEYPOINT_float16) {
    heatmapMaxKeypointOpTest(ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, HEATMAP_MAX_KEYPOINT_float32) {
    heatmapMaxKeypointOpTest(ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, HEATMAP_MAX_KEYPOINT_quant) {
    heatmapMaxKeypointOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM,
                             ANEURALNETWORKS_TENSOR_QUANT16_ASYMM);
}

TEST(OperationValidationTest, HEATMAP_MAX_KEYPOINT_quant_signed) {
    heatmapMaxKeypointOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED,
                             ANEURALNETWORKS_TENSOR_QUANT16_ASYMM);
}

void instanceNormalizationOpTest(int32_t inputOperandType) {
    SCOPED_TRACE(inputOperandType);
    uint32_t inputDims[4] = {4, 4, 4, 4};
    ANeuralNetworksOperandType input = getOpType(inputOperandType, 4, inputDims);
    ANeuralNetworksOperandType floatScalar = getOpType(ANEURALNETWORKS_FLOAT32);
    if (inputOperandType == ANEURALNETWORKS_TENSOR_FLOAT16) {
        floatScalar = getOpType(ANEURALNETWORKS_FLOAT16);
    }
    ANeuralNetworksOperandType gamma = floatScalar;
    ANeuralNetworksOperandType beta = floatScalar;
    ANeuralNetworksOperandType epsilon = floatScalar;
    ANeuralNetworksOperandType isNCHW = getOpType(ANEURALNETWORKS_BOOL);
    ANeuralNetworksOperandType output = input;

    OperationTestBase test(ANEURALNETWORKS_INSTANCE_NORMALIZATION,
                           {input, gamma, beta, epsilon, isNCHW}, {output});
    test.testOpsValidations();
}

TEST(OperationValidationTest, INSTANCE_NORMALIZATION) {
    instanceNormalizationOpTest(ANEURALNETWORKS_TENSOR_FLOAT16);
    instanceNormalizationOpTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

void groupedConvOpTest(int32_t inputOperandCode, int32_t filterOperandCode) {
    uint32_t inDim[] = {1, 3, 3, 2}, filterDim[] = {2, 2, 2, 1}, biasDim[] = {2};
    uint32_t outDim[] = {1, 2, 2, 2};
    ANeuralNetworksOperandType input = getOpType(inputOperandCode, 4, inDim);

    float filterScales[2] = {0.5f, 1.0f};
    ANeuralNetworksOperandType filter = getOpType(filterOperandCode, 4, filterDim);

    ANeuralNetworksSymmPerChannelQuantParams filterChannelQuantParams = {
            .channelDim = 0,
            .scaleCount = 2,
            .scales = filterScales,
    };

    ANeuralNetworksOperandType bias = getOpType(inputOperandCode, 1, biasDim);
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM ||
        filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED) {
        bias.type = ANEURALNETWORKS_TENSOR_INT32;
        bias.scale = 0.25f;
    }
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        bias.type = ANEURALNETWORKS_TENSOR_INT32;
        bias.scale = 0.0f;
    }

    ANeuralNetworksOperandType scalar = getOpType(ANEURALNETWORKS_INT32);
    ANeuralNetworksOperandType layout = getOpType(ANEURALNETWORKS_BOOL);

    ANeuralNetworksOperandType output = getOpType(inputOperandCode, 4, outDim);

    OperationTestBase explicitGroupedConvTest(ANEURALNETWORKS_GROUPED_CONV_2D,
                                              {input, filter, bias, scalar, scalar, scalar, scalar,
                                               scalar, scalar, scalar, scalar, layout},
                                              {output});
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        explicitGroupedConvTest.setInputSymmPerChannelQuantParams(1, filterChannelQuantParams);
    }
    explicitGroupedConvTest.testOpsValidations();

    OperationTestBase implicitGroupedConvTest(
            ANEURALNETWORKS_GROUPED_CONV_2D,
            {input, filter, bias, scalar, scalar, scalar, scalar, scalar, layout}, {output});
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        implicitGroupedConvTest.setInputSymmPerChannelQuantParams(1, filterChannelQuantParams);
    }
    implicitGroupedConvTest.testOpsValidations();
}

TEST(OperationValidationTest, GROUPED_CONV_2D_float16) {
    groupedConvOpTest(ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, GROUPED_CONV_2D_float32) {
    groupedConvOpTest(ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, GROUPED_CONV_2D_quant8) {
    groupedConvOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, GROUPED_CONV_2D_quant8_per_channel) {
    groupedConvOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM,
                      ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL);
}

TEST(OperationValidationTest, GROUPED_CONV_2D_quant8signed) {
    groupedConvOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED,
                      ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, GROUPED_CONV_2D_quant8signed_per_channel) {
    groupedConvOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED,
                      ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL);
}

void transposeConvOpTest(int32_t inputOperandCode, int32_t filterOperandCode) {
    uint32_t inDim[] = {1, 2, 2, 2}, filterDim[] = {2, 3, 3, 1}, biasDim[] = {2};
    uint32_t outDim[] = {1, 5, 5, 2}, outShapeDim[] = {4};
    ANeuralNetworksOperandType input = getOpType(inputOperandCode, 4, inDim);
    ANeuralNetworksOperandType filter = getOpType(filterOperandCode, 4, filterDim);

    float filterScales[2] = {0.5f, 1.0f};
    ANeuralNetworksSymmPerChannelQuantParams filterChannelQuantParams = {
            .channelDim = 0,
            .scaleCount = 2,
            .scales = filterScales,
    };

    ANeuralNetworksOperandType bias = getOpType(inputOperandCode, 1, biasDim);
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM ||
        filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED) {
        bias.type = ANEURALNETWORKS_TENSOR_INT32;
        bias.scale = 0.25f;
    }
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        bias.type = ANEURALNETWORKS_TENSOR_INT32;
        bias.scale = 0.0f;
    }

    ANeuralNetworksOperandType scalar = getOpType(ANEURALNETWORKS_INT32);
    ANeuralNetworksOperandType layout = getOpType(ANEURALNETWORKS_BOOL);
    ANeuralNetworksOperandType output = getOpType(inputOperandCode, 4, outDim);

    OperationTestBase explicitTransposeConvTest(
            ANEURALNETWORKS_TRANSPOSE_CONV_2D,
            {input, filter, bias, scalar, scalar, scalar, scalar, scalar, scalar, scalar, layout},
            {output});
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        explicitTransposeConvTest.setInputSymmPerChannelQuantParams(1, filterChannelQuantParams);
    }
    explicitTransposeConvTest.testOpsValidations();

    OperationTestBase implicitTransposeConvTest(
            ANEURALNETWORKS_TRANSPOSE_CONV_2D,
            {input, filter, bias, getOpType(ANEURALNETWORKS_TENSOR_INT32, 1, outShapeDim), scalar,
             scalar, scalar, scalar, layout},
            {output});
    if (filterOperandCode == ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        implicitTransposeConvTest.setInputSymmPerChannelQuantParams(1, filterChannelQuantParams);
    }
    implicitTransposeConvTest.testOpsValidations();
}

TEST(OperationValidationTest, TRANSPOSE_CONV_2D_float16) {
    transposeConvOpTest(ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, TRANSPOSE_CONV_2D_float32) {
    transposeConvOpTest(ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, TRANSPOSE_CONV_2D_quant8) {
    transposeConvOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, TRANSPOSE_CONV_2D_quant8_per_channel) {
    transposeConvOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM,
                        ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL);
}

TEST(OperationValidationTest, TRANSPOSE_CONV_2D_quant8_signed) {
    transposeConvOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED,
                        ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, TRANSPOSE_CONV_2D_quant8_signed_per_channel) {
    transposeConvOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED,
                        ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL);
}

void channelShuffleOpTest(int32_t operandCode) {
    uint32_t inoutDim[] = {2, 2, 3, 12};
    OperationTestBase channelShuffleTest(
            ANEURALNETWORKS_CHANNEL_SHUFFLE,
            {getOpType(operandCode, 2, inoutDim), getOpType(ANEURALNETWORKS_INT32),
             getOpType(ANEURALNETWORKS_INT32)},
            {getOpType(operandCode, 2, inoutDim)}, {{TensorRankConstraint::UpTo(4)}});
    channelShuffleTest.testOpsValidations();
}

TEST(OperationValidationTest, CHANNEL_SHUFFLE_float16) {
    channelShuffleOpTest(ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, CHANNEL_SHUFFLE_float32) {
    channelShuffleOpTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, CHANNEL_SHUFFLE_quant8) {
    channelShuffleOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, CHANNEL_SHUFFLE_quant8signed) {
    channelShuffleOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void detectionPostprocessingOpTest(int32_t inputOperandCode) {
    SCOPED_TRACE(inputOperandCode);
    const int numBatches = 2;
    const int numAnchors = 10;
    const int numClasses = 5;
    const int lengthBoxEncoding = 4;

    uint32_t inputDims[3] = {numBatches, numAnchors, numClasses};
    ANeuralNetworksOperandType input = getOpType(inputOperandCode, 3, inputDims);
    uint32_t deltasDims[3] = {numBatches, numAnchors, lengthBoxEncoding};
    ANeuralNetworksOperandType deltas = getOpType(inputOperandCode, 3, deltasDims);
    uint32_t anchorsDims[2] = {numAnchors, 4};
    ANeuralNetworksOperandType anchors = getOpType(inputOperandCode, 2, anchorsDims);
    ANeuralNetworksOperandType scaleScalar = getOpType(ANEURALNETWORKS_FLOAT32);
    if (inputOperandCode == ANEURALNETWORKS_TENSOR_FLOAT16) {
        scaleScalar = getOpType(ANEURALNETWORKS_FLOAT16);
    }
    ANeuralNetworksOperandType isRegularNMS = getOpType(ANEURALNETWORKS_BOOL);
    ANeuralNetworksOperandType maxNumDetections = getOpType(ANEURALNETWORKS_INT32);
    ANeuralNetworksOperandType numOfClassesPerDetection = maxNumDetections;
    ANeuralNetworksOperandType numOfDetections = numOfClassesPerDetection;
    ANeuralNetworksOperandType scoreThreshold = scaleScalar;
    ANeuralNetworksOperandType iouThreshold = scaleScalar;
    ANeuralNetworksOperandType includeBackground = getOpType(ANEURALNETWORKS_BOOL);
    // Outputs
    const int maxNumDetectionsValue = 5;
    uint32_t outputScoreDims[2] = {numBatches, maxNumDetectionsValue};
    ANeuralNetworksOperandType outputScore = getOpType(inputOperandCode, 2, outputScoreDims);
    uint32_t boundingBoxesDims[3] = {numBatches, maxNumDetectionsValue, 4};
    ANeuralNetworksOperandType boundingBoxes = getOpType(inputOperandCode, 3, boundingBoxesDims);
    ANeuralNetworksOperandType classLabel =
            getOpType(ANEURALNETWORKS_TENSOR_INT32, 2, outputScoreDims);
    uint32_t numValidDims[1] = {numBatches};
    ANeuralNetworksOperandType numValid = getOpType(ANEURALNETWORKS_TENSOR_INT32, 1, numValidDims);

    OperationTestBase test(ANEURALNETWORKS_DETECTION_POSTPROCESSING,
                           {input, deltas, anchors, scaleScalar, scaleScalar, scaleScalar,
                            scaleScalar, isRegularNMS, maxNumDetections, numOfClassesPerDetection,
                            numOfDetections, scoreThreshold, iouThreshold, includeBackground},
                           {outputScore, boundingBoxes, classLabel, numValid});
    test.testOpsValidations();
}

TEST(OperationValidationTest, DETECTION_POSTPROCESSING) {
    detectionPostprocessingOpTest(ANEURALNETWORKS_TENSOR_FLOAT16);
    detectionPostprocessingOpTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

void preluOpTest(int32_t operandCode) {
    uint32_t inoutDim[] = {1, 2, 2, 3}, alphaDim[] = {1, 1, 3};
    OperationTestBase preluTest(
            ANEURALNETWORKS_PRELU,
            {getOpType(operandCode, 4, inoutDim), getOpType(operandCode, 3, alphaDim)},
            {getOpType(operandCode, 4, inoutDim)});
    preluTest.testOpsValidations();
}

TEST(OperationValidationTest, PRELU_float16) {
    preluOpTest(ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, PRELU_float32) {
    preluOpTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, PRELU_quant8) {
    preluOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, PRELU_quant8signed) {
    preluOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void normalizationOpTest(ANeuralNetworksOperationType operationCode, int32_t operandCode) {
    uint32_t inputDim[] = {2, 2, 2, 2};
    OperationTestBase normalizationTest(operationCode, {getOpType(operandCode, 4, inputDim)},
                                        {getOpType(operandCode, 4, inputDim)});
    normalizationTest.testOpsValidations();

    OperationTestBase normalizationAxisTest(
            operationCode, {getOpType(operandCode, 4, inputDim), getOpType(ANEURALNETWORKS_INT32)},
            {getOpType(operandCode, 4, inputDim)}, {{TensorRankConstraint::UpTo(4)}});
    normalizationAxisTest.testOpsValidations();
}

TEST(OperationValidationTest, L2_NORMALIZATION_float16) {
    normalizationOpTest(ANEURALNETWORKS_L2_NORMALIZATION, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, L2_NORMALIZATION_float32) {
    normalizationOpTest(ANEURALNETWORKS_L2_NORMALIZATION, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, L2_NORMALIZATION_quant8) {
    normalizationOpTest(ANEURALNETWORKS_L2_NORMALIZATION, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, L2_NORMALIZATION_quant8_signed) {
    normalizationOpTest(ANEURALNETWORKS_L2_NORMALIZATION,
                        ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void localResponseNormOpTest(int32_t operandCode) {
    int32_t floatScalarType = (operandCode == ANEURALNETWORKS_TENSOR_FLOAT32)
                                      ? ANEURALNETWORKS_FLOAT32
                                      : ANEURALNETWORKS_FLOAT16;
    uint32_t inputDim[] = {2, 2, 2, 6};
    OperationTestBase lrnTest(
            ANEURALNETWORKS_LOCAL_RESPONSE_NORMALIZATION,
            {getOpType(operandCode, 4, inputDim), getOpType(ANEURALNETWORKS_INT32),
             getOpType(floatScalarType), getOpType(floatScalarType), getOpType(floatScalarType)},
            {getOpType(operandCode, 4, inputDim)}, {{TensorRankConstraint::UpTo(4), {0}}});
    lrnTest.testOpsValidations();

    OperationTestBase lrnAxisTest(
            ANEURALNETWORKS_LOCAL_RESPONSE_NORMALIZATION,
            {getOpType(operandCode, 4, inputDim), getOpType(ANEURALNETWORKS_INT32),
             getOpType(floatScalarType), getOpType(floatScalarType), getOpType(floatScalarType),
             getOpType(ANEURALNETWORKS_INT32)},
            {getOpType(operandCode, 4, inputDim)}, {{TensorRankConstraint::UpTo(4), {0}}});
    lrnAxisTest.testOpsValidations();
}

TEST(OperationValidationTest, LOCAL_RESPONSE_NORMALIZATION_float16) {
    localResponseNormOpTest(ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, LOCAL_RESPONSE_NORMALIZATION_float32) {
    localResponseNormOpTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

void axisAlignedBBoxTransformOpTest(int32_t roiOperandCode, int32_t deltaOperandCode) {
    uint32_t roiDim[] = {5, 4}, deltaDim[] = {5, 8}, bsDim[] = {5}, imageDim[] = {5, 2};
    uint32_t outDim[] = {5, 8};
    OperationTestBase axisAlignedBBoxTransformTest(
            ANEURALNETWORKS_AXIS_ALIGNED_BBOX_TRANSFORM,
            {getOpType(roiOperandCode, 2, roiDim), getOpType(deltaOperandCode, 2, deltaDim),
             getOpType(ANEURALNETWORKS_TENSOR_INT32, 1, bsDim),
             getOpType(roiOperandCode, 2, imageDim)},
            {getOpType(roiOperandCode, 2, outDim)});
    axisAlignedBBoxTransformTest.testOpsValidations();
}

TEST(OperationValidationTest, AXIS_ALIGNED_BBOX_TRANSFORM_float16) {
    axisAlignedBBoxTransformOpTest(ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, AXIS_ALIGNED_BBOX_TRANSFORM_float32) {
    axisAlignedBBoxTransformOpTest(ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, AXIS_ALIGNED_BBOX_TRANSFORM_quant) {
    axisAlignedBBoxTransformOpTest(ANEURALNETWORKS_TENSOR_QUANT16_ASYMM,
                                   ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, AXIS_ALIGNED_BBOX_TRANSFORM_quant_signed) {
    axisAlignedBBoxTransformOpTest(ANEURALNETWORKS_TENSOR_QUANT16_ASYMM,
                                   ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void sliceTest(int32_t operandCode) {
    uint32_t inputDim[] = {3, 3, 3};
    uint32_t startDim[] = {3};
    uint32_t sizeDim[] = {3};
    uint32_t outputDim[] = {1, 2, 3};

    OperationTestBase sliceTest(ANEURALNETWORKS_SLICE,
                                {getOpType(operandCode, 3, inputDim),
                                 getOpType(ANEURALNETWORKS_TENSOR_INT32, 1, startDim),
                                 getOpType(ANEURALNETWORKS_TENSOR_INT32, 1, sizeDim)},
                                {getOpType(operandCode, 3, outputDim)});
    sliceTest.testOpsValidations();
}

TEST(OperationValidationTest, SLICE_float32) {
    sliceTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}
TEST(OperationValidationTest, SLICE_int32) {
    sliceTest(ANEURALNETWORKS_TENSOR_INT32);
}
TEST(OperationValidationTest, SLICE_uint8) {
    sliceTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}
TEST(OperationValidationTest, SLICE_int8) {
    sliceTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}
TEST(OperationValidationTest, SLICE_float16) {
    sliceTest(ANEURALNETWORKS_TENSOR_FLOAT16);
}

void logicalTest(ANeuralNetworksOperationType operationCode) {
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input1 = {.type = ANEURALNETWORKS_TENSOR_BOOL8,
                                         .dimensionCount = 4,
                                         .dimensions = inputDimensions,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};
    ANeuralNetworksOperandType input2 = input1;
    ANeuralNetworksOperandType output = input1;

    OperationTestBase test(operationCode, {input1, input2}, {output});
    test.testOpsValidations();
}

TEST(OperationValidationTest, LOGICAL_AND) {
    logicalTest(ANEURALNETWORKS_LOGICAL_AND);
}

TEST(OperationValidationTest, LOGICAL_OR) {
    logicalTest(ANEURALNETWORKS_LOGICAL_OR);
}

void comparisonTest(ANeuralNetworksOperationType operationCode, int32_t inputOperandType) {
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input1 = getOpType(inputOperandType, 4, inputDimensions);
    ANeuralNetworksOperandType input2 = input1;
    ANeuralNetworksOperandType output = {.type = ANEURALNETWORKS_TENSOR_BOOL8,
                                         .dimensionCount = 4,
                                         .dimensions = inputDimensions,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};
    OperationTestBase test(operationCode, {input1, input2}, {output});
    test.testOpsValidations();
}

TEST(OperationValidationTest, LESS) {
    comparisonTest(ANEURALNETWORKS_LESS, ANEURALNETWORKS_TENSOR_BOOL8);
    comparisonTest(ANEURALNETWORKS_LESS, ANEURALNETWORKS_TENSOR_FLOAT16);
    comparisonTest(ANEURALNETWORKS_LESS, ANEURALNETWORKS_TENSOR_FLOAT32);
    comparisonTest(ANEURALNETWORKS_LESS, ANEURALNETWORKS_TENSOR_INT32);
    comparisonTest(ANEURALNETWORKS_LESS, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    comparisonTest(ANEURALNETWORKS_LESS, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, LESS_EQUAL) {
    comparisonTest(ANEURALNETWORKS_LESS_EQUAL, ANEURALNETWORKS_TENSOR_BOOL8);
    comparisonTest(ANEURALNETWORKS_LESS_EQUAL, ANEURALNETWORKS_TENSOR_FLOAT16);
    comparisonTest(ANEURALNETWORKS_LESS_EQUAL, ANEURALNETWORKS_TENSOR_FLOAT32);
    comparisonTest(ANEURALNETWORKS_LESS_EQUAL, ANEURALNETWORKS_TENSOR_INT32);
    comparisonTest(ANEURALNETWORKS_LESS_EQUAL, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    comparisonTest(ANEURALNETWORKS_LESS_EQUAL, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, EQUAL) {
    comparisonTest(ANEURALNETWORKS_EQUAL, ANEURALNETWORKS_TENSOR_BOOL8);
    comparisonTest(ANEURALNETWORKS_EQUAL, ANEURALNETWORKS_TENSOR_FLOAT16);
    comparisonTest(ANEURALNETWORKS_EQUAL, ANEURALNETWORKS_TENSOR_FLOAT32);
    comparisonTest(ANEURALNETWORKS_EQUAL, ANEURALNETWORKS_TENSOR_INT32);
    comparisonTest(ANEURALNETWORKS_EQUAL, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    comparisonTest(ANEURALNETWORKS_EQUAL, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, NOT_EQUAL) {
    comparisonTest(ANEURALNETWORKS_NOT_EQUAL, ANEURALNETWORKS_TENSOR_BOOL8);
    comparisonTest(ANEURALNETWORKS_NOT_EQUAL, ANEURALNETWORKS_TENSOR_FLOAT16);
    comparisonTest(ANEURALNETWORKS_NOT_EQUAL, ANEURALNETWORKS_TENSOR_FLOAT32);
    comparisonTest(ANEURALNETWORKS_NOT_EQUAL, ANEURALNETWORKS_TENSOR_INT32);
    comparisonTest(ANEURALNETWORKS_NOT_EQUAL, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    comparisonTest(ANEURALNETWORKS_NOT_EQUAL, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, GREATER) {
    comparisonTest(ANEURALNETWORKS_GREATER, ANEURALNETWORKS_TENSOR_BOOL8);
    comparisonTest(ANEURALNETWORKS_GREATER, ANEURALNETWORKS_TENSOR_FLOAT16);
    comparisonTest(ANEURALNETWORKS_GREATER, ANEURALNETWORKS_TENSOR_FLOAT32);
    comparisonTest(ANEURALNETWORKS_GREATER, ANEURALNETWORKS_TENSOR_INT32);
    comparisonTest(ANEURALNETWORKS_GREATER, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    comparisonTest(ANEURALNETWORKS_GREATER, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, GREATER_EQUAL) {
    comparisonTest(ANEURALNETWORKS_GREATER_EQUAL, ANEURALNETWORKS_TENSOR_BOOL8);
    comparisonTest(ANEURALNETWORKS_GREATER_EQUAL, ANEURALNETWORKS_TENSOR_FLOAT16);
    comparisonTest(ANEURALNETWORKS_GREATER_EQUAL, ANEURALNETWORKS_TENSOR_FLOAT32);
    comparisonTest(ANEURALNETWORKS_GREATER_EQUAL, ANEURALNETWORKS_TENSOR_INT32);
    comparisonTest(ANEURALNETWORKS_GREATER_EQUAL, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    comparisonTest(ANEURALNETWORKS_GREATER_EQUAL, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void reduceOpTest(ANeuralNetworksOperationType operationCode, int32_t inputOperandType) {
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input1 = getOpType(inputOperandType, 4, inputDimensions);
    uint32_t axesDimensions[1] = {2};
    ANeuralNetworksOperandType input2 = getOpType(ANEURALNETWORKS_TENSOR_INT32, 1, axesDimensions);
    ANeuralNetworksOperandType input3 = getOpType(ANEURALNETWORKS_BOOL, 0);
    ANeuralNetworksOperandType output = getOpType(inputOperandType, 4, inputDimensions);
    OperationTestBase test(operationCode, {input1, input2, input3}, {output},
                           {{TensorRankConstraint::UpTo(4)}});
    test.testOpsValidations();
}

TEST(OperationValidationTest, REDUCE_PROD) {
    reduceOpTest(ANEURALNETWORKS_REDUCE_PROD, ANEURALNETWORKS_TENSOR_FLOAT16);
    reduceOpTest(ANEURALNETWORKS_REDUCE_PROD, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, REDUCE_SUM) {
    reduceOpTest(ANEURALNETWORKS_REDUCE_SUM, ANEURALNETWORKS_TENSOR_FLOAT16);
    reduceOpTest(ANEURALNETWORKS_REDUCE_SUM, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, REDUCE_MAX) {
    reduceOpTest(ANEURALNETWORKS_REDUCE_MAX, ANEURALNETWORKS_TENSOR_FLOAT16);
    reduceOpTest(ANEURALNETWORKS_REDUCE_MAX, ANEURALNETWORKS_TENSOR_FLOAT32);
    reduceOpTest(ANEURALNETWORKS_REDUCE_MAX, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    reduceOpTest(ANEURALNETWORKS_REDUCE_MAX, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, REDUCE_MIN) {
    reduceOpTest(ANEURALNETWORKS_REDUCE_MIN, ANEURALNETWORKS_TENSOR_FLOAT16);
    reduceOpTest(ANEURALNETWORKS_REDUCE_MIN, ANEURALNETWORKS_TENSOR_FLOAT32);
    reduceOpTest(ANEURALNETWORKS_REDUCE_MIN, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    reduceOpTest(ANEURALNETWORKS_REDUCE_MIN, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

TEST(OperationValidationTest, REDUCE_ANY) {
    reduceOpTest(ANEURALNETWORKS_REDUCE_ANY, ANEURALNETWORKS_TENSOR_BOOL8);
}

TEST(OperationValidationTest, REDUCE_ALL) {
    reduceOpTest(ANEURALNETWORKS_REDUCE_ALL, ANEURALNETWORKS_TENSOR_BOOL8);
}

void selectTest(ANeuralNetworksOperationType operationCode, int32_t inputOperandType) {
    uint32_t inputDimensions[4] = {2, 2, 2, 2};
    ANeuralNetworksOperandType input0 = getOpType(ANEURALNETWORKS_TENSOR_BOOL8, 4, inputDimensions);
    ANeuralNetworksOperandType input1 = getOpType(inputOperandType, 4, inputDimensions);
    ANeuralNetworksOperandType input2 = input1;
    ANeuralNetworksOperandType output = input1;

    OperationTestBase test(operationCode, {input0, input1, input2}, {output});
    test.testOpsValidations();
}

TEST(OperationValidationTest, SELECT) {
    selectTest(ANEURALNETWORKS_SELECT, ANEURALNETWORKS_TENSOR_FLOAT16);
    selectTest(ANEURALNETWORKS_SELECT, ANEURALNETWORKS_TENSOR_FLOAT32);
    selectTest(ANEURALNETWORKS_SELECT, ANEURALNETWORKS_TENSOR_INT32);
    selectTest(ANEURALNETWORKS_SELECT, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
    selectTest(ANEURALNETWORKS_SELECT, ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

void powTest(int32_t inputOperandType) {
    const uint32_t inputDimensions[] = {3, 3};
    ANeuralNetworksOperandType inputType = {.type = inputOperandType,
                                            .dimensionCount = 2,
                                            .dimensions = inputDimensions,
                                            .scale = 0.0f,
                                            .zeroPoint = 0};

    OperationTestBase test(ANEURALNETWORKS_POW, {inputType, inputType}, {inputType});
    test.testOpsValidations();
}

TEST(OperationValidationTest, POW) {
    powTest(ANEURALNETWORKS_TENSOR_FLOAT16);
    powTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

void boxWithNmsLimitOpTest(int32_t scoreOperandCode, int32_t roiOperandCode,
                           int32_t scalarOperandCode) {
    uint32_t scoreDim[] = {19, 3}, roiDim[] = {19, 12}, splitDim[] = {2};
    uint32_t outScoreDim[] = {12}, outRoiDim[] = {12, 4}, outClassDim[] = {12}, outSplitDim[] = {2};
    OperationTestBase boxWithNmsLimitTest(
            ANEURALNETWORKS_BOX_WITH_NMS_LIMIT,
            {getOpType(scoreOperandCode, 2, scoreDim), getOpType(roiOperandCode, 2, roiDim),
             getOpType(ANEURALNETWORKS_TENSOR_INT32, 1, splitDim), getOpType(scalarOperandCode),
             getOpType(ANEURALNETWORKS_INT32), getOpType(ANEURALNETWORKS_INT32),
             getOpType(scalarOperandCode), getOpType(scalarOperandCode),
             getOpType(scalarOperandCode)},
            {getOpType(scoreOperandCode, 1, outScoreDim), getOpType(roiOperandCode, 2, outRoiDim),
             getOpType(ANEURALNETWORKS_TENSOR_INT32, 1, outClassDim),
             getOpType(ANEURALNETWORKS_TENSOR_INT32, 1, outSplitDim)});
    boxWithNmsLimitTest.testOpsValidations();
}

TEST(OperationValidationTest, BOX_WITH_NMS_LIMIT_float16) {
    boxWithNmsLimitOpTest(ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_TENSOR_FLOAT16,
                          ANEURALNETWORKS_FLOAT16);
}

TEST(OperationValidationTest, BOX_WITH_NMS_LIMIT_float32) {
    boxWithNmsLimitOpTest(ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_TENSOR_FLOAT32,
                          ANEURALNETWORKS_FLOAT32);
}

TEST(OperationValidationTest, BOX_WITH_NMS_LIMIT_quant) {
    boxWithNmsLimitOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, ANEURALNETWORKS_TENSOR_QUANT16_ASYMM,
                          ANEURALNETWORKS_FLOAT32);
}

TEST(OperationValidationTest, BOX_WITH_NMS_LIMIT_quant_signed) {
    boxWithNmsLimitOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED,
                          ANEURALNETWORKS_TENSOR_QUANT16_ASYMM, ANEURALNETWORKS_FLOAT32);
}

void castOpTest(int32_t inputOperandCode, int32_t outputOperandCode) {
    SCOPED_TRACE(testing::Message()
                 << "inputType: " << inputOperandCode << ", outputType: " << outputOperandCode);
    uint32_t inputDimensions[3] = {2, 2, 2};
    ANeuralNetworksOperandType input = getOpType(inputOperandCode, 3, inputDimensions);
    ANeuralNetworksOperandType output = getOpType(outputOperandCode, 3, inputDimensions);
    OperationTestBase test(ANEURALNETWORKS_CAST, {input}, {output});
    test.testOpsValidations();
}

TEST(OperationValidationTest, CAST) {
    std::vector<int32_t> inputTypes = {ANEURALNETWORKS_TENSOR_FLOAT16,
                                       ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_TENSOR_INT32,
                                       ANEURALNETWORKS_TENSOR_QUANT8_ASYMM};
    std::vector<int32_t> outputTypes = inputTypes;
    for (auto inputType : inputTypes) {
        for (auto outputType : outputTypes) {
            castOpTest(inputType, outputType);
        }
    }
}

TEST(OperationValidationTest, CAST_identity) {
    std::vector<int32_t> inputTypes = {
            ANEURALNETWORKS_TENSOR_FLOAT32,
            ANEURALNETWORKS_TENSOR_INT32,
            ANEURALNETWORKS_TENSOR_QUANT8_ASYMM,
            ANEURALNETWORKS_TENSOR_QUANT16_SYMM,
            ANEURALNETWORKS_TENSOR_FLOAT16,
            ANEURALNETWORKS_TENSOR_BOOL8,
            ANEURALNETWORKS_TENSOR_QUANT16_ASYMM,
            ANEURALNETWORKS_TENSOR_QUANT8_SYMM,
            ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED,
    };
    for (auto inputType : inputTypes) {
        castOpTest(inputType, inputType);
    }
}

void bidirectionlSequenceRNNTest(int32_t inputOperandCode) {
    const uint32_t batchSize = 2;
    const uint32_t maxTime = 3;
    const uint32_t inputSize = 4;
    const uint32_t numUnits = 5;

    uint32_t inputDims[3] = {maxTime, batchSize, inputSize};
    uint32_t weightsDims[2] = {inputSize, numUnits};
    uint32_t recurrentWeightsDims[2] = {numUnits, numUnits};
    uint32_t biasDims[1] = {numUnits};
    uint32_t hiddenStateDims[2] = {batchSize, numUnits};
    uint32_t outputDims[2] = {batchSize, numUnits};

    ANeuralNetworksOperandType input = {.type = inputOperandCode,
                                        .dimensionCount = 3,
                                        .dimensions = inputDims,
                                        .scale = 0.0f,
                                        .zeroPoint = 0};
    ANeuralNetworksOperandType fwWeights = {.type = inputOperandCode,
                                            .dimensionCount = 2,
                                            .dimensions = weightsDims,
                                            .scale = 0.0f,
                                            .zeroPoint = 0};
    ANeuralNetworksOperandType bwWeights = fwWeights;
    ANeuralNetworksOperandType fwRecurrentWeights = {.type = inputOperandCode,
                                                     .dimensionCount = 2,
                                                     .dimensions = recurrentWeightsDims,
                                                     .scale = 0.0f,
                                                     .zeroPoint = 0};
    ANeuralNetworksOperandType bwRecurrentWeights = fwRecurrentWeights;
    ANeuralNetworksOperandType fwBias = {.type = inputOperandCode,
                                         .dimensionCount = 1,
                                         .dimensions = biasDims,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};
    ANeuralNetworksOperandType bwBias = fwBias;
    ANeuralNetworksOperandType fwHiddenState = {.type = inputOperandCode,
                                                .dimensionCount = 2,
                                                .dimensions = hiddenStateDims,
                                                .scale = 0.0f,
                                                .zeroPoint = 0};
    ANeuralNetworksOperandType bwHiddenState = fwHiddenState;
    ANeuralNetworksOperandType output = {.type = inputOperandCode,
                                         .dimensionCount = 2,
                                         .dimensions = outputDims,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};
    ANeuralNetworksOperandType activation = {.type = ANEURALNETWORKS_INT32,
                                             .dimensionCount = 0,
                                             .dimensions = nullptr,
                                             .scale = 0.0f,
                                             .zeroPoint = 0};
    ANeuralNetworksOperandType boolScalar = {.type = ANEURALNETWORKS_BOOL,
                                             .dimensionCount = 0,
                                             .dimensions = nullptr,
                                             .scale = 0.0f,
                                             .zeroPoint = 0};
    ANeuralNetworksOperandType timeMajor = boolScalar;
    ANeuralNetworksOperandType mergeOutputs = boolScalar;

    OperationTestBase rnnTest(ANEURALNETWORKS_BIDIRECTIONAL_SEQUENCE_RNN,
                              {input, fwWeights, fwRecurrentWeights, fwBias, fwHiddenState,
                               bwWeights, bwRecurrentWeights, bwBias, bwHiddenState, input,
                               fwWeights, bwWeights, activation, timeMajor, mergeOutputs},
                              {output, output});
    rnnTest.testOpsValidations();
}

TEST(OperationValidationTest, BIDIRECTIONAL_SEQUENCE_RNN_float32) {
    bidirectionlSequenceRNNTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, BIDIRECTIONAL_SEQUENCE_RNN_float16) {
    bidirectionlSequenceRNNTest(ANEURALNETWORKS_TENSOR_FLOAT16);
}

void unidirectionlSequenceRNNTest(int32_t inputOperandCode) {
    const uint32_t batchSize = 2;
    const uint32_t maxTime = 3;
    const uint32_t inputSize = 4;
    const uint32_t numUnits = 5;

    uint32_t inputDims[3] = {maxTime, batchSize, inputSize};
    uint32_t weightsDims[2] = {inputSize, numUnits};
    uint32_t recurrentWeightsDims[2] = {numUnits, numUnits};
    uint32_t biasDims[1] = {numUnits};
    uint32_t hiddenStateDims[2] = {batchSize, numUnits};
    uint32_t outputDims[2] = {batchSize, numUnits};

    ANeuralNetworksOperandType input = {.type = inputOperandCode,
                                        .dimensionCount = 3,
                                        .dimensions = inputDims,
                                        .scale = 0.0f,
                                        .zeroPoint = 0};
    ANeuralNetworksOperandType weights = {.type = inputOperandCode,
                                          .dimensionCount = 2,
                                          .dimensions = weightsDims,
                                          .scale = 0.0f,
                                          .zeroPoint = 0};
    ANeuralNetworksOperandType recurrentWeights = {.type = inputOperandCode,
                                                   .dimensionCount = 2,
                                                   .dimensions = recurrentWeightsDims,
                                                   .scale = 0.0f,
                                                   .zeroPoint = 0};
    ANeuralNetworksOperandType bias = {.type = inputOperandCode,
                                       .dimensionCount = 1,
                                       .dimensions = biasDims,
                                       .scale = 0.0f,
                                       .zeroPoint = 0};
    ANeuralNetworksOperandType hiddenState = {.type = inputOperandCode,
                                              .dimensionCount = 2,
                                              .dimensions = hiddenStateDims,
                                              .scale = 0.0f,
                                              .zeroPoint = 0};
    ANeuralNetworksOperandType output = {.type = inputOperandCode,
                                         .dimensionCount = 2,
                                         .dimensions = outputDims,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};
    ANeuralNetworksOperandType intScalar = {.type = ANEURALNETWORKS_INT32,
                                            .dimensionCount = 0,
                                            .dimensions = nullptr,
                                            .scale = 0.0f,
                                            .zeroPoint = 0};
    ANeuralNetworksOperandType activation = intScalar;
    ANeuralNetworksOperandType timeMajor = intScalar;

    OperationTestBase rnnTest(
            ANEURALNETWORKS_UNIDIRECTIONAL_SEQUENCE_RNN,
            {input, weights, recurrentWeights, bias, hiddenState, activation, timeMajor}, {output});
    rnnTest.testOpsValidations();
}

TEST(OperationValidationTest, UNIDIRECTIONAL_SEQUENCE_RNN_float32) {
    unidirectionlSequenceRNNTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, UNIDIRECTIONAL_SEQUENCE_RNN_float16) {
    unidirectionlSequenceRNNTest(ANEURALNETWORKS_TENSOR_FLOAT16);
}

void unidirectionalSequenceLSTMTest(int32_t inputOperandCode) {
    const uint32_t maxTime = 2;
    const uint32_t batchSize = 3;
    const uint32_t numUnits = 4;
    const uint32_t inputSize = 5;
    const uint32_t outputSize = 6;

    uint32_t inputDims[3] = {maxTime, batchSize, inputSize};
    uint32_t inputWeightsDims[2] = {numUnits, inputSize};
    uint32_t recurrentWeightsDims[2] = {numUnits, outputSize};
    uint32_t diagonalDims[1] = {numUnits};
    uint32_t projectionDims[2] = {outputSize, numUnits};
    uint32_t projectionBiasDims[1] = {outputSize};
    uint32_t outputStateDims[2] = {batchSize, outputSize};
    uint32_t cellStateDims[2] = {batchSize, numUnits};

    uint32_t outputDims[3] = {maxTime, batchSize, outputSize};

    ANeuralNetworksOperandType input = {.type = inputOperandCode,
                                        .dimensionCount = 3,
                                        .dimensions = inputDims,
                                        .scale = 0.0f,
                                        .zeroPoint = 0};
    ANeuralNetworksOperandType inputToInputWeights = {.type = inputOperandCode,
                                                      .dimensionCount = 2,
                                                      .dimensions = inputWeightsDims,
                                                      .scale = 0.0f,
                                                      .zeroPoint = 0};
    ANeuralNetworksOperandType inputToForgetWeights = inputToInputWeights;
    ANeuralNetworksOperandType inputToCellWeights = inputToInputWeights;
    ANeuralNetworksOperandType inputToOutputWeights = inputToInputWeights;
    ANeuralNetworksOperandType recurrentToInputWeights = {.type = inputOperandCode,
                                                          .dimensionCount = 2,
                                                          .dimensions = recurrentWeightsDims,
                                                          .scale = 0.0f,
                                                          .zeroPoint = 0};
    ANeuralNetworksOperandType recurrentToForgetWeights = recurrentToInputWeights;
    ANeuralNetworksOperandType recurrentToCellWeights = recurrentToInputWeights;
    ANeuralNetworksOperandType recurrentToOutputWeights = recurrentToInputWeights;
    ANeuralNetworksOperandType cellToInputWeights = {.type = inputOperandCode,
                                                     .dimensionCount = 1,
                                                     .dimensions = diagonalDims,
                                                     .scale = 0.0f,
                                                     .zeroPoint = 0};
    ANeuralNetworksOperandType cellToForgetWeights = cellToInputWeights;
    ANeuralNetworksOperandType cellToOutputWeights = cellToInputWeights;
    ANeuralNetworksOperandType inputGateBias = {.type = inputOperandCode,
                                                .dimensionCount = 1,
                                                .dimensions = diagonalDims,
                                                .scale = 0.0f,
                                                .zeroPoint = 0};
    ANeuralNetworksOperandType forgetGateBias = inputGateBias;
    ANeuralNetworksOperandType cellGateBias = inputGateBias;
    ANeuralNetworksOperandType outputGateBias = inputGateBias;
    ANeuralNetworksOperandType projectionWeights = {.type = inputOperandCode,
                                                    .dimensionCount = 2,
                                                    .dimensions = projectionDims,
                                                    .scale = 0.0f,
                                                    .zeroPoint = 0};
    ANeuralNetworksOperandType projectionBias = {.type = inputOperandCode,
                                                 .dimensionCount = 1,
                                                 .dimensions = projectionBiasDims,
                                                 .scale = 0.0f,
                                                 .zeroPoint = 0};
    ANeuralNetworksOperandType outputStateIn = {.type = inputOperandCode,
                                                .dimensionCount = 2,
                                                .dimensions = outputStateDims,
                                                .scale = 0.0f,
                                                .zeroPoint = 0};
    ANeuralNetworksOperandType cellStateIn = {.type = inputOperandCode,
                                              .dimensionCount = 2,
                                              .dimensions = cellStateDims,
                                              .scale = 0.0f,
                                              .zeroPoint = 0};
    ANeuralNetworksOperandType intScalar = {
            .type = ANEURALNETWORKS_INT32,
            .dimensionCount = 0,
            .dimensions = nullptr,
            .scale = 0.0f,
            .zeroPoint = 0,
    };
    ANeuralNetworksOperandType activation = intScalar;
    ANeuralNetworksOperandType floatScalar = {
            .type = inputOperandCode == ANEURALNETWORKS_TENSOR_FLOAT32 ? ANEURALNETWORKS_FLOAT32
                                                                       : ANEURALNETWORKS_FLOAT16,
            .dimensionCount = 0,
            .dimensions = nullptr,
            .scale = 0.0f,
            .zeroPoint = 0,
    };
    ANeuralNetworksOperandType cellClip = floatScalar;
    ANeuralNetworksOperandType projClip = floatScalar;
    ANeuralNetworksOperandType boolScalar = {
            .type = ANEURALNETWORKS_BOOL,
            .dimensionCount = 0,
            .dimensions = nullptr,
            .scale = 0.0f,
            .zeroPoint = 0,
    };
    ANeuralNetworksOperandType timeMajor = boolScalar;
    ANeuralNetworksOperandType inputLayerNormWeights = {.type = inputOperandCode,
                                                        .dimensionCount = 1,
                                                        .dimensions = diagonalDims,
                                                        .scale = 0.0f,
                                                        .zeroPoint = 0};
    ANeuralNetworksOperandType forgetLayerNormWeights = inputLayerNormWeights;
    ANeuralNetworksOperandType cellLayerNormWeights = inputLayerNormWeights;
    ANeuralNetworksOperandType outputLayerNormWeights = inputLayerNormWeights;

    ANeuralNetworksOperandType output = {.type = inputOperandCode,
                                         .dimensionCount = 3,
                                         .dimensions = outputDims,
                                         .scale = 0.0f,
                                         .zeroPoint = 0};

    OperationTestBase ulstmTest(ANEURALNETWORKS_UNIDIRECTIONAL_SEQUENCE_LSTM,
                                {input,
                                 inputToInputWeights,
                                 inputToForgetWeights,
                                 inputToCellWeights,
                                 inputToOutputWeights,
                                 recurrentToInputWeights,
                                 recurrentToForgetWeights,
                                 recurrentToCellWeights,
                                 recurrentToOutputWeights,
                                 cellToInputWeights,
                                 cellToForgetWeights,
                                 cellToOutputWeights,
                                 inputGateBias,
                                 forgetGateBias,
                                 cellGateBias,
                                 outputGateBias,
                                 projectionWeights,
                                 projectionBias,
                                 outputStateIn,
                                 cellStateIn,
                                 activation,
                                 cellClip,
                                 projClip,
                                 timeMajor,
                                 inputLayerNormWeights,
                                 forgetLayerNormWeights,
                                 cellLayerNormWeights,
                                 outputLayerNormWeights},
                                {output});
    ulstmTest.testOpsValidations();
}

TEST(OperationValidationTest, UNIDIRECTIONAL_SEQUENCE_LSTM_float32) {
    unidirectionalSequenceLSTMTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, UNIDIRECTIONAL_SEQUENCE_LSTM_float16) {
    unidirectionalSequenceLSTMTest(ANEURALNETWORKS_TENSOR_FLOAT16);
}

void generateProposalsOpTest(int32_t scoreOperandCode, int32_t deltaOperandCode,
                             int32_t anchorOperandCode, int32_t roiOperandCode,
                             int32_t scalarOperandCode) {
    uint32_t scoreDim[] = {1, 2, 2, 2}, deltaDim[] = {1, 2, 2, 8}, anchorDim[] = {2, 4},
             imageInfoDim[] = {1, 2};
    uint32_t outScoreDim[] = {4}, outRoiDim[] = {4, 4}, outSplitDim[] = {1};
    OperationTestBase generateProposalsTest(
            ANEURALNETWORKS_GENERATE_PROPOSALS,
            {getOpType(scoreOperandCode, 4, scoreDim), getOpType(deltaOperandCode, 4, deltaDim),
             getOpType(anchorOperandCode, 2, anchorDim), getOpType(roiOperandCode, 2, imageInfoDim),
             getOpType(scalarOperandCode), getOpType(scalarOperandCode),
             getOpType(ANEURALNETWORKS_INT32), getOpType(ANEURALNETWORKS_INT32),
             getOpType(scalarOperandCode), getOpType(scalarOperandCode),
             getOpType(ANEURALNETWORKS_BOOL)},
            {getOpType(scoreOperandCode, 1, outScoreDim), getOpType(roiOperandCode, 2, outRoiDim),
             getOpType(ANEURALNETWORKS_TENSOR_INT32, 1, outSplitDim)});
    generateProposalsTest.testOpsValidations();
}

TEST(OperationValidationTest, GENERATE_PROPOSALS_float16) {
    generateProposalsOpTest(ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_TENSOR_FLOAT16,
                            ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_TENSOR_FLOAT16,
                            ANEURALNETWORKS_FLOAT16);
}

TEST(OperationValidationTest, GENERATE_PROPOSALS_float32) {
    generateProposalsOpTest(ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_TENSOR_FLOAT32,
                            ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_TENSOR_FLOAT32,
                            ANEURALNETWORKS_FLOAT32);
}

TEST(OperationValidationTest, GENERATE_PROPOSALS_quant) {
    generateProposalsOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM,
                            ANEURALNETWORKS_TENSOR_QUANT8_ASYMM,
                            ANEURALNETWORKS_TENSOR_QUANT16_SYMM,
                            ANEURALNETWORKS_TENSOR_QUANT16_ASYMM, ANEURALNETWORKS_FLOAT32);
}

TEST(OperationValidationTest, GENERATE_PROPOSALS_quant_signed) {
    generateProposalsOpTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED,
                            ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED,
                            ANEURALNETWORKS_TENSOR_QUANT16_SYMM,
                            ANEURALNETWORKS_TENSOR_QUANT16_ASYMM, ANEURALNETWORKS_FLOAT32);
}

void resizeNearestNeighborTest(int32_t inputCode, int32_t scalarCode) {
    uint32_t inputDim[] = {1, 2, 2, 1}, outputDim[] = {1, 1, 1, 1};
    OperationTestBase resizeImageOpTest(ANEURALNETWORKS_RESIZE_NEAREST_NEIGHBOR,
                                        {getOpType(inputCode, 4, inputDim), getOpType(scalarCode),
                                         getOpType(scalarCode), getOpType(ANEURALNETWORKS_BOOL)},
                                        {getOpType(inputCode, 4, outputDim)});
    resizeImageOpTest.testOpsValidations();
}

TEST(OperationValidationTest, RESIZE_NEAREST_NEIGHBOR) {
    resizeNearestNeighborTest(ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_INT32);
    resizeNearestNeighborTest(ANEURALNETWORKS_TENSOR_FLOAT32, ANEURALNETWORKS_FLOAT32);
}

TEST(OperationValidationTest, RESIZE_NEAREST_NEIGHBOR_float16) {
    resizeNearestNeighborTest(ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_INT32);
    resizeNearestNeighborTest(ANEURALNETWORKS_TENSOR_FLOAT16, ANEURALNETWORKS_FLOAT16);
}

TEST(OperationValidationTest, RESIZE_NEAREST_NEIGHBOR_quant8) {
    resizeNearestNeighborTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, ANEURALNETWORKS_INT32);
    resizeNearestNeighborTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, ANEURALNETWORKS_FLOAT32);
}

TEST(OperationValidationTest, RESIZE_NEAREST_NEIGHBOR_quant8_signed) {
    resizeNearestNeighborTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED, ANEURALNETWORKS_INT32);
    resizeNearestNeighborTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED, ANEURALNETWORKS_FLOAT32);
}

TEST(OperationValidationTest, QUANTIZED_LSTM) {
    uint32_t oneDimensional[1] = {5};
    uint32_t twoDimensional[2] = {5, 5};

    ANeuralNetworksOperandType quant8AsymSignedTensor2D = {
            .type = ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED,
            .dimensionCount = 2,
            .dimensions = twoDimensional,
            .scale = 0.0078125,
            .zeroPoint = 0,
    };
    ANeuralNetworksOperandType quant8SymTensor2D = {
            .type = ANEURALNETWORKS_TENSOR_QUANT8_SYMM,
            .dimensionCount = 2,
            .dimensions = twoDimensional,
            .scale = 0.0078125,
            .zeroPoint = 0,
    };
    ANeuralNetworksOperandType quant16SymTensor1D = {
            .type = ANEURALNETWORKS_TENSOR_QUANT16_SYMM,
            .dimensionCount = 1,
            .dimensions = oneDimensional,
            .scale = 1.0,
            .zeroPoint = 0,
    };
    ANeuralNetworksOperandType quant16SymTensor2D = {
            .type = ANEURALNETWORKS_TENSOR_QUANT16_SYMM,
            .dimensionCount = 2,
            .dimensions = twoDimensional,
            .scale = 1.0,
            .zeroPoint = 0,
    };
    ANeuralNetworksOperandType int32Tensor1D = {
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = 1,
            .dimensions = oneDimensional,
            .scale = 4.65661e-08,
            .zeroPoint = 0,
    };
    ANeuralNetworksOperandType int32Scalar = {
            .type = ANEURALNETWORKS_INT32,
    };
    ANeuralNetworksOperandType float32Scalar = {
            .type = ANEURALNETWORKS_FLOAT32,
    };

    ANeuralNetworksOperandType input = quant8AsymSignedTensor2D;
    ANeuralNetworksOperandType input_to_input_weights = quant8SymTensor2D;
    ANeuralNetworksOperandType input_to_forget_weights = quant8SymTensor2D;
    ANeuralNetworksOperandType input_to_cell_weights = quant8SymTensor2D;
    ANeuralNetworksOperandType input_to_output_weights = quant8SymTensor2D;
    ANeuralNetworksOperandType recurrent_to_input_weights = quant8SymTensor2D;
    ANeuralNetworksOperandType recurrent_to_forget_weights = quant8SymTensor2D;
    ANeuralNetworksOperandType recurrent_to_cell_weights = quant8SymTensor2D;
    ANeuralNetworksOperandType recurrent_to_output_weights = quant8SymTensor2D;
    ANeuralNetworksOperandType cell_to_input_weights = quant16SymTensor2D;
    ANeuralNetworksOperandType cell_to_forget_weights = quant16SymTensor2D;
    ANeuralNetworksOperandType cell_to_output_weights = quant16SymTensor2D;
    ANeuralNetworksOperandType input_gate_bias = int32Tensor1D;
    ANeuralNetworksOperandType forget_gate_bias = int32Tensor1D;
    ANeuralNetworksOperandType cell_gate_bias = int32Tensor1D;
    ANeuralNetworksOperandType output_gate_bias = int32Tensor1D;
    ANeuralNetworksOperandType projection_weights = quant8SymTensor2D;
    ANeuralNetworksOperandType projection_bias = int32Tensor1D;
    ANeuralNetworksOperandType output_state_in = quant8AsymSignedTensor2D;
    ANeuralNetworksOperandType cell_state_in = quant16SymTensor2D;
    ANeuralNetworksOperandType input_layer_norm_weights = quant16SymTensor1D;
    ANeuralNetworksOperandType forget_layer_norm_weights = quant16SymTensor1D;
    ANeuralNetworksOperandType cell_layer_norm_weights = quant16SymTensor1D;
    ANeuralNetworksOperandType output_layer_norm_weights = quant16SymTensor1D;
    ANeuralNetworksOperandType cell_clip = float32Scalar;
    ANeuralNetworksOperandType projection_clip = float32Scalar;
    ANeuralNetworksOperandType input_intermediate_scale = float32Scalar;
    ANeuralNetworksOperandType forget_intermediate_scale = float32Scalar;
    ANeuralNetworksOperandType cell_intermediate_scale = float32Scalar;
    ANeuralNetworksOperandType output_intermediate_scale = float32Scalar;
    ANeuralNetworksOperandType hidden_state_zero_point = int32Scalar;
    ANeuralNetworksOperandType hidden_state_scale = float32Scalar;

    ANeuralNetworksOperandType output_state_out = quant8AsymSignedTensor2D;
    ANeuralNetworksOperandType cell_state_out = quant16SymTensor2D;
    ANeuralNetworksOperandType output = quant8AsymSignedTensor2D;

    OperationTestBase test(ANEURALNETWORKS_QUANTIZED_LSTM,
                           {input,
                            input_to_input_weights,
                            input_to_forget_weights,
                            input_to_cell_weights,
                            input_to_output_weights,
                            recurrent_to_input_weights,
                            recurrent_to_forget_weights,
                            recurrent_to_cell_weights,
                            recurrent_to_output_weights,
                            cell_to_input_weights,
                            cell_to_forget_weights,
                            cell_to_output_weights,
                            input_gate_bias,
                            forget_gate_bias,
                            cell_gate_bias,
                            output_gate_bias,
                            projection_weights,
                            projection_bias,
                            output_state_in,
                            cell_state_in,
                            input_layer_norm_weights,
                            forget_layer_norm_weights,
                            cell_layer_norm_weights,
                            output_layer_norm_weights,
                            cell_clip,
                            projection_clip,
                            input_intermediate_scale,
                            forget_intermediate_scale,
                            cell_intermediate_scale,
                            output_intermediate_scale,
                            hidden_state_zero_point,
                            hidden_state_scale},
                           {output_state_out, cell_state_out, output});
    test.testOpsValidations();
}

void fillTest(int32_t valueOperandType, int32_t outputOperandType) {
    uint32_t inputDimensions[1] = {3};
    ANeuralNetworksOperandType input0 = getOpType(ANEURALNETWORKS_TENSOR_INT32, 1, inputDimensions);
    ANeuralNetworksOperandType input1 = getOpType(valueOperandType);
    uint32_t outputDimensions[3] = {3, 4, 5};
    ANeuralNetworksOperandType output = getOpType(outputOperandType, 3, outputDimensions);
    OperationTestBase test(ANEURALNETWORKS_FILL, {input0, input1}, {output});
    test.testOpsValidations();
}

TEST(OperationValidationTest, FILL_float16) {
    fillTest(ANEURALNETWORKS_FLOAT16, ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, FILL_float32) {
    fillTest(ANEURALNETWORKS_FLOAT32, ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, FILL_int32) {
    fillTest(ANEURALNETWORKS_INT32, ANEURALNETWORKS_TENSOR_INT32);
}

void rankTest(int32_t inputOperandType) {
    uint32_t inputDimensions[3] = {3, 4, 5};
    ANeuralNetworksOperandType input = getOpType(inputOperandType, 3, inputDimensions);
    ANeuralNetworksOperandType output = getOpType(ANEURALNETWORKS_INT32);
    OperationTestBase test(ANEURALNETWORKS_RANK, {input}, {output});
    test.testOpsValidations();
}

TEST(OperationValidationTest, RANK_float16) {
    rankTest(ANEURALNETWORKS_TENSOR_FLOAT16);
}

TEST(OperationValidationTest, RANK_float32) {
    rankTest(ANEURALNETWORKS_TENSOR_FLOAT32);
}

TEST(OperationValidationTest, RANK_int32) {
    rankTest(ANEURALNETWORKS_TENSOR_INT32);
}

TEST(OperationValidationTest, RANK_quant8) {
    rankTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM);
}

TEST(OperationValidationTest, RANK_quant8_signed) {
    rankTest(ANEURALNETWORKS_TENSOR_QUANT8_ASYMM_SIGNED);
}

ANeuralNetworksModel* makeIdentityModel(const ANeuralNetworksOperandType* type) {
    ANeuralNetworksModel* model = nullptr;
    EXPECT_EQ(ANeuralNetworksModel_create(&model), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, type), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, type), ANEURALNETWORKS_NO_ERROR);
    uint32_t inputs[] = {0};
    uint32_t outputs[] = {1};
    EXPECT_EQ(ANeuralNetworksModel_addOperation(model, ANEURALNETWORKS_CAST, std::size(inputs),
                                                inputs, std::size(outputs), outputs),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_identifyInputsAndOutputs(model, std::size(inputs), inputs,
                                                            std::size(outputs), outputs),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_finish(model), ANEURALNETWORKS_NO_ERROR);
    return model;
}

void testIf(const std::vector<uint32_t>& outerDims, const ANeuralNetworksModel* thenModel,
            const ANeuralNetworksModel* elseModel, bool testMutations) {
    const uint32_t kThenOperand = 1;
    const uint32_t kElseOperand = 2;
    const uint32_t boolDims[] = {1};
    ANeuralNetworksOperandType boolType =
            getOpType(ANEURALNETWORKS_TENSOR_BOOL8, std::size(boolDims), boolDims);
    ANeuralNetworksOperandType dataType =
            getOpType(ANEURALNETWORKS_TENSOR_FLOAT32, outerDims.size(), outerDims.data());
    ANeuralNetworksOperandType modelType = getOpType(ANEURALNETWORKS_MODEL);
    OperationTestBase test(ANEURALNETWORKS_IF, {boolType, modelType, modelType, dataType},
                           {dataType});
    test.setInputOperandValueFromModel(kThenOperand, thenModel);
    test.setInputOperandValueFromModel(kElseOperand, elseModel);
    if (testMutations) {
        test.testOpsValidations();
    } else {
        EXPECT_TRUE(test.testSuccess());
    }
}

void testIf(const std::vector<uint32_t>& outerDims, const std::vector<uint32_t>& thenDims,
            const std::vector<uint32_t>& elseDims, bool testMutations) {
    ANeuralNetworksOperandType thenDataType =
            getOpType(ANEURALNETWORKS_TENSOR_FLOAT32, thenDims.size(), thenDims.data());
    ANeuralNetworksOperandType elseDataType =
            getOpType(ANEURALNETWORKS_TENSOR_FLOAT32, elseDims.size(), elseDims.data());
    ANeuralNetworksModel* thenModel = makeIdentityModel(&thenDataType);
    ANeuralNetworksModel* elseModel = makeIdentityModel(&elseDataType);
    testIf(outerDims, thenModel, elseModel, testMutations);
    ANeuralNetworksModel_free(thenModel);
    ANeuralNetworksModel_free(elseModel);
}

TEST(OperationValidationTest, IF) {
    const std::vector<std::pair<std::string, std::vector<uint32_t>>> configurations = {
            {"fully specified", {1, 2, 3}},
            {"unknown dimensions", {0, 2, 0}},
            {"unknown rank", {}},
    };
    // We skip mutation testing for all but the first configuration to avoid the
    // exponential runtime blowup. The value of additional operand code and
    // count mutations is negligible because whether the shapes are fully
    // specified should have nothing to do with the operand code or count.
    bool testMutations = true;
    for (const auto& [outerTrace, outerDims] : configurations) {
        SCOPED_TRACE(testing::Message() << "outerDims: " << outerTrace);
        for (const auto& [thenTrace, thenDims] : configurations) {
            SCOPED_TRACE(testing::Message() << "thenDims: " << thenTrace);
            for (const auto& [elseTrace, elseDims] : configurations) {
                SCOPED_TRACE(testing::Message() << "elseDims: " << elseTrace);
                testIf(outerDims, thenDims, elseDims, testMutations);
                testMutations = false;
            }
        }
    }
}

// operand 0 --> +------+
//               | LESS | --> operand 2
// operand 1 --> +------+
//
ANeuralNetworksModel* makeWhileCondModel(const ANeuralNetworksOperandType* dataType,
                                         const ANeuralNetworksOperandType* boolType) {
    ANeuralNetworksModel* model = nullptr;
    EXPECT_EQ(ANeuralNetworksModel_create(&model), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, dataType), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, dataType), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, boolType), ANEURALNETWORKS_NO_ERROR);
    const uint32_t inputs[] = {0, 1};
    const uint32_t outputs[] = {2};
    EXPECT_EQ(ANeuralNetworksModel_addOperation(model, ANEURALNETWORKS_LESS, std::size(inputs),
                                                inputs, std::size(outputs), outputs),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_identifyInputsAndOutputs(model, std::size(inputs), inputs,
                                                            std::size(outputs), outputs),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_finish(model), ANEURALNETWORKS_NO_ERROR);
    return model;
}

//               +------+
// operand 0 --> | CAST | --> operand 2
//               +------+
//
// operand 1 --> (unused)
//
ANeuralNetworksModel* makeWhileBodyModel(const ANeuralNetworksOperandType* type) {
    ANeuralNetworksModel* model = nullptr;
    EXPECT_EQ(ANeuralNetworksModel_create(&model), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, type), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, type), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, type), ANEURALNETWORKS_NO_ERROR);
    const uint32_t castInputs[] = {0};
    const uint32_t castOutputs[] = {2};
    EXPECT_EQ(ANeuralNetworksModel_addOperation(model, ANEURALNETWORKS_CAST, std::size(castInputs),
                                                castInputs, std::size(castOutputs), castOutputs),
              ANEURALNETWORKS_NO_ERROR);
    const uint32_t modelInputs[] = {0, 1};
    const uint32_t modelOutputs[] = {2};
    EXPECT_EQ(ANeuralNetworksModel_identifyInputsAndOutputs(model, std::size(modelInputs),
                                                            modelInputs, std::size(modelOutputs),
                                                            modelOutputs),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_finish(model), ANEURALNETWORKS_NO_ERROR);
    return model;
}

void testWhile(const std::vector<uint32_t>& outerDims, const ANeuralNetworksModel* condModel,
               const ANeuralNetworksModel* bodyModel, bool testMutations) {
    const uint32_t kCondOperand = 0;
    const uint32_t kBodyOperand = 1;
    ANeuralNetworksOperandType modelType = getOpType(ANEURALNETWORKS_MODEL);
    ANeuralNetworksOperandType dataType =
            getOpType(ANEURALNETWORKS_TENSOR_FLOAT32, outerDims.size(), outerDims.data());
    OperationTestBase test(ANEURALNETWORKS_WHILE, {modelType, modelType, dataType, dataType},
                           {dataType});
    test.setInputOperandValueFromModel(kCondOperand, condModel);
    test.setInputOperandValueFromModel(kBodyOperand, bodyModel);
    if (testMutations) {
        test.testOpsValidations();
    } else {
        EXPECT_TRUE(test.testSuccess());
    }
}

void testWhile(const std::vector<uint32_t>& outerDims, const std::vector<uint32_t>& condDims,
               const std::vector<uint32_t>& bodyDims, bool testMutations) {
    const uint32_t boolDims[] = {1};
    ANeuralNetworksOperandType boolType =
            getOpType(ANEURALNETWORKS_TENSOR_BOOL8, std::size(boolDims), boolDims);
    ANeuralNetworksOperandType condDataType =
            getOpType(ANEURALNETWORKS_TENSOR_FLOAT32, condDims.size(), condDims.data());
    ANeuralNetworksOperandType bodyDataType =
            getOpType(ANEURALNETWORKS_TENSOR_FLOAT32, bodyDims.size(), bodyDims.data());
    ANeuralNetworksModel* condModel = makeWhileCondModel(&condDataType, &boolType);
    ANeuralNetworksModel* bodyModel = makeWhileBodyModel(&bodyDataType);
    testWhile(outerDims, condModel, bodyModel, testMutations);
    ANeuralNetworksModel_free(condModel);
    ANeuralNetworksModel_free(bodyModel);
}

TEST(OperationValidationTest, WHILE) {
    const std::vector<std::pair<std::string, std::vector<uint32_t>>> configurations = {
            {"fully specified", {1, 2, 3}},
            {"unknown dimensions", {0, 2, 0}},
            {"unknown rank", {}},
    };
    // We skip mutation testing for all but the first configuration to avoid the
    // exponential runtime blowup. The value of additional operand code and
    // count mutations is negligible because whether the shapes are fully
    // specified should have nothing to do with the operand code or count.
    bool testMutations = true;
    for (const auto& [outerTrace, outerDims] : configurations) {
        SCOPED_TRACE(testing::Message() << "outerDims: " << outerTrace);
        for (const auto& [condTrace, condDims] : configurations) {
            SCOPED_TRACE(testing::Message() << "condDims: " << condTrace);
            for (const auto& [bodyTrace, bodyDims] : configurations) {
                SCOPED_TRACE(testing::Message() << "bodyDims: " << bodyTrace);
                testWhile(outerDims, condDims, bodyDims, testMutations);
                testMutations = false;
            }
        }
    }
}

}  // end namespace
