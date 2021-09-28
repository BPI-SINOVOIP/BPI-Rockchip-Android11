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

#define LOG_TAG "MetaModel"

#include "MetaModel.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <type_traits>
#include <utility>

#include "GraphDump.h"
#include "HalInterfaces.h"
#include "Utils.h"

namespace android::nn {

using namespace hal;

namespace {

// Add an element to the end of the vector and return a pair consisting of the
// index of the new element and a pointer to the new element.
template <class T>
std::pair<uint32_t, T*> extend(hidl_vec<T>* vec) {
    size_t nextIndex = vec->size();
    vec->resize(nextIndex + 1);
    return {nextIndex, &(*vec)[nextIndex]};
}

// Add an element to the end of the vector, set it to the specified value, and
// return a pair consisting of the index of the new element and a pointer to the
// new element.
template <class T>
std::pair<uint32_t, T*> extend(hidl_vec<T>* vec, const T& val) {
    auto extended = extend(vec);
    *extended.second = val;
    return extended;
}

template <typename T>
bool operator<(const hidl_vec<T>& a, const hidl_vec<T>& b) {
    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
}

// Compile-time mapping from a particular Model type to a name for that type.
template <class T_Model>
struct ModelVersion;
template <>
struct ModelVersion<hal::V1_0::Model> {
    static constexpr char name[] = "V1_0";
};
template <>
struct ModelVersion<hal::V1_1::Model> {
    static constexpr char name[] = "V1_1";
};
template <>
struct ModelVersion<hal::V1_2::Model> {
    static constexpr char name[] = "V1_2";
};
template <>
struct ModelVersion<hal::V1_3::Model> {
    static constexpr char name[] = "V1_3";
};

// Dispatcher mechanism for calling an appropriate uncheckedConvertToV1_*
// given the desired return type.
template <typename T_ReturnType>
T_ReturnType uncheckedConvertTo(OperationType type);
template <>
hal::V1_0::OperationType uncheckedConvertTo<hal::V1_0::OperationType>(OperationType type) {
    return uncheckedConvertToV1_0(type);
}
template <>
hal::V1_1::OperationType uncheckedConvertTo<hal::V1_1::OperationType>(OperationType type) {
    return uncheckedConvertToV1_1(type);
}
template <>
hal::V1_2::OperationType uncheckedConvertTo<hal::V1_2::OperationType>(OperationType type) {
    return uncheckedConvertToV1_2(type);
}

// Dispatcher mechanism for calling an appropriate convertToV1_* given the
// desired return type.  Note that there is no V1_1::Operand type.
template <typename T_ReturnType>
T_ReturnType convertTo(Operand operand);
template <>
hal::V1_0::Operand convertTo<hal::V1_0::Operand>(Operand operand) {
    return convertToV1_0(operand);
}
template <>
hal::V1_2::Operand convertTo<hal::V1_2::Operand>(Operand operand) {
    return convertToV1_2(operand);
}

// Dispatcher mechanism for calling an appropriate convertToV1_* given the
// desired return type.  Note that there are no V1_[12]::OperandLifeTime types.
template <typename T_ReturnType>
T_ReturnType convertTo(OperandLifeTime lifetime);
template <>
hal::V1_0::OperandLifeTime convertTo<hal::V1_0::OperandLifeTime>(OperandLifeTime lifetime) {
    return convertToV1_0(lifetime);
}
template <>
hal::V1_3::OperandLifeTime convertTo<hal::V1_3::OperandLifeTime>(OperandLifeTime lifetime) {
    return lifetime;
}

// Dispatcher mechanism for calling an appropriate compliantWithV1_* given the
// desired target model type.
template <typename T_SlicedModel>
void getNoncompliantOperations(const hal::V1_3::Model& model,
                               std::set<uint32_t>* noncompliantOperations);
template <>
void getNoncompliantOperations<hal::V1_0::Model>(const hal::V1_3::Model& model,
                                                 std::set<uint32_t>* noncompliantOperations) {
    compliantWithV1_0(model, noncompliantOperations);
}
template <>
void getNoncompliantOperations<hal::V1_1::Model>(const hal::V1_3::Model& model,
                                                 std::set<uint32_t>* noncompliantOperations) {
    compliantWithV1_1(model, noncompliantOperations);
}
template <>
void getNoncompliantOperations<hal::V1_2::Model>(const hal::V1_3::Model& model,
                                                 std::set<uint32_t>* noncompliantOperations) {
    compliantWithV1_2(model, noncompliantOperations);
}

template <class T_SlicedModel>
bool invalid(const T_SlicedModel& model, bool strictSlicing) {
    // A model must have at least one operation.  However, it's possible that a
    // slice has no operations (because no operations from the original model
    // are compliant with the sliced model type).  In this case, the sliced
    // model would be invalid.
    const bool looksEmpty = (model.operations.size() == 0);
    if (strictSlicing) {
        CHECK_EQ(looksEmpty, (model.operands.size() == 0));
    }
    if (looksEmpty) return true;

    // A model must have at least one output.  However, it's possible for a
    // model to contain dead operations (i.e., outputs on which no model outputs
    // are data dependent).  A slice might contain only dead operations, and
    // hence have no model outputs.  In this case, the sliced model would be
    // invalid.
    if (model.outputIndexes.size() == 0) return true;

    // We shouldn't have to check whether the model is valid.
    // However, it could be invalid if:
    // - there is an error in the slicing algorithm; or
    // - there is an error in compliantWith (see http://b/131845106)
    if (!validateModel(model)) {
        LOG(WARNING) << "Sliced model fails validateModel()";
        CHECK(!strictSlicing);
        return true;
    }

    return false;
}

}  // anonymous namespace

template <class T_SlicedModel>
MetaModel::ReturnedSlice<T_SlicedModel> MetaModel::getSlice(Slice<T_SlicedModel>* slice) const {
    CHECK(slice != nullptr);
    if (slice->mState == SliceState::UNINITIALIZED) {
        *slice = makeSlice<T_SlicedModel>();
    }
    if (slice->mState == SliceState::INVALID) {
        return {};
    }
    return MetaModel::ReturnedSlice<T_SlicedModel>(std::make_pair(
            slice->mHidlModel, Mapper([slice](uint32_t slicedOperationIndex) {
                return slice->mSlicedOperationIndexToOrigIndex.at(slicedOperationIndex);
            })));
}
template MetaModel::ReturnedSlice<hal::V1_0::Model> MetaModel::getSlice(
        Slice<hal::V1_0::Model>* slice) const;
template MetaModel::ReturnedSlice<hal::V1_1::Model> MetaModel::getSlice(
        Slice<hal::V1_1::Model>* slice) const;
template MetaModel::ReturnedSlice<hal::V1_2::Model> MetaModel::getSlice(
        Slice<hal::V1_2::Model>* slice) const;
// When adding HAL version 1.4, make sure to handle control flow and referenced
// subgraphs here properly. A V1_3 sliced model should contain an IF/WHILE and
// its referenced subgraphs only if there are no V1_4+ operations in those
// subgraphs.
// template MetaModel::ReturnedSlice<hal::V1_3::Model> MetaModel::getSlice(
//         Slice<hal::V1_3::Model>* slice) const;

// Utility class for makeSlice().
//
// For each output operand of a noncompliant operation that is the input
// operand of at least one compliant operation, we will ensure that there is
// a sliced model input whose "type" is that of the output operand.  This is
// a map from operand "type" (in the original model) to model input
// operand index (in the sliced model).  Unfortunately, there is no
// representation of operand "type" defined in the HAL that we can use
// naively here -- we want (OperandType, dimensions, scale, zeroPoint,
// extraParams), but these fields exist in Operand along with other fields
// that need to be excluded from the map key (numberOfConsumers, lifetime,
// location).  There are several choices:
// - Don't have a map -- each output identified above gets its own sliced
//   model input (no sharing of sliced model inputs).
// - Create an operand "type" representation solely for use as a map key.
// - Write a tailored comparison function that ignores the excluded fields.
// We choose to write a tailored comparison function.  If Treble were to
// generate a comparison function for us (http://b/130567619) then it might
// be better to instead reset the excluded fields to canonical values --
// then we could use the Treble provided comparison function, and the
// solution would be robust (in a correctness sense, not a sharing sense) if
// more fields are added and we neglect to canonicalize them.
//
// We also use this map for model input operands of the original model that
// become input operands of the sliced model.  This means that an original
// model input operand might be commoned with other original model input
// operands and/or with original model temporary operands.
template <typename T_SlicedOperand>
class MetaModel::OrigOperandToSlicedInputOperandIndex {
   public:
    OrigOperandToSlicedInputOperandIndex(hidl_vec<T_SlicedOperand>* slicedOperands,
                                         hidl_vec<uint32_t>* slicedInputIndexes)
        : mSlicedOperands(*slicedOperands), mSlicedInputIndexes(*slicedInputIndexes) {}

