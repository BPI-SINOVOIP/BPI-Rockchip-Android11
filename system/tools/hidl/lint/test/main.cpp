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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <hidl-util/StringHelper.h>

#include <string>
#include <vector>

#include "../Lint.h"
#include "../LintRegistry.h"
#include "Coordinator.h"

using ::testing::Contains;
using ::testing::ContainsRegex;
using ::testing::Property;

namespace android {
class HidlLintTest : public ::testing::Test {
  protected:
    Coordinator coordinator;

    void SetUp() override {
        char* argv[2] = {const_cast<char*>("hidl-lint"),
                         const_cast<char*>("-rlint_test:system/tools/hidl/lint/test/interfaces")};
        coordinator.parseOptions(2, argv, "", [](int /* opt */, char* /* optarg */) {});
    }

    void getLintsForHal(const std::string& name, std::vector<Lint>* errors) {
        std::vector<FQName> targets;

        FQName fqName;
        if (!FQName::parse(name, &fqName)) {
            FAIL() << "Could not parse fqName: " << name;
        }

        if (fqName.isFullyQualified()) {
            targets.push_back(fqName);
        } else {
            status_t err = coordinator.appendPackageInterfacesToVector(fqName, &targets);
            if (err != OK) {
                FAIL() << "Could not get sources for: " << name;
            }
        }

        for (const FQName& fqName : targets) {
            AST* ast = coordinator.parse(fqName);
            if (ast == nullptr) {
                FAIL() << "Could not parse " << fqName.name() << ". Aborting.";
            }

            LintRegistry::get()->runAllLintFunctions(*ast, errors);
        }
    }
};

#define EXPECT_NO_LINT(interface)           \
    do {                                    \
        std::vector<Lint> errors;           \
        getLintsForHal(interface, &errors); \
        EXPECT_EQ(0, errors.size());        \
    } while (false)

#define EXPECT_LINT(interface, errorMsg)                              \
    do {                                                              \
        std::vector<Lint> errors;                                     \
        getLintsForHal(interface, &errors);                           \
        EXPECT_EQ(1, errors.size());                                  \
        if (errors.size() != 1) break;                                \
        EXPECT_THAT(errors[0].getMessage(), ContainsRegex(errorMsg)); \
    } while (false)

#define EXPECT_A_LINT(interface, errorMsg)                                                   \
    do {                                                                                     \
        std::vector<Lint> errors;                                                            \
        getLintsForHal(interface, &errors);                                                  \
        EXPECT_LE(1, errors.size());                                                         \
        if (errors.size() < 1) break;                                                        \
        EXPECT_THAT(errors, Contains(Property(&Lint::getMessage, ContainsRegex(errorMsg)))); \
    } while (false)

TEST_F(HidlLintTest, OnewayLintTest) {
    // Has no errors (empty). Lint size should be 0.
    EXPECT_NO_LINT("lint_test.oneway@1.0::IEmpty");

    // Only has either oneway or nononeway methods. Lint size should be 0.
    EXPECT_NO_LINT("lint_test.oneway@1.0::IOneway");
    EXPECT_NO_LINT("lint_test.oneway@1.0::INonOneway");

    // A child of a mixed interface should not trigger a lint if it is oneway/nononeway.
    // Lint size should be 0
    EXPECT_NO_LINT("lint_test.oneway@1.0::IMixedOnewayChild");
    EXPECT_NO_LINT("lint_test.oneway@1.0::IMixedNonOnewayChild");

    // A child with the same oneway type should not trigger a lint. Lint size should be 0.
    EXPECT_NO_LINT("lint_test.oneway@1.0::IOnewayChild");
    EXPECT_NO_LINT("lint_test.oneway@1.0::INonOnewayChild");

    // This interface is mixed. Should have a lint.
    EXPECT_LINT("lint_test.oneway@1.0::IMixed", "IMixed has both oneway and non-oneway methods.");

    // Regardless of parent, if interface is mixed, it should have a lint.
    EXPECT_LINT("lint_test.oneway@1.0::IMixedMixedChild",
                "IMixedMixedChild has both oneway and non-oneway methods.");

    // When onewaytype is different from parent it should trigger a lint.
    EXPECT_LINT("lint_test.oneway@1.0::IOnewayOpposite",
                "IOnewayOpposite should only have oneway methods");

    EXPECT_LINT("lint_test.oneway@1.0::INonOnewayOpposite",
                "INonOnewayOpposite should only have non-oneway methods");
}

TEST_F(HidlLintTest, SafeunionLintTest) {
    // Has no errors (empty). Even though types.hal has a lint.
    EXPECT_NO_LINT("lint_test.safeunion@1.0::IEmpty");

    // A child of an interface that refers to a union should not lint unless it refers to a union
    EXPECT_NO_LINT("lint_test.safeunion@1.1::IReference");

    // Should lint the union type definition
    EXPECT_LINT("lint_test.safeunion@1.0::types", "union InTypes.*defined");
    EXPECT_LINT("lint_test.safeunion@1.0::IDefined", "union SomeUnion.*defined");

    // Should mention that a union type is being referenced and where that type is.
    EXPECT_LINT("lint_test.safeunion@1.0::IReference", "Reference to union type.*types.hal");

    // Referencing a union inside a struct should lint
    EXPECT_LINT("lint_test.safeunion@1.1::types", "Reference to union type.*1\\.0/types.hal");

    // Defining a union inside a struct should lint
    EXPECT_LINT("lint_test.safeunion@1.0::IUnionInStruct", "union SomeUnionInStruct.*defined");

    // Reference to a struct that contains a union should lint
    EXPECT_LINT("lint_test.safeunion@1.1::IReferStructWithUnion",
                "Reference to struct.*contains a union type.");
}

TEST_F(HidlLintTest, ImportTypesTest) {
    // Imports types.hal file from package
    EXPECT_LINT("lint_test.import_types@1.0::IImport", "Redundant import");

    // Imports types.hal from other package
    EXPECT_LINT("lint_test.import_types@1.0::IImportOther", "This imports every type");

    // Imports types.hal from previous version of the same package
    EXPECT_LINT("lint_test.import_types@1.1::types", "This imports every type");

    // Imports types.hal from same package with fully qualified name
    EXPECT_LINT("lint_test.import_types@1.1::IImport", "Redundant import");
}

TEST_F(HidlLintTest, SmallStructsTest) {
    // Referencing bad structs should not lint
    EXPECT_NO_LINT("lint_test.small_structs@1.0::IReference");

    // Empty structs/unions should lint
    EXPECT_LINT("lint_test.small_structs@1.0::IEmptyStruct", "contains no elements");
    EXPECT_A_LINT("lint_test.small_structs@1.0::IEmptyUnion", "contains no elements");

    // Structs/unions with single field should lint
    EXPECT_LINT("lint_test.small_structs@1.0::ISingleStruct", "only contains 1 element");
    EXPECT_A_LINT("lint_test.small_structs@1.0::ISingleUnion", "only contains 1 element");
}

TEST_F(HidlLintTest, DocCommentRefTest) {
    EXPECT_NO_LINT("lint_test.doc_comments@1.0::ICorrect");

    // Should lint since nothing follows the keyword
    EXPECT_LINT("lint_test.doc_comments@1.0::INoReturn",
                "should be followed by a return parameter");
    EXPECT_LINT("lint_test.doc_comments@1.0::INoParam", "should be followed by a parameter name");
    EXPECT_LINT("lint_test.doc_comments@1.0::IReturnSpace",
                "should be followed by a return parameter");

    // Typos should be caught
    EXPECT_LINT("lint_test.doc_comments@1.0::IWrongReturn", "is not a return parameter");
    EXPECT_LINT("lint_test.doc_comments@1.0::IWrongParam", "is not an argument");

    // Incorrectly marked as @param should lint as a param
    EXPECT_LINT("lint_test.doc_comments@1.0::ISwitched", "is not an argument");

    // Incorrectly marked as @param should lint as a param
    EXPECT_LINT("lint_test.doc_comments@1.0::IParamAfterReturn",
                "@param references should come before @return");

    // Reversed order should be caught
    EXPECT_LINT("lint_test.doc_comments@1.0::IRevReturn",
                "@return references should be ordered the same way they show up");
    EXPECT_LINT("lint_test.doc_comments@1.0::IRevParam",
                "@param references should be ordered the same way they show up");

    // Referencing the same param/return multiple times should be caught
    EXPECT_LINT("lint_test.doc_comments@1.0::IDoubleReturn", "was referenced multiple times");
    EXPECT_LINT("lint_test.doc_comments@1.0::IDoubleParam", "was referenced multiple times");
}

TEST_F(HidlLintTest, MethodVersionsTest) {
    // Extends baseMethod correctly
    EXPECT_NO_LINT("lint_test.method_versions@1.0::IChangeBase");

    // Extends IBase.foo through @1.0::IChangeBase correctly
    EXPECT_NO_LINT("lint_test.method_versions@1.1::IChangeBase");

    // Lints because lintBadName_V1_x is not minor_major version naming
    EXPECT_LINT("lint_test.method_versions@1.0::IBase",
                "Methods should follow the camelCase naming convention.");

    // Lints because incorrect package name
    EXPECT_LINT("lint_test.method_versions@1.0::IChild", "interface is in package version 1.0");

    // Lints because wrong minor version
    EXPECT_LINT("lint_test.method_versions@1.0::IWrongMinor",
                "Methods should follow the camelCase naming convention.");

    // Lints because underscore in wrong place
    EXPECT_LINT("lint_test.method_versions@1.0::IWrongUnderscore",
                "when defining a new version of a method");

    // Method does not exist in any of the super types
    EXPECT_LINT("lint_test.method_versions@1.1::IMethodDNE", "Could not find method");

    // Methods are not in camel case
    EXPECT_LINT("lint_test.method_versions@1.0::IPascalCase",
                "Methods should follow the camelCase naming convention.");
    EXPECT_LINT("lint_test.method_versions@1.0::IHybrid",
                "Methods should follow the camelCase naming convention.");
    EXPECT_LINT("lint_test.method_versions@1.0::ISnakeCase",
                "Methods should follow the camelCase naming convention.");
}

TEST_F(HidlLintTest, EnumMaxAllTest) {
    // Implements MAX correctly
    EXPECT_NO_LINT("lint_test.enum_max_all@1.0::IFoo");

    // Lint since MAX and ALL are enum values
    EXPECT_LINT("lint_test.enum_max_all@1.0::IMax",
                "\"MAX\" enum values have been known to become out of date");
    EXPECT_LINT("lint_test.enum_max_all@1.0::IAll",
                "\"ALL\" enum values have been known to become out of date");
    EXPECT_LINT("lint_test.enum_max_all@1.0::ICount",
                "\"COUNT\" enum values have been known to become out of date");

    // Lint since MAX and ALL are parts of the enum values
    EXPECT_LINT("lint_test.enum_max_all@1.0::IMax2",
                "\"MAX\" enum values have been known to become out of date");
    EXPECT_LINT("lint_test.enum_max_all@1.0::IAll2",
                "\"ALL\" enum values have been known to become out of date");
    EXPECT_LINT("lint_test.enum_max_all@1.0::ICount2",
                "\"COUNT\" enum values have been known to become out of date");
}

TEST_F(HidlLintTest, UnhandledDocCommentTest) {
    EXPECT_LINT("lint_test.unhandled_comments@1.0::types",
                "cannot be processed since it is in an unrecognized place");

    // Even single line comments are unhandled
    EXPECT_LINT("lint_test.unhandled_comments@1.0::ISingleComment",
                "cannot be processed since it is in an unrecognized place");
}

TEST_F(HidlLintTest, NamingConventionsTest) {
    EXPECT_LINT("lint_test.naming_conventions@1.0::IBad_Interface",
                "type .* should be named .* PascalCase");
    EXPECT_LINT("lint_test.naming_conventions@1.0::IBadStruct",
                "type .* should be named .* PascalCase");
    EXPECT_LINT("lint_test.naming_conventions@1.0::IBadEnum",
                "type .* should be named .* PascalCase");
    EXPECT_A_LINT("lint_test.naming_conventions@1.0::IBadUnion",
                  "type .* should be named .* PascalCase");

    EXPECT_LINT("lint_test.naming_conventions@1.0::IBadStructMember",
                "member .* of type .* should be named .* camelCase");
    EXPECT_A_LINT("lint_test.naming_conventions@1.0::IBadUnionMember",
                  "member .* of type .* should be named .* camelCase");

    EXPECT_LINT("lint_test.naming_conventions@1.0::IBadEnumValue",
                "enumeration .* of enum .* should be named .* UPPER_SNAKE_CASE");
}
}  // namespace android
