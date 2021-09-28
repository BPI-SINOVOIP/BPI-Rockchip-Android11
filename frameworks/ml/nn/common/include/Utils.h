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

#ifndef ANDROID_FRAMEWORKS_ML_NN_COMMON_UTILS_H
#define ANDROID_FRAMEWORKS_ML_NN_COMMON_UTILS_H

#include <android-base/logging.h>

#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "HalInterfaces.h"
#include "NeuralNetworks.h"
#include "ValidateHal.h"

namespace android {
namespace nn {

// The number of data types (OperandCode) defined in NeuralNetworks.h.
const int kNumberOfDataTypes = 16;

// The number of operation types (OperationCode) defined in NeuralNetworks.h.
const int kNumberOfOperationTypes = 102;

// The number of execution preferences defined in NeuralNetworks.h.
const int kNumberOfPreferences = 3;

// The number of data types (OperandCode) defined in NeuralNetworksOEM.h.
const int kNumberOfDataTypesOEM = 2;

// The number of operation types (OperationCode) defined in NeuralNetworksOEM.h.
const int kNumberOfOperationTypesOEM = 1;

// The lowest number assigned to any OEM Code in NeuralNetworksOEM.h.
const int kOEMCodeBase = 10000;

/* IMPORTANT: if you change the following list, don't
 * forget to update the corresponding 'tags' table in
 * the initVlogMask() function implemented in Utils.cpp.
 */
enum VLogFlags { MODEL = 0, COMPILATION, EXECUTION, CPUEXE, MANAGER, DRIVER, MEMORY };

#define VLOG_IS_ON(TAG) ((vLogMask & (1 << (TAG))) != 0)

#define VLOG(TAG)                 \
    if (LIKELY(!VLOG_IS_ON(TAG))) \
        ;                         \
    else                          \
        LOG(INFO)

extern int vLogMask;
void initVLogMask();

#ifdef NN_DEBUGGABLE
#define SHOW_IF_DEBUG(msg) msg
#else
#define SHOW_IF_DEBUG(msg) ""
#endif

// DEPRECATED(b/118737105). Use CHECK.
#define nnAssert(v) CHECK(v)

#define NN_RETURN_IF_ERROR(expr)                      \
    do {                                              \
        int _errorCode = (expr);                      \
        if (_errorCode != ANEURALNETWORKS_NO_ERROR) { \
            return _errorCode;                        \
        }                                             \
    } while (0)

// The NN_RET_CHECK family of macros defined below is similar to the CHECK family defined in
// system/core/base/include/android-base/logging.h
//
// The difference is that NN_RET_CHECK macros use LOG(ERROR) instead of LOG(FATAL)
// and return false instead of aborting.

// Logs an error and returns false. Append context using << after. For example:
//
//   NN_RET_CHECK_FAIL() << "Something went wrong";
//
// The containing function must return a bool.
#define NN_RET_CHECK_FAIL()                   \
    return ::android::nn::FalseyErrorStream() \
           << "NN_RET_CHECK failed (" << __FILE__ << ":" << __LINE__ << "): "

// Logs an error and returns false if condition is false. Extra logging can be appended using <<
// after. For example:
//
//   NN_RET_CHECK(false) << "Something went wrong";
//
// The containing function must return a bool.
#define NN_RET_CHECK(condition) \
    while (UNLIKELY(!(condition))) NN_RET_CHECK_FAIL() << #condition << " "

// Helper for NN_CHECK_xx(x, y) macros.
#define NN_RET_CHECK_OP(LHS, RHS, OP)                                                 \
    for (auto _values = ::android::base::MakeEagerEvaluator(LHS, RHS);                \
         UNLIKELY(!(_values.lhs OP _values.rhs));                                     \
         /* empty */)                                                                 \
    NN_RET_CHECK_FAIL() << #LHS << " " << #OP << " " << #RHS << " (" << #LHS << " = " \
                        << _values.lhs << ", " << #RHS << " = " << _values.rhs << ") "

// Logs an error and returns false if a condition between x and y does not hold. Extra logging can
// be appended using << after. For example:
//
//   NN_RET_CHECK_EQ(a, b) << "Something went wrong";
//
// The values must implement the appropriate comparison operator as well as
// `operator<<(std::ostream&, ...)`.
// The containing function must return a bool.
#define NN_RET_CHECK_EQ(x, y) NN_RET_CHECK_OP(x, y, ==)
#define NN_RET_CHECK_NE(x, y) NN_RET_CHECK_OP(x, y, !=)
#define NN_RET_CHECK_LE(x, y) NN_RET_CHECK_OP(x, y, <=)
#define NN_RET_CHECK_LT(x, y) NN_RET_CHECK_OP(x, y, <)
#define NN_RET_CHECK_GE(x, y) NN_RET_CHECK_OP(x, y, >=)
#define NN_RET_CHECK_GT(x, y) NN_RET_CHECK_OP(x, y, >)

// Type to represent a deadline time point across processes.
using Deadline = std::chrono::steady_clock::time_point;

// Make an Deadline from a duration. If the sum of the current time and the
// duration exceeds the max time, return a time point holding the maximum
// expressible time.
Deadline makeDeadline(uint64_t duration);

// Convenience function. If the duration is provided, this function creates a
// Deadline using makeDeadline. If the duration is not provided, this function
// returns std::nullopt.
std::optional<Deadline> makeDeadline(std::optional<uint64_t> duration);

// Make an optional Deadline from an OptionalTimePoint. If
// timePoint.nanosecondsSinceEpoch cannot be represented in Deadline, return a
// time point holding the maximum Deadline. If the OptionalTimePoint is none,
// this function returns std::nullopt.
std::optional<Deadline> makeDeadline(const hal::OptionalTimePoint& timePoint);

// Returns true if the deadline has passed. Returns false if either the deadline
// has not been exceeded or if the deadline is not present.
bool hasDeadlinePassed(const std::optional<Deadline>& deadline);

// Make an OptionalTimePoint from an optional Deadline. If the Deadline is not
// provided, this function returns none for OptionalTimePoint.
hal::OptionalTimePoint makeTimePoint(const std::optional<Deadline>& deadline);

// Ensure that every user of FalseyErrorStream is linked to the
// correct instance, using the correct LOG_TAG
namespace {

// A wrapper around LOG(ERROR) that can be implicitly converted to bool (always evaluates to false).
// Used to implement stream logging in NN_RET_CHECK.
class FalseyErrorStream {
    DISALLOW_COPY_AND_ASSIGN(FalseyErrorStream);

