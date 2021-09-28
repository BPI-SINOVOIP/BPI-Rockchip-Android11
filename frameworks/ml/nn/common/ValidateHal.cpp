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

#define LOG_TAG "ValidateHal"

#include "ValidateHal.h"

#include <android-base/logging.h>

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "NeuralNetworks.h"
#include "OperationsUtils.h"
#include "Tracing.h"
#include "Utils.h"

namespace android {
namespace nn {

using namespace hal;

template <class T_Model>
struct ModelToHalVersion;
template <>
struct ModelToHalVersion<V1_0::Model> {
    static constexpr HalVersion version = HalVersion::V1_0;
};
template <>
struct ModelToHalVersion<V1_1::Model> {
    static constexpr HalVersion version = HalVersion::V1_1;
};
template <>
struct ModelToHalVersion<V1_2::Model> {
    static constexpr HalVersion version = HalVersion::V1_2;
};
template <>
struct ModelToHalVersion<V1_3::Model> {
    static constexpr HalVersion version = HalVersion::V1_3;
};

class MemoryAccessVerifier {
   public:
    MemoryAccessVerifier(const hidl_vec<hidl_memory>& pools)
        : mPoolCount(pools.size()), mPoolSizes(mPoolCount) {
        for (size_t i = 0; i < mPoolCount; i++) {
            mPoolSizes[i] = pools[i].size();
        }
    }
    MemoryAccessVerifier(const hidl_vec<V1_3::Request::MemoryPool>& pools)
        : mPoolCount(pools.size()), mPoolSizes(mPoolCount) {
        for (size_t i = 0; i < mPoolCount; i++) {
            switch (pools[i].getDiscriminator()) {
                case Request::MemoryPool::hidl_discriminator::hidlMemory:
                    mPoolSizes[i] = pools[i].hidlMemory().size();
                    break;
                case Request::MemoryPool::hidl_discriminator::token:
                    // Set size to 0 to enforce length == 0 && offset == 0.
                    mPoolSizes[i] = 0;
                    break;
            }
        }
    }
    bool validate(const DataLocation& location) const {
        if (location.poolIndex >= mPoolCount) {
            LOG(ERROR) << "Invalid poolIndex " << location.poolIndex << "/" << mPoolCount;
            return false;
        }
        const size_t size = mPoolSizes[location.poolIndex];
        // Do the addition using size_t to avoid potential wrap-around problems.
        if (static_cast<size_t>(location.offset) + location.length > size) {
            LOG(ERROR) << "Reference to pool " << location.poolIndex << " with offset "
                       << location.offset << " and length " << location.length
                       << " exceeds pool size of " << size;
            return false;
        }
        return true;
    }

