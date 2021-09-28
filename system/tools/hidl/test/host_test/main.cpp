/*
 * Copyright (C) 2016 The Android Open Source Project
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

#define LOG_TAG "libhidl-gen-utils"

#include <vector>

#include <gtest/gtest.h>

#include <ConstantExpression.h>
#include <Coordinator.h>
#include <hidl-util/FQName.h>

#define EXPECT_EQ_OK(expectResult, call, ...)        \
    do {                                             \
        std::string result;                          \
        status_t err = (call)(__VA_ARGS__, &result); \
        EXPECT_EQ(err, ::android::OK);               \
        EXPECT_EQ(expectResult, result);             \
    } while (false)

namespace android {

class HidlGenHostTest : public ::testing::Test {};

class HidlGenHostDeathTest : public ::testing::Test {};

TEST_F(HidlGenHostTest, CoordinatorTest) {
    Coordinator coordinator;

    std::string error;
    EXPECT_EQ(OK, coordinator.addPackagePath("a.b", "a1/b1", &error));
    EXPECT_TRUE(error.empty());
    EXPECT_NE(OK, coordinator.addPackagePath("a.b", "a2/b2/", &error));
    EXPECT_FALSE(error.empty());

    coordinator.addDefaultPackagePath("a.b", "a3/b3/"); // should take path above
    coordinator.addDefaultPackagePath("a.c", "a4/b4/"); // should succeed

    EXPECT_EQ_OK("a.b", coordinator.getPackageRoot, FQName("a.b.foo", "1.0"));
    EXPECT_EQ_OK("a.c", coordinator.getPackageRoot, FQName("a.c.foo.bar", "1.0", "IFoo"));

    // getPackagePath(fqname, relative, sanitized, ...)
    EXPECT_EQ_OK("a1/b1/foo/1.0/", coordinator.getPackagePath, FQName("a.b.foo", "1.0"), false,
                 false);
    EXPECT_EQ_OK("a4/b4/foo/bar/1.0/", coordinator.getPackagePath,
                 FQName("a.c.foo.bar", "1.0", "IFoo"), false, false);
    EXPECT_EQ_OK("a1/b1/foo/V1_0/", coordinator.getPackagePath, FQName("a.b.foo", "1.0"), false,
                 true);
    EXPECT_EQ_OK("a4/b4/foo/bar/V1_0/", coordinator.getPackagePath,
                 FQName("a.c.foo.bar", "1.0", "IFoo"), false, true);
    EXPECT_EQ_OK("foo/1.0/", coordinator.getPackagePath, FQName("a.b.foo", "1.0"), true, false);
    EXPECT_EQ_OK("foo/bar/1.0/", coordinator.getPackagePath, FQName("a.c.foo.bar", "1.0", "IFoo"),
                 true, false);
    EXPECT_EQ_OK("foo/V1_0/", coordinator.getPackagePath, FQName("a.b.foo", "1.0"), true, true);
    EXPECT_EQ_OK("foo/bar/V1_0/", coordinator.getPackagePath, FQName("a.c.foo.bar", "1.0", "IFoo"),
                 true, true);
}

TEST_F(HidlGenHostTest, CoordinatorFilepathTest) {
    using Location = Coordinator::Location;

    Coordinator coordinator;
    coordinator.setOutputPath("foo/");
    coordinator.setRootPath("bar/");

    std::string error;
    EXPECT_EQ(OK, coordinator.addPackagePath("a.b", "a1/b1", &error));
    EXPECT_TRUE(error.empty());

    const static FQName kName = FQName("a.b.c", "1.2");

    // get file names
    EXPECT_EQ_OK("foo/x.y", coordinator.getFilepath, kName, Location::DIRECT, "x.y");
    EXPECT_EQ_OK("foo/a1/b1/c/1.2/x.y", coordinator.getFilepath, kName, Location::PACKAGE_ROOT,
                 "x.y");
    EXPECT_EQ_OK("foo/a/b/c/1.2/x.y", coordinator.getFilepath, kName, Location::GEN_OUTPUT, "x.y");
    EXPECT_EQ_OK("foo/a/b/c/V1_2/x.y", coordinator.getFilepath, kName, Location::GEN_SANITIZED,
                 "x.y");

    // get directories
    EXPECT_EQ_OK("foo/", coordinator.getFilepath, kName, Location::DIRECT, "");
    EXPECT_EQ_OK("foo/a1/b1/c/1.2/", coordinator.getFilepath, kName, Location::PACKAGE_ROOT, "");
    EXPECT_EQ_OK("foo/a/b/c/1.2/", coordinator.getFilepath, kName, Location::GEN_OUTPUT, "");
    EXPECT_EQ_OK("foo/a/b/c/V1_2/", coordinator.getFilepath, kName, Location::GEN_SANITIZED, "");
}

TEST_F(HidlGenHostTest, LocationTest) {
    Location a{{"file", 3, 4}, {"file", 3, 5}};
    Location b{{"file", 3, 6}, {"file", 3, 7}};
    Location c{{"file", 4, 4}, {"file", 4, 5}};

    Location other{{"other", 0, 0}, {"other", 0, 1}};

    EXPECT_LT(a, b);
    EXPECT_LT(b, c);
    EXPECT_LT(a, c);
    EXPECT_FALSE(Location::inSameFile(a, other));
}

void populateArgv(std::vector<const char*> options, char** argv) {
    for (int i = 0; i < options.size(); i++) {
        argv[i] = const_cast<char*>(options.at(i));
    }
}

TEST_F(HidlGenHostTest, CoordinatorRootPathTest) {
    // Test that rootPath is set correctly
    Coordinator coordinator;

    std::vector<const char*> options = {"hidl-gen", "-p", "~/"};
    char* argv[options.size()];

    populateArgv(options, argv);
    coordinator.parseOptions(options.size(), argv, "", [&](int /* res */, char* /* arg */) {
        // Coordinator should always handle -p
        FAIL() << "Coordinator should handle -p";
    });

    EXPECT_EQ("~/", coordinator.getRootPath());
}