   public:
    FalseyErrorStream() {}

    template <typename T>
    FalseyErrorStream& operator<<(const T& value) {
        mBuffer << value;
        return *this;
    }

    ~FalseyErrorStream() { LOG(ERROR) << mBuffer.str(); }

    operator bool() const { return false; }

   private:
    std::ostringstream mBuffer;
};

template <HalVersion version>
struct VersionedType {};

template <>
struct VersionedType<HalVersion::V1_2> {
    using OperandPerformance = hal::V1_2::Capabilities::OperandPerformance;
    using OperandType = hal::V1_2::OperandType;
};

template <>
struct VersionedType<HalVersion::V1_3> {
    using OperandPerformance = hal::V1_3::Capabilities::OperandPerformance;
    using OperandType = hal::V1_3::OperandType;
};

template <HalVersion version>
using VersionedOperandPerformance = typename VersionedType<version>::OperandPerformance;
template <HalVersion version>
using VersionedOperandType = typename VersionedType<version>::OperandType;

}  // namespace

// Return a vector with one entry for each non-extension OperandType except
// SUBGRAPH, set to the specified PerformanceInfo value.  The vector will be
// sorted by OperandType.
//
// Control flow (OperandType::SUBGRAPH) operation performance is specified
// separately using Capabilities::ifPerformance and
// Capabilities::whilePerformance.
template <HalVersion version>
hal::hidl_vec<VersionedOperandPerformance<version>> nonExtensionOperandPerformance(
        hal::PerformanceInfo perf);

// Update the vector entry corresponding to the specified OperandType with the
// specified PerformanceInfo value.  The vector must already have an entry for
// that OperandType, and must be sorted by OperandType.
void update(hal::hidl_vec<hal::V1_2::Capabilities::OperandPerformance>* operandPerformance,
            hal::V1_2::OperandType type, hal::PerformanceInfo perf);
void update(hal::hidl_vec<hal::V1_3::Capabilities::OperandPerformance>* operandPerformance,
            hal::V1_3::OperandType type, hal::PerformanceInfo perf);

// Look for a vector entry corresponding to the specified OperandType.  If
// found, return the associated PerformanceInfo.  If not, return a pessimistic
// PerformanceInfo (FLT_MAX).  The vector must be sorted by OperandType.
hal::PerformanceInfo lookup(
        const hal::hidl_vec<hal::V1_2::Capabilities::OperandPerformance>& operandPerformance,
        hal::V1_2::OperandType type);
hal::PerformanceInfo lookup(
        const hal::hidl_vec<hal::V1_3::Capabilities::OperandPerformance>& operandPerformance,
        hal::V1_3::OperandType type);

// Returns true if an operand type is an extension type.
bool isExtensionOperandType(hal::OperandType type);

// Returns true if an operation type is an extension type.
bool isExtensionOperationType(hal::OperationType type);

// Returns the amount of space needed to store a value of the specified
// dimensions and type. For a tensor with unspecified rank or at least one
// unspecified dimension, returns zero.
//
// Aborts if the specified type is an extension type.
// Aborts if the size would overflow the return type.
//
// See also TypeManager::getSizeOfData(OperandType, const std::vector<uint32_t>&).
uint32_t nonExtensionOperandSizeOfData(hal::OperandType type,
                                       const std::vector<uint32_t>& dimensions);

// Returns the amount of space needed to store a value of the dimensions and
// type of this operand. For a tensor with unspecified rank or at least one
// unspecified dimension, returns zero.
//
// Aborts if the specified type is an extension type.
// Aborts if the size would overflow the return type.
//
// See also TypeManager::getSizeOfData(const Operand&).
inline uint32_t nonExtensionOperandSizeOfData(const hal::Operand& operand) {
    return nonExtensionOperandSizeOfData(operand.type, operand.dimensions);
}

// Returns the amount of space needed to store a value of the specified
// dimensions and element size. For a tensor with unspecified rank or at least
// one unspecified dimension, returns zero.
//
// Aborts if the size would overflow the return type.
//
// See also TypeManager::getSizeOfData(const Operand&).
uint32_t sizeOfTensorData(uint32_t sizeOfElement, const std::vector<uint32_t>& dimensions);

// Returns true if the amount of space needed to store a value of the specified
// dimensions and element size overflows the uint32_t type.
//
// Aborts if the specified type is an extension type.
//
// See also TypeManager::sizeOfDataOverflowsUInt32(OperandType, const std::vector<uint32_t>&).
bool nonExtensionOperandSizeOfDataOverflowsUInt32(hal::OperandType type,
                                                  const std::vector<uint32_t>& dimensions);

// Returns true if the amount of space needed to store a value of the specified
// dimensions and element size overflows the uint32_t type.
//
// See also TypeManager::sizeOfDataOverflowsUInt32(OperandType, const std::vector<uint32_t>&).
bool sizeOfTensorDataOverflowsUInt32(uint32_t elementSize, const std::vector<uint32_t>& dimensions);

// Returns true if a non-extension operand type is a scalar type.
//
// Aborts if the specified type is an extension type.
//
// See also TypeManager::isTensorType(OperandType).
bool nonExtensionOperandTypeIsScalar(int type);

// Returns the name of the operation type in ASCII.
std::string getOperationName(hal::OperationType opCode);

// Returns the name of the operand type in ASCII.
std::string getOperandTypeName(hal::OperandType type);

// Whether an operand of tensor type has unspecified dimensions.
//
// Undefined behavior if the operand type is a scalar type.
bool tensorHasUnspecifiedDimensions(int type, const uint32_t* dim, uint32_t dimCount);
bool tensorHasUnspecifiedDimensions(hal::OperandType type, const std::vector<uint32_t>& dimensions);
bool tensorHasUnspecifiedDimensions(const hal::Operand& operand);
bool tensorHasUnspecifiedDimensions(const ANeuralNetworksOperandType* type);

// Returns the number of padding bytes needed to align data of the
// specified length.  It aligns object of length:
// 2, 3 on a 2 byte boundary,
// 4+ on a 4 byte boundary.
// We may want to have different alignments for tensors.
// TODO: This is arbitrary, more a proof of concept.  We need
// to determine what this should be.
uint32_t alignBytesNeeded(uint32_t index, size_t length);

// Does a detailed LOG(INFO) of the model
void logModelToInfo(const hal::V1_0::Model& model);
void logModelToInfo(const hal::V1_1::Model& model);
void logModelToInfo(const hal::V1_2::Model& model);
void logModelToInfo(const hal::V1_3::Model& model);

inline std::string toString(uint32_t obj) {
    return std::to_string(obj);
}

template <typename Type>
std::string toString(const std::vector<Type>& range) {
    std::string os = "[";
    for (size_t i = 0; i < range.size(); ++i) {
        os += (i == 0 ? "" : ", ") + toString(range[i]);
    }
    return os += "]";
}

template <typename A, typename B>
std::string toString(const std::pair<A, B>& pair) {
    std::ostringstream oss;
    oss << "(" << toString(pair.first) << ", " << toString(pair.second) << ")";
    return oss.str();
}

inline std::string toString(HalVersion halVersion) {
    switch (halVersion) {
        case HalVersion::UNKNOWN:
            return "UNKNOWN HAL version";
        case HalVersion::V1_0:
            return "HAL version 1.0";
        case HalVersion::V1_1:
            return "HAL version 1.1";
        case HalVersion::V1_2:
            return "HAL version 1.2";
        case HalVersion::V1_3:
            return "HAL version 1.3";
    }
}

inline bool validCode(uint32_t codeCount, uint32_t codeCountOEM, uint32_t code) {
    return (code < codeCount) || (code >= kOEMCodeBase && (code - kOEMCodeBase) < codeCountOEM);
}

bool validateOperandSymmPerChannelQuantParams(
        const hal::Operand& halOperand,
        const ANeuralNetworksSymmPerChannelQuantParams& channelQuant, const char* tag);

// Validates an operand type.
//
// extensionOperandTypeInfo must be nullptr iff the type is not an extension type.
//
// If allowPartial is true, the dimensions may be underspecified.
int validateOperandType(
        const ANeuralNetworksOperandType& type,
        const hal::Extension::OperandTypeInformation* const extensionOperandTypeInfo,
        const char* tag, bool allowPartial);
int validateOperandList(uint32_t count, const uint32_t* list, uint32_t operandCount,
                        const char* tag);

// A set of functions to help validate models containing IF or WHILE operations.
struct SubgraphValidationHelper {
    // Checks if a given operand is a SUBGRAPH operand with a valid offset.
    std::function<bool(const hal::Operand&)> isValidSubgraphReference;
    // Gets the input count of a subgraph referenced by a given operand.
    std::function<uint32_t(const hal::Operand&)> getSubgraphInputCount;
    // Gets the output count of a subgraph referenced by a given operand.
    std::function<uint32_t(const hal::Operand&)> getSubgraphOutputCount;
    // Gets the specified input operand of a subgraph referenced by a given operand.
    std::function<const hal::Operand*(const hal::Operand&, uint32_t)> getSubgraphInputOperand;
    // Gets the specified output operand of a subgraph referenced by a given operand.
    std::function<const hal::Operand*(const hal::Operand&, uint32_t)> getSubgraphOutputOperand;
    // Whether control flow operations with inner or outer input or output
    // operands of unknown size are allowed.
    bool allowControlFlowOperationWithOperandOfUnknownSize;
};

// Returns ANEURALNETWORKS_NO_ERROR if the corresponding operation is defined and can handle the
// provided operand types in the given HAL version, otherwise returns ANEURALNETWORKS_BAD_DATA.
// The last argument is only used for validating IF and WHILE operations.
int validateOperation(ANeuralNetworksOperationType opType, uint32_t inputCount,
                      const uint32_t* inputIndexes, uint32_t outputCount,
                      const uint32_t* outputIndexes, const std::vector<hal::Operand>& operands,
                      HalVersion halVersion, const SubgraphValidationHelper& helper);

inline size_t getSizeFromInts(int lower, int higher) {
    return (uint32_t)(lower) + ((uint64_t)(uint32_t)(higher) << 32);
}

// Convert ANEURALNETWORKS_* result code to ErrorStatus.
// Not guaranteed to be a 1-to-1 mapping.
hal::ErrorStatus convertResultCodeToErrorStatus(int resultCode);

// Convert ErrorStatus to ANEURALNETWORKS_* result code.
// Not guaranteed to be a 1-to-1 mapping.
int convertErrorStatusToResultCode(hal::ErrorStatus status);

// Convert execution results to runtime format. Additionally checks that the
// returned results abide by the HAL specification, and logs an error if the
// result violates the specification.
std::tuple<int, std::vector<hal::OutputShape>, hal::Timing> getExecutionResult(
        hal::ErrorStatus status, std::vector<hal::OutputShape> outputShapes, hal::Timing timing);

// Combine two tensor dimensions, both may have unspecified dimensions or rank.
std::optional<std::vector<uint32_t>> combineDimensions(const std::vector<uint32_t>& lhs,
                                                       const std::vector<uint32_t>& rhs);

// Versioning

bool compliantWithV1_0(const hal::V1_0::Capabilities& capabilities);
bool compliantWithV1_0(const hal::V1_1::Capabilities& capabilities);
bool compliantWithV1_0(const hal::V1_2::Capabilities& capabilities);
bool compliantWithV1_0(const hal::V1_3::Capabilities& capabilities);
bool compliantWithV1_1(const hal::V1_0::Capabilities& capabilities);
bool compliantWithV1_1(const hal::V1_1::Capabilities& capabilities);
bool compliantWithV1_1(const hal::V1_2::Capabilities& capabilities);
bool compliantWithV1_1(const hal::V1_3::Capabilities& capabilities);
bool compliantWithV1_2(const hal::V1_0::Capabilities& capabilities);
bool compliantWithV1_2(const hal::V1_1::Capabilities& capabilities);
bool compliantWithV1_2(const hal::V1_2::Capabilities& capabilities);
bool compliantWithV1_2(const hal::V1_3::Capabilities& capabilities);
bool compliantWithV1_3(const hal::V1_0::Capabilities& capabilities);
bool compliantWithV1_3(const hal::V1_1::Capabilities& capabilities);
bool compliantWithV1_3(const hal::V1_2::Capabilities& capabilities);
bool compliantWithV1_3(const hal::V1_3::Capabilities& capabilities);

// If noncompliantOperations != nullptr, then
//     precondition: noncompliantOperations->empty()
//     postcondition: *noncompliantOperations consists of the indices of the noncompliant
//                    operations; if the compliance check fails for some reason
//                    other than a noncompliant operation,
//                    *noncompliantOperations consists of the indices of all operations
bool compliantWithV1_0(const hal::V1_0::Model& model);
bool compliantWithV1_0(const hal::V1_1::Model& model);
bool compliantWithV1_0(const hal::V1_2::Model& model,
                       std::set<uint32_t>* noncompliantOperations = nullptr);
bool compliantWithV1_0(const hal::V1_3::Model& model,
                       std::set<uint32_t>* noncompliantOperations = nullptr);
bool compliantWithV1_1(const hal::V1_0::Model& model);
bool compliantWithV1_1(const hal::V1_1::Model& model);
bool compliantWithV1_1(const hal::V1_2::Model& model,
                       std::set<uint32_t>* noncompliantOperations = nullptr);
bool compliantWithV1_1(const hal::V1_3::Model& model,
                       std::set<uint32_t>* noncompliantOperations = nullptr);
bool compliantWithV1_2(const hal::V1_0::Model& model);
bool compliantWithV1_2(const hal::V1_1::Model& model);
bool compliantWithV1_2(const hal::V1_2::Model& model,
                       std::set<uint32_t>* noncompliantOperations = nullptr);
bool compliantWithV1_2(const hal::V1_3::Model& model,
                       std::set<uint32_t>* noncompliantOperations = nullptr);

hal::V1_0::ErrorStatus convertToV1_0(hal::V1_0::ErrorStatus status);
hal::V1_0::ErrorStatus convertToV1_0(hal::V1_3::ErrorStatus status);
hal::V1_3::ErrorStatus convertToV1_3(hal::V1_0::ErrorStatus status);
hal::V1_3::ErrorStatus convertToV1_3(hal::V1_3::ErrorStatus status);

hal::V1_0::Capabilities convertToV1_0(const hal::V1_0::Capabilities& capabilities);
hal::V1_0::Capabilities convertToV1_0(const hal::V1_1::Capabilities& capabilities);
hal::V1_0::Capabilities convertToV1_0(const hal::V1_2::Capabilities& capabilities);
hal::V1_0::Capabilities convertToV1_0(const hal::V1_3::Capabilities& capabilities);
hal::V1_1::Capabilities convertToV1_1(const hal::V1_0::Capabilities& capabilities);
hal::V1_1::Capabilities convertToV1_1(const hal::V1_1::Capabilities& capabilities);
hal::V1_1::Capabilities convertToV1_1(const hal::V1_2::Capabilities& capabilities);
hal::V1_1::Capabilities convertToV1_1(const hal::V1_3::Capabilities& capabilities);
hal::V1_2::Capabilities convertToV1_2(const hal::V1_0::Capabilities& capabilities);
hal::V1_2::Capabilities convertToV1_2(const hal::V1_1::Capabilities& capabilities);
hal::V1_2::Capabilities convertToV1_2(const hal::V1_2::Capabilities& capabilities);
hal::V1_2::Capabilities convertToV1_2(const hal::V1_3::Capabilities& capabilities);
hal::V1_3::Capabilities convertToV1_3(const hal::V1_0::Capabilities& capabilities);
hal::V1_3::Capabilities convertToV1_3(const hal::V1_1::Capabilities& capabilities);
hal::V1_3::Capabilities convertToV1_3(const hal::V1_2::Capabilities& capabilities);
hal::V1_3::Capabilities convertToV1_3(const hal::V1_3::Capabilities& capabilities);

hal::V1_0::Model convertToV1_0(const hal::V1_0::Model& model);
hal::V1_0::Model convertToV1_0(const hal::V1_1::Model& model);
hal::V1_0::Model convertToV1_0(const hal::V1_2::Model& model);
hal::V1_0::Model convertToV1_0(const hal::V1_3::Model& model);
hal::V1_1::Model convertToV1_1(const hal::V1_0::Model& model);
hal::V1_1::Model convertToV1_1(const hal::V1_1::Model& model);
hal::V1_1::Model convertToV1_1(const hal::V1_2::Model& model);
hal::V1_1::Model convertToV1_1(const hal::V1_3::Model& model);
hal::V1_2::Model convertToV1_2(const hal::V1_0::Model& model);
hal::V1_2::Model convertToV1_2(const hal::V1_1::Model& model);
hal::V1_2::Model convertToV1_2(const hal::V1_2::Model& model);
hal::V1_2::Model convertToV1_2(const hal::V1_3::Model& model);
hal::V1_3::Model convertToV1_3(const hal::V1_0::Model& model);
hal::V1_3::Model convertToV1_3(const hal::V1_1::Model& model);
hal::V1_3::Model convertToV1_3(const hal::V1_2::Model& model);
hal::V1_3::Model convertToV1_3(const hal::V1_3::Model& model);

hal::V1_0::OperationType uncheckedConvertToV1_0(hal::V1_3::OperationType type);
hal::V1_1::OperationType uncheckedConvertToV1_1(hal::V1_3::OperationType type);
hal::V1_2::OperationType uncheckedConvertToV1_2(hal::V1_3::OperationType type);

hal::V1_0::Operand convertToV1_0(const hal::V1_2::Operand& operand);
hal::V1_0::Operand convertToV1_0(const hal::V1_3::Operand& operand);
hal::V1_2::Operand convertToV1_2(const hal::V1_0::Operand& operand);
hal::V1_2::Operand convertToV1_2(const hal::V1_3::Operand& operand);
hal::V1_3::Operand convertToV1_3(const hal::V1_0::Operand& operand);
hal::V1_3::Operand convertToV1_3(const hal::V1_2::Operand& operand);
hal::V1_3::Operand convertToV1_3(const hal::V1_3::Operand& operand);

hal::hidl_vec<hal::V1_0::Operand> convertToV1_0(const hal::hidl_vec<hal::V1_0::Operand>& operands);
hal::hidl_vec<hal::V1_0::Operand> convertToV1_0(const hal::hidl_vec<hal::V1_2::Operand>& operands);
hal::hidl_vec<hal::V1_0::Operand> convertToV1_0(const hal::hidl_vec<hal::V1_3::Operand>& operands);
hal::hidl_vec<hal::V1_2::Operand> convertToV1_2(const hal::hidl_vec<hal::V1_0::Operand>& operands);
hal::hidl_vec<hal::V1_2::Operand> convertToV1_2(const hal::hidl_vec<hal::V1_2::Operand>& operands);
hal::hidl_vec<hal::V1_2::Operand> convertToV1_2(const hal::hidl_vec<hal::V1_3::Operand>& operands);
hal::hidl_vec<hal::V1_3::Operand> convertToV1_3(const hal::hidl_vec<hal::V1_0::Operand>& operands);
hal::hidl_vec<hal::V1_3::Operand> convertToV1_3(const hal::hidl_vec<hal::V1_2::Operand>& operands);
hal::hidl_vec<hal::V1_3::Operand> convertToV1_3(const hal::hidl_vec<hal::V1_3::Operand>& operands);

bool compliantWithV1_0(const hal::V1_0::Request& request);
bool compliantWithV1_0(const hal::V1_3::Request& request);
bool compliantWithV1_2(const hal::V1_3::Request& request);

hal::V1_0::Request convertToV1_0(const hal::V1_0::Request& request);
hal::V1_0::Request convertToV1_0(const hal::V1_3::Request& request);
hal::V1_0::Request convertToV1_2(const hal::V1_3::Request& request);
hal::V1_3::Request convertToV1_3(const hal::V1_0::Request& request);
hal::V1_3::Request convertToV1_3(const hal::V1_3::Request& request);

bool compliantWithV1_0(hal::V1_0::OperandLifeTime lifetime);
bool compliantWithV1_0(hal::V1_3::OperandLifeTime lifetime);
bool compliantWithV1_3(hal::V1_0::OperandLifeTime lifetime);
bool compliantWithV1_3(hal::V1_3::OperandLifeTime lifetime);

hal::V1_0::OperandLifeTime convertToV1_0(hal::V1_0::OperandLifeTime lifetime);
hal::V1_0::OperandLifeTime convertToV1_0(hal::V1_3::OperandLifeTime lifetime);
hal::V1_3::OperandLifeTime convertToV1_3(hal::V1_0::OperandLifeTime lifetime);
hal::V1_3::OperandLifeTime convertToV1_3(hal::V1_3::OperandLifeTime lifetime);

constexpr hal::Priority convertToHalPriority(int32_t priority) {
    switch (priority) {
        case ANEURALNETWORKS_PRIORITY_LOW:
            return hal::Priority::LOW;
        case ANEURALNETWORKS_PRIORITY_MEDIUM:
            return hal::Priority::MEDIUM;
        case ANEURALNETWORKS_PRIORITY_HIGH:
            return hal::Priority::HIGH;
    }
    LOG(FATAL) << "unrecognized priority: " << priority;
    return {};
}

// The function syncWait() has the same semantics as the system function
// ::sync_wait(), except that the syncWait() return value is semantically
// richer.  The timeout parameter is in msecs.
enum class FenceState {
    ACTIVE,    // fence has not been signaled
    SIGNALED,  // fence has been signaled
    ERROR,     // fence has been placed in the error state
    UNKNOWN,   // either bad argument passed to syncWait(), or internal error
};
FenceState syncWait(int fd, int timeout);

#ifdef NN_DEBUGGABLE
uint32_t getProp(const char* str, uint32_t defaultValue = 0);
#endif  // NN_DEBUGGABLE

}  // namespace nn
}  // namespace android

#endif  // ANDROID_FRAMEWORKS_ML_NN_COMMON_UTILS_H