    // Given an operand from the original model, return the index of the
    // corresponding model input operand from the sliced model.  Creates a
    // new operand in the sliced model if necessary.
    uint32_t getIndex(Operand operand) {
        // Lookup
        auto it = mMap.find(operand);
        if (it != mMap.end()) {
            VLOG(COMPILATION) << "OrigOperandToSlicedInputOperandIndex::getIndex looked for "
                              << toString(operand) << " and found " << it->second << ": "
                              << toString(it->first);
            return it->second;
        }

        // Create
        operand.numberOfConsumers = 0;
        operand.lifetime = convertTo<decltype(operand.lifetime)>(OperandLifeTime::SUBGRAPH_INPUT);
        operand.location = {};
        uint32_t slicedOperandIndex =
                extend(&mSlicedOperands, convertTo<T_SlicedOperand>(operand)).first;
        mMap[operand] = slicedOperandIndex;
        extend(&mSlicedInputIndexes, slicedOperandIndex);
        VLOG(COMPILATION) << "OrigOperandToSlicedInputOperandIndex::getIndex created "
                          << slicedOperandIndex << ": " << toString(operand);
        return slicedOperandIndex;
    }

   private:
    class Compare {
       public:
        bool operator()(const Operand& a, const Operand& b) const {
            if (a.type != b.type) {
                return a.type < b.type;
            }
            if (a.dimensions != b.dimensions) {
                return a.dimensions < b.dimensions;
            }
            if (a.scale != b.scale) {
                return a.scale < b.scale;
            }
            if (a.zeroPoint != b.zeroPoint) {
                return a.zeroPoint < b.zeroPoint;
            }
            return compare(a.extraParams, b.extraParams);
        }

