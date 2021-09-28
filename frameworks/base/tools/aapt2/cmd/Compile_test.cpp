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

#include "Compile.h"

#include "android-base/file.h"
#include "android-base/stringprintf.h"
#include "android-base/utf8.h"

#include "io/StringStream.h"
#include "io/ZipArchive.h"
#include "java/AnnotationProcessor.h"
#include "test/Test.h"

namespace aapt {

using CompilerTest = CommandTestFixture;

std::string BuildPath(std::vector<std::string> args) {
  std::string out;
  if (args.empty()) {
    return out;
  }
  out = args[0];
  for (int i = 1; i < args.size(); i++) {
    file::AppendPath(&out, args[i]);
  }
  return out;
}

int TestCompile(const std::string& path, const std::string& outDir, bool legacy,
                StdErrDiagnostics& diag) {
  std::vector<android::StringPiece> args;
  args.push_back(path);
  args.push_back("-o");
  args.push_back(outDir);
  if (legacy) {
    args.push_back("--legacy");
  }
  return CompileCommand(&diag).Execute(args, &std::cerr);
}

TEST_F(CompilerTest, MultiplePeriods) {
  StdErrDiagnostics diag;
  std::unique_ptr<IAaptContext> context = test::ContextBuilder().Build();
  const std::string kResDir = BuildPath({android::base::Dirname(android::base::GetExecutablePath()),
                                         "integration-tests", "CompileTest", "res"});

  // Resource files without periods in the file name should not throw errors
  const std::string path0 = BuildPath({kResDir, "values", "values.xml"});
  const std::string path0_out = BuildPath({kResDir, "values_values.arsc.flat"});
  ::android::base::utf8::unlink(path0_out.c_str());
  ASSERT_EQ(TestCompile(path0, kResDir, /** legacy */ false, diag), 0);
  ASSERT_EQ(::android::base::utf8::unlink(path0_out.c_str()), 0);
  ASSERT_EQ(TestCompile(path0, kResDir, /** legacy */ true, diag), 0);
  ASSERT_EQ(::android::base::utf8::unlink(path0_out.c_str()), 0);

  const std::string path1 = BuildPath({kResDir, "drawable", "image.png"});
  const std::string path1_out = BuildPath({kResDir, "drawable_image.png.flat"});
  ::android::base::utf8::unlink(path1_out.c_str());
  ASSERT_EQ(TestCompile(path1, kResDir, /** legacy */ false, diag), 0);
  ASSERT_EQ(::android::base::utf8::unlink(path1_out.c_str()), 0);
  ASSERT_EQ(TestCompile(path1, kResDir, /** legacy */ true, diag), 0);
  ASSERT_EQ(::android::base::utf8::unlink(path1_out.c_str()), 0);

  const std::string path2 = BuildPath({kResDir, "drawable", "image.9.png"});
  const std::string path2_out = BuildPath({kResDir, "drawable_image.9.png.flat"});
  ::android::base::utf8::unlink(path2_out.c_str());
  ASSERT_EQ(TestCompile(path2, kResDir, /** legacy */ false, diag), 0);
  ASSERT_EQ(::android::base::utf8::unlink(path2_out.c_str()), 0);
  ASSERT_EQ(TestCompile(path2, kResDir, /** legacy */ true, diag), 0);
  ASSERT_EQ(::android::base::utf8::unlink(path2_out.c_str()), 0);

  // Resource files with periods in the file name should fail on non-legacy compilations
  const std::string path3 = BuildPath({kResDir, "values", "values.all.xml"});
  const std::string path3_out = BuildPath({kResDir, "values_values.all.arsc.flat"});
  ::android::base::utf8::unlink(path3_out.c_str());
  ASSERT_NE(TestCompile(path3, kResDir, /** legacy */ false, diag), 0);
  ASSERT_NE(::android::base::utf8::unlink(path3_out.c_str()), 0);
  ASSERT_EQ(TestCompile(path3, kResDir, /** legacy */ true, diag), 0);
  ASSERT_EQ(::android::base::utf8::unlink(path3_out.c_str()), 0);

  const std::string path4 = BuildPath({kResDir, "drawable", "image.small.png"});
  const std::string path4_out = BuildPath({kResDir, "drawable_image.small.png.flat"});
  ::android::base::utf8::unlink(path4_out.c_str());
  ASSERT_NE(TestCompile(path4, kResDir, /** legacy */ false, diag), 0);
  ASSERT_NE(::android::base::utf8::unlink(path4_out.c_str()), 0);
  ASSERT_EQ(TestCompile(path4, kResDir, /** legacy */ true, diag), 0);
  ASSERT_EQ(::android::base::utf8::unlink(path4_out.c_str()), 0);

  const std::string path5 = BuildPath({kResDir, "drawable", "image.small.9.png"});
  const std::string path5_out = BuildPath({kResDir, "drawable_image.small.9.png.flat"});
  ::android::base::utf8::unlink(path5_out.c_str());
  ASSERT_NE(TestCompile(path5, kResDir, /** legacy */ false, diag), 0);
  ASSERT_NE(::android::base::utf8::unlink(path5_out.c_str()), 0);
  ASSERT_EQ(TestCompile(path5, kResDir, /** legacy */ true, diag), 0);
  ASSERT_EQ(::android::base::utf8::unlink(path5_out.c_str()), 0);
}

TEST_F(CompilerTest, DirInput) {
  StdErrDiagnostics diag;
  std::unique_ptr<IAaptContext> context = test::ContextBuilder().Build();
  const std::string kResDir = BuildPath({android::base::Dirname(android::base::GetExecutablePath()),
                                         "integration-tests", "CompileTest", "DirInput", "res"});
  const std::string kOutputFlata =
      BuildPath({android::base::Dirname(android::base::GetExecutablePath()), "integration-tests",
                 "CompileTest", "DirInput", "compiled.flata"});
  ::android::base::utf8::unlink(kOutputFlata.c_str());

  std::vector<android::StringPiece> args;
  args.push_back("--dir");
  args.push_back(kResDir);
  args.push_back("-o");
  args.push_back(kOutputFlata);
  args.push_back("-v");
  ASSERT_EQ(CompileCommand(&diag).Execute(args, &std::cerr), 0);

  {
    // Check for the presence of the compiled files
    std::string err;
    std::unique_ptr<io::ZipFileCollection> zip = io::ZipFileCollection::Create(kOutputFlata, &err);
    ASSERT_NE(zip, nullptr) << err;
    ASSERT_NE(zip->FindFile("drawable_image.png.flat"), nullptr);
    ASSERT_NE(zip->FindFile("layout_layout.xml.flat"), nullptr);
    ASSERT_NE(zip->FindFile("values_values.arsc.flat"), nullptr);
  }
  ASSERT_EQ(::android::base::utf8::unlink(kOutputFlata.c_str()), 0);
}

TEST_F(CompilerTest, ZipInput) {
  StdErrDiagnostics diag;
  std::unique_ptr<IAaptContext> context = test::ContextBuilder().Build();
  const std::string kResZip =
      BuildPath({android::base::Dirname(android::base::GetExecutablePath()), "integration-tests",
                 "CompileTest", "ZipInput", "res.zip"});
  const std::string kOutputFlata =
      BuildPath({android::base::Dirname(android::base::GetExecutablePath()), "integration-tests",
                 "CompileTest", "ZipInput", "compiled.flata"});

  ::android::base::utf8::unlink(kOutputFlata.c_str());

  std::vector<android::StringPiece> args;
  args.push_back("--zip");
  args.push_back(kResZip);
  args.push_back("-o");
  args.push_back(kOutputFlata);
  ASSERT_EQ(CompileCommand(&diag).Execute(args, &std::cerr), 0);

  {
    // Check for the presence of the compiled files
    std::string err;
    std::unique_ptr<io::ZipFileCollection> zip = io::ZipFileCollection::Create(kOutputFlata, &err);
    ASSERT_NE(zip, nullptr) << err;
    ASSERT_NE(zip->FindFile("drawable_image.png.flat"), nullptr);
    ASSERT_NE(zip->FindFile("layout_layout.xml.flat"), nullptr);
    ASSERT_NE(zip->FindFile("values_values.arsc.flat"), nullptr);
  }
  ASSERT_EQ(::android::base::utf8::unlink(kOutputFlata.c_str()), 0);
}

/*
 * This tests the "protection" from pseudo-translation of
 * non-translatable files (starting with 'donotranslate')
 * and strings (with the translatable="false" attribute)
 *
 * We check 4 string files, 2 translatable, and 2 not (based on file name)
 * Each file contains 2 strings, one translatable, one not (attribute based)
 * Each of these files are compiled and linked into one .apk, then we load the
 * strings from the apk and check if there are pseudo-translated strings.
 */

// Using 000 and 111 because they are not changed by pseudo-translation,
// making our life easier.
constexpr static const char sTranslatableXmlContent[] =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<resources>"
    "  <string name=\"normal\">000</string>"
    "  <string name=\"non_translatable\" translatable=\"false\">111</string>"
    "</resources>";

static void AssertTranslations(CommandTestFixture *ctf, std::string file_name,
    std::vector<std::string> expected) {

  StdErrDiagnostics diag;

  const std::string source_file = ctf->GetTestPath("/res/values/" + file_name + ".xml");
  const std::string compiled_files_dir = ctf->GetTestPath("/compiled_" + file_name);
  const std::string out_apk = ctf->GetTestPath("/" + file_name + ".apk");

  ctf->WriteFile(source_file, sTranslatableXmlContent);
  CHECK(file::mkdirs(compiled_files_dir.data()));

  ASSERT_EQ(CompileCommand(&diag).Execute({
      source_file,
      "-o", compiled_files_dir,
      "-v",
      "--pseudo-localize"
  }, &std::cerr), 0);

  ASSERT_TRUE(ctf->Link({
      "--manifest", ctf->GetDefaultManifest(),
      "-o", out_apk
  }, compiled_files_dir, &diag));

  std::unique_ptr<LoadedApk> apk = LoadedApk::LoadApkFromPath(out_apk, &diag);

  ResourceTable* table = apk->GetResourceTable();
  ASSERT_NE(table, nullptr);
  table->string_pool.Sort();

  const std::vector<std::unique_ptr<StringPool::Entry>>& pool_strings =
      table->string_pool.strings();

  // The actual / expected vectors have the same size
  const size_t pool_size = pool_strings.size();
  ASSERT_EQ(pool_size, expected.size());

  for (size_t i = 0; i < pool_size; i++) {
    std::string actual = pool_strings[i]->value;
    ASSERT_EQ(actual, expected[i]);
  }
}

TEST_F(CompilerTest, DoNotTranslateTest) {
  // The first string (000) is translatable, the second is not
  // ar-XB uses "\u200F\u202E...\u202C\u200F"
  std::vector<std::string> expected_translatable = {
      "000", "111", // default locale
      "[000 one]", // en-XA
      "\xE2\x80\x8F\xE2\x80\xAE" "000" "\xE2\x80\xAC\xE2\x80\x8F", // ar-XB
  };
  AssertTranslations(this, "foo", expected_translatable);
  AssertTranslations(this, "foo_donottranslate", expected_translatable);

  // No translatable strings because these are non-translatable files
  std::vector<std::string> expected_not_translatable = {
      "000", "111", // default locale
  };
  AssertTranslations(this, "donottranslate", expected_not_translatable);
  AssertTranslations(this, "donottranslate_foo", expected_not_translatable);
}

}  // namespace aapt