TEST_F(HidlGenHostDeathTest, CoordinatorTooManyRootPathsTest) {
    // Test that cannot set multiple rootPaths
    Coordinator coordinator;

    std::vector<const char*> options = {"hidl-gen", "-p", "~/", "-p", "."};
    char* argv[options.size()];

    populateArgv(options, argv);
    EXPECT_DEATH(coordinator.parseOptions(options.size(), argv, "",
                                          [&](int /* res */, char* /* arg */) {
                                              // Coordinator should always handle -p
                                              FAIL() << "Coordinator should handle -p";
                                          }),
                 "ERROR: -p <root path> can only be specified once.");
}

TEST_F(HidlGenHostTest, CoordinatorNoDefaultRootTest) {
    // Test that overrides default root paths without specifying new roots
    Coordinator coordinator;

    std::vector<const char*> options = {"hidl-gen", "-R"};
    char* argv[options.size()];

    populateArgv(options, argv);
    coordinator.parseOptions(options.size(), argv, "", [&](int /* res */, char* /* arg */) {
        // Coordinator should always handle -R
        FAIL() << "Coordinator should handle -R";
    });

    // android.hardware is a default path. with -R specified it should not be set
    std::string root;
    EXPECT_NE(::android::OK,
              coordinator.getPackageRoot(FQName("android.hardware.tests.Baz", "1.0"), &root));
    EXPECT_EQ("", root);
}

TEST_F(HidlGenHostTest, CoordinatorCustomArgParseTest) {
    // Test custom args are sent to the function
    Coordinator coordinator;

    std::string optstring = "xy:";
    std::vector<const char*> options = {"hidl-gen", "-y", "yvalue", "-x"};
    char* argv[options.size()];

    populateArgv(options, argv);

    bool xCalled = false;
    bool yCalled = false;
    coordinator.parseOptions(options.size(), argv, optstring, [&](int res, char* arg) {
        switch (res) {
            case 'x': {
                EXPECT_STREQ(nullptr, arg);
                xCalled = true;
                break;
            }
            case 'y': {
                EXPECT_STREQ(argv[2] /* "yvalue" */, arg);
                yCalled = true;
                break;
            }
            default: { FAIL() << "Coordinator sent invalid param " << (char)res; }
        }
    });

    // Ensure the function was called with both x and y
    EXPECT_TRUE(xCalled);
    EXPECT_TRUE(yCalled);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

}  // namespace android