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

#include "AppInfo.h"
#include "Link.h"

#include "LoadedApk.h"
#include "test/Test.h"

using testing::Eq;
using testing::Ne;

namespace aapt {

using LinkTest = CommandTestFixture;

TEST_F(LinkTest, RemoveRawXmlStrings) {
  StdErrDiagnostics diag;
  const std::string compiled_files_dir = GetTestPath("compiled");
  ASSERT_TRUE(CompileFile(GetTestPath("res/xml/test.xml"), R"(<Item AgentCode="007"/>)",
                          compiled_files_dir, &diag));

  const std::string out_apk = GetTestPath("out.apk");
  std::vector<std::string> link_args = {
      "--manifest", GetDefaultManifest(),
      "-o", out_apk,
  };

  ASSERT_TRUE(Link(link_args, compiled_files_dir, &diag));

  // Load the binary xml tree
  android::ResXMLTree tree;
  std::unique_ptr<LoadedApk> apk = LoadedApk::LoadApkFromPath(out_apk, &diag);
  std::unique_ptr<io::IData> data = OpenFileAsData(apk.get(), "res/xml/test.xml");
  ASSERT_THAT(data, Ne(nullptr));
  AssertLoadXml(apk.get(), data.get(), &tree);

  // Check that the raw string index has not been assigned
  EXPECT_THAT(tree.getAttributeValueStringID(0), Eq(-1));
}

TEST_F(LinkTest, KeepRawXmlStrings) {
  StdErrDiagnostics diag;
  const std::string compiled_files_dir = GetTestPath("compiled");
  ASSERT_TRUE(CompileFile(GetTestPath("res/xml/test.xml"), R"(<Item AgentCode="007"/>)",
                          compiled_files_dir, &diag));

  const std::string out_apk = GetTestPath("out.apk");
  std::vector<std::string> link_args = {
      "--manifest", GetDefaultManifest(),
      "-o", out_apk,
      "--keep-raw-values"
  };

  ASSERT_TRUE(Link(link_args, compiled_files_dir, &diag));

  // Load the binary xml tree
  android::ResXMLTree tree;
  std::unique_ptr<LoadedApk> apk = LoadedApk::LoadApkFromPath(out_apk, &diag);
  std::unique_ptr<io::IData> data = OpenFileAsData(apk.get(), "res/xml/test.xml");
  ASSERT_THAT(data, Ne(nullptr));
  AssertLoadXml(apk.get(), data.get(), &tree);

  // Check that the raw string index has been set to the correct string pool entry
  int32_t raw_index = tree.getAttributeValueStringID(0);
  ASSERT_THAT(raw_index, Ne(-1));
  EXPECT_THAT(util::GetString(tree.getStrings(), static_cast<size_t>(raw_index)), Eq("007"));
}

TEST_F(LinkTest, NoCompressAssets) {
  StdErrDiagnostics diag;
  std::string content(500, 'a');
  WriteFile(GetTestPath("assets/testtxt"), content);
  WriteFile(GetTestPath("assets/testtxt2"), content);
  WriteFile(GetTestPath("assets/test.txt"), content);
  WriteFile(GetTestPath("assets/test.hello.txt"), content);
  WriteFile(GetTestPath("assets/test.hello.xml"), content);

  const std::string out_apk = GetTestPath("out.apk");
  std::vector<std::string> link_args = {
      "--manifest", GetDefaultManifest(),
      "-o", out_apk,
      "-0", ".txt",
      "-0", "txt2",
      "-0", ".hello.txt",
      "-0", "hello.xml",
      "-A", GetTestPath("assets")
  };

  ASSERT_TRUE(Link(link_args, &diag));

  std::unique_ptr<LoadedApk> apk = LoadedApk::LoadApkFromPath(out_apk, &diag);
  ASSERT_THAT(apk, Ne(nullptr));
  io::IFileCollection* zip = apk->GetFileCollection();
  ASSERT_THAT(zip, Ne(nullptr));

  auto file = zip->FindFile("assets/testtxt");
  ASSERT_THAT(file, Ne(nullptr));
  EXPECT_TRUE(file->WasCompressed());

  file = zip->FindFile("assets/testtxt2");
  ASSERT_THAT(file, Ne(nullptr));
  EXPECT_FALSE(file->WasCompressed());

  file = zip->FindFile("assets/test.txt");
  ASSERT_THAT(file, Ne(nullptr));
  EXPECT_FALSE(file->WasCompressed());

  file = zip->FindFile("assets/test.hello.txt");
  ASSERT_THAT(file, Ne(nullptr));
  EXPECT_FALSE(file->WasCompressed());

  file = zip->FindFile("assets/test.hello.xml");
  ASSERT_THAT(file, Ne(nullptr));
  EXPECT_FALSE(file->WasCompressed());
}

TEST_F(LinkTest, NoCompressResources) {
  StdErrDiagnostics diag;
  std::string content(500, 'a');
  const std::string compiled_files_dir = GetTestPath("compiled");
  ASSERT_TRUE(CompileFile(GetTestPath("res/raw/testtxt"), content, compiled_files_dir, &diag));
  ASSERT_TRUE(CompileFile(GetTestPath("res/raw/test.txt"), content, compiled_files_dir, &diag));
  ASSERT_TRUE(CompileFile(GetTestPath("res/raw/test1.hello.txt"), content, compiled_files_dir,
              &diag));
  ASSERT_TRUE(CompileFile(GetTestPath("res/raw/test2.goodbye.xml"), content, compiled_files_dir,
              &diag));

  const std::string out_apk = GetTestPath("out.apk");
  std::vector<std::string> link_args = {
      "--manifest", GetDefaultManifest(),
      "-o", out_apk,
      "-0", ".txt",
      "-0", ".hello.txt",
      "-0", "goodbye.xml",
  };

  ASSERT_TRUE(Link(link_args, compiled_files_dir, &diag));

  std::unique_ptr<LoadedApk> apk = LoadedApk::LoadApkFromPath(out_apk, &diag);
  ASSERT_THAT(apk, Ne(nullptr));
  io::IFileCollection* zip = apk->GetFileCollection();
  ASSERT_THAT(zip, Ne(nullptr));

  auto file = zip->FindFile("res/raw/testtxt");
  ASSERT_THAT(file, Ne(nullptr));
  EXPECT_TRUE(file->WasCompressed());

  file = zip->FindFile("res/raw/test.txt");
  ASSERT_THAT(file, Ne(nullptr));
  EXPECT_FALSE(file->WasCompressed());

  file = zip->FindFile("res/raw/test1.hello.hello.txt");
  ASSERT_THAT(file, Ne(nullptr));
  EXPECT_FALSE(file->WasCompressed());

  file = zip->FindFile("res/raw/test2.goodbye.goodbye.xml");
  ASSERT_THAT(file, Ne(nullptr));
  EXPECT_FALSE(file->WasCompressed());
}

TEST_F(LinkTest, OverlayStyles) {
  StdErrDiagnostics diag;
  const std::string compiled_files_dir = GetTestPath("compiled");
  const std::string override_files_dir = GetTestPath("compiled-override");
  ASSERT_TRUE(CompileFile(GetTestPath("res/values/values.xml"),
                          R"(<resources>
                               <style name="MyStyle">
                                 <item name="android:textColor">#123</item>
                               </style>
                             </resources>)",
                          compiled_files_dir, &diag));
  ASSERT_TRUE(CompileFile(GetTestPath("res/values/values-override.xml"),
                          R"(<resources>
                               <style name="MyStyle">
                                 <item name="android:background">#456</item>
                               </style>
                             </resources>)",
                          override_files_dir, &diag));


  const std::string out_apk = GetTestPath("out.apk");
  std::vector<std::string> link_args = {
      "--manifest", GetDefaultManifest(kDefaultPackageName),
      "-o", out_apk,
  };
  const auto override_files = file::FindFiles(override_files_dir, &diag);
  for (const auto &override_file : override_files.value()) {
      link_args.push_back("-R");
      link_args.push_back(file::BuildPath({override_files_dir, override_file}));
  }
  ASSERT_TRUE(Link(link_args, compiled_files_dir, &diag));

  std::unique_ptr<LoadedApk> apk = LoadedApk::LoadApkFromPath(out_apk, &diag);
  const Style* actual_style = test::GetValue<Style>(
      apk->GetResourceTable(), std::string(kDefaultPackageName) + ":style/MyStyle");
  ASSERT_NE(actual_style, nullptr);
  ASSERT_EQ(actual_style->entries.size(), 2);
  EXPECT_EQ(actual_style->entries[0].key.id, 0x01010098);  // android:textColor
  EXPECT_EQ(actual_style->entries[1].key.id, 0x010100d4);  // android:background
}

