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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <unistd.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <hidl-util/FQName.h>

#include <vintf/VintfObject.h>
#include <vintf/parse_string.h>
#include <vintf/parse_xml.h>
#include "test_constants.h"
#include "utils-fake.h"

using namespace ::testing;
using namespace std::literals;

using android::FqInstance;

static AssertionResult In(const std::string& sub, const std::string& str) {
    return (str.find(sub) != std::string::npos ? AssertionSuccess() : AssertionFailure())
           << "Value is " << str;
}
#define EXPECT_IN(sub, str) EXPECT_TRUE(In((sub), (str)))
#define EXPECT_NOT_IN(sub, str) EXPECT_FALSE(In((sub), (str)))

namespace android {
namespace vintf {

extern XmlConverter<KernelInfo>& gKernelInfoConverter;

namespace testing {

using namespace ::android::vintf::details;

// clang-format off

//
// Set of Xml1 metadata compatible with each other.
//

const std::string systemMatrixXml1 =
    "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\">\n"
    "    <hal format=\"hidl\" optional=\"false\">\n"
    "        <name>android.hardware.camera</name>\n"
    "        <version>2.0-5</version>\n"
    "        <version>3.4-16</version>\n"
    "    </hal>\n"
    "    <hal format=\"hidl\" optional=\"false\">\n"
    "        <name>android.hardware.nfc</name>\n"
    "        <version>1.0</version>\n"
    "        <version>2.0</version>\n"
    "    </hal>\n"
    "    <hal format=\"hidl\" optional=\"true\">\n"
    "        <name>android.hardware.foo</name>\n"
    "        <version>1.0</version>\n"
    "    </hal>\n"
    "    <kernel version=\"3.18.31\"></kernel>\n"
    "    <sepolicy>\n"
    "        <kernel-sepolicy-version>30</kernel-sepolicy-version>\n"
    "        <sepolicy-version>25.5</sepolicy-version>\n"
    "        <sepolicy-version>26.0-3</sepolicy-version>\n"
    "    </sepolicy>\n"
    "    <avb>\n"
    "        <vbmeta-version>0.0</vbmeta-version>\n"
    "    </avb>\n"
    "</compatibility-matrix>\n";

const std::string vendorManifestXml1 =
    "<manifest " + kMetaVersionStr + " type=\"device\">\n"
    "    <hal format=\"hidl\">\n"
    "        <name>android.hardware.camera</name>\n"
    "        <transport>hwbinder</transport>\n"
    "        <version>3.5</version>\n"
    "        <interface>\n"
    "            <name>IBetterCamera</name>\n"
    "            <instance>camera</instance>\n"
    "        </interface>\n"
    "        <interface>\n"
    "            <name>ICamera</name>\n"
    "            <instance>default</instance>\n"
    "            <instance>legacy/0</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "    <hal format=\"hidl\">\n"
    "        <name>android.hardware.nfc</name>\n"
    "        <transport>hwbinder</transport>\n"
    "        <version>1.0</version>\n"
    "        <interface>\n"
    "            <name>INfc</name>\n"
    "            <instance>nfc_nci</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "    <hal format=\"hidl\">\n"
    "        <name>android.hardware.nfc</name>\n"
    "        <transport>hwbinder</transport>\n"
    "        <version>2.0</version>\n"
    "        <interface>\n"
    "            <name>INfc</name>\n"
    "            <instance>default</instance>\n"
    "            <instance>nfc_nci</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "    <sepolicy>\n"
    "        <version>25.5</version>\n"
    "    </sepolicy>\n"
    "</manifest>\n";

const std::string systemManifestXml1 =
    "<manifest " + kMetaVersionStr + " type=\"framework\">\n"
    "    <hal format=\"hidl\">\n"
    "        <name>android.hidl.manager</name>\n"
    "        <transport>hwbinder</transport>\n"
    "        <version>1.0</version>\n"
    "        <interface>\n"
    "            <name>IServiceManager</name>\n"
    "            <instance>default</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "    <vndk>\n"
    "        <version>25.0.5</version>\n"
    "        <library>libbase.so</library>\n"
    "        <library>libjpeg.so</library>\n"
    "    </vndk>\n"
    "</manifest>\n";

const std::string vendorMatrixXml1 =
    "<compatibility-matrix " + kMetaVersionStr + " type=\"device\">\n"
    "    <hal format=\"hidl\" optional=\"false\">\n"
    "        <name>android.hidl.manager</name>\n"
    "        <version>1.0</version>\n"
    "    </hal>\n"
    "    <vndk>\n"
    "        <version>25.0.1-5</version>\n"
    "        <library>libbase.so</library>\n"
    "        <library>libjpeg.so</library>\n"
    "    </vndk>\n"
    "</compatibility-matrix>\n";

//
// Set of Xml2 metadata compatible with each other.
//

const std::string systemMatrixXml2 =
    "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\">\n"
    "    <hal format=\"hidl\">\n"
    "        <name>android.hardware.foo</name>\n"
    "        <version>1.0</version>\n"
    "    </hal>\n"
    "    <kernel version=\"3.18.31\"></kernel>\n"
    "    <sepolicy>\n"
    "        <kernel-sepolicy-version>30</kernel-sepolicy-version>\n"
    "        <sepolicy-version>25.5</sepolicy-version>\n"
    "        <sepolicy-version>26.0-3</sepolicy-version>\n"
    "    </sepolicy>\n"
    "    <avb>\n"
    "        <vbmeta-version>0.0</vbmeta-version>\n"
    "    </avb>\n"
    "</compatibility-matrix>\n";

const std::string vendorManifestXml2 =
    "<manifest " + kMetaVersionStr + " type=\"device\">"
    "    <hal>"
    "        <name>android.hardware.foo</name>"
    "        <transport>hwbinder</transport>"
    "        <version>1.0</version>"
    "    </hal>"
    "    <sepolicy>\n"
    "        <version>25.5</version>\n"
    "    </sepolicy>\n"
    "</manifest>";

//
// Set of framework matrices of different FCM version.
//

const std::string systemMatrixLevel1 =
    "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\" level=\"1\">\n"
    "    <hal format=\"hidl\" optional=\"true\">\n"
    "        <name>android.hardware.major</name>\n"
    "        <version>1.0</version>\n"
    "        <interface>\n"
    "            <name>IMajor</name>\n"
    "            <instance>default</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "    <hal format=\"hidl\" optional=\"true\">\n"
    "        <name>android.hardware.removed</name>\n"
    "        <version>1.0</version>\n"
    "        <interface>\n"
    "            <name>IRemoved</name>\n"
    "            <instance>default</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "    <hal format=\"hidl\" optional=\"true\">\n"
    "        <name>android.hardware.minor</name>\n"
    "        <version>1.0</version>\n"
    "        <interface>\n"
    "            <name>IMinor</name>\n"
    "            <instance>default</instance>\n"
    "            <instance>legacy</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "</compatibility-matrix>\n";

const std::string systemMatrixLevel2 =
    "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\" level=\"2\">\n"
    "    <hal format=\"hidl\" optional=\"true\">\n"
    "        <name>android.hardware.major</name>\n"
    "        <version>2.0</version>\n"
    "        <interface>\n"
    "            <name>IMajor</name>\n"
    "            <instance>default</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "    <hal format=\"hidl\" optional=\"true\">\n"
    "        <name>android.hardware.minor</name>\n"
    "        <version>1.1</version>\n"
    "        <interface>\n"
    "            <name>IMinor</name>\n"
    "            <instance>default</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "</compatibility-matrix>\n";

//
// Set of framework matrices of different FCM version with regex.
//

const static std::vector<std::string> systemMatrixRegexXmls = {
    // 1.xml
    "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\" level=\"1\">\n"
    "    <hal format=\"hidl\" optional=\"false\">\n"
    "        <name>android.hardware.regex</name>\n"
    "        <version>1.0-1</version>\n"
    "        <interface>\n"
    "            <name>IRegex</name>\n"
    "            <instance>default</instance>\n"
    "            <instance>special/1.0</instance>\n"
    "            <regex-instance>regex/1.0/[0-9]+</regex-instance>\n"
    "            <regex-instance>regex_common/[0-9]+</regex-instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "</compatibility-matrix>\n",
    // 2.xml
    "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\" level=\"2\">\n"
    "    <hal format=\"hidl\" optional=\"false\">\n"
    "        <name>android.hardware.regex</name>\n"
    "        <version>1.1-2</version>\n"
    "        <interface>\n"
    "            <name>IRegex</name>\n"
    "            <instance>default</instance>\n"
    "            <instance>special/1.1</instance>\n"
    "            <regex-instance>regex/1.1/[0-9]+</regex-instance>\n"
    "            <regex-instance>[a-z]+_[a-z]+/[0-9]+</regex-instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "</compatibility-matrix>\n",
    // 3.xml
    "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\" level=\"3\">\n"
    "    <hal format=\"hidl\" optional=\"false\">\n"
    "        <name>android.hardware.regex</name>\n"
    "        <version>2.0</version>\n"
    "        <interface>\n"
    "            <name>IRegex</name>\n"
    "            <instance>default</instance>\n"
    "            <instance>special/2.0</instance>\n"
    "            <regex-instance>regex/2.0/[0-9]+</regex-instance>\n"
    "            <regex-instance>regex_[a-z]+/[0-9]+</regex-instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "</compatibility-matrix>\n"};

//
// Set of metadata at different FCM version that has requirements
//

const std::vector<std::string> systemMatrixRequire = {
    // 1.xml
    "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\" level=\"1\">\n"
    "    <hal format=\"hidl\" optional=\"false\">\n"
    "        <name>android.hardware.foo</name>\n"
    "        <version>1.0</version>\n"
    "        <interface>\n"
    "            <name>IFoo</name>\n"
    "            <instance>default</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "</compatibility-matrix>\n",
    // 2.xml
    "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\" level=\"2\">\n"
    "    <hal format=\"hidl\" optional=\"false\">\n"
    "        <name>android.hardware.bar</name>\n"
    "        <version>1.0</version>\n"
    "        <interface>\n"
    "            <name>IBar</name>\n"
    "            <instance>default</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "</compatibility-matrix>\n"};

const std::string vendorManifestRequire1 =
    "<manifest " + kMetaVersionStr + " type=\"device\" target-level=\"1\">\n"
    "    <hal format=\"hidl\">\n"
    "        <name>android.hardware.foo</name>\n"
    "        <transport>hwbinder</transport>\n"
    "        <fqname>@1.0::IFoo/default</fqname>\n"
    "    </hal>\n"
    "</manifest>\n";

const std::string vendorManifestRequire2 =
    "<manifest " + kMetaVersionStr + " type=\"device\" target-level=\"2\">\n"
    "    <hal format=\"hidl\">\n"
    "        <name>android.hardware.bar</name>\n"
    "        <transport>hwbinder</transport>\n"
    "        <fqname>@1.0::IBar/default</fqname>\n"
    "    </hal>\n"
    "</manifest>\n";

//
// Set of metadata for kernel requirements
//

const std::string vendorManifestKernel318 =
    "<manifest " + kMetaVersionStr + " type=\"device\">\n"
    "    <kernel version=\"3.18.999\" />\n"
    "    <sepolicy>\n"
    "        <version>25.5</version>\n"
    "    </sepolicy>\n"
    "</manifest>\n";

const std::string systemMatrixKernel318 =
    "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\">\n"
    "    <kernel version=\"3.18.999\"></kernel>\n"
    "    <sepolicy>\n"
    "        <kernel-sepolicy-version>30</kernel-sepolicy-version>\n"
    "        <sepolicy-version>25.5</sepolicy-version>\n"
    "    </sepolicy>\n"
    "</compatibility-matrix>\n";

class VintfObjectTestBase : public ::testing::Test {
   protected:
    MockFileSystem& fetcher() {
        return static_cast<MockFileSystem&>(*vintfObject->getFileSystem());
    }
    MockPropertyFetcher& propertyFetcher() {
        return static_cast<MockPropertyFetcher&>(*vintfObject->getPropertyFetcher());
    }

    void useEmptyFileSystem() {
        // By default, no files exist in the file system.
        // Use EXPECT_CALL because more specific expectation of fetch and listFiles will come along.
        EXPECT_CALL(fetcher(), listFiles(_, _, _)).Times(AnyNumber())
            .WillRepeatedly(Return(::android::NAME_NOT_FOUND));
        EXPECT_CALL(fetcher(), fetch(_, _)).Times(AnyNumber())
            .WillRepeatedly(Return(::android::NAME_NOT_FOUND));
    }

    // Setup the MockFileSystem used by the fetchAllInformation template
    // so it returns the given metadata info instead of fetching from device.
    void setupMockFetcher(const std::string& vendorManifestXml, const std::string& systemMatrixXml,
                          const std::string& systemManifestXml, const std::string& vendorMatrixXml) {

        useEmptyFileSystem();

        ON_CALL(fetcher(), fetch(StrEq(kVendorLegacyManifest), _))
            .WillByDefault(
                Invoke([vendorManifestXml](const std::string& path, std::string& fetched) {
                    (void)path;
                    fetched = vendorManifestXml;
                    return 0;
                }));
        ON_CALL(fetcher(), fetch(StrEq(kSystemManifest), _))
            .WillByDefault(
                Invoke([systemManifestXml](const std::string& path, std::string& fetched) {
                    (void)path;
                    fetched = systemManifestXml;
                    return 0;
                }));
        ON_CALL(fetcher(), fetch(StrEq(kVendorLegacyMatrix), _))
            .WillByDefault(Invoke([vendorMatrixXml](const std::string& path, std::string& fetched) {
                (void)path;
                fetched = vendorMatrixXml;
                return 0;
            }));
        ON_CALL(fetcher(), fetch(StrEq(kSystemLegacyMatrix), _))
            .WillByDefault(Invoke([systemMatrixXml](const std::string& path, std::string& fetched) {
                (void)path;
                fetched = systemMatrixXml;
                return 0;
            }));
    }

    virtual void SetUp() {
        vintfObject = VintfObject::Builder()
                          .setFileSystem(std::make_unique<NiceMock<MockFileSystem>>())
                          .setRuntimeInfoFactory(std::make_unique<NiceMock<MockRuntimeInfoFactory>>(
                              std::make_shared<NiceMock<MockRuntimeInfo>>()))
                          .setPropertyFetcher(std::make_unique<NiceMock<MockPropertyFetcher>>())
                          .build();
    }
    virtual void TearDown() {
        Mock::VerifyAndClear(&fetcher());
    }

    void expectVendorManifest(size_t times = 1) {
        EXPECT_CALL(fetcher(), fetch(StrEq(kVendorLegacyManifest), _)).Times(times);
    }

    void expectSystemManifest(size_t times = 1) {
        EXPECT_CALL(fetcher(), fetch(StrEq(kSystemManifest), _)).Times(times);
    }

    void expectVendorMatrix(size_t times = 1) {
        EXPECT_CALL(fetcher(), fetch(StrEq(kVendorLegacyMatrix), _)).Times(times);
    }

    void expectSystemMatrix(size_t times = 1) {
        EXPECT_CALL(fetcher(), fetch(StrEq(kSystemLegacyMatrix), _)).Times(times);
    }

    // Expect that a file exist and should be fetched once.
    void expectFetch(const std::string& path, const std::string& content) {
        EXPECT_CALL(fetcher(), fetch(StrEq(path), _))
            .WillOnce(Invoke([content](const auto&, auto& out) {
                out = content;
                return ::android::OK;
            }));
    }

    // Expect that a file exist and can be fetched 0 or more times.
    void expectFetchRepeatedly(const std::string& path, const std::string& content) {
        EXPECT_CALL(fetcher(), fetch(StrEq(path), _))
            .Times(AnyNumber())
            .WillRepeatedly(Invoke([content](const auto&, auto& out) {
                out = content;
                return ::android::OK;
            }));
    }

    // Expect that the file should never be fetched (whether it exists or not).
    void expectNeverFetch(const std::string& path) {
        EXPECT_CALL(fetcher(), fetch(StrEq(path), _)).Times(0);
    }

    // Expect that the file does not exist, and can be fetched 0 or more times.
    template <typename Matcher>
    void expectFileNotExist(const Matcher& matcher) {
        EXPECT_CALL(fetcher(), fetch(matcher, _))
            .Times(AnyNumber())
            .WillRepeatedly(Return(::android::NAME_NOT_FOUND));
    }

    MockRuntimeInfoFactory& runtimeInfoFactory() {
        return static_cast<MockRuntimeInfoFactory&>(*vintfObject->getRuntimeInfoFactory());
    }

    std::unique_ptr<VintfObject> vintfObject;
};

// Test fixture that provides compatible metadata from the mock device.
class VintfObjectCompatibleTest : public VintfObjectTestBase {
   protected:
    virtual void SetUp() {
        VintfObjectTestBase::SetUp();
        setupMockFetcher(vendorManifestXml1, systemMatrixXml1, systemManifestXml1, vendorMatrixXml1);
    }
};

// Tests that local info is checked.
TEST_F(VintfObjectCompatibleTest, TestDeviceCompatibility) {
    std::string error;

    expectVendorManifest();
    expectSystemManifest();
    expectVendorMatrix();
    expectSystemMatrix();

    int result = vintfObject->checkCompatibility(&error);

    ASSERT_EQ(result, 0) << "Fail message:" << error.c_str();
    // Check that nothing was ignored.
    ASSERT_STREQ(error.c_str(), "");
}

// Test fixture that provides incompatible metadata from the mock device.
class VintfObjectIncompatibleTest : public VintfObjectTestBase {
   protected:
    virtual void SetUp() {
        VintfObjectTestBase::SetUp();
        setupMockFetcher(vendorManifestXml1, systemMatrixXml2, systemManifestXml1, vendorMatrixXml1);
    }
};

// Fetch all metadata from device and ensure that it fails.
TEST_F(VintfObjectIncompatibleTest, TestDeviceCompatibility) {
    std::string error;

    expectVendorManifest();
    expectSystemManifest();
    expectVendorMatrix();
    expectSystemMatrix();

    int result = vintfObject->checkCompatibility(&error);

    ASSERT_EQ(result, 1) << "Should have failed:" << error.c_str();
}

const std::string vendorManifestKernelFcm =
        "<manifest " + kMetaVersionStr + " type=\"device\">\n"
        "    <kernel version=\"3.18.999\" target-level=\"92\"/>\n"
        "</manifest>\n";

// Test fixture that provides compatible metadata from the mock device.
class VintfObjectRuntimeInfoTest : public VintfObjectTestBase {
   protected:
    virtual void SetUp() {
        VintfObjectTestBase::SetUp();
        setupMockFetcher(vendorManifestKernelFcm, "", "", "");
        expectVendorManifest();
    }
    virtual void TearDown() {
        Mock::VerifyAndClear(&runtimeInfoFactory());
        Mock::VerifyAndClear(runtimeInfoFactory().getInfo().get());
    }
};

TEST_F(VintfObjectRuntimeInfoTest, GetRuntimeInfo) {
    // RuntimeInfo.fetchAllInformation is never called with KERNEL_FCM set.
    auto allExceptKernelFcm = RuntimeInfo::FetchFlag::ALL & ~RuntimeInfo::FetchFlag::KERNEL_FCM;

    InSequence s;

    EXPECT_CALL(*runtimeInfoFactory().getInfo(),
                fetchAllInformation(RuntimeInfo::FetchFlag::CPU_VERSION));
    EXPECT_CALL(*runtimeInfoFactory().getInfo(), fetchAllInformation(RuntimeInfo::FetchFlag::NONE));
    EXPECT_CALL(*runtimeInfoFactory().getInfo(),
                fetchAllInformation(RuntimeInfo::FetchFlag::CPU_VERSION));
    EXPECT_CALL(
        *runtimeInfoFactory().getInfo(),
        fetchAllInformation(allExceptKernelFcm & ~RuntimeInfo::FetchFlag::CPU_VERSION));
    EXPECT_CALL(*runtimeInfoFactory().getInfo(), fetchAllInformation(allExceptKernelFcm));
    EXPECT_CALL(*runtimeInfoFactory().getInfo(), fetchAllInformation(RuntimeInfo::FetchFlag::NONE));

    EXPECT_NE(nullptr, vintfObject->getRuntimeInfo(false /* skipCache */,
                                                   RuntimeInfo::FetchFlag::CPU_VERSION));
    EXPECT_NE(nullptr, vintfObject->getRuntimeInfo(false /* skipCache */,
                                                   RuntimeInfo::FetchFlag::CPU_VERSION));
    EXPECT_NE(nullptr, vintfObject->getRuntimeInfo(true /* skipCache */,
                                                   RuntimeInfo::FetchFlag::CPU_VERSION));
    EXPECT_NE(nullptr, vintfObject->getRuntimeInfo(false /* skipCache */,
                                                   RuntimeInfo::FetchFlag::ALL));
    EXPECT_NE(nullptr, vintfObject->getRuntimeInfo(true /* skipCache */,
                                                   RuntimeInfo::FetchFlag::ALL));
    EXPECT_NE(nullptr, vintfObject->getRuntimeInfo(false /* skipCache */,
                                                   RuntimeInfo::FetchFlag::ALL));
}

TEST_F(VintfObjectRuntimeInfoTest, GetRuntimeInfoKernelFcm) {
    ASSERT_EQ(Level{92}, vintfObject->getKernelLevel());
}

// Test fixture that provides incompatible metadata from the mock device.
class VintfObjectTest : public VintfObjectTestBase {
   protected:
    virtual void SetUp() {
        VintfObjectTestBase::SetUp();
        useEmptyFileSystem();
    }
};

// Test framework compatibility matrix is combined at runtime
TEST_F(VintfObjectTest, FrameworkCompatibilityMatrixCombine) {
    EXPECT_CALL(fetcher(), listFiles(StrEq(kSystemVintfDir), _, _))
        .WillOnce(Invoke([](const auto&, auto* out, auto*) {
            *out = {
                "compatibility_matrix.1.xml",
                "compatibility_matrix.empty.xml",
            };
            return ::android::OK;
        }));
    expectFetch(kSystemVintfDir + "compatibility_matrix.1.xml",
                "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\" level=\"1\"/>");
    expectFetch(kSystemVintfDir + "compatibility_matrix.empty.xml",
                "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\"/>");
    expectFileNotExist(StrEq(kProductMatrix));
    expectFetch(kVendorManifest, "<manifest " + kMetaVersionStr + " type=\"device\" />\n");
    expectNeverFetch(kSystemLegacyMatrix);

    EXPECT_NE(nullptr, vintfObject->getFrameworkCompatibilityMatrix(true /* skipCache */));
}

// Test product compatibility matrix is fetched
TEST_F(VintfObjectTest, ProductCompatibilityMatrix) {
    EXPECT_CALL(fetcher(), listFiles(StrEq(kSystemVintfDir), _, _))
        .WillOnce(Invoke([](const auto&, auto* out, auto*) {
            *out = {
                "compatibility_matrix.1.xml",
                "compatibility_matrix.empty.xml",
            };
            return ::android::OK;
        }));
    EXPECT_CALL(fetcher(), listFiles(StrEq(kProductVintfDir), _, _))
        .WillRepeatedly(Invoke([](const auto&, auto* out, auto*) {
            *out = {android::base::Basename(kProductMatrix)};
            return ::android::OK;
        }));
    expectFetch(kSystemVintfDir + "compatibility_matrix.1.xml",
                "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\" level=\"1\"/>");
    expectFetch(kSystemVintfDir + "compatibility_matrix.empty.xml",
                "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\"/>");
    expectFetch(kProductMatrix,
                "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\">\n"
                "    <hal format=\"hidl\" optional=\"true\">\n"
                "        <name>android.hardware.foo</name>\n"
                "        <version>1.0</version>\n"
                "        <interface>\n"
                "            <name>IFoo</name>\n"
                "            <instance>default</instance>\n"
                "        </interface>\n"
                "    </hal>\n"
                "</compatibility-matrix>\n");
    expectFetch(kVendorManifest, "<manifest " + kMetaVersionStr + " type=\"device\" />\n");
    expectNeverFetch(kSystemLegacyMatrix);

    auto fcm = vintfObject->getFrameworkCompatibilityMatrix(true /* skipCache */);
    ASSERT_NE(nullptr, fcm);

    FqInstance expectInstance;
    EXPECT_TRUE(expectInstance.setTo("android.hardware.foo@1.0::IFoo/default"));
    bool found = false;
    fcm->forEachHidlInstance([&found, &expectInstance](const auto& matrixInstance) {
        found |= matrixInstance.isSatisfiedBy(expectInstance);
        return !found;  // continue if not found
    });
    EXPECT_TRUE(found) << "android.hardware.foo@1.0::IFoo/default should be found in matrix:\n"
                       << gCompatibilityMatrixConverter(*fcm);
}

const std::string vendorEtcManifest =
    "<manifest " + kMetaVersionStr + " type=\"device\">\n"
    "    <hal format=\"hidl\">\n"
    "        <name>android.hardware.foo</name>\n"
    "        <transport>hwbinder</transport>\n"
    "        <version>1.0</version>\n"
    "        <version>2.0</version>\n"
    "        <interface>\n"
    "            <name>IVendorEtc</name>\n"
    "            <instance>default</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "</manifest>\n";

const std::string vendorManifest =
    "<manifest " + kMetaVersionStr + " type=\"device\">\n"
    "    <hal format=\"hidl\">\n"
    "        <name>android.hardware.foo</name>\n"
    "        <transport>hwbinder</transport>\n"
    "        <version>1.0</version>\n"
    "        <interface>\n"
    "            <name>IVendor</name>\n"
    "            <instance>default</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "</manifest>\n";

const std::string odmProductManifest =
    "<manifest " + kMetaVersionStr + " type=\"device\">\n"
    "    <hal format=\"hidl\" override=\"true\">\n"
    "        <name>android.hardware.foo</name>\n"
    "        <transport>hwbinder</transport>\n"
    "        <version>1.1</version>\n"
    "        <interface>\n"
    "            <name>IOdmProduct</name>\n"
    "            <instance>default</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "</manifest>\n";

const std::string odmManifest =
    "<manifest " + kMetaVersionStr + " type=\"device\">\n"
    "    <hal format=\"hidl\" override=\"true\">\n"
    "        <name>android.hardware.foo</name>\n"
    "        <transport>hwbinder</transport>\n"
    "        <version>1.1</version>\n"
    "        <interface>\n"
    "            <name>IOdm</name>\n"
    "            <instance>default</instance>\n"
    "        </interface>\n"
    "    </hal>\n"
    "</manifest>\n";

bool containsVendorManifest(const std::shared_ptr<const HalManifest>& p) {
    return !p->getHidlInstances("android.hardware.foo", {1, 0}, "IVendor").empty();
}

bool containsVendorEtcManifest(const std::shared_ptr<const HalManifest>& p) {
    return !p->getHidlInstances("android.hardware.foo", {2, 0}, "IVendorEtc").empty();
}

bool vendorEtcManifestOverridden(const std::shared_ptr<const HalManifest>& p) {
    return p->getHidlInstances("android.hardware.foo", {1, 0}, "IVendorEtc").empty();
}

bool containsOdmManifest(const std::shared_ptr<const HalManifest>& p) {
    return !p->getHidlInstances("android.hardware.foo", {1, 1}, "IOdm").empty();
}

bool containsOdmProductManifest(const std::shared_ptr<const HalManifest>& p) {
    return !p->getHidlInstances("android.hardware.foo", {1, 1}, "IOdmProduct").empty();
}

class DeviceManifestTest : public VintfObjectTestBase {
   protected:
    // Expect that /vendor/etc/vintf/manifest.xml is fetched.
    void expectVendorManifest() { expectFetch(kVendorManifest, vendorEtcManifest); }
    // /vendor/etc/vintf/manifest.xml does not exist.
    void noVendorManifest() { expectFileNotExist(StrEq(kVendorManifest)); }
    // Expect some ODM manifest is fetched.
    void expectOdmManifest() {
        expectFetch(kOdmManifest, odmManifest);
    }
    void noOdmManifest() { expectFileNotExist(StartsWith("/odm/")); }
    std::shared_ptr<const HalManifest> get() {
        return vintfObject->getDeviceHalManifest(true /* skipCache */);
    }
};

// Test /vendor/etc/vintf/manifest.xml + ODM manifest
TEST_F(DeviceManifestTest, Combine1) {
    expectVendorManifest();
    expectOdmManifest();
    auto p = get();
    ASSERT_NE(nullptr, p);
    EXPECT_TRUE(containsVendorEtcManifest(p));
    EXPECT_TRUE(vendorEtcManifestOverridden(p));
    EXPECT_TRUE(containsOdmManifest(p));
    EXPECT_FALSE(containsVendorManifest(p));
}

// Test /vendor/etc/vintf/manifest.xml
TEST_F(DeviceManifestTest, Combine2) {
    expectVendorManifest();
    noOdmManifest();
    auto p = get();
    ASSERT_NE(nullptr, p);
    EXPECT_TRUE(containsVendorEtcManifest(p));
    EXPECT_FALSE(vendorEtcManifestOverridden(p));
    EXPECT_FALSE(containsOdmManifest(p));
    EXPECT_FALSE(containsVendorManifest(p));
}

// Test ODM manifest
TEST_F(DeviceManifestTest, Combine3) {
    noVendorManifest();
    expectOdmManifest();
    auto p = get();
    ASSERT_NE(nullptr, p);
    EXPECT_FALSE(containsVendorEtcManifest(p));
    EXPECT_TRUE(vendorEtcManifestOverridden(p));
    EXPECT_TRUE(containsOdmManifest(p));
    EXPECT_FALSE(containsVendorManifest(p));
}

// Test /vendor/manifest.xml
TEST_F(DeviceManifestTest, Combine4) {
    noVendorManifest();
    noOdmManifest();
    expectFetch(kVendorLegacyManifest, vendorManifest);
    auto p = get();
    ASSERT_NE(nullptr, p);
    EXPECT_FALSE(containsVendorEtcManifest(p));
    EXPECT_TRUE(vendorEtcManifestOverridden(p));
    EXPECT_FALSE(containsOdmManifest(p));
    EXPECT_TRUE(containsVendorManifest(p));
}

class OdmManifestTest : public VintfObjectTestBase,
                         public ::testing::WithParamInterface<const char*> {
   protected:
    virtual void SetUp() override {
        VintfObjectTestBase::SetUp();
        // Assume /vendor/etc/vintf/manifest.xml does not exist to simplify
        // testing logic.
        expectFileNotExist(StrEq(kVendorManifest));
        // Expect that the legacy /vendor/manifest.xml is never fetched.
        expectNeverFetch(kVendorLegacyManifest);
        // Assume no files exist under /odm/ unless otherwise specified.
        expectFileNotExist(StartsWith("/odm/"));

        // set SKU
        productModel = GetParam();
        ON_CALL(propertyFetcher(), getProperty("ro.boot.product.hardware.sku", _))
            .WillByDefault(Return(productModel));
    }
    std::shared_ptr<const HalManifest> get() {
        return vintfObject->getDeviceHalManifest(true /* skipCache */);
    }
    std::string productModel;
};

TEST_P(OdmManifestTest, OdmProductManifest) {
    if (productModel.empty()) return;
    expectFetch(kOdmVintfDir + "manifest_" + productModel + ".xml", odmProductManifest);
    // /odm/etc/vintf/manifest.xml should not be fetched when the product variant exists.
    expectNeverFetch(kOdmManifest);
    auto p = get();
    ASSERT_NE(nullptr, p);
    EXPECT_TRUE(containsOdmProductManifest(p));
}

TEST_P(OdmManifestTest, OdmManifest) {
    expectFetch(kOdmManifest, odmManifest);
    auto p = get();
    ASSERT_NE(nullptr, p);
    EXPECT_TRUE(containsOdmManifest(p));
}

TEST_P(OdmManifestTest, OdmLegacyProductManifest) {
    if (productModel.empty()) return;
    expectFetch(kOdmLegacyVintfDir + "manifest_" + productModel + ".xml", odmProductManifest);
    // /odm/manifest.xml should not be fetched when the product variant exists.
    expectNeverFetch(kOdmLegacyManifest);
    auto p = get();
    ASSERT_NE(nullptr, p);
    EXPECT_TRUE(containsOdmProductManifest(p));
}

TEST_P(OdmManifestTest, OdmLegacyManifest) {
    expectFetch(kOdmLegacyManifest, odmManifest);
    auto p = get();
    ASSERT_NE(nullptr, p);
    EXPECT_TRUE(containsOdmManifest(p));
}

INSTANTIATE_TEST_SUITE_P(OdmManifest, OdmManifestTest, ::testing::Values("", "fake_sku"));

struct CheckedFqInstance : FqInstance {
    CheckedFqInstance(const char* s) : CheckedFqInstance(std::string(s)) {}
    CheckedFqInstance(const std::string& s) { CHECK(setTo(s)) << s; }

    Version getVersion() const { return FqInstance::getVersion(); }
};

static VintfObject::ListInstances getInstanceListFunc(
    const std::vector<CheckedFqInstance>& instances) {
    return [instances](const std::string& package, Version version, const std::string& interface,
                       const auto& /* instanceHint */) {
        std::vector<std::pair<std::string, Version>> ret;
        for (auto&& existing : instances) {
            if (existing.getPackage() == package && existing.getVersion().minorAtLeast(version) &&
                existing.getInterface() == interface) {
                ret.push_back(std::make_pair(existing.getInstance(), existing.getVersion()));
            }
        }

        return ret;
    };
}

class DeprecateTest : public VintfObjectTestBase {
   protected:
    virtual void SetUp() override {
        VintfObjectTestBase::SetUp();
        useEmptyFileSystem();
        EXPECT_CALL(fetcher(), listFiles(StrEq(kSystemVintfDir), _, _))
            .WillRepeatedly(Invoke([](const auto&, auto* out, auto*) {
                *out = {
                    "compatibility_matrix.1.xml",
                    "compatibility_matrix.2.xml",
                };
                return ::android::OK;
            }));
        expectFetchRepeatedly(kSystemVintfDir + "compatibility_matrix.1.xml", systemMatrixLevel1);
        expectFetchRepeatedly(kSystemVintfDir + "compatibility_matrix.2.xml", systemMatrixLevel2);
        expectFileNotExist(StrEq(kProductMatrix));
        expectNeverFetch(kSystemLegacyMatrix);

        expectFetchRepeatedly(kVendorManifest,
                    "<manifest " + kMetaVersionStr + " type=\"device\" target-level=\"2\"/>");
        expectFileNotExist(StartsWith("/odm/"));

        // Update the device manifest cache because CheckDeprecate does not fetch
        // device manifest again if cache exist.
        vintfObject->getDeviceHalManifest(true /* skipCache */);
    }

};

TEST_F(DeprecateTest, CheckNoDeprecate) {
    auto pred = getInstanceListFunc({
        "android.hardware.minor@1.1::IMinor/default",
        "android.hardware.major@2.0::IMajor/default",
    });
    std::string error;
    EXPECT_EQ(NO_DEPRECATED_HALS, vintfObject->checkDeprecation(pred, {}, &error)) << error;
}

TEST_F(DeprecateTest, CheckRemoved) {
    auto pred = getInstanceListFunc({
        "android.hardware.removed@1.0::IRemoved/default",
        "android.hardware.minor@1.1::IMinor/default",
        "android.hardware.major@2.0::IMajor/default",
    });
    std::string error;
    EXPECT_EQ(DEPRECATED, vintfObject->checkDeprecation(pred, {}, &error))
        << "removed@1.0 should be deprecated. " << error;
}

TEST_F(DeprecateTest, CheckMinor) {
    auto pred = getInstanceListFunc({
        "android.hardware.minor@1.0::IMinor/default",
        "android.hardware.major@2.0::IMajor/default",
    });
    std::string error;
    EXPECT_EQ(DEPRECATED, vintfObject->checkDeprecation(pred, {}, &error))
        << "minor@1.0 should be deprecated. " << error;
}

TEST_F(DeprecateTest, CheckMinorDeprecatedInstance1) {
    auto pred = getInstanceListFunc({
        "android.hardware.minor@1.0::IMinor/legacy",
        "android.hardware.minor@1.1::IMinor/default",
        "android.hardware.major@2.0::IMajor/default",
    });
    std::string error;
    EXPECT_EQ(DEPRECATED, vintfObject->checkDeprecation(pred, {}, &error))
        << "minor@1.0::IMinor/legacy should be deprecated. " << error;
}

TEST_F(DeprecateTest, CheckMinorDeprecatedInstance2) {
    auto pred = getInstanceListFunc({
        "android.hardware.minor@1.1::IMinor/default",
        "android.hardware.minor@1.1::IMinor/legacy",
        "android.hardware.major@2.0::IMajor/default",
    });
    std::string error;
    EXPECT_EQ(DEPRECATED, vintfObject->checkDeprecation(pred, {}, &error))
        << "minor@1.1::IMinor/legacy should be deprecated. " << error;
}

TEST_F(DeprecateTest, CheckMajor1) {
    auto pred = getInstanceListFunc({
        "android.hardware.minor@1.1::IMinor/default",
        "android.hardware.major@1.0::IMajor/default",
        "android.hardware.major@2.0::IMajor/default",
    });
    std::string error;
    EXPECT_EQ(DEPRECATED, vintfObject->checkDeprecation(pred, {}, &error))
        << "major@1.0 should be deprecated. " << error;
}

TEST_F(DeprecateTest, CheckMajor2) {
    auto pred = getInstanceListFunc({
        "android.hardware.minor@1.1::IMinor/default",
        "android.hardware.major@1.0::IMajor/default",
    });
    std::string error;
    EXPECT_EQ(DEPRECATED, vintfObject->checkDeprecation(pred, {}, &error))
        << "major@1.0 should be deprecated. " << error;
}

TEST_F(DeprecateTest, HidlMetadataNotDeprecate) {
    auto pred = getInstanceListFunc({
        "android.hardware.major@1.0::IMajor/default",
        "android.hardware.major@2.0::IMajor/default",
    });
    std::string error;
    EXPECT_EQ(DEPRECATED, vintfObject->checkDeprecation(pred, {}, &error))
        << "major@1.0 should be deprecated. " << error;
    std::vector<HidlInterfaceMetadata> hidlMetadata{
      {"android.hardware.major@2.0::IMajor", {"android.hardware.major@1.0::IMajor"}},
    };
    EXPECT_EQ(NO_DEPRECATED_HALS, vintfObject->checkDeprecation(pred, hidlMetadata, &error))
        << "major@1.0 should not be deprecated because it extends from 2.0: " << error;
}

TEST_F(DeprecateTest, HidlMetadataDeprecate) {
    auto pred = getInstanceListFunc({
        "android.hardware.major@1.0::IMajor/default",
    });
    std::string error;
    EXPECT_EQ(DEPRECATED, vintfObject->checkDeprecation(pred, {}, &error))
        << "major@1.0 should be deprecated. " << error;
    std::vector<HidlInterfaceMetadata> hidlMetadata{
      {"android.hardware.major@2.0::IMajor", {"android.hardware.major@1.0::IMajor"}},
    };
    EXPECT_EQ(DEPRECATED, vintfObject->checkDeprecation(pred, hidlMetadata, &error))
        << "major@1.0 should be deprecated. " << error;
}

class MultiMatrixTest : public VintfObjectTestBase {
   protected:
    void SetUp() override {
        VintfObjectTestBase::SetUp();
        useEmptyFileSystem();
    }
    static std::string getFileName(size_t i) {
        return "compatibility_matrix." + std::to_string(static_cast<Level>(i)) + ".xml";
    }
    void SetUpMockSystemMatrices(const std::vector<std::string>& xmls) {
        SetUpMockMatrices(kSystemVintfDir, xmls);
    }
    void SetUpMockMatrices(const std::string& dir, const std::vector<std::string>& xmls) {
        EXPECT_CALL(fetcher(), listFiles(StrEq(dir), _, _))
            .WillRepeatedly(Invoke([=](const auto&, auto* out, auto*) {
                size_t i = 1;
                for (const auto& content : xmls) {
                    (void)content;
                    out->push_back(getFileName(i));
                    ++i;
                }
                return ::android::OK;
            }));
        size_t i = 1;
        for (const auto& content : xmls) {
            expectFetchRepeatedly(dir + getFileName(i), content);
            ++i;
        }
    }
    void expectTargetFcmVersion(size_t level) {
        expectFetch(kVendorManifest, "<manifest " + kMetaVersionStr + " type=\"device\" target-level=\"" +
                                         to_string(static_cast<Level>(level)) + "\"/>");
        vintfObject->getDeviceHalManifest(true /* skipCache */);
    }
};

class RegexTest : public MultiMatrixTest {
   protected:
    virtual void SetUp() {
        MultiMatrixTest::SetUp();
        SetUpMockSystemMatrices(systemMatrixRegexXmls);
    }
};

TEST_F(RegexTest, CombineLevel1) {
    expectTargetFcmVersion(1);
    auto matrix = vintfObject->getFrameworkCompatibilityMatrix(true /* skipCache */);
    ASSERT_NE(nullptr, matrix);
    std::string xml = gCompatibilityMatrixConverter(*matrix);

    EXPECT_IN(
        "    <hal format=\"hidl\" optional=\"false\">\n"
        "        <name>android.hardware.regex</name>\n"
        "        <version>1.0-2</version>\n"
        "        <version>2.0</version>\n"
        "        <interface>\n"
        "            <name>IRegex</name>\n"
        "            <instance>default</instance>\n"
        "        </interface>\n"
        "    </hal>\n",
        xml);
    EXPECT_IN(
        "    <hal format=\"hidl\" optional=\"false\">\n"
        "        <name>android.hardware.regex</name>\n"
        "        <version>1.0-1</version>\n"
        "        <interface>\n"
        "            <name>IRegex</name>\n"
        "            <instance>special/1.0</instance>\n"
        "            <regex-instance>regex/1.0/[0-9]+</regex-instance>\n"
        "            <regex-instance>regex_common/[0-9]+</regex-instance>\n"
        "        </interface>\n"
        "    </hal>\n",
        xml);
    EXPECT_IN(
        "    <hal format=\"hidl\" optional=\"true\">\n"
        "        <name>android.hardware.regex</name>\n"
        "        <version>1.1-2</version>\n"
        "        <interface>\n"
        "            <name>IRegex</name>\n"
        "            <instance>special/1.1</instance>\n"
        "            <regex-instance>[a-z]+_[a-z]+/[0-9]+</regex-instance>\n"
        "            <regex-instance>regex/1.1/[0-9]+</regex-instance>\n"
        "        </interface>\n"
        "    </hal>\n",
        xml);
    EXPECT_IN(
        "    <hal format=\"hidl\" optional=\"true\">\n"
        "        <name>android.hardware.regex</name>\n"
        "        <version>2.0</version>\n"
        "        <interface>\n"
        "            <name>IRegex</name>\n"
        "            <instance>special/2.0</instance>\n"
        "            <regex-instance>regex/2.0/[0-9]+</regex-instance>\n"
        "            <regex-instance>regex_[a-z]+/[0-9]+</regex-instance>\n"
        "        </interface>\n"
        "    </hal>\n",
        xml);
}

TEST_F(RegexTest, CombineLevel2) {
    expectTargetFcmVersion(2);
    auto matrix = vintfObject->getFrameworkCompatibilityMatrix(true /* skipCache */);
    ASSERT_NE(nullptr, matrix);
    std::string xml = gCompatibilityMatrixConverter(*matrix);

    EXPECT_IN(
        "    <hal format=\"hidl\" optional=\"false\">\n"
        "        <name>android.hardware.regex</name>\n"
        "        <version>1.1-2</version>\n"
        "        <version>2.0</version>\n"
        "        <interface>\n"
        "            <name>IRegex</name>\n"
        "            <instance>default</instance>\n"
        "        </interface>\n"
        "    </hal>\n",
        xml);
    EXPECT_IN(
        "    <hal format=\"hidl\" optional=\"false\">\n"
        "        <name>android.hardware.regex</name>\n"
        "        <version>1.1-2</version>\n"
        "        <interface>\n"
        "            <name>IRegex</name>\n"
        "            <instance>special/1.1</instance>\n"
        "            <regex-instance>[a-z]+_[a-z]+/[0-9]+</regex-instance>\n"
        "            <regex-instance>regex/1.1/[0-9]+</regex-instance>\n"
        "        </interface>\n"
        "    </hal>\n",
        xml);
    EXPECT_IN(
        "    <hal format=\"hidl\" optional=\"true\">\n"
        "        <name>android.hardware.regex</name>\n"
        "        <version>2.0</version>\n"
        "        <interface>\n"
        "            <name>IRegex</name>\n"
        "            <instance>special/2.0</instance>\n"
        "            <regex-instance>regex/2.0/[0-9]+</regex-instance>\n"
        "            <regex-instance>regex_[a-z]+/[0-9]+</regex-instance>\n"
        "        </interface>\n"
        "    </hal>\n",
        xml);
}

TEST_F(RegexTest, DeprecateLevel2) {
    std::string error;
    expectTargetFcmVersion(2);

    auto pred = getInstanceListFunc({
        "android.hardware.regex@1.1::IRegex/default",
        "android.hardware.regex@1.1::IRegex/special/1.1",
        "android.hardware.regex@1.1::IRegex/regex/1.1/1",
        "android.hardware.regex@1.1::IRegex/regex_common/0",
        "android.hardware.regex@2.0::IRegex/default",
    });
    EXPECT_EQ(NO_DEPRECATED_HALS, vintfObject->checkDeprecation(pred, {}, &error)) << error;

    for (const auto& deprecated : {
             "android.hardware.regex@1.0::IRegex/default",
             "android.hardware.regex@1.0::IRegex/special/1.0",
             "android.hardware.regex@1.0::IRegex/regex/1.0/1",
             "android.hardware.regex@1.0::IRegex/regex_common/0",
             "android.hardware.regex@1.1::IRegex/special/1.0",
             "android.hardware.regex@1.1::IRegex/regex/1.0/1",
         }) {
        // 2.0/default ensures compatibility.
        pred = getInstanceListFunc({
            deprecated,
            "android.hardware.regex@2.0::IRegex/default",
        });
        error.clear();
        EXPECT_EQ(DEPRECATED, vintfObject->checkDeprecation(pred, {}, &error))
            << deprecated << " should be deprecated. " << error;
    }
}

TEST_F(RegexTest, DeprecateLevel3) {
    std::string error;
    expectTargetFcmVersion(3);

    auto pred = getInstanceListFunc({
        "android.hardware.regex@2.0::IRegex/special/2.0",
        "android.hardware.regex@2.0::IRegex/regex/2.0/1",
        "android.hardware.regex@2.0::IRegex/default",
    });
    EXPECT_EQ(NO_DEPRECATED_HALS, vintfObject->checkDeprecation(pred, {}, &error)) << error;

    for (const auto& deprecated : {
             "android.hardware.regex@1.0::IRegex/default",
             "android.hardware.regex@1.0::IRegex/special/1.0",
             "android.hardware.regex@1.0::IRegex/regex/1.0/1",
             "android.hardware.regex@1.0::IRegex/regex_common/0",
             "android.hardware.regex@1.1::IRegex/special/1.0",
             "android.hardware.regex@1.1::IRegex/regex/1.0/1",
             "android.hardware.regex@1.1::IRegex/special/1.1",
             "android.hardware.regex@1.1::IRegex/regex/1.1/1",
             "android.hardware.regex@1.1::IRegex/regex_common/0",
         }) {
        // 2.0/default ensures compatibility.
        pred = getInstanceListFunc({
            deprecated,
            "android.hardware.regex@2.0::IRegex/default",
        });

        error.clear();
        EXPECT_EQ(DEPRECATED, vintfObject->checkDeprecation(pred, {}, &error))
            << deprecated << " should be deprecated.";
    }
}

//
// Set of framework matrices of different FCM version with <kernel>.
//

#define FAKE_KERNEL(__version__, __key__, __level__)                   \
    "    <kernel version=\"" __version__ "\" level=\"" #__level__ "\">\n"            \
    "        <config>\n"                                    \
    "            <key>CONFIG_" __key__ "</key>\n"           \
    "            <value type=\"tristate\">y</value>\n"      \
    "        </config>\n"                                   \
    "    </kernel>\n"

const static std::vector<std::string> systemMatrixKernelXmls = {
    // 1.xml
    "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\" level=\"1\">\n"
    FAKE_KERNEL("1.0.0", "A1", 1)
    FAKE_KERNEL("2.0.0", "B1", 1)
    "</compatibility-matrix>\n",
    // 2.xml
    "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\" level=\"2\">\n"
    FAKE_KERNEL("2.0.0", "B2", 2)
    FAKE_KERNEL("3.0.0", "C2", 2)
    FAKE_KERNEL("4.0.0", "D2", 2)
    "</compatibility-matrix>\n",
    // 3.xml
    "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\" level=\"3\">\n"
    FAKE_KERNEL("4.0.0", "D3", 3)
    FAKE_KERNEL("5.0.0", "E3", 3)
    "</compatibility-matrix>\n",
    // 4.xml
    "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\" level=\"4\">\n"
    FAKE_KERNEL("5.0.0", "E4", 4)
    FAKE_KERNEL("6.0.0", "F4", 4)
    "</compatibility-matrix>\n",
    // 5.xml
    "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\" level=\"5\">\n"
    FAKE_KERNEL("6.0.0", "F5", 5)
    FAKE_KERNEL("7.0.0", "G5", 5)
    "</compatibility-matrix>\n",
};

class KernelTest : public MultiMatrixTest {
   public:
    void expectKernelFcmVersion(size_t targetFcm, Level kernelFcm) {
        std::string xml = "<manifest " + kMetaVersionStr + " type=\"device\" target-level=\"" +
                          to_string(static_cast<Level>(targetFcm)) + "\">\n";
        if (kernelFcm != Level::UNSPECIFIED) {
            xml += "    <kernel target-level=\"" + to_string(kernelFcm) + "\"/>\n";
        }
        xml += "</manifest>";
        expectFetch(kVendorManifest, xml);
    }
};

// Assume that we are developing level 2. Test that old <kernel> requirements should
// not change and new <kernel> versions are added.
TEST_F(KernelTest, Level1AndLevel2) {
    SetUpMockSystemMatrices({systemMatrixKernelXmls[0], systemMatrixKernelXmls[1]});

    expectTargetFcmVersion(1);
    auto matrix = vintfObject->getFrameworkCompatibilityMatrix(true /* skipCache */);
    ASSERT_NE(nullptr, matrix);
    std::string xml = gCompatibilityMatrixConverter(*matrix);

    EXPECT_IN(FAKE_KERNEL("1.0.0", "A1", 1), xml) << "\nOld requirements must not change.";
    EXPECT_IN(FAKE_KERNEL("2.0.0", "B1", 1), xml) << "\nOld requirements must not change.";
    EXPECT_IN(FAKE_KERNEL("3.0.0", "C2", 2), xml) << "\nShould see <kernel> from new matrices";
    EXPECT_IN(FAKE_KERNEL("4.0.0", "D2", 2), xml) << "\nShould see <kernel> from new matrices";

    EXPECT_IN(FAKE_KERNEL("2.0.0", "B2", 2), xml) << "\nShould see <kernel> from new matrices";
}

// Assume that we are developing level 3. Test that old <kernel> requirements should
// not change and new <kernel> versions are added.
TEST_F(KernelTest, Level1AndMore) {
    SetUpMockSystemMatrices({systemMatrixKernelXmls});

    expectTargetFcmVersion(1);
    auto matrix = vintfObject->getFrameworkCompatibilityMatrix(true /* skipCache */);
    ASSERT_NE(nullptr, matrix);
    std::string xml = gCompatibilityMatrixConverter(*matrix);

    EXPECT_IN(FAKE_KERNEL("1.0.0", "A1", 1), xml) << "\nOld requirements must not change.";
    EXPECT_IN(FAKE_KERNEL("2.0.0", "B1", 1), xml) << "\nOld requirements must not change.";
    EXPECT_IN(FAKE_KERNEL("3.0.0", "C2", 2), xml) << "\nOld requirements must not change.";
    EXPECT_IN(FAKE_KERNEL("4.0.0", "D2", 2), xml) << "\nOld requirements must not change.";
    EXPECT_IN(FAKE_KERNEL("5.0.0", "E3", 3), xml) << "\nShould see <kernel> from new matrices";

    EXPECT_IN(FAKE_KERNEL("2.0.0", "B2", 2), xml) << "\nShould see <kernel> from new matrices";
    EXPECT_IN(FAKE_KERNEL("4.0.0", "D3", 3), xml) << "\nShould see <kernel> from new matrices";
}

KernelInfo MakeKernelInfo(const std::string& version, const std::string& key) {
    KernelInfo info;
    CHECK(gKernelInfoConverter(&info,
                               "    <kernel version=\"" + version + "\">\n"
                               "        <config>\n"
                               "            <key>CONFIG_" + key + "</key>\n"
                               "            <value type=\"tristate\">y</value>\n"
                               "        </config>\n"
                               "    </kernel>\n"));
    return info;
}

TEST_F(KernelTest, Compatible) {
    setupMockFetcher(vendorManifestXml1, systemMatrixXml1, systemManifestXml1, vendorMatrixXml1);

    SetUpMockSystemMatrices({
        "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\" level=\"1\">\n"
        FAKE_KERNEL("1.0.0", "A1", 1)
        FAKE_KERNEL("2.0.0", "B1", 1)
        "    <sepolicy>\n"
        "        <kernel-sepolicy-version>0</kernel-sepolicy-version>\n"
        "        <sepolicy-version>0.0</sepolicy-version>\n"
        "    </sepolicy>\n"
        "</compatibility-matrix>\n"});
    expectKernelFcmVersion(Level{1}, Level{1});
    expectSystemManifest();
    expectVendorMatrix();

    auto info = MakeKernelInfo("1.0.0", "A1");
    runtimeInfoFactory().getInfo()->setNextFetchKernelInfo(info.version(), info.configs());
    std::string error;
    ASSERT_EQ(COMPATIBLE, vintfObject->checkCompatibility(&error)) << error;
}

TEST_F(KernelTest, Level) {
    expectKernelFcmVersion(1, Level{10});
    EXPECT_EQ(Level{10}, vintfObject->getKernelLevel());
}

TEST_F(KernelTest, LevelUnspecified) {
    expectKernelFcmVersion(1, Level::UNSPECIFIED);
    EXPECT_EQ(Level::UNSPECIFIED, vintfObject->getKernelLevel());
}

class KernelTestP : public KernelTest, public WithParamInterface<
    std::tuple<std::vector<std::string>, KernelInfo, Level, Level, bool>> {};
// Assume that we are developing level 2. Test that old <kernel> requirements should
// not change and new <kernel> versions are added.
TEST_P(KernelTestP, Test) {
    auto&& [matrices, info, targetFcm, kernelFcm, pass] = GetParam();

    SetUpMockSystemMatrices(matrices);
    expectKernelFcmVersion(targetFcm, kernelFcm);
    runtimeInfoFactory().getInfo()->setNextFetchKernelInfo(info.version(), info.configs());
    auto matrix = vintfObject->getFrameworkCompatibilityMatrix();
    auto runtime = vintfObject->getRuntimeInfo();
    ASSERT_NE(nullptr, matrix);
    ASSERT_NE(nullptr, runtime);
    std::string fallbackError = kernelFcm == Level::UNSPECIFIED
        ? "\nOld requirements must not change"
        : "\nMust not pull unnecessary requirements from new matrices";
    std::string error;
    ASSERT_EQ(pass, runtime->checkCompatibility(*matrix, &error))
            << (pass ? error : fallbackError);
}


std::vector<KernelTestP::ParamType> KernelTestParamValues() {
    std::vector<KernelTestP::ParamType> ret;
    std::vector<std::string> matrices = {systemMatrixKernelXmls[0], systemMatrixKernelXmls[1]};
    ret.emplace_back(matrices, MakeKernelInfo("1.0.0", "A1"), Level{1}, Level::UNSPECIFIED, true);
    ret.emplace_back(matrices, MakeKernelInfo("2.0.0", "B1"), Level{1}, Level::UNSPECIFIED, true);
    ret.emplace_back(matrices, MakeKernelInfo("3.0.0", "C2"), Level{1}, Level::UNSPECIFIED, true);
    ret.emplace_back(matrices, MakeKernelInfo("4.0.0", "D2"), Level{1}, Level::UNSPECIFIED, true);
    ret.emplace_back(matrices, MakeKernelInfo("2.0.0", "B2"), Level{1}, Level::UNSPECIFIED, false);

    ret.emplace_back(matrices, MakeKernelInfo("1.0.0", "A1"), Level{1}, Level{1}, true);
    ret.emplace_back(matrices, MakeKernelInfo("2.0.0", "B1"), Level{1}, Level{1}, true);
    ret.emplace_back(matrices, MakeKernelInfo("3.0.0", "C2"), Level{1}, Level{1}, false);
    ret.emplace_back(matrices, MakeKernelInfo("4.0.0", "D2"), Level{1}, Level{1}, false);
    ret.emplace_back(matrices, MakeKernelInfo("2.0.0", "B2"), Level{1}, Level{1}, true);

    matrices = systemMatrixKernelXmls;
    ret.emplace_back(matrices, MakeKernelInfo("1.0.0", "A1"), Level{1}, Level::UNSPECIFIED, true);
    ret.emplace_back(matrices, MakeKernelInfo("2.0.0", "B1"), Level{1}, Level::UNSPECIFIED, true);
    ret.emplace_back(matrices, MakeKernelInfo("3.0.0", "C2"), Level{1}, Level::UNSPECIFIED, true);
    ret.emplace_back(matrices, MakeKernelInfo("4.0.0", "D2"), Level{1}, Level::UNSPECIFIED, true);
    ret.emplace_back(matrices, MakeKernelInfo("5.0.0", "E3"), Level{1}, Level::UNSPECIFIED, true);
    ret.emplace_back(matrices, MakeKernelInfo("6.0.0", "F4"), Level{1}, Level::UNSPECIFIED, true);
    ret.emplace_back(matrices, MakeKernelInfo("2.0.0", "B2"), Level{1}, Level::UNSPECIFIED, false);
    ret.emplace_back(matrices, MakeKernelInfo("4.0.0", "D3"), Level{1}, Level::UNSPECIFIED, false);
    ret.emplace_back(matrices, MakeKernelInfo("5.0.0", "E4"), Level{1}, Level::UNSPECIFIED, false);
    ret.emplace_back(matrices, MakeKernelInfo("6.0.0", "F5"), Level{1}, Level::UNSPECIFIED, false);

    ret.emplace_back(matrices, MakeKernelInfo("6.0.0", "F4"), Level{2}, Level::UNSPECIFIED, true);
    ret.emplace_back(matrices, MakeKernelInfo("6.0.0", "F4"), Level{3}, Level::UNSPECIFIED, true);
    ret.emplace_back(matrices, MakeKernelInfo("6.0.0", "F4"), Level{4}, Level::UNSPECIFIED, true);
    ret.emplace_back(matrices, MakeKernelInfo("6.0.0", "F4"), Level{5}, Level::UNSPECIFIED, false);

    ret.emplace_back(matrices, MakeKernelInfo("1.0.0", "A1"), Level{1}, Level{1}, true);
    ret.emplace_back(matrices, MakeKernelInfo("2.0.0", "B1"), Level{1}, Level{1}, true);
    ret.emplace_back(matrices, MakeKernelInfo("2.0.0", "B2"), Level{1}, Level{1}, true);
    ret.emplace_back(matrices, MakeKernelInfo("3.0.0", "C2"), Level{1}, Level{1}, false);
    ret.emplace_back(matrices, MakeKernelInfo("3.0.0", "C3"), Level{1}, Level{1}, false);
    ret.emplace_back(matrices, MakeKernelInfo("4.0.0", "D2"), Level{1}, Level{1}, false);
    ret.emplace_back(matrices, MakeKernelInfo("4.0.0", "D3"), Level{1}, Level{1}, false);
    ret.emplace_back(matrices, MakeKernelInfo("5.0.0", "E3"), Level{1}, Level{1}, false);
    ret.emplace_back(matrices, MakeKernelInfo("5.0.0", "E4"), Level{1}, Level{1}, false);
    ret.emplace_back(matrices, MakeKernelInfo("6.0.0", "F4"), Level{1}, Level{1}, false);
    ret.emplace_back(matrices, MakeKernelInfo("6.0.0", "F5"), Level{1}, Level{1}, false);
    ret.emplace_back(matrices, MakeKernelInfo("7.0.0", "G5"), Level{1}, Level{1}, false);

    ret.emplace_back(matrices, MakeKernelInfo("6.0.0", "F4"), Level{2}, Level{2}, false);
    ret.emplace_back(matrices, MakeKernelInfo("6.0.0", "F4"), Level{3}, Level{3}, false);
    ret.emplace_back(matrices, MakeKernelInfo("6.0.0", "F4"), Level{4}, Level{4}, true);
    ret.emplace_back(matrices, MakeKernelInfo("6.0.0", "F4"), Level{5}, Level{5}, false);

    return ret;
}

std::vector<KernelTestP::ParamType> RKernelTestParamValues() {
    std::vector<KernelTestP::ParamType> ret;
    std::vector<std::string> matrices = systemMatrixKernelXmls;

    // Must not use *-r+ kernels without specifying kernel FCM version
    ret.emplace_back(matrices, MakeKernelInfo("7.0.0", "G5"), Level{1}, Level::UNSPECIFIED, false);
    ret.emplace_back(matrices, MakeKernelInfo("7.0.0", "G5"), Level{2}, Level::UNSPECIFIED, false);
    ret.emplace_back(matrices, MakeKernelInfo("7.0.0", "G5"), Level{3}, Level::UNSPECIFIED, false);
    ret.emplace_back(matrices, MakeKernelInfo("7.0.0", "G5"), Level{4}, Level::UNSPECIFIED, false);
    ret.emplace_back(matrices, MakeKernelInfo("7.0.0", "G5"), Level{5}, Level::UNSPECIFIED, false);

    // May use *-r+ kernels with kernel FCM version
    ret.emplace_back(matrices, MakeKernelInfo("7.0.0", "G5"), Level{1}, Level{5}, true);
    ret.emplace_back(matrices, MakeKernelInfo("7.0.0", "G5"), Level{2}, Level{5}, true);
    ret.emplace_back(matrices, MakeKernelInfo("7.0.0", "G5"), Level{3}, Level{5}, true);
    ret.emplace_back(matrices, MakeKernelInfo("7.0.0", "G5"), Level{4}, Level{5}, true);
    ret.emplace_back(matrices, MakeKernelInfo("7.0.0", "G5"), Level{5}, Level{5}, true);

    return ret;
}

std::string PrintKernelTestParam(const TestParamInfo<KernelTestP::ParamType>& info) {
    const auto& [matrices, kernelInfo, targetFcm, kernelFcm, pass] = info.param;
    return (matrices.size() == 2 ? "Level1AndLevel2_" : "Level1AndMore_") +
           android::base::StringReplace(to_string(kernelInfo.version()), ".", "_", true) + "_" +
           android::base::StringReplace(kernelInfo.configs().begin()->first, "CONFIG_", "", false) +
           "_TargetFcm" +
           (targetFcm == Level::UNSPECIFIED ? "Unspecified" : to_string(targetFcm)) +
           "_KernelFcm" +
           (kernelFcm == Level::UNSPECIFIED ? "Unspecified" : to_string(kernelFcm)) +
           "_Should" + (pass ? "Pass" : "Fail");
}

INSTANTIATE_TEST_SUITE_P(KernelTest, KernelTestP, ValuesIn(KernelTestParamValues()),
                         &PrintKernelTestParam);
INSTANTIATE_TEST_SUITE_P(NoRKernelWithoutFcm, KernelTestP, ValuesIn(RKernelTestParamValues()),
                         &PrintKernelTestParam);

class VintfObjectPartialUpdateTest : public MultiMatrixTest {
   protected:
    void SetUp() override {
        MultiMatrixTest::SetUp();
    }
};

TEST_F(VintfObjectPartialUpdateTest, DeviceCompatibility) {
    setupMockFetcher(vendorManifestRequire1, "", systemManifestXml1, vendorMatrixXml1);
    SetUpMockSystemMatrices(systemMatrixRequire);

    expectSystemManifest();
    expectVendorMatrix();
    expectVendorManifest();

    std::string error;
    EXPECT_TRUE(vintfObject->checkCompatibility(&error)) << error;
}

std::string CreateFrameworkManifestFrag(const std::string& interface) {
    return "<manifest " + kMetaVersionStr + " type=\"framework\">\n"
           "    <hal format=\"hidl\">\n"
           "        <name>android.hardware.foo</name>\n"
           "        <transport>hwbinder</transport>\n"
           "        <fqname>@1.0::" + interface + "/default</fqname>\n"
           "    </hal>\n"
           "</manifest>\n";
}

using FrameworkManifestTestParam =
    std::tuple<bool /* Existence of /system/etc/vintf/manifest.xml */,
               bool /* Existence of /system/etc/vintf/manifest/fragment.xml */,
               bool /* Existence of /product/etc/vintf/manifest.xml */,
               bool /* Existence of /product/etc/vintf/manifest/fragment.xml */,
               bool /* Existence of /system_ext/etc/vintf/manifest.xml */,
               bool /* Existence of /system_ext/etc/vintf/manifest/fragment.xml */>;
class FrameworkManifestTest : public VintfObjectTestBase,
                              public ::testing::WithParamInterface<FrameworkManifestTestParam> {
   protected:
    // Set the existence of |path|.
    void expectManifest(const std::string& path, const std::string& interface, bool exists) {
        if (exists) {
            expectFetchRepeatedly(path, CreateFrameworkManifestFrag(interface));
        } else {
            expectFileNotExist(StrEq(path));
        }
    }

    // Set the existence of |path| as a fragment dir
    void expectFragment(const std::string& path, const std::string& interface, bool exists) {
        if (exists) {
            EXPECT_CALL(fetcher(), listFiles(StrEq(path), _, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([](const auto&, auto* out, auto*) {
                    *out = {"fragment.xml"};
                    return ::android::OK;
                }));
            expectFetchRepeatedly(path + "fragment.xml",
                                  CreateFrameworkManifestFrag(interface));
        } else {
            EXPECT_CALL(fetcher(), listFiles(StrEq(path), _, _))
                .Times(AnyNumber())
                .WillRepeatedly(Return(::android::OK));
            expectFileNotExist(path + "fragment.xml");
        }
    }

    void expectContainsInterface(const std::string& interface, bool contains = true) {
        auto manifest = vintfObject->getFrameworkHalManifest();
        ASSERT_NE(nullptr, manifest);
        EXPECT_NE(manifest->getHidlInstances("android.hardware.foo", {1, 0}, interface).empty(),
                  contains)
            << interface << " is missing.";
    }
};

TEST_P(FrameworkManifestTest, Existence) {
    expectFileNotExist(StrEq(kSystemLegacyManifest));

    expectManifest(kSystemManifest, "ISystemEtc", std::get<0>(GetParam()));
    expectFragment(kSystemManifestFragmentDir, "ISystemEtcFragment", std::get<1>(GetParam()));
    expectManifest(kProductManifest, "IProductEtc", std::get<2>(GetParam()));
    expectFragment(kProductManifestFragmentDir, "IProductEtcFragment", std::get<3>(GetParam()));
    expectManifest(kSystemExtManifest, "ISystemExtEtc", std::get<4>(GetParam()));
    expectFragment(kSystemExtManifestFragmentDir, "ISystemExtEtcFragment", std::get<5>(GetParam()));

    if (!std::get<0>(GetParam())) {
        EXPECT_EQ(nullptr, vintfObject->getFrameworkHalManifest())
            << "getFrameworkHalManifest must return nullptr if " << kSystemManifest
            << " does not exist";
    } else {
        expectContainsInterface("ISystemEtc", std::get<0>(GetParam()));
        expectContainsInterface("ISystemEtcFragment", std::get<1>(GetParam()));
        expectContainsInterface("IProductEtc", std::get<2>(GetParam()));
        expectContainsInterface("IProductEtcFragment", std::get<3>(GetParam()));
        expectContainsInterface("ISystemExtEtc", std::get<4>(GetParam()));
        expectContainsInterface("ISystemExtEtcFragment", std::get<5>(GetParam()));
    }
}
INSTANTIATE_TEST_SUITE_P(Vintf, FrameworkManifestTest,
                         ::testing::Combine(Bool(), Bool(), Bool(), Bool(), Bool(), Bool()));


//
// Set of OEM FCM matrices at different FCM version.
//

std::vector<std::string> GetOemFcmMatrixLevels(const std::string& name) {
    return {
        // 1.xml
        "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\" level=\"1\">\n"
        "    <hal format=\"hidl\" optional=\"true\">\n"
        "        <name>vendor.foo." + name + "</name>\n"
        "        <version>1.0</version>\n"
        "        <interface>\n"
        "            <name>IExtra</name>\n"
        "            <instance>default</instance>\n"
        "        </interface>\n"
        "    </hal>\n"
        "</compatibility-matrix>\n",
        // 2.xml
        "<compatibility-matrix " + kMetaVersionStr + " type=\"framework\" level=\"2\">\n"
        "    <hal format=\"hidl\" optional=\"true\">\n"
        "        <name>vendor.foo." + name + "</name>\n"
        "        <version>2.0</version>\n"
        "        <interface>\n"
        "            <name>IExtra</name>\n"
        "            <instance>default</instance>\n"
        "        </interface>\n"
        "    </hal>\n"
        "</compatibility-matrix>\n",
    };
}

class OemFcmLevelTest : public MultiMatrixTest,
                        public WithParamInterface<std::tuple<size_t, bool, bool>> {
   protected:
    virtual void SetUp() override {
        MultiMatrixTest::SetUp();
        SetUpMockSystemMatrices({systemMatrixLevel1, systemMatrixLevel2});
    }
    using Instances = std::set<std::string>;
    Instances GetInstances(const CompatibilityMatrix* fcm) {
        Instances instances;
        fcm->forEachHidlInstance([&instances](const auto& matrixInstance) {
            instances.insert(matrixInstance.description(matrixInstance.versionRange().minVer()));
            return true; // continue
        });
        return instances;
    }
};

TEST_P(OemFcmLevelTest, Test) {
    auto&& [level, hasProduct, hasSystemExt] = GetParam();

    expectTargetFcmVersion(level);
    if (hasProduct) {
        SetUpMockMatrices(kProductVintfDir, GetOemFcmMatrixLevels("product"));
    }
    if (hasSystemExt) {
        SetUpMockMatrices(kSystemExtVintfDir, GetOemFcmMatrixLevels("systemext"));
    }

    auto fcm = vintfObject->getFrameworkCompatibilityMatrix();
    ASSERT_NE(nullptr, fcm);
    auto instances = GetInstances(fcm.get());

    auto containsOrNot = [](bool contains, const std::string& e) {
        return contains ? SafeMatcherCast<Instances>(Contains(e))
                        : SafeMatcherCast<Instances>(Not(Contains(e)));
    };

    EXPECT_THAT(instances, containsOrNot(level == 1,
                                         "android.hardware.major@1.0::IMajor/default"));
    EXPECT_THAT(instances, containsOrNot(level == 1 && hasProduct,
                                         "vendor.foo.product@1.0::IExtra/default"));
    EXPECT_THAT(instances, containsOrNot(level == 1 && hasSystemExt,
                                         "vendor.foo.systemext@1.0::IExtra/default"));
    EXPECT_THAT(instances, Contains("android.hardware.major@2.0::IMajor/default"));
    EXPECT_THAT(instances, containsOrNot(hasProduct,
                                         "vendor.foo.product@2.0::IExtra/default"));
    EXPECT_THAT(instances, containsOrNot(hasSystemExt,
                                         "vendor.foo.systemext@2.0::IExtra/default"));
}

static std::string OemFcmLevelTestParamToString(
        const TestParamInfo<OemFcmLevelTest::ParamType>& info) {
    auto&& [level, hasProduct, hasSystemExt] = info.param;
    auto name = "Level" + std::to_string(level);
    name += "With"s + (hasProduct ? "" : "out") + "Product";
    name += "With"s + (hasSystemExt ? "" : "out") + "SystemExt";
    return name;
}
INSTANTIATE_TEST_SUITE_P(OemFcmLevel, OemFcmLevelTest, Combine(Values(1, 2), Bool(), Bool()),
    OemFcmLevelTestParamToString);
// clang-format on

}  // namespace testing
}  // namespace vintf
}  // namespace android

int main(int argc, char** argv) {
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
