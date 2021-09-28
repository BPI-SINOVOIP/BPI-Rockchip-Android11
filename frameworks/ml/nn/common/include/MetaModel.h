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

#ifndef ANDROID_FRAMEWORKS_ML_NN_COMMON_META_MODEL_H
#define ANDROID_FRAMEWORKS_ML_NN_COMMON_META_MODEL_H

#include "HalInterfaces.h"

#include <android-base/macros.h>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace android::nn {

// The MetaModel class encapsulates a Model and provides machinery to create
// from that original Model a "slice" of that Model consisting of:
// - the subset of operations that is compliant with a particular HAL version; and
// - a mechanism for mapping operations from the slice back to operations of the
//   original Model.
// The slice is intended to be passed to IDevice::getSupportedOperations*(),
// with the mapping used to translate the results of that call from the slice's
// operations to the original Model's operations.  The slice has no other
// purpose (for example, it is not guaranteed to have the same topology as a
// subgraph of the original model).
//
// When a getSlice*() method is called, a slice is created and cached, if
// necessary; and then the cached slice is returned.
//
// The meaning of the return value of the getSlice*() methods is explained by
// the following example:
//
//     const MetaModel& metaModel = ...;
//     auto ret = metaModel.getSliceV1_0();  // getSliceV1_1() is similar
//     if (ret.has_value()) {
//         const V1_0::Model model = ret->first;  // the slice
//         auto mapper = ret->second;
//         // mapper is a functor that takes an operation index in the
//         // slice and returns the corresponding operation index in the
//         // original Model.  The functor will remain valid for the lifetime
//         // of the MetaModel.
//     } else {
//         // Could not obtain a slice.  For example, perhaps none of the
//         // original model's operations are compliant with V1_0.
//     }
//
class MetaModel {
   public:
    using Mapper = std::function<uint32_t(uint32_t)>;

    template <class T_Model>
    using ReturnedSlice = std::optional<std::pair<T_Model, Mapper>>;

    MetaModel(hal::Model model, bool strictSlicing)
        : mHidlModel(std::move(model)), mStrictSlicing(strictSlicing) {}

    const hal::Model& getModel() const { return mHidlModel; }

    ReturnedSlice<hal::V1_0::Model> getSliceV1_0() const { return getSlice(&mSliceV1_0); }
    ReturnedSlice<hal::V1_1::Model> getSliceV1_1() const { return getSlice(&mSliceV1_1); }
    ReturnedSlice<hal::V1_2::Model> getSliceV1_2() const { return getSlice(&mSliceV1_2); }

    // Disallowing copy constructor and assignment operator is for efficiency,
    // not for correctness.  The default copy constructor and assignment
    // operator would work fine.  However, they could be surprisingly expensive
    // if the mSlice* members get copied: Up to three Model instances and two
    // std::vector instances could be copied.  We could choose to accept this
    // expense; or we could write custom copy and assign that do not copy the
    // mSlice* members but instead set the destination mSlice* members to
    // SliceState::UNINITIALIZED.
    //
    // There are no such issues with move constructor and move assignment.
    MetaModel(const MetaModel&) = delete;
    MetaModel& operator=(const MetaModel&) = delete;
    MetaModel(MetaModel&&) = default;
    MetaModel& operator=(MetaModel&&) = default;

   private:
    hal::Model mHidlModel;

    // mStrictSlicing controls sanity checking.  If the slicing algorithm
    // produces an invalid model (because something has gone wrong with the
    // algorithm or with a utility function it depends on), getSlice*() can
    // return an std::optional<> for which has_value() returns false, signifying
    // that no slice is available.  However, if mStrictSlicing is true,
    // getSlice*() cause a CHECK*() to fail.  This can be used in debugging to
    // find situations where slicing has failed unexpectedly.
    bool mStrictSlicing;

    enum class SliceState { UNINITIALIZED, INVALID, NORMAL };
    template <class T_SlicedModel>
    struct Slice {
        SliceState mState = SliceState::UNINITIALIZED;
        T_SlicedModel mHidlModel;
        std::vector<uint32_t> mSlicedOperationIndexToOrigIndex;

        using Operand = typename decltype(mHidlModel.operands)::value_type;
        using Operation = typename decltype(mHidlModel.operations)::value_type;
        using OperationType = decltype(Operation::type);
    };
    mutable Slice<hal::V1_0::Model> mSliceV1_0;
    mutable Slice<hal::V1_1::Model> mSliceV1_1;
    mutable Slice<hal::V1_2::Model> mSliceV1_2;

    template <class T_SlicedModel>
    ReturnedSlice<T_SlicedModel> getSlice(Slice<T_SlicedModel>* slice) const;

    template <class T_SlicedModel>
    Slice<T_SlicedModel> makeSlice() const;

    // Utility class for makeSlice().
    template <typename T_SlicedOperand>
    class OrigOperandToSlicedInputOperandIndex;

    // Utility function for makeSlice(): Walks operations of original
    // model and populates sliced model accordingly.
    template <class T_SlicedModel>
    void processOperations(
            Slice<T_SlicedModel>* slice,
            std::map<uint32_t, uint32_t>* origOperandIndexToSlicedIndex,
            OrigOperandToSlicedInputOperandIndex<typename Slice<T_SlicedModel>::Operand>*
                    origOperandToSlicedInputOperandIndex,
            const std::set<uint32_t>& noncompliantOperations,
            const std::set<uint32_t>& inputOperandIndexesOfCompliantOperations) const;
};

}  // namespace android::nn

#endif  // ANDROID_FRAMEWORKS_ML_NN_COMMON_META_MODEL_H