TEST_F(LinkTest, OverrideStylesInsteadOfOverlaying) {
  StdErrDiagnostics diag;
  const std::string compiled_files_dir = GetTestPath("compiled");
  const std::string override_files_dir = GetTestPath("compiled-override");
  ASSERT_TRUE(CompileFile(GetTestPath("res/values/values.xml"),
                          R"(<resources>
                               <style name="MyStyle">
                                 <item name="android:textColor">#123</item>
                               </style>
                             </resources>)",
                          compiled_files_dir, &diag));
  ASSERT_TRUE(CompileFile(GetTestPath("res/values/values-override.xml"),
                          R"(<resources>
                               <style name="MyStyle">
                                 <item name="android:background">#456</item>
                               </style>
                             </resources>)",
                          override_files_dir, &diag));


  const std::string out_apk = GetTestPath("out.apk");
  std::vector<std::string> link_args = {
      "--manifest", GetDefaultManifest(kDefaultPackageName),
      "--override-styles-instead-of-overlaying",
      "-o", out_apk,
  };
  const auto override_files = file::FindFiles(override_files_dir, &diag);
  for (const auto &override_file : override_files.value()) {
      link_args.push_back("-R");
      link_args.push_back(file::BuildPath({override_files_dir, override_file}));
  }
  ASSERT_TRUE(Link(link_args, compiled_files_dir, &diag));

  std::unique_ptr<LoadedApk> apk = LoadedApk::LoadApkFromPath(out_apk, &diag);
  const Style* actual_style = test::GetValue<Style>(
      apk->GetResourceTable(), std::string(kDefaultPackageName) + ":style/MyStyle");
  ASSERT_NE(actual_style, nullptr);
  ASSERT_EQ(actual_style->entries.size(), 1);
  EXPECT_EQ(actual_style->entries[0].key.id, 0x010100d4);  // android:background
}