   private:
    size_t mPoolCount;
    std::vector<size_t> mPoolSizes;
};

static bool validateOperandExtraParams(const V1_3::Operand& operand, uint32_t index) {
    switch (operand.type) {
        case OperandType::FLOAT32:
        case OperandType::INT32:
        case OperandType::UINT32:
        case OperandType::BOOL:
        case OperandType::SUBGRAPH:
        case OperandType::TENSOR_FLOAT32:
        case OperandType::TENSOR_FLOAT16:
        case OperandType::TENSOR_INT32:
        case OperandType::TENSOR_QUANT8_ASYMM:
        case OperandType::TENSOR_QUANT8_ASYMM_SIGNED:
        case OperandType::TENSOR_QUANT8_SYMM:
        case OperandType::TENSOR_QUANT16_ASYMM:
        case OperandType::TENSOR_QUANT16_SYMM:
        case OperandType::TENSOR_BOOL8: {
            NN_RET_CHECK(operand.extraParams.getDiscriminator() ==
                         OperandExtraParams::hidl_discriminator::none)
                    << "Operand " << index << ": Operand of type "
                    << getOperandTypeName(operand.type)
                    << " has incorrect extraParams: " << toString(operand.extraParams);
        } break;
        case OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL: {
            NN_RET_CHECK(operand.extraParams.getDiscriminator() ==
                         OperandExtraParams::hidl_discriminator::channelQuant)
                    << "Operand " << index << ": Operand of type "
                    << getOperandTypeName(operand.type) << " without a Channel Quantization params";
            auto& channelQuant = operand.extraParams.channelQuant();

            size_t count = operand.dimensions.size();
            NN_RET_CHECK_LT(channelQuant.channelDim, count)
                    << "Operand " << index << ": Operand of type "
                    << getOperandTypeName(operand.type)
                    << " with an invalid channelQuant.channelDim " << channelQuant.channelDim
                    << ", must be valid dimension index in range [0, " << count << ")";
            uint32_t expected = operand.dimensions[channelQuant.channelDim];
            NN_RET_CHECK_EQ(channelQuant.scales.size(), expected)
                    << "Operand " << index << ": Operand of type "
                    << getOperandTypeName(operand.type) << " with a wrong-sized scales, "
                    << "expected " << expected << " was " << channelQuant.scales.size();
            NN_RET_CHECK_NE(expected, 0)
                    << "Operand " << index << ": Operand of type "
                    << getOperandTypeName(operand.type) << " channel dimension "
                    << channelQuant.channelDim << " is underspecified (can't be 0)";
            for (uint32_t i = 0; i < expected; ++i) {
                NN_RET_CHECK_GT(channelQuant.scales[i], .0f)
                        << "Operand " << index << ": Operand of type "
                        << getOperandTypeName(operand.type) << " with a negative value in scales["
                        << i << "]=" << channelQuant.scales[i];
            }
        } break;
        default: {
            if (isExtensionOperandType(operand.type)) {
                NN_RET_CHECK(operand.extraParams.getDiscriminator() ==
                                     OperandExtraParams::hidl_discriminator::extension ||
                             operand.extraParams.getDiscriminator() ==
                                     OperandExtraParams::hidl_discriminator::none)
                        << "Operand " << index << ": Extension operand of type "
                        << getOperandTypeName(operand.type)
                        << " has incorrect extraParams: " << toString(operand.extraParams);
            }
            // No validation for OEM types.
        } break;
    }
    return true;
}

template <typename VersionedOperand>
static bool validateOperands(const hidl_vec<VersionedOperand>& operands,
                             const hidl_vec<uint8_t>& operandValues,
                             const hidl_vec<hidl_memory>& pools,
                             const hidl_vec<Subgraph>& subgraphs, bool allowUnspecifiedRank) {
    uint32_t index = 0;
    MemoryAccessVerifier poolVerifier(pools);
    for (auto& versionedOperand : operands) {
        if (!validOperandType(versionedOperand.type)) {
            LOG(ERROR) << "Operand is not supported by this version: "
                       << toString(versionedOperand.type);
            return false;
        }
        // Once we are sure the operand is supported by its version, it is safe
        // to convert it to the latest version for the rest of the validations.
        V1_3::Operand operand = convertToV1_3(versionedOperand);
        // Validate type and dimensions.
        switch (operand.type) {
            case OperandType::FLOAT16:
            case OperandType::FLOAT32:
            case OperandType::INT32:
            case OperandType::UINT32:
            case OperandType::BOOL:
            case OperandType::SUBGRAPH:
            case OperandType::OEM: {
                size_t count = operand.dimensions.size();
                if (count != 0) {
                    LOG(ERROR) << "Operand " << index << ": Scalar data has dimensions of rank "
                               << count;
                    return false;
                }
                break;
            }
            case OperandType::TENSOR_FLOAT16:
            case OperandType::TENSOR_FLOAT32:
            case OperandType::TENSOR_INT32:
            case OperandType::TENSOR_QUANT8_ASYMM:
            case OperandType::TENSOR_QUANT8_ASYMM_SIGNED:
            case OperandType::TENSOR_QUANT8_SYMM:
            case OperandType::TENSOR_QUANT16_ASYMM:
            case OperandType::TENSOR_QUANT16_SYMM:
            case OperandType::TENSOR_BOOL8:
            case OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL:
            case OperandType::TENSOR_OEM_BYTE: {
                if ((!allowUnspecifiedRank || operand.lifetime == OperandLifeTime::CONSTANT_COPY ||
                     operand.lifetime == OperandLifeTime::CONSTANT_REFERENCE) &&
                    operand.dimensions.size() == 0) {
                    LOG(ERROR) << "Operand " << index << ": Tensor has dimensions of rank 0";
                    return false;
                }
                break;
            }
            default: {
                if (!isExtensionOperandType(operand.type)) {
                    LOG(ERROR) << "Operand " << index << ": Invalid operand type "
                               << toString(operand.type);
                    return false;
                }
            } break;
        }

        // Validate the scale.
        switch (operand.type) {
            case OperandType::FLOAT16:
            case OperandType::FLOAT32:
            case OperandType::INT32:
            case OperandType::UINT32:
            case OperandType::BOOL:
            case OperandType::SUBGRAPH:
            case OperandType::TENSOR_FLOAT16:
            case OperandType::TENSOR_FLOAT32:
            case OperandType::TENSOR_BOOL8:
            case OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL:
                if (operand.scale != 0.f) {
                    LOG(ERROR) << "Operand " << index << ": Operand of type "
                               << getOperandTypeName(operand.type) << " with a non-zero scale ("
                               << operand.scale << ")";
                    return false;
                }
                break;
            case OperandType::TENSOR_INT32:
                // TENSOR_INT32 may be used with or without scale, depending on the operation.
                if (operand.scale < 0.f) {
                    LOG(ERROR) << "Operand " << index << ": Operand of type "
                               << getOperandTypeName(operand.type) << " with a negative scale";
                    return false;
                }
                break;
            case OperandType::TENSOR_QUANT8_ASYMM:
            case OperandType::TENSOR_QUANT8_ASYMM_SIGNED:
            case OperandType::TENSOR_QUANT8_SYMM:
            case OperandType::TENSOR_QUANT16_ASYMM:
            case OperandType::TENSOR_QUANT16_SYMM:
                if (operand.scale <= 0.f) {
                    LOG(ERROR) << "Operand " << index << ": Operand of type "
                               << getOperandTypeName(operand.type) << " with a non-positive scale";
                    return false;
                }
                break;
            default:
                if (isExtensionOperandType(operand.type) && operand.scale != 0.f) {
                    LOG(ERROR) << "Operand " << index << ": Operand of type "
                               << getOperandTypeName(operand.type) << " with a non-zero scale ("
                               << operand.scale << ")";
                    return false;
                }
                // No validation for OEM types.
                // TODO(b/119869082) We should have a separate type for TENSOR_INT32 with a scale.
                break;
        }

        // Validate the zeroPoint.
        switch (operand.type) {
            case OperandType::FLOAT16:
            case OperandType::FLOAT32:
            case OperandType::INT32:
            case OperandType::UINT32:
            case OperandType::BOOL:
            case OperandType::SUBGRAPH:
            case OperandType::TENSOR_FLOAT16:
            case OperandType::TENSOR_FLOAT32:
            case OperandType::TENSOR_INT32:
            case OperandType::TENSOR_BOOL8:
            case OperandType::TENSOR_QUANT8_SYMM:
            case OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL:
                if (operand.zeroPoint != 0) {
                    LOG(ERROR) << "Operand " << index << ": Operand of type "
                               << getOperandTypeName(operand.type) << " with a non-zero zeroPoint "
                               << operand.zeroPoint;
                    return false;
                }
                break;
            case OperandType::TENSOR_QUANT8_ASYMM:
                if (operand.zeroPoint < 0 || operand.zeroPoint > 255) {
                    LOG(ERROR) << "Operand " << index << ": Operand of type "
                               << getOperandTypeName(operand.type) << " with an invalid zeroPoint "
                               << operand.zeroPoint << ", must be in range [0, 255]";
                    return false;
                }
                break;
            case OperandType::TENSOR_QUANT8_ASYMM_SIGNED:
                if (operand.zeroPoint < -128 || operand.zeroPoint > 127) {
                    LOG(ERROR) << "Operand " << index << ": Operand of type "
                               << getOperandTypeName(operand.type) << " with an invalid zeroPoint "
                               << operand.zeroPoint << ", must be in range [-128, 127]";
                    return false;
                }
                break;
            case OperandType::TENSOR_QUANT16_ASYMM:
                if (operand.zeroPoint < 0 || operand.zeroPoint > 65535) {
                    LOG(ERROR) << "Operand " << index << ": Operand of type "
                               << getOperandTypeName(operand.type) << " with an invalid zeroPoint "
                               << operand.zeroPoint << ", must be in range [0, 65535]";
                    return false;
                }
                break;
            case OperandType::TENSOR_QUANT16_SYMM:
                if (operand.zeroPoint != 0) {
                    LOG(ERROR) << "Operand " << index << ": Operand of type "
                               << getOperandTypeName(operand.type) << " with a non-zero zeroPoint "
                               << operand.zeroPoint;
                    return false;
                }
                break;
            default:
                if (isExtensionOperandType(operand.type) && operand.zeroPoint != 0) {
                    LOG(ERROR) << "Operand " << index << ": Operand of type "
                               << getOperandTypeName(operand.type) << " with a non-zero zeroPoint "
                               << operand.zeroPoint;
                    return false;
                }
                // No validation for OEM types.
                break;
        }

        NN_RET_CHECK(validateOperandExtraParams(operand, index));

        // Validate the lifetime and the location.
        const DataLocation& location = operand.location;
        switch (operand.lifetime) {
            case OperandLifeTime::CONSTANT_COPY:
                if (location.poolIndex != 0) {
                    LOG(ERROR) << "Operand " << index
                               << ": CONSTANT_COPY with a non-zero poolIndex "
                               << location.poolIndex;
                    return false;
                }
                // Do the addition using size_t to avoid potential wrap-around problems.
                if (static_cast<size_t>(location.offset) + location.length > operandValues.size()) {
                    LOG(ERROR) << "Operand " << index
                               << ": OperandValue location out of range.  Starts at "
                               << location.offset << ", length " << location.length << ", max "
                               << operandValues.size();
                    return false;
                }
                break;
            case OperandLifeTime::CONSTANT_REFERENCE:
                if (!poolVerifier.validate(location)) {
                    return false;
                }
                break;
            case OperandLifeTime::TEMPORARY_VARIABLE:
            case OperandLifeTime::SUBGRAPH_INPUT:
            case OperandLifeTime::SUBGRAPH_OUTPUT:
            case OperandLifeTime::NO_VALUE:
                if (location.poolIndex != 0 || location.offset != 0 || location.length != 0) {
                    LOG(ERROR) << "Operand " << index << ": Unexpected poolIndex "
                               << location.poolIndex << ", offset " << location.offset
                               << ", or length " << location.length << " for operand of lifetime "
                               << toString(operand.lifetime);
                    return false;
                }
                break;
            case OperandLifeTime::SUBGRAPH: {
                if (location.poolIndex != 0) {
                    LOG(ERROR) << "Operand " << index << ": SUBGRAPH with a non-zero poolIndex "
                               << location.poolIndex;
                    return false;
                }
                if (location.offset >= subgraphs.size()) {
                    LOG(ERROR) << "Subgraph index out of range: " << location.offset
                               << " >= " << subgraphs.size();
                    return false;
                }
                if (location.length != 0) {
                    LOG(ERROR) << "Operand " << index << ": SUBGRAPH with a non-zero length "
                               << location.length;
                    return false;
                }
            } break;
            default:
                LOG(ERROR) << "Operand " << index << ": Invalid lifetime "
                           << toString(operand.lifetime);
                return false;
        }

        // Make sure SUBGRAPH operand type and lifetime always go together.
        if ((operand.type == OperandType::SUBGRAPH) !=
            (operand.lifetime == OperandLifeTime::SUBGRAPH)) {
            LOG(ERROR) << "Operand " << index << ": Operand of type " << toString(operand.type)
                       << " cannot have lifetime " << toString(operand.lifetime);
            return false;
        }

        // For constants, validate that the length is as expected. The other lifetimes
        // expect the length to be 0. Don't validate for OEM types.
        if (operand.lifetime == OperandLifeTime::CONSTANT_REFERENCE ||
            operand.lifetime == OperandLifeTime::CONSTANT_COPY) {
            if (!isExtensionOperandType(operand.type) && operand.type != OperandType::OEM &&
                operand.type != OperandType::TENSOR_OEM_BYTE) {
                uint32_t expectedLength = nonExtensionOperandSizeOfData(operand);
                if (location.length != expectedLength) {
                    LOG(ERROR) << "Operand " << index << ": For operand " << toString(operand)
                               << " expected a size of " << expectedLength << " but got "
                               << location.length;
                    return false;
                }
            }
        }

        index++;
    }
    return true;
}

static HalVersion getHalVersion(const V1_0::Operation&) {
    return HalVersion::V1_0;
}

static HalVersion getHalVersion(const V1_1::Operation&) {
    return HalVersion::V1_1;
}

static HalVersion getHalVersion(const V1_2::Operation&) {
    return HalVersion::V1_2;
}

static HalVersion getHalVersion(const V1_3::Operation&) {
    return HalVersion::V1_3;
}

template <typename VersionedOperation>
static bool validateOperations(const hidl_vec<VersionedOperation>& operations,
                               const hidl_vec<Operand>& operands,
                               const hidl_vec<Subgraph>& subgraphs, ValidationMode mode) {
    auto isValidSubgraphReference = [&subgraphs](const Operand& modelOperand) -> bool {
        NN_RET_CHECK(modelOperand.type == OperandType::SUBGRAPH)
                << "Unexpected operand type: " << toString(modelOperand.type);
        NN_RET_CHECK_LT(modelOperand.location.offset, subgraphs.size())
                << "Invalid subgraph reference";
        return true;
    };
    auto getSubgraph = [&subgraphs](const Operand& modelOperand) -> const Subgraph* {
        CHECK_LT(modelOperand.location.offset, subgraphs.size());
        return &subgraphs[modelOperand.location.offset];
    };
    auto getInputCount = [&getSubgraph](const Operand& modelOperand) -> uint32_t {
        return getSubgraph(modelOperand)->inputIndexes.size();
    };
    auto getOutputCount = [&getSubgraph](const Operand& modelOperand) -> uint32_t {
        return getSubgraph(modelOperand)->outputIndexes.size();
    };
    auto getInputOperand = [&getSubgraph](const Operand& modelOperand,
                                          uint32_t index) -> const Operand* {
        const Subgraph& subgraph = *getSubgraph(modelOperand);
        CHECK_LT(subgraph.inputIndexes[index], subgraph.operands.size());
        return &subgraph.operands[subgraph.inputIndexes[index]];
    };
    auto getOutputOperand = [&getSubgraph](const Operand& modelOperand,
                                           uint32_t index) -> const Operand* {
        const Subgraph& subgraph = *getSubgraph(modelOperand);
        CHECK_LT(subgraph.outputIndexes[index], subgraph.operands.size());
        return &subgraph.operands[subgraph.outputIndexes[index]];
    };
    const size_t operandCount = operands.size();
    for (auto& op : operations) {
        // TODO Validate the shapes and any known values. This is currently
        // done in CpuExecutor but should be done here for all drivers.
        int error = validateOperation(
                static_cast<int32_t>(op.type), op.inputs.size(),
                op.inputs.size() > 0 ? op.inputs.data() : nullptr, op.outputs.size(),
                op.outputs.size() > 0 ? op.outputs.data() : nullptr, operands, getHalVersion(op),
                {.isValidSubgraphReference = isValidSubgraphReference,
                 .getSubgraphInputCount = getInputCount,
                 .getSubgraphOutputCount = getOutputCount,
                 .getSubgraphInputOperand = getInputOperand,
                 .getSubgraphOutputOperand = getOutputOperand,
                 // 1.3 HAL does not support CF operations with operands of
                 // unknown size. See http://b/132458982#comment63.
                 .allowControlFlowOperationWithOperandOfUnknownSize =
                         mode == ValidationMode::RUNTIME});
        if (error != ANEURALNETWORKS_NO_ERROR) {
            LOG(ERROR) << "Invalid operation " << toString(op.type);
            return false;
        }

        // This is redundant because of the checks in validateGraph(),
        // but it is retained here in order to emit more informative
        // error messages.
        for (uint32_t i : op.outputs) {
            const Operand& operand = operands[i];
            if (operand.lifetime != OperandLifeTime::TEMPORARY_VARIABLE &&
                operand.lifetime != OperandLifeTime::SUBGRAPH_OUTPUT) {
                LOG(ERROR) << "Writing to operand " << i << " with incompatible lifetime "
                           << toString(operand.lifetime);
                return false;
            }
        }
    }
    return true;
}

bool validatePool(const hidl_memory& pool, HalVersion ver) {
    const auto& name = pool.name();
    if (name != "ashmem" && name != "mmap_fd" &&
        ((ver < HalVersion::V1_2) ||
         (name != "hardware_buffer_blob" && name != "hardware_buffer"))) {
        LOG(ERROR) << "Unsupported memory type " << name;
        return false;
    }
    if (pool.handle() == nullptr) {
        LOG(ERROR) << "Memory of type " << name << " is null";
        return false;
    }
    return true;
}

bool validatePool(const V1_3::Request::MemoryPool& pool, HalVersion ver) {
    switch (pool.getDiscriminator()) {
        case Request::MemoryPool::hidl_discriminator::hidlMemory:
            return validatePool(pool.hidlMemory(), ver);
        case Request::MemoryPool::hidl_discriminator::token:
            return pool.token() > 0;
    }
    LOG(FATAL) << "unknown MemoryPool discriminator";
    return false;
}

template <class T_MemoryPool>
static bool validatePools(const hidl_vec<T_MemoryPool>& pools, HalVersion ver) {
    return std::all_of(pools.begin(), pools.end(),
                       [ver](const auto& pool) { return validatePool(pool, ver); });
}

static bool validateModelInputOutputs(const hidl_vec<uint32_t> indexes,
                                      const hidl_vec<Operand>& operands, OperandLifeTime lifetime) {
    const size_t operandCount = operands.size();
    for (uint32_t i : indexes) {
        if (i >= operandCount) {
            LOG(ERROR) << "Model input or output index out of range: " << i << "/" << operandCount;
            return false;
        }
        const Operand& operand = operands[i];
        if (operand.lifetime != lifetime) {
            LOG(ERROR) << "Model input or output operand " << i << " has lifetime of "
                       << toString(operand.lifetime) << " instead of the expected "
                       << toString(lifetime);
            return false;
        }
    }

    std::vector<uint32_t> sortedIndexes = indexes;
    std::sort(sortedIndexes.begin(), sortedIndexes.end());
    auto adjacentI = std::adjacent_find(sortedIndexes.begin(), sortedIndexes.end());
    if (adjacentI != sortedIndexes.end()) {
        LOG(ERROR) << "Model input or output occurs multiple times: " << *adjacentI;
        return false;
    }

    for (size_t i = 0; i < operands.size(); ++i) {
        if (operands[i].lifetime == lifetime &&
            !binary_search(sortedIndexes.begin(), sortedIndexes.end(), i)) {
            LOG(ERROR) << "Operand " << i << " marked as " << toString(lifetime)
                       << " but is not included in Model input or output indexes";
            return false;
        }
    }

    return true;
}

template <typename VersionedModelOrSubgraph>
static bool validateGraph(const VersionedModelOrSubgraph& model) {
    // set up counts
    std::vector<uint32_t> operandNumberOfConsumers(model.operands.size(), 0);
    //     Either the operand has a known value before model execution
    //     begins, or we've seen a writer for this operand while
    //     walking operands in execution order.
    std::vector<bool> operandValueKnown(model.operands.size(), false);

    // mark known operands
    for (size_t i = 0; i < model.operands.size(); ++i) {
        const auto& operand = model.operands[i];
        const OperandLifeTime lifetime = convertToV1_3(operand.lifetime);
        operandValueKnown[i] = lifetime == OperandLifeTime::SUBGRAPH_INPUT ||
                               lifetime == OperandLifeTime::CONSTANT_COPY ||
                               lifetime == OperandLifeTime::CONSTANT_REFERENCE ||
                               lifetime == OperandLifeTime::NO_VALUE ||
                               lifetime == OperandLifeTime::SUBGRAPH;
    }

    // Validate that operations are sorted into execution order.
    //
    // If there is a cycle in the graph, the operations will not
    // appear to be sorted into execution order: Some operation will
    // have an input for which operandValueKnown[] is false.
    for (size_t i = 0; i < model.operations.size(); ++i) {
        const auto& operation = model.operations[i];

        for (size_t j = 0; j < operation.inputs.size(); ++j) {
            uint32_t k = operation.inputs[j];
            if (!operandValueKnown[k]) {
                LOG(ERROR) << "Operation " << i << " input " << j << " (operand " << k
                           << ") is read before it is written";
                return false;
            }
            operandNumberOfConsumers[k]++;
        }

        for (size_t j = 0; j < operation.outputs.size(); ++j) {
            uint32_t k = operation.outputs[j];
            if (operandValueKnown[k]) {
                // Assuming validateOperations() has returned true, we
                // know that this output is TEMPORARY_VARIABLE or
                // MODEL_OUTPUT, and so the only way
                // operandValueKnown[k] can be true is if we've
                // already seen a writer for this operand.
                LOG(ERROR) << "Operation " << i << " output " << j << " (operand " << k
                           << ") has already been written";
                return false;
            }
            operandValueKnown[k] = true;
        }
    }

    // validate number of consumers
    //
    // TODO Because we have to validate it, there was no point in including it
    // in struct Operand. For the next release, consider removing unless we have
    // an additional process in system space that creates this value. In that
    // case, it would not have to be validated.
    for (size_t i = 0; i < model.operands.size(); ++i) {
        if (model.operands[i].numberOfConsumers != operandNumberOfConsumers[i]) {
            LOG(ERROR) << "Operand " << i << " has incorrect number of consumers "
                       << model.operands[i].numberOfConsumers << ", expected "
                       << operandNumberOfConsumers[i];
            return false;
        }
    }

    // verify all operands are written
    for (size_t i = 0; i < model.operands.size(); ++i) {
        if (!operandValueKnown[i]) {
            LOG(ERROR) << "Operand " << i << " is never written";
            return false;
        }
    }

    return true;
}

// Makes sure the model does not contain subgraph reference cycles.
static bool checkNoReferenceCycles(const V1_3::Model& model, const V1_3::Subgraph& subgraph,
                                   std::set<const V1_3::Subgraph*>* path) {
    auto [_, isNew] = path->insert(&subgraph);
    if (!isNew) {
        LOG(ERROR) << "Model contains a circular subgraph reference";
        return false;
    }
    for (const Operand& operand : subgraph.operands) {
        if (operand.lifetime == OperandLifeTime::SUBGRAPH) {
            uint32_t refSubgraphIndex = operand.location.offset;
            if (!checkNoReferenceCycles(model, model.referenced[refSubgraphIndex], path)) {
                return false;
            }
        }
    }
    path->erase(&subgraph);
    return true;
}

static bool checkNoReferenceCycles(const V1_3::Model& model) {
    std::set<const V1_3::Subgraph*> path;
    return checkNoReferenceCycles(model, model.main, &path);
}

template <class T_Model>
bool validateModel(const T_Model& model, ValidationMode mode) {
    NNTRACE_FULL(NNTRACE_LAYER_UTILITY, NNTRACE_PHASE_UNSPECIFIED, "validateModel");
    HalVersion version = ModelToHalVersion<T_Model>::version;
    if (model.operations.size() == 0 || model.operands.size() == 0) {
        LOG(ERROR) << "Invalid empty model.";
        return false;
    }
    // We only need versioned operands for their validation. For all the other
    // validations we can use operands upcasted to the latest version.
    const hidl_vec<Operand> latestVersionOperands = convertToV1_3(model.operands);
    return (validateOperands(model.operands, model.operandValues, model.pools, /*subgraphs=*/{},
                             /*allowUnspecifiedRank=*/version >= HalVersion::V1_2) &&
            validateOperations(model.operations, latestVersionOperands, /*subgraphs=*/{}, mode) &&
            validateModelInputOutputs(model.inputIndexes, latestVersionOperands,
                                      OperandLifeTime::SUBGRAPH_INPUT) &&
            validateModelInputOutputs(model.outputIndexes, latestVersionOperands,
                                      OperandLifeTime::SUBGRAPH_OUTPUT) &&
            validatePools(model.pools, version) && validateGraph(model));
}

template bool validateModel<V1_0::Model>(const V1_0::Model& model, ValidationMode mode);
template bool validateModel<V1_1::Model>(const V1_1::Model& model, ValidationMode mode);
template bool validateModel<V1_2::Model>(const V1_2::Model& model, ValidationMode mode);

template <>
bool validateModel(const V1_3::Model& model, ValidationMode mode) {
    NNTRACE_FULL(NNTRACE_LAYER_UTILITY, NNTRACE_PHASE_UNSPECIFIED, "validateModel");
    if (model.main.operations.size() == 0 || model.main.operands.size() == 0) {
        LOG(ERROR) << "Invalid empty model.";
        return false;
    }
    auto validateSubgraph = [&model, mode](const Subgraph& subgraph) -> bool {
        return (validateOperands(subgraph.operands, model.operandValues, model.pools,
                                 model.referenced, /*allowUnspecifiedRank=*/true) &&
                validateOperations(subgraph.operations, subgraph.operands, model.referenced,
                                   mode) &&
                validateModelInputOutputs(subgraph.inputIndexes, subgraph.operands,
                                          OperandLifeTime::SUBGRAPH_INPUT) &&
                validateModelInputOutputs(subgraph.outputIndexes, subgraph.operands,
                                          OperandLifeTime::SUBGRAPH_OUTPUT) &&
                validateGraph(subgraph));
    };
    return (validateSubgraph(model.main) &&
            std::all_of(model.referenced.begin(), model.referenced.end(), validateSubgraph) &&
            validatePools(model.pools, HalVersion::V1_3) && checkNoReferenceCycles(model));
}

// Validates the arguments of a request. type is either "input" or "output" and is used
// for printing error messages. The operandIndexes is the appropriate array of input
// or output operand indexes that was passed to the ANeuralNetworksModel_identifyInputsAndOutputs.
static bool validateRequestArguments(const hidl_vec<RequestArgument>& requestArguments,
                                     const hidl_vec<uint32_t>& operandIndexes,
                                     const hidl_vec<Operand>& operands,
                                     const MemoryAccessVerifier& poolVerifier,
                                     bool allowUnspecified, const char* type) {
    // The request should specify as many arguments as were described in the model.
    const size_t requestArgumentCount = requestArguments.size();
    if (requestArgumentCount != operandIndexes.size()) {
        LOG(ERROR) << "Request specifies " << requestArgumentCount << " " << type
                   << "s but the model has " << operandIndexes.size();
        return false;
    }
    for (size_t requestArgumentIndex = 0; requestArgumentIndex < requestArgumentCount;
         requestArgumentIndex++) {
        const RequestArgument& requestArgument = requestArguments[requestArgumentIndex];
        const DataLocation& location = requestArgument.location;
        // Get the operand index for this argument. We extract it from the list
        // that was provided in the call to ANeuralNetworksModel_identifyInputsAndOutputs.
        // We assume in this function that the model has been validated already.
        const uint32_t operandIndex = operandIndexes[requestArgumentIndex];
        const Operand& operand = operands[operandIndex];
        if (requestArgument.hasNoValue) {
            if (location.poolIndex != 0 || location.offset != 0 || location.length != 0 ||
                requestArgument.dimensions.size() != 0) {
                LOG(ERROR) << "Request " << type << " " << requestArgumentIndex
                           << " has no value yet has details.";
                return false;
            }
        } else {
            // Validate the location.
            if (!poolVerifier.validate(location)) {
                return false;
            }
            // If the argument specified a dimension, validate it.
            uint32_t modelRank = operand.dimensions.size();
            uint32_t requestRank = requestArgument.dimensions.size();
            if (requestRank == 0) {
                if (!allowUnspecified) {
                    // Validate that all the dimensions are specified in the model.
                    for (size_t i = 0; i < modelRank; i++) {
                        if (operand.dimensions[i] == 0) {
                            LOG(ERROR) << "Model has dimension " << i
                                       << " set to 0 but the request does specify the dimension.";
                            return false;
                        }
                    }
                }
            } else {
                if (modelRank != 0 && requestRank != modelRank) {
                    LOG(ERROR) << "Request " << type << " " << requestArgumentIndex
                               << " has number of dimensions (" << requestRank
                               << ") different than the model's (" << modelRank << ")";
                    return false;
                }
                for (size_t i = 0; i < requestRank; i++) {
                    if (modelRank != 0 && requestArgument.dimensions[i] != operand.dimensions[i] &&
                        operand.dimensions[i] != 0) {
                        LOG(ERROR)
                                << "Request " << type << " " << requestArgumentIndex
                                << " has dimension " << i << " of " << requestArgument.dimensions[i]
                                << " different than the model's " << operand.dimensions[i];
                        return false;
                    }
                    if (requestArgument.dimensions[i] == 0 && !allowUnspecified) {
                        LOG(ERROR) << "Request " << type << " " << requestArgumentIndex
                                   << " has dimension " << i << " of zero";
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

template <class T_Request, class T_Model>
bool validateRequest(const T_Request& request, const T_Model& model, bool allowUnspecifiedOutput) {
    HalVersion version = ModelToHalVersion<T_Model>::version;
    MemoryAccessVerifier poolVerifier(request.pools);
    return (validateRequestArguments(request.inputs, model.inputIndexes,
                                     convertToV1_3(model.operands), poolVerifier,
                                     /*allowUnspecified=*/false, "input") &&
            validateRequestArguments(
                    request.outputs, model.outputIndexes, convertToV1_3(model.operands),
                    poolVerifier,
                    /*allowUnspecified=*/version >= HalVersion::V1_2 && allowUnspecifiedOutput,
                    "output") &&
            validatePools(request.pools, version));
}

template bool validateRequest<V1_0::Request, V1_0::Model>(const V1_0::Request& request,
                                                          const V1_0::Model& model,
                                                          bool allowUnspecifiedOutput);
template bool validateRequest<V1_0::Request, V1_1::Model>(const V1_0::Request& request,
                                                          const V1_1::Model& model,
                                                          bool allowUnspecifiedOutput);
template bool validateRequest<V1_0::Request, V1_2::Model>(const V1_0::Request& request,
                                                          const V1_2::Model& model,
                                                          bool allowUnspecifiedOutput);

template <>
bool validateRequest(const V1_3::Request& request, const V1_3::Model& model,
                     bool allowUnspecifiedOutput) {
    return (validateRequestArguments(request.inputs, model.main.inputIndexes, model.main.operands,
                                     request.pools,
                                     /*allowUnspecified=*/false, "input") &&
            validateRequestArguments(request.outputs, model.main.outputIndexes, model.main.operands,
                                     request.pools, allowUnspecifiedOutput, "output") &&
            validatePools(request.pools, HalVersion::V1_3));
}

bool validateMemoryDesc(const V1_3::BufferDesc& desc,
                        const hidl_vec<sp<V1_3::IPreparedModel>>& preparedModels,
                        const hidl_vec<V1_3::BufferRole>& inputRoles,
                        const hidl_vec<V1_3::BufferRole>& outputRoles,
                        std::function<const V1_3::Model*(const sp<V1_3::IPreparedModel>&)> getModel,
                        std::set<PreparedModelRole>* preparedModelRoles,
                        V1_3::Operand* combinedOperand) {
    NN_RET_CHECK(preparedModels.size() != 0);
    NN_RET_CHECK(inputRoles.size() != 0 || outputRoles.size() != 0);

    std::set<PreparedModelRole> roles;
    std::vector<V1_3::Operand> operands;
    operands.reserve(inputRoles.size() + outputRoles.size());
    for (const auto& role : inputRoles) {
        NN_RET_CHECK_LT(role.modelIndex, preparedModels.size());
        const auto& preparedModel = preparedModels[role.modelIndex];
        NN_RET_CHECK(preparedModel != nullptr);
        const auto* model = getModel(preparedModel);
        NN_RET_CHECK(model != nullptr);
        const auto& inputIndexes = model->main.inputIndexes;
        NN_RET_CHECK_LT(role.ioIndex, inputIndexes.size());
        NN_RET_CHECK_GT(role.frequency, 0.0f);
        NN_RET_CHECK_LE(role.frequency, 1.0f);
        const auto [it, success] = roles.emplace(preparedModel.get(), IOType::INPUT, role.ioIndex);
        NN_RET_CHECK(success);
        operands.push_back(model->main.operands[inputIndexes[role.ioIndex]]);
    }
    for (const auto& role : outputRoles) {
        NN_RET_CHECK_LT(role.modelIndex, preparedModels.size());
        const auto& preparedModel = preparedModels[role.modelIndex];
        NN_RET_CHECK(preparedModel != nullptr);
        const auto* model = getModel(preparedModel);
        NN_RET_CHECK(model != nullptr);
        const auto& outputIndexes = model->main.outputIndexes;
        NN_RET_CHECK_LT(role.ioIndex, outputIndexes.size());
        NN_RET_CHECK_GT(role.frequency, 0.0f);
        NN_RET_CHECK_LE(role.frequency, 1.0f);
        const auto [it, success] = roles.emplace(preparedModel.get(), IOType::OUTPUT, role.ioIndex);
        NN_RET_CHECK(success);
        operands.push_back(model->main.operands[outputIndexes[role.ioIndex]]);
    }

    CHECK(!operands.empty());
    const auto opType = operands[0].type;
    const bool isExtension = isExtensionOperandType(opType);

    std::vector<uint32_t> dimensions = desc.dimensions;
    for (const auto& operand : operands) {
        NN_RET_CHECK(operand.type == operands[0].type)
                << toString(operand.type) << " vs " << toString(operands[0].type);
        NN_RET_CHECK_EQ(operand.scale, operands[0].scale);
        NN_RET_CHECK_EQ(operand.zeroPoint, operands[0].zeroPoint);
        // NOTE: validateMemoryDesc cannot validate extra parameters for extension operand type.
        if (!isExtension) {
            NN_RET_CHECK(operand.extraParams == operands[0].extraParams)
                    << toString(operand.extraParams) << " vs " << toString(operands[0].extraParams);
        }
        const auto combined = combineDimensions(dimensions, operand.dimensions);
        NN_RET_CHECK(combined.has_value());
        dimensions = combined.value();
    }

    // NOTE: validateMemoryDesc cannot validate scalar dimensions with extension operand type.
    if (!isExtension) {
        NN_RET_CHECK(!nonExtensionOperandTypeIsScalar(static_cast<int>(opType)) ||
                     dimensions.empty())
                << "invalid dimensions with scalar operand type.";
    }

    if (preparedModelRoles != nullptr) {
        *preparedModelRoles = std::move(roles);
    }
    if (combinedOperand != nullptr) {
        *combinedOperand = operands[0];
        combinedOperand->dimensions = dimensions;
    }
    return true;
}

bool validateExecutionPreference(ExecutionPreference preference) {
    return preference == ExecutionPreference::LOW_POWER ||
           preference == ExecutionPreference::FAST_SINGLE_ANSWER ||
           preference == ExecutionPreference::SUSTAINED_SPEED;
}

bool validatePriority(Priority priority) {
    return priority == Priority::LOW || priority == Priority::MEDIUM || priority == Priority::HIGH;
}

bool validOperandType(V1_0::OperandType operandType) {
    switch (operandType) {
        case V1_0::OperandType::FLOAT32:
        case V1_0::OperandType::INT32:
        case V1_0::OperandType::UINT32:
        case V1_0::OperandType::TENSOR_FLOAT32:
        case V1_0::OperandType::TENSOR_INT32:
        case V1_0::OperandType::TENSOR_QUANT8_ASYMM:
        case V1_0::OperandType::OEM:
        case V1_0::OperandType::TENSOR_OEM_BYTE:
            return true;
        default:
            return false;
    }
}

bool validOperandType(V1_2::OperandType operandType) {
    switch (operandType) {
        case V1_2::OperandType::FLOAT16:
        case V1_2::OperandType::FLOAT32:
        case V1_2::OperandType::INT32:
        case V1_2::OperandType::UINT32:
        case V1_2::OperandType::BOOL:
        case V1_2::OperandType::TENSOR_FLOAT16:
        case V1_2::OperandType::TENSOR_FLOAT32:
        case V1_2::OperandType::TENSOR_INT32:
        case V1_2::OperandType::TENSOR_QUANT8_ASYMM:
        case V1_2::OperandType::TENSOR_QUANT8_SYMM:
        case V1_2::OperandType::TENSOR_QUANT16_ASYMM:
        case V1_2::OperandType::TENSOR_QUANT16_SYMM:
        case V1_2::OperandType::TENSOR_BOOL8:
        case V1_2::OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL:
        case V1_2::OperandType::OEM:
        case V1_2::OperandType::TENSOR_OEM_BYTE:
            return true;
        default:
            return isExtensionOperandType(static_cast<V1_3::OperandType>(operandType));
    }
}

bool validOperandType(V1_3::OperandType operandType) {
    switch (operandType) {
        case V1_3::OperandType::FLOAT16:
        case V1_3::OperandType::FLOAT32:
        case V1_3::OperandType::INT32:
        case V1_3::OperandType::UINT32:
        case V1_3::OperandType::BOOL:
        case V1_3::OperandType::TENSOR_FLOAT16:
        case V1_3::OperandType::TENSOR_FLOAT32:
        case V1_3::OperandType::TENSOR_INT32:
        case V1_3::OperandType::TENSOR_QUANT8_ASYMM:
        case V1_3::OperandType::TENSOR_QUANT8_SYMM:
        case V1_3::OperandType::TENSOR_QUANT16_ASYMM:
        case V1_3::OperandType::TENSOR_QUANT16_SYMM:
        case V1_3::OperandType::TENSOR_BOOL8:
        case V1_3::OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL:
        case V1_3::OperandType::TENSOR_QUANT8_ASYMM_SIGNED:
        case V1_3::OperandType::SUBGRAPH:
        case V1_3::OperandType::OEM:
        case V1_3::OperandType::TENSOR_OEM_BYTE:
            return true;
        default:
            return isExtensionOperandType(operandType);
    }
}

}  // namespace nn
}  // namespace android
