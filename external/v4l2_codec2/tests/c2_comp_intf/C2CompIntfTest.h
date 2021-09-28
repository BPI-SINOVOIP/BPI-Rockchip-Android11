// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_C2_COMPONENT_INTERFACE_TEST_H
#define ANDROID_C2_COMPONENT_INTERFACE_TEST_H

#include <C2Config.h>
#include <C2PlatformSupport.h>
#include <util/C2InterfaceHelper.h>

#include <gtest/gtest.h>

#include <inttypes.h>
#include <limits>
#include <memory>

namespace android {

class C2CompIntfTest : public ::testing::Test {
public:
    void dumpParamDescriptions();

    // The following codes are template implicit instantiations. These codes should not be separated
    // into an individual cpp file from explicit specializations. Otherwise the compiler will fail
    // to instantiate templates when the explicit specialization is present.

    template <typename T>
    void testReadOnlyParam(const T* expected, T* invalid) {
        testReadOnlyParamOnStack(expected, invalid);
        testReadOnlyParamOnHeap(expected, invalid);
    }

    template <typename T>
    void checkReadOnlyFailureOnConfig(T* param) {
        std::vector<C2Param*> params{param};
        std::vector<std::unique_ptr<C2SettingResult>> failures;

        // TODO: do not assert on checking return value since it is not consistent for
        //       C2InterfaceHelper now. (b/79720928)
        //   1) if config same value, it returns C2_OK
        //   2) if config different value, it returns C2_CORRUPTED. But when you config again, it
        //      returns C2_OK
        //ASSERT_EQ(C2_BAD_VALUE, mIntf->config_vb(params, C2_DONT_BLOCK, &failures));
        mIntf->config_vb(params, C2_DONT_BLOCK, &failures);

        // TODO: failure is not yet supported for C2InterfaceHelper
        //ASSERT_EQ(1u, failures.size());
        //EXPECT_EQ(C2SettingResult::READ_ONLY, failures[0]->failure);
    }

    // Note: this is not suitable for testing flex-type parameters.
    template <typename T>
    void testReadOnlyParamOnStack(const T* expected, T* invalid) {
        T param;
        std::vector<C2Param*> stackParams{&param};
        ASSERT_EQ(C2_OK, mIntf->query_vb(stackParams, {}, C2_DONT_BLOCK, nullptr));
        EXPECT_EQ(*expected, param);

        checkReadOnlyFailureOnConfig(&param);
        checkReadOnlyFailureOnConfig(invalid);

        // The param must not change after failed config.
        ASSERT_EQ(C2_OK, mIntf->query_vb(stackParams, {}, C2_DONT_BLOCK, nullptr));
        EXPECT_EQ(*expected, param);
    }

    template <typename T>
    void testReadOnlyParamOnHeap(const T* expected, T* invalid) {
        std::vector<std::unique_ptr<C2Param>> heapParams;

        uint32_t index = expected->index();

        ASSERT_EQ(C2_OK, mIntf->query_vb({}, {index}, C2_DONT_BLOCK, &heapParams));
        ASSERT_EQ(1u, heapParams.size());
        EXPECT_EQ(*expected, *heapParams[0]);

        checkReadOnlyFailureOnConfig(heapParams[0].get());
        checkReadOnlyFailureOnConfig(invalid);

        // The param must not change after failed config.
        heapParams.clear();
        ASSERT_EQ(C2_OK, mIntf->query_vb({}, {index}, C2_DONT_BLOCK, &heapParams));
        ASSERT_EQ(1u, heapParams.size());
        EXPECT_EQ(*expected, *heapParams[0]);
    }

    template <typename T>
    void testWritableParam(T* newParam) {
        std::vector<C2Param*> params{newParam};
        std::vector<std::unique_ptr<C2SettingResult>> failures;
        ASSERT_EQ(C2_OK, mIntf->config_vb(params, C2_DONT_BLOCK, &failures));
        EXPECT_EQ(0u, failures.size());

        // The param must change to newParam
        // Check like param on stack
        T param;
        std::vector<C2Param*> stackParams{&param};
        ASSERT_EQ(C2_OK, mIntf->query_vb(stackParams, {}, C2_DONT_BLOCK, nullptr));
        EXPECT_EQ(*newParam, param);

        // Check also like param on heap
        std::vector<std::unique_ptr<C2Param>> heapParams;
        ASSERT_EQ(C2_OK, mIntf->query_vb({}, {newParam->index()}, C2_DONT_BLOCK, &heapParams));
        ASSERT_EQ(1u, heapParams.size());
        EXPECT_EQ(*newParam, *heapParams[0]);
    }

    template <typename T>
    void testInvalidWritableParam(T* invalidParam) {
        // Get the current parameter info
        T preParam;
        std::vector<C2Param*> stackParams{&preParam};
        ASSERT_EQ(C2_OK, mIntf->query_vb(stackParams, {}, C2_DONT_BLOCK, nullptr));

        // Config invalid value. The failure is expected
        std::vector<C2Param*> params{invalidParam};
        std::vector<std::unique_ptr<C2SettingResult>> failures;
        ASSERT_EQ(C2_BAD_VALUE, mIntf->config_vb(params, C2_DONT_BLOCK, &failures));
        EXPECT_EQ(1u, failures.size());

        // The param must not change after config failed
        T param;
        std::vector<C2Param*> stackParams2{&param};
        ASSERT_EQ(C2_OK, mIntf->query_vb(stackParams2, {}, C2_DONT_BLOCK, nullptr));
        EXPECT_EQ(preParam, param);

        // Check also like param on heap
        std::vector<std::unique_ptr<C2Param>> heapParams;
        ASSERT_EQ(C2_OK, mIntf->query_vb({}, {invalidParam->index()}, C2_DONT_BLOCK, &heapParams));
        ASSERT_EQ(1u, heapParams.size());
        EXPECT_EQ(preParam, *heapParams[0]);
    }