TEST_F(LinkTest, AppInfoWithUsesSplit) {
  StdErrDiagnostics diag;
  const std::string base_files_dir = GetTestPath("base");
  ASSERT_TRUE(CompileFile(GetTestPath("res/values/values.xml"),
                          R"(<resources>
                               <string name="bar">bar</string>
                             </resources>)",
                          base_files_dir, &diag));
  const std::string base_apk = GetTestPath("base.apk");
  std::vector<std::string> link_args = {
      "--manifest", GetDefaultManifest("com.aapt2.app"),
      "-o", base_apk,
  };
  ASSERT_TRUE(Link(link_args, base_files_dir, &diag));

  const std::string feature_manifest = GetTestPath("feature_manifest.xml");
  WriteFile(feature_manifest, android::base::StringPrintf(R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="com.aapt2.app" split="feature1">
      </manifest>)"));
  const std::string feature_files_dir = GetTestPath("feature");
  ASSERT_TRUE(CompileFile(GetTestPath("res/values/values.xml"),
                          R"(<resources>
                               <string name="foo">foo</string>
                             </resources>)",
                          feature_files_dir, &diag));
  const std::string feature_apk = GetTestPath("feature.apk");
  const std::string feature_package_id = "0x80";
  link_args = {
      "--manifest", feature_manifest,
      "-I", base_apk,
      "--package-id", feature_package_id,
      "-o", feature_apk,
  };
  ASSERT_TRUE(Link(link_args, feature_files_dir, &diag));

  const std::string feature2_manifest = GetTestPath("feature2_manifest.xml");
  WriteFile(feature2_manifest, android::base::StringPrintf(R"(
        <manifest xmlns:android="http://schemas.android.com/apk/res/android"
            package="com.aapt2.app" split="feature2">
          <uses-split android:name="feature1"/>
        </manifest>)"));
  const std::string feature2_files_dir = GetTestPath("feature2");
  ASSERT_TRUE(CompileFile(GetTestPath("res/values/values.xml"),
                          R"(<resources>
                               <string-array name="string_array">
                                 <item>@string/bar</item>
                                 <item>@string/foo</item>
                               </string-array>
                             </resources>)",
                          feature2_files_dir, &diag));
  const std::string feature2_apk = GetTestPath("feature2.apk");
  const std::string feature2_package_id = "0x81";
  link_args = {
      "--manifest", feature2_manifest,
      "-I", base_apk,
      "-I", feature_apk,
      "--package-id", feature2_package_id,
      "-o", feature2_apk,
  };
  ASSERT_TRUE(Link(link_args, feature2_files_dir, &diag));
}

}  // namespace aapt