       private:
        static bool compare(const SymmPerChannelQuantParams& a,
                            const SymmPerChannelQuantParams& b) {
            if (a.scales != b.scales) {
                return a.scales < b.scales;
            }
            return a.channelDim < b.channelDim;
        }

        static bool compare(const OperandExtraParams& a, const OperandExtraParams& b) {
            if (a.getDiscriminator() != b.getDiscriminator()) {
                return a.getDiscriminator() < b.getDiscriminator();
            }

            switch (a.getDiscriminator()) {
                case OperandExtraParams::hidl_discriminator::channelQuant:
                    return compare(a.channelQuant(), b.channelQuant());

                case OperandExtraParams::hidl_discriminator::extension:
                    return a.extension() < b.extension();

                case OperandExtraParams::hidl_discriminator::none:
                    return false;

                default:
                    CHECK(false) << "Unexpected";
                    return false;
            }
        }
    };
    std::map<Operand, uint32_t, Compare> mMap;
    hidl_vec<T_SlicedOperand>& mSlicedOperands;
    hidl_vec<uint32_t>& mSlicedInputIndexes;
};

template <class T_SlicedModel>
void MetaModel::processOperations(
        Slice<T_SlicedModel>* slice, std::map<uint32_t, uint32_t>* origOperandIndexToSlicedIndex,
        OrigOperandToSlicedInputOperandIndex<typename Slice<T_SlicedModel>::Operand>*
                origOperandToSlicedInputOperandIndex,
        const std::set<uint32_t>& noncompliantOperations,
        const std::set<uint32_t>& inputOperandIndexesOfCompliantOperations) const {
    using SlicedOperand = typename Slice<T_SlicedModel>::Operand;
    using SlicedOperation = typename Slice<T_SlicedModel>::Operation;
    using SlicedOperationType = typename Slice<T_SlicedModel>::OperationType;

    const auto& origOperands = mHidlModel.main.operands;
    const auto& origOperations = mHidlModel.main.operations;
    auto& slicedOperands = slice->mHidlModel.operands;
    auto& slicedOperations = slice->mHidlModel.operations;

    for (uint32_t origOperationIndex = 0; origOperationIndex < origOperations.size();
         ++origOperationIndex) {
        const Operation& origOperation = origOperations[origOperationIndex];

        if (noncompliantOperations.count(origOperationIndex)) {
            for (uint32_t output : origOperation.outputs) {
                if (!inputOperandIndexesOfCompliantOperations.count(output)) {
                    continue;
                }
                const uint32_t slicedIndex =
                        origOperandToSlicedInputOperandIndex->getIndex(origOperands[output]);
                (*origOperandIndexToSlicedIndex)[output] = slicedIndex;
                VLOG(COMPILATION)
                        << "origOperandIndexToSlicedIndex noncompliant output processing created "
                        << output << " -> " << slicedIndex << ": "
                        << toString(slicedOperands[slicedIndex]);
            }
        } else {
            slice->mSlicedOperationIndexToOrigIndex.push_back(origOperationIndex);
            SlicedOperation& slicedOperation = *extend(&slicedOperations).second;
            CHECK_EQ(slice->mSlicedOperationIndexToOrigIndex.size(), slicedOperations.size());

            slicedOperation.type = uncheckedConvertTo<SlicedOperationType>(origOperation.type);

            // Model is topologically sorted, so all operation inputs must be
            // present in origOperandIndexToSlicedIndex, and no operation
            // outputs may be.

            // Operation inputs
            // - Fill in slicedOperation.inputs
            // - Update number of consumers for each input operand
            slicedOperation.inputs.resize(origOperation.inputs.size());
            std::transform(
                    origOperation.inputs.begin(), origOperation.inputs.end(),
                    slicedOperation.inputs.begin(),
                    [&origOperandIndexToSlicedIndex, &slicedOperands](uint32_t origOperandIndex) {
                        uint32_t slicedOperandIndex =
                                origOperandIndexToSlicedIndex->at(origOperandIndex);
                        slicedOperands[slicedOperandIndex].numberOfConsumers++;
                        VLOG(COMPILATION) << "origOperandIndexToSlicedIndex compliant input "
                                             "processing created "
                                          << origOperandIndex << " -> " << slicedOperandIndex
                                          << ": " << toString(slicedOperands[slicedOperandIndex]);
                        return slicedOperandIndex;
                    });

            // Operation outputs
            // - Add new operands to slicedOperands
            // - Update origOperandIndexToSlicedIndex
            // - Fill in slicedOperation.outputs
            // - Record as a model output, if necessary
            const uint32_t firstOutputSlicedOperandIndex = slicedOperands.size();
            slicedOperands.resize(firstOutputSlicedOperandIndex + origOperation.outputs.size());
            slicedOperation.outputs.resize(origOperation.outputs.size());
            for (uint32_t outputNum = 0; outputNum < slicedOperation.outputs.size(); ++outputNum) {
                uint32_t origOperandIndex = origOperation.outputs[outputNum];
                uint32_t slicedOperandIndex = firstOutputSlicedOperandIndex + outputNum;
                auto& slicedOperand = slicedOperands[slicedOperandIndex];
                const auto& origOperand = origOperands[origOperandIndex];
                slicedOperand = convertTo<SlicedOperand>(origOperand);
                slicedOperand.numberOfConsumers = 0;

                CHECK_EQ(origOperandIndexToSlicedIndex->count(origOperandIndex), size_t(0));
                (*origOperandIndexToSlicedIndex)[origOperandIndex] = slicedOperandIndex;
                slicedOperation.outputs[outputNum] = slicedOperandIndex;

                const auto subgraphOutputLifetime = convertTo<decltype(slicedOperand.lifetime)>(
                        OperandLifeTime::SUBGRAPH_OUTPUT);
                if (!inputOperandIndexesOfCompliantOperations.count(origOperandIndex) &&
                    origOperand.numberOfConsumers) {
                    // Was consumed only by noncompliant operations; convert to
                    // an output of the sliced model.
                    slicedOperand.lifetime = subgraphOutputLifetime;
                }

                VLOG(COMPILATION) << "origOperandIndexToSlicedIndex compliant output created "
                                  << origOperandIndex << " -> " << slicedOperandIndex << ": "
                                  << toString(slicedOperand);

                if (slicedOperand.lifetime == subgraphOutputLifetime) {
                    extend(&slice->mHidlModel.outputIndexes, slicedOperandIndex);
                }
            }
        }
    }
}

template <class T_SlicedModel>
MetaModel::Slice<T_SlicedModel> MetaModel::makeSlice() const {
    using SlicedOperand = typename Slice<T_SlicedModel>::Operand;

    Slice<T_SlicedModel> slice;

    const auto& origOperands = mHidlModel.main.operands;
    const auto& origOperations = mHidlModel.main.operations;
    auto& slicedOperands = slice.mHidlModel.operands;

    // Indexes of elements of noncompliant origOperations
    std::set<uint32_t> noncompliantOperations;
    getNoncompliantOperations<T_SlicedModel>(mHidlModel, &noncompliantOperations);

    // Map from an operand index in origOperands to the corresponding operand index in
    // slicedOperands
    std::map<uint32_t, uint32_t> origOperandIndexToSlicedIndex;

    // Collect the operand indexes of every operand that is an input to a
    // compliant operation.  If the operand is a CONSTANT_* or a NO_VALUE, copy
    // it to the sliced model and update origOperandIndexToSlicedIndex
    // accordingly.  Otherwise, we'll deal with the operand in the subsequent
    // "Main loop", where we process operation outputs (intermediates and model
    // outputs).
    std::set<uint32_t> inputOperandIndexesOfCompliantOperations;
    for (uint32_t origOperationIndex = 0; origOperationIndex < origOperations.size();
         ++origOperationIndex) {
        if (noncompliantOperations.count(origOperationIndex)) {
            continue;
        }
        for (uint32_t input : origOperations[origOperationIndex].inputs) {
            if (inputOperandIndexesOfCompliantOperations.insert(input).second) {
                const Operand& origOperand = origOperands[input];
                switch (origOperand.lifetime) {
                    case OperandLifeTime::CONSTANT_COPY:
                    case OperandLifeTime::CONSTANT_REFERENCE:
                    case OperandLifeTime::NO_VALUE: {
                        const uint32_t slicedOperandIndex =
                                extend(&slicedOperands, convertTo<SlicedOperand>(origOperand))
                                        .first;
                        slicedOperands[slicedOperandIndex].numberOfConsumers = 0;
                        origOperandIndexToSlicedIndex[input] = slicedOperandIndex;
                        VLOG(COMPILATION) << "origOperandIndexToSlicedIndex initialization created "
                                          << input << " -> " << slicedOperandIndex << ": "
                                          << toString(slicedOperands[slicedOperandIndex]);
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }

    OrigOperandToSlicedInputOperandIndex origOperandToSlicedInputOperandIndex(
            &slicedOperands, &slice.mHidlModel.inputIndexes);

    // An input of the original model is an input of the sliced model if and
    // only if it is consumed by at least one compliant operation.  Note that in
    // the sliced model we share all model inputs of the same "type"; and that
    // we may later add model inputs to the sliced model.
    for (uint32_t origInputIndex : mHidlModel.main.inputIndexes) {
        if (inputOperandIndexesOfCompliantOperations.count(origInputIndex)) {
            const uint32_t slicedIndex =
                    origOperandToSlicedInputOperandIndex.getIndex(origOperands[origInputIndex]);
            origOperandIndexToSlicedIndex[origInputIndex] = slicedIndex;
            VLOG(COMPILATION) << "origOperandIndexToSlicedIndex inputIndexes processing created "
                              << origInputIndex << " -> " << slicedIndex << ": "
                              << toString(slicedOperands[slicedIndex]);
        }
    }

    // Main loop: Process each operation of the original model.
    processOperations(&slice, &origOperandIndexToSlicedIndex, &origOperandToSlicedInputOperandIndex,
                      noncompliantOperations, inputOperandIndexesOfCompliantOperations);

    // To keep things simple, we copy over these fields as-is.  We could instead
    // opt to regenerate them based on the operands present in the sliced model:
    // This would be more complex and probably take more computation time, but
    // it would reduce the size of the sliced model, and hence the time spent
    // copying it around and passing it across the HAL interface.
    slice.mHidlModel.operandValues = mHidlModel.operandValues;
    slice.mHidlModel.pools = mHidlModel.pools;

    if (VLOG_IS_ON(COMPILATION)) {
        {
            std::ostringstream fromName;
            fromName << "Slice: From " << ModelVersion<decltype(mHidlModel)>::name;
            graphDump(fromName.str().c_str(), mHidlModel);
        }
        {
            std::ostringstream toName;
            toName << "Slice: To " << ModelVersion<decltype(slice.mHidlModel)>::name;
            graphDump(toName.str().c_str(), convertToV1_3(slice.mHidlModel));
        }
    }

    slice.mState =
            invalid(slice.mHidlModel, mStrictSlicing) ? SliceState::INVALID : SliceState::NORMAL;

    return slice;
}

template MetaModel::Slice<V1_0::Model> MetaModel::makeSlice() const;
template MetaModel::Slice<V1_1::Model> MetaModel::makeSlice() const;

}  // namespace android::nn