    bool isUnderflowSubstract(int32_t a, int32_t b) {
        return a < 0 && b > a - std::numeric_limits<int32_t>::min();
    }

    bool isOverflowAdd(int32_t a, int32_t b) {
        return a > 0 && b > std::numeric_limits<int32_t>::max() - a;
    }

    template <typename T>
    void testWritableVideoSizeParam(int32_t widthMin, int32_t widthMax, int32_t widthStep,
                                    int32_t heightMin, int32_t heightMax, int32_t heightStep) {
        // Test supported values of video size
        T valid;
        for (int32_t h = heightMin; h <= heightMax; h += heightStep) {
            for (int32_t w = widthMin; w <= widthMax; w += widthStep) {
                valid.width = w;
                valid.height = h;
                {
                    SCOPED_TRACE("testWritableParam");
                    testWritableParam(&valid);
                    if (HasFailure()) {
                        printf("Failed while config width = %d, height = %d\n", valid.width,
                               valid.height);
                    }
                    if (HasFatalFailure()) return;
                }
            }
        }

        // TODO: validate possible values in C2InterfaceHelper is not implemented yet.
        //// Test invalid values video size
        //T invalid;
        //// Width or height is smaller than min values
        //if (!isUnderflowSubstract(widthMin, widthStep)) {
        //    invalid.width = widthMin - widthStep;
        //    invalid.height = heightMin;
        //    testInvalidWritableParam(&invalid);
        //}
        //if (!isUnderflowSubstract(heightMin, heightStep)) {
        //    invalid.width = widthMin;
        //    invalid.height = heightMin - heightStep;
        //    testInvalidWritableParam(&invalid);
        //}

        //// Width or height is bigger than max values
        //if (!isOverflowAdd(widthMax, widthStep)) {
        //    invalid.width = widthMax + widthStep;
        //    invalid.height = heightMax;
        //    testInvalidWritableParam(&invalid);
        //}
        //if (!isOverflowAdd(heightMax, heightStep)) {
        //    invalid.width = widthMax;
        //    invalid.height = heightMax + heightStep;
        //    testInvalidWritableParam(&invalid);
        //}

        //// Invalid width/height within the range
        //if (widthStep != 1) {
        //    invalid.width = widthMin + 1;
        //    invalid.height = heightMin;
        //    testInvalidWritableParam(&invalid);
        //}
        //if (heightStep != 1) {
        //    invalid.width = widthMin;
        //    invalid.height = heightMin + 1;
        //    testInvalidWritableParam(&invalid);
        //}
    }

    template <typename T>
    void testWritableProfileLevelParam() {
        T info;

        std::vector<C2FieldSupportedValuesQuery> profileC2FSV = {
                {C2ParamField(&info, &C2StreamProfileLevelInfo::profile),
                 C2FieldSupportedValuesQuery::CURRENT},
        };
        ASSERT_EQ(C2_OK, mIntf->querySupportedValues_vb(profileC2FSV, C2_DONT_BLOCK));
        ASSERT_EQ(1u, profileC2FSV.size());
        ASSERT_EQ(C2_OK, profileC2FSV[0].status);
        ASSERT_EQ(C2FieldSupportedValues::VALUES, profileC2FSV[0].values.type);
        auto& profileFSVValues = profileC2FSV[0].values.values;

        std::vector<C2FieldSupportedValuesQuery> levelC2FSV = {
                {C2ParamField(&info, &C2StreamProfileLevelInfo::level),
                 C2FieldSupportedValuesQuery::CURRENT},
        };
        ASSERT_EQ(C2_OK, mIntf->querySupportedValues_vb(levelC2FSV, C2_DONT_BLOCK));
        ASSERT_EQ(1u, levelC2FSV.size());
        ASSERT_EQ(C2_OK, levelC2FSV[0].status);
        ASSERT_EQ(C2FieldSupportedValues::VALUES, levelC2FSV[0].values.type);
        auto& levelFSVValues = levelC2FSV[0].values.values;

        for (const auto& profile : profileFSVValues) {
            for (const auto& level : levelFSVValues) {
                info.profile = static_cast<C2Config::profile_t>(profile.u32);
                info.level = static_cast<C2Config::level_t>(level.u32);
                {
                    SCOPED_TRACE("testWritableParam");
                    testWritableParam(&info);
                    if (HasFailure()) {
                        printf("Failed while config profile = 0x%x, level = 0x%x\n", info.profile,
                               info.level);
                    }
                    if (HasFatalFailure()) return;
                }
            }
        }
        // TODO: Add invalid value test after validate possible values in C2InterfaceHelper is
        //       implemented.
    }

protected:
    std::shared_ptr<C2ComponentInterface> mIntf;
    std::shared_ptr<C2ReflectorHelper> mReflector;
};

}  // namespace android

#endif  // ANDROID_C2_COMPONENT_INTERFACE_TEST_H
