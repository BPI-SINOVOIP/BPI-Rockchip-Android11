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

#include "class_loader_context.h"

#include <gtest/gtest.h>

#include "android-base/strings.h"
#include "art_field-inl.h"
#include "base/dchecked_vector.h"
#include "base/stl_util.h"
#include "class_linker.h"
#include "class_root.h"
#include "common_runtime_test.h"
#include "dex/dex_file.h"
#include "handle_scope-inl.h"
#include "jni/jni_internal.h"
#include "mirror/class.h"
#include "mirror/class_loader-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-alloc-inl.h"
#include "oat_file_assistant.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"
#include "well_known_classes.h"

namespace art {

class ClassLoaderContextTest : public CommonRuntimeTest {
 public:
  void VerifyContextSize(ClassLoaderContext* context, size_t expected_size) {
    ASSERT_TRUE(context != nullptr);
    ASSERT_EQ(expected_size, context->GetParentChainSize());
  }

  void VerifyClassLoaderPCL(ClassLoaderContext* context,
                            size_t index,
                            const std::string& classpath) {
    VerifyClassLoaderInfo(
        context, index, ClassLoaderContext::kPathClassLoader, classpath);
  }

  void VerifyClassLoaderDLC(ClassLoaderContext* context,
                            size_t index,
                            const std::string& classpath) {
    VerifyClassLoaderInfo(
        context, index, ClassLoaderContext::kDelegateLastClassLoader, classpath);
  }

  void VerifyClassLoaderIMC(ClassLoaderContext* context,
                            size_t index,
                            const std::string& classpath) {
    VerifyClassLoaderInfo(
        context, index, ClassLoaderContext::kInMemoryDexClassLoader, classpath);
  }

  void VerifyClassLoaderSharedLibraryPCL(ClassLoaderContext* context,
                                         size_t loader_index,
                                         size_t shared_library_index,
                                         const std::string& classpath) {
    VerifyClassLoaderInfoSL(
        context, loader_index, shared_library_index, ClassLoaderContext::kPathClassLoader,
        classpath);
  }

  void VerifyClassLoaderSharedLibraryIMC(ClassLoaderContext* context,
                                         size_t loader_index,
                                         size_t shared_library_index,
                                         const std::string& classpath) {
    VerifyClassLoaderInfoSL(
        context, loader_index, shared_library_index, ClassLoaderContext::kInMemoryDexClassLoader,
        classpath);
  }

  void VerifySharedLibrariesSize(ClassLoaderContext* context,
                                 size_t loader_index,
                                 size_t expected_size) {
    ASSERT_TRUE(context != nullptr);
    ASSERT_GT(context->GetParentChainSize(), loader_index);
    const ClassLoaderContext::ClassLoaderInfo& info = *context->GetParent(loader_index);
    ASSERT_EQ(info.shared_libraries.size(), expected_size);
  }

  void VerifyClassLoaderSharedLibraryDLC(ClassLoaderContext* context,
                                         size_t loader_index,
                                         size_t shared_library_index,
                                         const std::string& classpath) {
    VerifyClassLoaderInfoSL(
        context, loader_index, shared_library_index, ClassLoaderContext::kDelegateLastClassLoader,
        classpath);
  }

  void VerifyClassLoaderPCLFromTestDex(ClassLoaderContext* context,
                                       size_t index,
                                       const std::string& test_name) {
    VerifyClassLoaderFromTestDex(
        context, index, ClassLoaderContext::kPathClassLoader, test_name);
  }

  void VerifyClassLoaderDLCFromTestDex(ClassLoaderContext* context,
                                       size_t index,
                                       const std::string& test_name) {
    VerifyClassLoaderFromTestDex(
        context, index, ClassLoaderContext::kDelegateLastClassLoader, test_name);
  }

  void VerifyClassLoaderIMCFromTestDex(ClassLoaderContext* context,
                                       size_t index,
                                       const std::string& test_name) {
    VerifyClassLoaderFromTestDex(
        context, index, ClassLoaderContext::kInMemoryDexClassLoader, test_name, "<unknown>");
  }

  enum class LocationCheck {
    kEquals,
    kEndsWith
  };
  enum class BaseLocationCheck {
    kEquals,
    kEndsWith
  };

  static bool IsAbsoluteLocation(const std::string& location) {
    return !location.empty() && location[0] == '/';
  }

  void VerifyOpenDexFiles(
      ClassLoaderContext* context,
      size_t index,
      std::vector<std::unique_ptr<const DexFile>>* all_dex_files,
      bool classpath_matches_dex_location = true) {
    ASSERT_TRUE(context != nullptr);
    ASSERT_TRUE(context->dex_files_open_attempted_);
    ASSERT_TRUE(context->dex_files_open_result_);
    ClassLoaderContext::ClassLoaderInfo& info = *context->GetParent(index);
    ASSERT_EQ(all_dex_files->size(), info.classpath.size());
    ASSERT_EQ(all_dex_files->size(), info.opened_dex_files.size());
    size_t cur_open_dex_index = 0;
    for (size_t k = 0; k < all_dex_files->size(); k++) {
      std::unique_ptr<const DexFile>& opened_dex_file =
            info.opened_dex_files[cur_open_dex_index++];
      std::unique_ptr<const DexFile>& expected_dex_file = (*all_dex_files)[k];

      std::string expected_location = expected_dex_file->GetLocation();

      const std::string& opened_location = opened_dex_file->GetLocation();
      if (!IsAbsoluteLocation(opened_location)) {
        // If the opened location is relative (it was open from a relative path without a
        // classpath_dir) it might not match the expected location which is absolute in tests).
        // So we compare the endings (the checksum will validate it's actually the same file).
        ASSERT_EQ(0, expected_location.compare(
            expected_location.length() - opened_location.length(),
            opened_location.length(),
            opened_location));
      } else {
        ASSERT_EQ(expected_location, opened_location);
      }
      ASSERT_EQ(expected_dex_file->GetLocationChecksum(), opened_dex_file->GetLocationChecksum());
      if (classpath_matches_dex_location) {
        ASSERT_EQ(info.classpath[k], opened_location);
      }
    }
  }

  std::unique_ptr<ClassLoaderContext> CreateContextForClassLoader(jobject class_loader) {
    return ClassLoaderContext::CreateContextForClassLoader(class_loader, nullptr);
  }

  std::unique_ptr<ClassLoaderContext> ParseContextWithChecksums(const std::string& context_spec) {
    std::unique_ptr<ClassLoaderContext> context(new ClassLoaderContext());
    if (!context->Parse(context_spec, /*parse_checksums=*/ true)) {
      return nullptr;
    }
    return context;
  }

  void VerifyContextForClassLoader(ClassLoaderContext* context) {
    ASSERT_TRUE(context != nullptr);
    ASSERT_TRUE(context->dex_files_open_attempted_);
    ASSERT_TRUE(context->dex_files_open_result_);
    ASSERT_FALSE(context->owns_the_dex_files_);
    ASSERT_FALSE(context->special_shared_library_);
  }

  void VerifyClassLoaderDexFiles(ScopedObjectAccess& soa,
                                 Handle<mirror::ClassLoader> class_loader,
                                 jclass type,
                                 std::vector<const DexFile*>& expected_dex_files)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ASSERT_TRUE(class_loader->GetClass() == soa.Decode<mirror::Class>(type));

    std::vector<const DexFile*> class_loader_dex_files = GetDexFiles(soa, class_loader);
    ASSERT_EQ(expected_dex_files.size(), class_loader_dex_files.size());

    for (size_t i = 0; i < expected_dex_files.size(); i++) {
      ASSERT_EQ(expected_dex_files[i]->GetLocation(),
                class_loader_dex_files[i]->GetLocation());
      ASSERT_EQ(expected_dex_files[i]->GetLocationChecksum(),
                class_loader_dex_files[i]->GetLocationChecksum());
    }
  }

  void PretendContextOpenedDexFiles(ClassLoaderContext* context) {
    context->dex_files_open_attempted_ = true;
    context->dex_files_open_result_ = true;
  }

 private:
  void VerifyClassLoaderInfo(ClassLoaderContext* context,
                             size_t index,
                             ClassLoaderContext::ClassLoaderType type,
                             const std::string& classpath) {
    ASSERT_TRUE(context != nullptr);
    ASSERT_GT(context->GetParentChainSize(), index);
    ClassLoaderContext::ClassLoaderInfo& info = *context->GetParent(index);
    ASSERT_EQ(type, info.type);
    std::vector<std::string> expected_classpath;
    Split(classpath, ':', &expected_classpath);
    ASSERT_EQ(expected_classpath, info.classpath);
  }

  void VerifyClassLoaderInfoSL(ClassLoaderContext* context,
                               size_t loader_index,
                               size_t shared_library_index,
                               ClassLoaderContext::ClassLoaderType type,
                               const std::string& classpath) {
    ASSERT_TRUE(context != nullptr);
    ASSERT_GT(context->GetParentChainSize(), loader_index);
    const ClassLoaderContext::ClassLoaderInfo& info = *context->GetParent(loader_index);
    ASSERT_GT(info.shared_libraries.size(), shared_library_index);
    const ClassLoaderContext::ClassLoaderInfo& sl =
        *info.shared_libraries[shared_library_index].get();
    ASSERT_EQ(type, info.type);
    std::vector<std::string> expected_classpath;
    Split(classpath, ':', &expected_classpath);
    ASSERT_EQ(expected_classpath, sl.classpath);
  }

  void VerifyClassLoaderFromTestDex(ClassLoaderContext* context,
                                    size_t index,
                                    ClassLoaderContext::ClassLoaderType type,
                                    const std::string& test_name,
                                    const std::string& classpath = "") {
    std::vector<std::unique_ptr<const DexFile>> dex_files = OpenTestDexFiles(test_name.c_str());

    // If `classpath` is set, override the expected value of ClassLoaderInfo::classpath.
    // Otherwise assume it is equal to dex location (here test dex file name).
    VerifyClassLoaderInfo(context,
                          index,
                          type,
                          classpath.empty() ? GetTestDexFileName(test_name.c_str()) : classpath);
    VerifyOpenDexFiles(context,
                       index,
                       &dex_files,
                       /* classpath_matches_dex_location= */ classpath.empty());
  }
};

TEST_F(ClassLoaderContextTest, ParseValidEmptyContext) {
  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create("");
  // An empty context should create a single empty PathClassLoader.
  VerifyContextSize(context.get(), 1);
  VerifyClassLoaderPCL(context.get(), 0, "");
}

TEST_F(ClassLoaderContextTest, ParseValidSharedLibraryContext) {
  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create("&");
  // An shared library context should have no class loader in the chain.
  VerifyContextSize(context.get(), 0);
}

TEST_F(ClassLoaderContextTest, ParseValidContextPCL) {
  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create("PCL[a.dex]");
  VerifyContextSize(context.get(), 1);
  VerifyClassLoaderPCL(context.get(), 0, "a.dex");
}

TEST_F(ClassLoaderContextTest, ParseValidContextDLC) {
  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create("DLC[a.dex]");
  VerifyContextSize(context.get(), 1);
  VerifyClassLoaderDLC(context.get(), 0, "a.dex");
}

TEST_F(ClassLoaderContextTest, ParseValidContextIMC) {
  std::unique_ptr<ClassLoaderContext> context = ParseContextWithChecksums("IMC[<unknown>*111]");
  ASSERT_FALSE(context == nullptr);
}

TEST_F(ClassLoaderContextTest, ParseInvalidContextIMCNoChecksum) {
  // IMC is treated as an unknown class loader unless a checksum is provided.
  // This is because the dex location is always bogus.
  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create("IMC[<unknown>]");
  ASSERT_TRUE(context == nullptr);
}

TEST_F(ClassLoaderContextTest, ParseInvalidContextIMCWrongClasspathMagic) {
  // IMC does not support arbitrary dex location. A magic marker must be used
  // otherwise the spec should be rejected.
  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create("IMC[a.dex*111]");
  ASSERT_TRUE(context == nullptr);
}

TEST_F(ClassLoaderContextTest, ParseValidContextChain) {
  std::unique_ptr<ClassLoaderContext> context =
      ClassLoaderContext::Create("PCL[a.dex:b.dex];DLC[c.dex:d.dex];PCL[e.dex]");
  VerifyContextSize(context.get(), 3);
  VerifyClassLoaderPCL(context.get(), 0, "a.dex:b.dex");
  VerifyClassLoaderDLC(context.get(), 1, "c.dex:d.dex");
  VerifyClassLoaderPCL(context.get(), 2, "e.dex");
}

TEST_F(ClassLoaderContextTest, ParseSharedLibraries) {
  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create(
      "PCL[a.dex:b.dex]{PCL[s1.dex]#PCL[s2.dex:s3.dex]};DLC[c.dex:d.dex]{DLC[s4.dex]}");
  VerifyContextSize(context.get(), 2);
  VerifyClassLoaderSharedLibraryPCL(context.get(), 0, 0, "s1.dex");
  VerifyClassLoaderSharedLibraryPCL(context.get(), 0, 1, "s2.dex:s3.dex");
  VerifyClassLoaderDLC(context.get(), 1, "c.dex:d.dex");
  VerifyClassLoaderSharedLibraryDLC(context.get(), 1, 0, "s4.dex");
}

TEST_F(ClassLoaderContextTest, ParseEnclosingSharedLibraries) {
  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create(
      "PCL[a.dex:b.dex]{PCL[s1.dex]{PCL[s2.dex:s3.dex];PCL[s4.dex]}}");
  VerifyContextSize(context.get(), 1);
  VerifyClassLoaderSharedLibraryPCL(context.get(), 0, 0, "s1.dex");
}

TEST_F(ClassLoaderContextTest, ParseComplexSharedLibraries1) {
  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create(
      "PCL[]{PCL[s4.dex]{PCL[s5.dex]{PCL[s6.dex]}#PCL[s6.dex]}}");
  VerifyContextSize(context.get(), 1);
  VerifyClassLoaderSharedLibraryPCL(context.get(), 0, 0, "s4.dex");
}

TEST_F(ClassLoaderContextTest, ParseComplexSharedLibraries2) {
  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create(
      "PCL[]{PCL[s1.dex]{PCL[s2.dex]}#PCL[s2.dex]#"
      "PCL[s3.dex]#PCL[s4.dex]{PCL[s5.dex]{PCL[s6.dex]}#PCL[s6.dex]}#PCL[s5.dex]{PCL[s6.dex]}}");
  VerifyContextSize(context.get(), 1);
  VerifyClassLoaderSharedLibraryPCL(context.get(), 0, 0, "s1.dex");
  VerifyClassLoaderSharedLibraryPCL(context.get(), 0, 1, "s2.dex");
  VerifyClassLoaderSharedLibraryPCL(context.get(), 0, 2, "s3.dex");
  VerifyClassLoaderSharedLibraryPCL(context.get(), 0, 3, "s4.dex");
  VerifyClassLoaderSharedLibraryPCL(context.get(), 0, 4, "s5.dex");
}

TEST_F(ClassLoaderContextTest, ParseValidEmptyContextDLC) {
  std::unique_ptr<ClassLoaderContext> context =
      ClassLoaderContext::Create("DLC[]");
  VerifyContextSize(context.get(), 1);
  VerifyClassLoaderDLC(context.get(), 0, "");
}

TEST_F(ClassLoaderContextTest, ParseValidEmptyContextSharedLibrary) {
  std::unique_ptr<ClassLoaderContext> context =
      ClassLoaderContext::Create("DLC[]{}");
  VerifyContextSize(context.get(), 1);
  VerifySharedLibrariesSize(context.get(), 0, 0);
}

TEST_F(ClassLoaderContextTest, ParseValidContextSpecialSymbol) {
  std::unique_ptr<ClassLoaderContext> context =
    ClassLoaderContext::Create(OatFile::kSpecialSharedLibrary);
  VerifyContextSize(context.get(), 0);
}

TEST_F(ClassLoaderContextTest, ParseInvalidValidContexts) {
  ASSERT_TRUE(nullptr == ClassLoaderContext::Create("ABC[a.dex]"));
  ASSERT_TRUE(nullptr == ClassLoaderContext::Create("PCL"));
  ASSERT_TRUE(nullptr == ClassLoaderContext::Create("PCL[a.dex"));
  ASSERT_TRUE(nullptr == ClassLoaderContext::Create("PCLa.dex]"));
  ASSERT_TRUE(nullptr == ClassLoaderContext::Create("PCL{a.dex}"));
  ASSERT_TRUE(nullptr == ClassLoaderContext::Create("PCL[a.dex];DLC[b.dex"));
  ASSERT_TRUE(nullptr == ClassLoaderContext::Create("PCL[a.dex]{ABC};DLC[b.dex"));
  ASSERT_TRUE(nullptr == ClassLoaderContext::Create("PCL[a.dex]{};DLC[b.dex"));
  ASSERT_TRUE(nullptr == ClassLoaderContext::Create("DLC[s4.dex]}"));
  ASSERT_TRUE(nullptr == ClassLoaderContext::Create("DLC[s4.dex]{"));
  ASSERT_TRUE(nullptr == ClassLoaderContext::Create("DLC{DLC[s4.dex]}"));
  ASSERT_TRUE(nullptr == ClassLoaderContext::Create("PCL{##}"));
  ASSERT_TRUE(nullptr == ClassLoaderContext::Create("PCL{PCL[s4.dex]#}"));
  ASSERT_TRUE(nullptr == ClassLoaderContext::Create("PCL{PCL[s4.dex]##}"));
  ASSERT_TRUE(nullptr == ClassLoaderContext::Create("PCL{PCL[s4.dex]{PCL[s3.dex]}#}"));
}

TEST_F(ClassLoaderContextTest, OpenInvalidDexFiles) {
  std::unique_ptr<ClassLoaderContext> context =
      ClassLoaderContext::Create("PCL[does_not_exist.dex]");
  VerifyContextSize(context.get(), 1);
  ASSERT_FALSE(context->OpenDexFiles(InstructionSet::kArm, "."));
}

TEST_F(ClassLoaderContextTest, OpenValidDexFiles) {
  std::string multidex_name = GetTestDexFileName("MultiDex");
  std::string myclass_dex_name = GetTestDexFileName("MyClass");
  std::string dex_name = GetTestDexFileName("Main");


  std::unique_ptr<ClassLoaderContext> context =
      ClassLoaderContext::Create(
          "PCL[" + multidex_name + ":" + myclass_dex_name + "];" +
          "DLC[" + dex_name + "]");

  ASSERT_TRUE(context->OpenDexFiles(InstructionSet::kArm, /*classpath_dir=*/ ""));

  VerifyContextSize(context.get(), 2);

  std::vector<std::unique_ptr<const DexFile>> all_dex_files0 = OpenTestDexFiles("MultiDex");
  std::vector<std::unique_ptr<const DexFile>> myclass_dex_files = OpenTestDexFiles("MyClass");
  for (size_t i = 0; i < myclass_dex_files.size(); i++) {
    all_dex_files0.emplace_back(myclass_dex_files[i].release());
  }
  VerifyOpenDexFiles(context.get(), 0, &all_dex_files0);

  std::vector<std::unique_ptr<const DexFile>> all_dex_files1 = OpenTestDexFiles("Main");
  VerifyOpenDexFiles(context.get(), 1, &all_dex_files1);
}

// Creates a relative path from cwd to 'in'. Returns false if it cannot be done.
// TODO We should somehow support this in all situations. b/72042237.
static bool CreateRelativeString(const std::string& in, const char* cwd, std::string* out) {
  int cwd_len = strlen(cwd);
  if (!android::base::StartsWith(in, cwd) || (cwd_len < 1)) {
    return false;
  }
  bool contains_trailing_slash = (cwd[cwd_len - 1] == '/');
  int start_position = cwd_len + (contains_trailing_slash ? 0 : 1);
  *out = in.substr(start_position);
  return true;
}

TEST_F(ClassLoaderContextTest, OpenValidDexFilesRelative) {
  char cwd_buf[4096];
  if (getcwd(cwd_buf, arraysize(cwd_buf)) == nullptr) {
    PLOG(FATAL) << "Could not get working directory";
  }
  std::string multidex_name;
  std::string myclass_dex_name;
  std::string dex_name;
  if (!CreateRelativeString(GetTestDexFileName("MultiDex"), cwd_buf, &multidex_name) ||
      !CreateRelativeString(GetTestDexFileName("MyClass"), cwd_buf, &myclass_dex_name) ||
      !CreateRelativeString(GetTestDexFileName("Main"), cwd_buf, &dex_name)) {
    LOG(ERROR) << "Test OpenValidDexFilesRelative cannot be run because target dex files have no "
               << "relative path.";
    SUCCEED();
    return;
  }

  std::unique_ptr<ClassLoaderContext> context =
      ClassLoaderContext::Create(
          "PCL[" + multidex_name + ":" + myclass_dex_name + "];" +
          "DLC[" + dex_name + "]");

  ASSERT_TRUE(context->OpenDexFiles(InstructionSet::kArm, /*classpath_dir=*/ ""));

  std::vector<std::unique_ptr<const DexFile>> all_dex_files0 = OpenTestDexFiles("MultiDex");
  std::vector<std::unique_ptr<const DexFile>> myclass_dex_files = OpenTestDexFiles("MyClass");
  for (size_t i = 0; i < myclass_dex_files.size(); i++) {
    all_dex_files0.emplace_back(myclass_dex_files[i].release());
  }
  VerifyOpenDexFiles(context.get(), 0, &all_dex_files0);

  std::vector<std::unique_ptr<const DexFile>> all_dex_files1 = OpenTestDexFiles("Main");
  VerifyOpenDexFiles(context.get(), 1, &all_dex_files1);
}

TEST_F(ClassLoaderContextTest, OpenValidDexFilesClasspathDir) {
  char cwd_buf[4096];
  if (getcwd(cwd_buf, arraysize(cwd_buf)) == nullptr) {
    PLOG(FATAL) << "Could not get working directory";
  }
  std::string multidex_name;
  std::string myclass_dex_name;
  std::string dex_name;
  if (!CreateRelativeString(GetTestDexFileName("MultiDex"), cwd_buf, &multidex_name) ||
      !CreateRelativeString(GetTestDexFileName("MyClass"), cwd_buf, &myclass_dex_name) ||
      !CreateRelativeString(GetTestDexFileName("Main"), cwd_buf, &dex_name)) {
    LOG(ERROR) << "Test OpenValidDexFilesClasspathDir cannot be run because target dex files have "
               << "no relative path.";
    SUCCEED();
    return;
  }
  std::unique_ptr<ClassLoaderContext> context =
      ClassLoaderContext::Create(
          "PCL[" + multidex_name + ":" + myclass_dex_name + "];" +
          "DLC[" + dex_name + "]");

  ASSERT_TRUE(context->OpenDexFiles(InstructionSet::kArm, cwd_buf));

  VerifyContextSize(context.get(), 2);
  std::vector<std::unique_ptr<const DexFile>> all_dex_files0 = OpenTestDexFiles("MultiDex");
  std::vector<std::unique_ptr<const DexFile>> myclass_dex_files = OpenTestDexFiles("MyClass");
  for (size_t i = 0; i < myclass_dex_files.size(); i++) {
    all_dex_files0.emplace_back(myclass_dex_files[i].release());
  }
  VerifyOpenDexFiles(context.get(), 0, &all_dex_files0);

  std::vector<std::unique_ptr<const DexFile>> all_dex_files1 = OpenTestDexFiles("Main");
  VerifyOpenDexFiles(context.get(), 1, &all_dex_files1);
}

TEST_F(ClassLoaderContextTest, OpenInvalidDexFilesMix) {
  std::string dex_name = GetTestDexFileName("Main");
  std::unique_ptr<ClassLoaderContext> context =
      ClassLoaderContext::Create("PCL[does_not_exist.dex];DLC[" + dex_name + "]");
  ASSERT_FALSE(context->OpenDexFiles(InstructionSet::kArm, ""));
}

TEST_F(ClassLoaderContextTest, OpenDexFilesForIMCFails) {
  std::unique_ptr<ClassLoaderContext> context;
  std::string dex_name = GetTestDexFileName("Main");

  context = ParseContextWithChecksums("IMC[<unknown>*111]");
  VerifyContextSize(context.get(), 1);
  ASSERT_FALSE(context->OpenDexFiles(InstructionSet::kArm, "."));
}

TEST_F(ClassLoaderContextTest, CreateClassLoader) {
  std::string dex_name = GetTestDexFileName("Main");
  std::unique_ptr<ClassLoaderContext> context =
      ClassLoaderContext::Create("PCL[" + dex_name + "]");
  ASSERT_TRUE(context->OpenDexFiles(InstructionSet::kArm, ""));

  std::vector<std::unique_ptr<const DexFile>> classpath_dex = OpenTestDexFiles("Main");
  std::vector<std::unique_ptr<const DexFile>> compilation_sources = OpenTestDexFiles("MultiDex");

  std::vector<const DexFile*> compilation_sources_raw =
      MakeNonOwningPointerVector(compilation_sources);
  jobject jclass_loader = context->CreateClassLoader(compilation_sources_raw);
  ASSERT_TRUE(jclass_loader != nullptr);

  ScopedObjectAccess soa(Thread::Current());

  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader = hs.NewHandle(
      soa.Decode<mirror::ClassLoader>(jclass_loader));

  ASSERT_TRUE(class_loader->GetClass() ==
      soa.Decode<mirror::Class>(WellKnownClasses::dalvik_system_PathClassLoader));
  ASSERT_TRUE(class_loader->GetParent()->GetClass() ==
      soa.Decode<mirror::Class>(WellKnownClasses::java_lang_BootClassLoader));

  // For the first class loader the class path dex files must come first and then the
  // compilation sources.
  std::vector<const DexFile*> expected_classpath = MakeNonOwningPointerVector(classpath_dex);
  for (auto& dex : compilation_sources_raw) {
    expected_classpath.push_back(dex);
  }

  VerifyClassLoaderDexFiles(soa,
                            class_loader,
                            WellKnownClasses::dalvik_system_PathClassLoader,
                            expected_classpath);
}

TEST_F(ClassLoaderContextTest, CreateClassLoaderWithEmptyContext) {
  std::unique_ptr<ClassLoaderContext> context =
      ClassLoaderContext::Create("");
  ASSERT_TRUE(context->OpenDexFiles(InstructionSet::kArm, ""));

  std::vector<std::unique_ptr<const DexFile>> compilation_sources = OpenTestDexFiles("MultiDex");

  std::vector<const DexFile*> compilation_sources_raw =
      MakeNonOwningPointerVector(compilation_sources);
  jobject jclass_loader = context->CreateClassLoader(compilation_sources_raw);
  ASSERT_TRUE(jclass_loader != nullptr);

  ScopedObjectAccess soa(Thread::Current());

  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader = hs.NewHandle(
      soa.Decode<mirror::ClassLoader>(jclass_loader));

  // An empty context should create a single PathClassLoader with only the compilation sources.
  VerifyClassLoaderDexFiles(soa,
                            class_loader,
                            WellKnownClasses::dalvik_system_PathClassLoader,
                            compilation_sources_raw);
  ASSERT_TRUE(class_loader->GetParent()->GetClass() ==
      soa.Decode<mirror::Class>(WellKnownClasses::java_lang_BootClassLoader));
}

TEST_F(ClassLoaderContextTest, CreateClassLoaderWithSharedLibraryContext) {
  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create("&");

  ASSERT_TRUE(context->OpenDexFiles(InstructionSet::kArm, ""));

  std::vector<std::unique_ptr<const DexFile>> compilation_sources = OpenTestDexFiles("MultiDex");

  std::vector<const DexFile*> compilation_sources_raw =
      MakeNonOwningPointerVector(compilation_sources);
  jobject jclass_loader = context->CreateClassLoader(compilation_sources_raw);
  ASSERT_TRUE(jclass_loader != nullptr);

  ScopedObjectAccess soa(Thread::Current());

  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader = hs.NewHandle(
      soa.Decode<mirror::ClassLoader>(jclass_loader));

  // A shared library context should create a single PathClassLoader with only the compilation
  // sources.
  VerifyClassLoaderDexFiles(soa,
      class_loader,
      WellKnownClasses::dalvik_system_PathClassLoader,
      compilation_sources_raw);
  ASSERT_TRUE(class_loader->GetParent()->GetClass() ==
  soa.Decode<mirror::Class>(WellKnownClasses::java_lang_BootClassLoader));
}

TEST_F(ClassLoaderContextTest, CreateClassLoaderWithComplexChain) {
  // Setup the context.
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_a = OpenTestDexFiles("ForClassLoaderA");
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_b = OpenTestDexFiles("ForClassLoaderB");
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_c = OpenTestDexFiles("ForClassLoaderC");
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_d = OpenTestDexFiles("ForClassLoaderD");

  std::string context_spec =
      "PCL[" + CreateClassPath(classpath_dex_a) + ":" + CreateClassPath(classpath_dex_b) + "];" +
      "DLC[" + CreateClassPath(classpath_dex_c) + "];" +
      "PCL[" + CreateClassPath(classpath_dex_d) + "]";

  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create(context_spec);
  ASSERT_TRUE(context->OpenDexFiles(InstructionSet::kArm, ""));

  // Setup the compilation sources.
  std::vector<std::unique_ptr<const DexFile>> compilation_sources = OpenTestDexFiles("MultiDex");
  std::vector<const DexFile*> compilation_sources_raw =
      MakeNonOwningPointerVector(compilation_sources);

  // Create the class loader.
  jobject jclass_loader = context->CreateClassLoader(compilation_sources_raw);
  ASSERT_TRUE(jclass_loader != nullptr);

  // Verify the class loader.
  ScopedObjectAccess soa(Thread::Current());

  StackHandleScope<3> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader_1 = hs.NewHandle(
      soa.Decode<mirror::ClassLoader>(jclass_loader));

  // Verify the first class loader

  // For the first class loader the class path dex files must come first and then the
  // compilation sources.
  std::vector<const DexFile*> class_loader_1_dex_files =
      MakeNonOwningPointerVector(classpath_dex_a);
  for (auto& dex : classpath_dex_b) {
    class_loader_1_dex_files.push_back(dex.get());
  }
  for (auto& dex : compilation_sources_raw) {
    class_loader_1_dex_files.push_back(dex);
  }
  VerifyClassLoaderDexFiles(soa,
                            class_loader_1,
                            WellKnownClasses::dalvik_system_PathClassLoader,
                            class_loader_1_dex_files);

  // Verify the second class loader
  Handle<mirror::ClassLoader> class_loader_2 = hs.NewHandle(class_loader_1->GetParent());
  std::vector<const DexFile*> class_loader_2_dex_files =
      MakeNonOwningPointerVector(classpath_dex_c);
  VerifyClassLoaderDexFiles(soa,
                            class_loader_2,
                            WellKnownClasses::dalvik_system_DelegateLastClassLoader,
                            class_loader_2_dex_files);

  // Verify the third class loader
  Handle<mirror::ClassLoader> class_loader_3 = hs.NewHandle(class_loader_2->GetParent());
  std::vector<const DexFile*> class_loader_3_dex_files =
      MakeNonOwningPointerVector(classpath_dex_d);
  VerifyClassLoaderDexFiles(soa,
                            class_loader_3,
                            WellKnownClasses::dalvik_system_PathClassLoader,
                            class_loader_3_dex_files);
  // The last class loader should have the BootClassLoader as a parent.
  ASSERT_TRUE(class_loader_3->GetParent()->GetClass() ==
      soa.Decode<mirror::Class>(WellKnownClasses::java_lang_BootClassLoader));
}

TEST_F(ClassLoaderContextTest, CreateClassLoaderWithSharedLibraries) {
  // Setup the context.
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_a = OpenTestDexFiles("ForClassLoaderA");
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_b = OpenTestDexFiles("ForClassLoaderB");
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_c = OpenTestDexFiles("ForClassLoaderC");
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_d = OpenTestDexFiles("ForClassLoaderD");

  std::string context_spec =
      "PCL[" + CreateClassPath(classpath_dex_a) + ":" + CreateClassPath(classpath_dex_b) + "]{" +
      "DLC[" + CreateClassPath(classpath_dex_c) + "]#" +
      "PCL[" + CreateClassPath(classpath_dex_d) + "]}";

  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create(context_spec);
  ASSERT_TRUE(context->OpenDexFiles(InstructionSet::kArm, ""));

  // Setup the compilation sources.
  std::vector<std::unique_ptr<const DexFile>> compilation_sources = OpenTestDexFiles("MultiDex");
  std::vector<const DexFile*> compilation_sources_raw =
      MakeNonOwningPointerVector(compilation_sources);

  // Create the class loader.
  jobject jclass_loader = context->CreateClassLoader(compilation_sources_raw);
  ASSERT_TRUE(jclass_loader != nullptr);

  // Verify the class loader.
  ScopedObjectAccess soa(Thread::Current());

  StackHandleScope<4> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader_1 = hs.NewHandle(
      soa.Decode<mirror::ClassLoader>(jclass_loader));

  // For the first class loader the class path dex files must come first and then the
  // compilation sources.
  std::vector<const DexFile*> class_loader_1_dex_files =
      MakeNonOwningPointerVector(classpath_dex_a);
  for (auto& dex : classpath_dex_b) {
    class_loader_1_dex_files.push_back(dex.get());
  }
  for (auto& dex : compilation_sources_raw) {
    class_loader_1_dex_files.push_back(dex);
  }
  VerifyClassLoaderDexFiles(soa,
                            class_loader_1,
                            WellKnownClasses::dalvik_system_PathClassLoader,
                            class_loader_1_dex_files);

  // Verify the shared libraries.
  ArtField* field =
      jni::DecodeArtField(WellKnownClasses::dalvik_system_BaseDexClassLoader_sharedLibraryLoaders);
  ObjPtr<mirror::Object> raw_shared_libraries = field->GetObject(class_loader_1.Get());
  ASSERT_TRUE(raw_shared_libraries != nullptr);

  Handle<mirror::ObjectArray<mirror::ClassLoader>> shared_libraries(
      hs.NewHandle(raw_shared_libraries->AsObjectArray<mirror::ClassLoader>()));
  ASSERT_EQ(shared_libraries->GetLength(), 2);

  // Verify the first shared library.
  Handle<mirror::ClassLoader> class_loader_2 = hs.NewHandle(shared_libraries->Get(0));
  std::vector<const DexFile*> class_loader_2_dex_files =
      MakeNonOwningPointerVector(classpath_dex_c);
  VerifyClassLoaderDexFiles(soa,
                            class_loader_2,
                            WellKnownClasses::dalvik_system_DelegateLastClassLoader,
                            class_loader_2_dex_files);
  raw_shared_libraries = field->GetObject(class_loader_2.Get());
  ASSERT_TRUE(raw_shared_libraries == nullptr);

  // Verify the second shared library.
  Handle<mirror::ClassLoader> class_loader_3 = hs.NewHandle(shared_libraries->Get(1));
  std::vector<const DexFile*> class_loader_3_dex_files =
      MakeNonOwningPointerVector(classpath_dex_d);
  VerifyClassLoaderDexFiles(soa,
                            class_loader_3,
                            WellKnownClasses::dalvik_system_PathClassLoader,
                            class_loader_3_dex_files);
  raw_shared_libraries = field->GetObject(class_loader_3.Get());
  ASSERT_TRUE(raw_shared_libraries == nullptr);

  // All class loaders should have the BootClassLoader as a parent.
  ASSERT_TRUE(class_loader_1->GetParent()->GetClass() ==
      soa.Decode<mirror::Class>(WellKnownClasses::java_lang_BootClassLoader));
  ASSERT_TRUE(class_loader_2->GetParent()->GetClass() ==
      soa.Decode<mirror::Class>(WellKnownClasses::java_lang_BootClassLoader));
  ASSERT_TRUE(class_loader_3->GetParent()->GetClass() ==
      soa.Decode<mirror::Class>(WellKnownClasses::java_lang_BootClassLoader));
}

TEST_F(ClassLoaderContextTest, CreateClassLoaderWithSharedLibrariesInParentToo) {
  // Setup the context.
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_a = OpenTestDexFiles("ForClassLoaderA");
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_b = OpenTestDexFiles("ForClassLoaderB");
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_c = OpenTestDexFiles("ForClassLoaderC");
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_d = OpenTestDexFiles("ForClassLoaderD");

  std::string context_spec =
      "PCL[" + CreateClassPath(classpath_dex_a) + "]{" +
      "PCL[" + CreateClassPath(classpath_dex_b) + "]};" +
      "PCL[" + CreateClassPath(classpath_dex_c) + "]{" +
      "PCL[" + CreateClassPath(classpath_dex_d) + "]}";

  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create(context_spec);
  ASSERT_TRUE(context->OpenDexFiles(InstructionSet::kArm, ""));

  // Setup the compilation sources.
  std::vector<std::unique_ptr<const DexFile>> compilation_sources = OpenTestDexFiles("MultiDex");
  std::vector<const DexFile*> compilation_sources_raw =
      MakeNonOwningPointerVector(compilation_sources);

  // Create the class loader.
  jobject jclass_loader = context->CreateClassLoader(compilation_sources_raw);
  ASSERT_TRUE(jclass_loader != nullptr);

  // Verify the class loader.
  ScopedObjectAccess soa(Thread::Current());

  StackHandleScope<6> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader_1 = hs.NewHandle(
      soa.Decode<mirror::ClassLoader>(jclass_loader));

  // For the first class loader the class path dex files must come first and then the
  // compilation sources.
  std::vector<const DexFile*> class_loader_1_dex_files =
      MakeNonOwningPointerVector(classpath_dex_a);
  for (auto& dex : compilation_sources_raw) {
    class_loader_1_dex_files.push_back(dex);
  }
  VerifyClassLoaderDexFiles(soa,
                            class_loader_1,
                            WellKnownClasses::dalvik_system_PathClassLoader,
                            class_loader_1_dex_files);

  // Verify its shared library.
  ArtField* field =
      jni::DecodeArtField(WellKnownClasses::dalvik_system_BaseDexClassLoader_sharedLibraryLoaders);
  ObjPtr<mirror::Object> raw_shared_libraries = field->GetObject(class_loader_1.Get());
  ASSERT_TRUE(raw_shared_libraries != nullptr);

  Handle<mirror::ObjectArray<mirror::ClassLoader>> shared_libraries(
      hs.NewHandle(raw_shared_libraries->AsObjectArray<mirror::ClassLoader>()));
  ASSERT_EQ(shared_libraries->GetLength(), 1);

  Handle<mirror::ClassLoader> class_loader_2 = hs.NewHandle(shared_libraries->Get(0));
  std::vector<const DexFile*> class_loader_2_dex_files =
      MakeNonOwningPointerVector(classpath_dex_b);
  VerifyClassLoaderDexFiles(soa,
                            class_loader_2,
                            WellKnownClasses::dalvik_system_PathClassLoader,
                            class_loader_2_dex_files);
  raw_shared_libraries = field->GetObject(class_loader_2.Get());
  ASSERT_TRUE(raw_shared_libraries == nullptr);

  // Verify the parent.
  Handle<mirror::ClassLoader> class_loader_3 = hs.NewHandle(class_loader_1->GetParent());
  std::vector<const DexFile*> class_loader_3_dex_files =
      MakeNonOwningPointerVector(classpath_dex_c);
  VerifyClassLoaderDexFiles(soa,
                            class_loader_3,
                            WellKnownClasses::dalvik_system_PathClassLoader,
                            class_loader_3_dex_files);

  // Verify its shared library.
  raw_shared_libraries = field->GetObject(class_loader_3.Get());
  ASSERT_TRUE(raw_shared_libraries != nullptr);

  Handle<mirror::ObjectArray<mirror::ClassLoader>> shared_libraries_2(
      hs.NewHandle(raw_shared_libraries->AsObjectArray<mirror::ClassLoader>()));
  ASSERT_EQ(shared_libraries->GetLength(), 1);

  Handle<mirror::ClassLoader> class_loader_4 = hs.NewHandle(shared_libraries_2->Get(0));
  std::vector<const DexFile*> class_loader_4_dex_files =
      MakeNonOwningPointerVector(classpath_dex_d);
  VerifyClassLoaderDexFiles(soa,
                            class_loader_4,
                            WellKnownClasses::dalvik_system_PathClassLoader,
                            class_loader_4_dex_files);
  raw_shared_libraries = field->GetObject(class_loader_4.Get());
  ASSERT_TRUE(raw_shared_libraries == nullptr);

  // Class loaders should have the BootClassLoader as a parent.
  ASSERT_TRUE(class_loader_2->GetParent()->GetClass() ==
      soa.Decode<mirror::Class>(WellKnownClasses::java_lang_BootClassLoader));
  ASSERT_TRUE(class_loader_3->GetParent()->GetClass() ==
      soa.Decode<mirror::Class>(WellKnownClasses::java_lang_BootClassLoader));
  ASSERT_TRUE(class_loader_4->GetParent()->GetClass() ==
      soa.Decode<mirror::Class>(WellKnownClasses::java_lang_BootClassLoader));
}

TEST_F(ClassLoaderContextTest, CreateClassLoaderWithSharedLibrariesDependencies) {
  // Setup the context.
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_a = OpenTestDexFiles("ForClassLoaderA");
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_b = OpenTestDexFiles("ForClassLoaderB");
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_c = OpenTestDexFiles("ForClassLoaderC");
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_d = OpenTestDexFiles("ForClassLoaderD");

  std::string context_spec =
      "PCL[" + CreateClassPath(classpath_dex_a) + "]{" +
      "PCL[" + CreateClassPath(classpath_dex_b) + "]{" +
      "PCL[" + CreateClassPath(classpath_dex_c) + "]}};" +
      "PCL[" + CreateClassPath(classpath_dex_d) + "]";

  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create(context_spec);
  ASSERT_TRUE(context->OpenDexFiles(InstructionSet::kArm, ""));

  // Setup the compilation sources.
  std::vector<std::unique_ptr<const DexFile>> compilation_sources = OpenTestDexFiles("MultiDex");
  std::vector<const DexFile*> compilation_sources_raw =
      MakeNonOwningPointerVector(compilation_sources);

  // Create the class loader.
  jobject jclass_loader = context->CreateClassLoader(compilation_sources_raw);
  ASSERT_TRUE(jclass_loader != nullptr);

  // Verify the class loader.
  ScopedObjectAccess soa(Thread::Current());

  StackHandleScope<6> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader_1 = hs.NewHandle(
      soa.Decode<mirror::ClassLoader>(jclass_loader));

  // For the first class loader the class path dex files must come first and then the
  // compilation sources.
  std::vector<const DexFile*> class_loader_1_dex_files =
      MakeNonOwningPointerVector(classpath_dex_a);
  for (auto& dex : compilation_sources_raw) {
    class_loader_1_dex_files.push_back(dex);
  }
  VerifyClassLoaderDexFiles(soa,
                            class_loader_1,
                            WellKnownClasses::dalvik_system_PathClassLoader,
                            class_loader_1_dex_files);

  // Verify its shared library.
  ArtField* field =
      jni::DecodeArtField(WellKnownClasses::dalvik_system_BaseDexClassLoader_sharedLibraryLoaders);
  ObjPtr<mirror::Object> raw_shared_libraries = field->GetObject(class_loader_1.Get());
  ASSERT_TRUE(raw_shared_libraries != nullptr);

  Handle<mirror::ObjectArray<mirror::ClassLoader>> shared_libraries(
      hs.NewHandle(raw_shared_libraries->AsObjectArray<mirror::ClassLoader>()));
  ASSERT_EQ(shared_libraries->GetLength(), 1);

  Handle<mirror::ClassLoader> class_loader_2 = hs.NewHandle(shared_libraries->Get(0));
  std::vector<const DexFile*> class_loader_2_dex_files =
      MakeNonOwningPointerVector(classpath_dex_b);
  VerifyClassLoaderDexFiles(soa,
                            class_loader_2,
                            WellKnownClasses::dalvik_system_PathClassLoader,
                            class_loader_2_dex_files);

  // Verify the shared library dependency of the shared library.
  raw_shared_libraries = field->GetObject(class_loader_2.Get());
  ASSERT_TRUE(raw_shared_libraries != nullptr);

  Handle<mirror::ObjectArray<mirror::ClassLoader>> shared_libraries_2(
      hs.NewHandle(raw_shared_libraries->AsObjectArray<mirror::ClassLoader>()));
  ASSERT_EQ(shared_libraries_2->GetLength(), 1);

  Handle<mirror::ClassLoader> class_loader_3 = hs.NewHandle(shared_libraries_2->Get(0));
  std::vector<const DexFile*> class_loader_3_dex_files =
      MakeNonOwningPointerVector(classpath_dex_c);
  VerifyClassLoaderDexFiles(soa,
                            class_loader_3,
                            WellKnownClasses::dalvik_system_PathClassLoader,
                            class_loader_3_dex_files);
  raw_shared_libraries = field->GetObject(class_loader_3.Get());
  ASSERT_TRUE(raw_shared_libraries == nullptr);

  // Verify the parent.
  Handle<mirror::ClassLoader> class_loader_4 = hs.NewHandle(class_loader_1->GetParent());
  std::vector<const DexFile*> class_loader_4_dex_files =
      MakeNonOwningPointerVector(classpath_dex_d);
  VerifyClassLoaderDexFiles(soa,
                            class_loader_4,
                            WellKnownClasses::dalvik_system_PathClassLoader,
                            class_loader_4_dex_files);
  raw_shared_libraries = field->GetObject(class_loader_4.Get());
  ASSERT_TRUE(raw_shared_libraries == nullptr);

  // Class loaders should have the BootClassLoader as a parent.
  ASSERT_TRUE(class_loader_2->GetParent()->GetClass() ==
      soa.Decode<mirror::Class>(WellKnownClasses::java_lang_BootClassLoader));
  ASSERT_TRUE(class_loader_3->GetParent()->GetClass() ==
      soa.Decode<mirror::Class>(WellKnownClasses::java_lang_BootClassLoader));
  ASSERT_TRUE(class_loader_4->GetParent()->GetClass() ==
      soa.Decode<mirror::Class>(WellKnownClasses::java_lang_BootClassLoader));
}

TEST_F(ClassLoaderContextTest, RemoveSourceLocations) {
  std::unique_ptr<ClassLoaderContext> context =
      ClassLoaderContext::Create("PCL[a.dex]");
  dchecked_vector<std::string> classpath_dex;
  classpath_dex.push_back("a.dex");
  dchecked_vector<std::string> compilation_sources;
  compilation_sources.push_back("src.dex");

  // Nothing should be removed.
  ASSERT_FALSE(context->RemoveLocationsFromClassPaths(compilation_sources));
  VerifyClassLoaderPCL(context.get(), 0, "a.dex");
  // Classes should be removed.
  ASSERT_TRUE(context->RemoveLocationsFromClassPaths(classpath_dex));
  VerifyClassLoaderPCL(context.get(), 0, "");
}

TEST_F(ClassLoaderContextTest, CreateClassLoaderWithSameSharedLibraries) {
  // Setup the context.
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_a = OpenTestDexFiles("ForClassLoaderA");
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_b = OpenTestDexFiles("ForClassLoaderB");
  std::vector<std::unique_ptr<const DexFile>> classpath_dex_c = OpenTestDexFiles("ForClassLoaderC");

  std::string context_spec =
      "PCL[" + CreateClassPath(classpath_dex_a) + "]{" +
      "PCL[" + CreateClassPath(classpath_dex_b) + "]};" +
      "PCL[" + CreateClassPath(classpath_dex_c) + "]{" +
      "PCL[" + CreateClassPath(classpath_dex_b) + "]}";

  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create(context_spec);
  ASSERT_TRUE(context->OpenDexFiles(InstructionSet::kArm, ""));

  // Setup the compilation sources.
  std::vector<std::unique_ptr<const DexFile>> compilation_sources = OpenTestDexFiles("MultiDex");
  std::vector<const DexFile*> compilation_sources_raw =
      MakeNonOwningPointerVector(compilation_sources);

  // Create the class loader.
  jobject jclass_loader = context->CreateClassLoader(compilation_sources_raw);
  ASSERT_TRUE(jclass_loader != nullptr);

  // Verify the class loader.
  ScopedObjectAccess soa(Thread::Current());

  StackHandleScope<6> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader_1 = hs.NewHandle(
      soa.Decode<mirror::ClassLoader>(jclass_loader));

  // For the first class loader the class path dex files must come first and then the
  // compilation sources.
  std::vector<const DexFile*> class_loader_1_dex_files =
      MakeNonOwningPointerVector(classpath_dex_a);
  for (auto& dex : compilation_sources_raw) {
    class_loader_1_dex_files.push_back(dex);
  }
  VerifyClassLoaderDexFiles(soa,
                            class_loader_1,
                            WellKnownClasses::dalvik_system_PathClassLoader,
                            class_loader_1_dex_files);

  // Verify its shared library.
  ArtField* field =
      jni::DecodeArtField(WellKnownClasses::dalvik_system_BaseDexClassLoader_sharedLibraryLoaders);
  ObjPtr<mirror::Object> raw_shared_libraries = field->GetObject(class_loader_1.Get());
  ASSERT_TRUE(raw_shared_libraries != nullptr);

  Handle<mirror::ObjectArray<mirror::ClassLoader>> shared_libraries(
      hs.NewHandle(raw_shared_libraries->AsObjectArray<mirror::ClassLoader>()));
  ASSERT_EQ(shared_libraries->GetLength(), 1);

  Handle<mirror::ClassLoader> class_loader_2 = hs.NewHandle(shared_libraries->Get(0));
  std::vector<const DexFile*> class_loader_2_dex_files =
      MakeNonOwningPointerVector(classpath_dex_b);
  VerifyClassLoaderDexFiles(soa,
                            class_loader_2,
                            WellKnownClasses::dalvik_system_PathClassLoader,
                            class_loader_2_dex_files);

  // Verify the parent.
  Handle<mirror::ClassLoader> class_loader_3 = hs.NewHandle(class_loader_1->GetParent());
  std::vector<const DexFile*> class_loader_3_dex_files =
      MakeNonOwningPointerVector(classpath_dex_c);
  VerifyClassLoaderDexFiles(soa,
                            class_loader_3,
                            WellKnownClasses::dalvik_system_PathClassLoader,
                            class_loader_3_dex_files);

  // Verify its shared library is the same as the child.
  raw_shared_libraries = field->GetObject(class_loader_3.Get());
  ASSERT_TRUE(raw_shared_libraries != nullptr);
  Handle<mirror::ObjectArray<mirror::ClassLoader>> shared_libraries_2(
      hs.NewHandle(raw_shared_libraries->AsObjectArray<mirror::ClassLoader>()));
  ASSERT_EQ(shared_libraries_2->GetLength(), 1);
  ASSERT_OBJ_PTR_EQ(shared_libraries_2->Get(0), class_loader_2.Get());

  // Class loaders should have the BootClassLoader as a parent.
  ASSERT_TRUE(class_loader_2->GetParent()->GetClass() ==
      soa.Decode<mirror::Class>(WellKnownClasses::java_lang_BootClassLoader));
  ASSERT_TRUE(class_loader_3->GetParent()->GetClass() ==
      soa.Decode<mirror::Class>(WellKnownClasses::java_lang_BootClassLoader));
}

TEST_F(ClassLoaderContextTest, EncodeInOatFile) {
  std::string dex1_name = GetTestDexFileName("Main");
  std::string dex2_name = GetTestDexFileName("MyClass");
  std::unique_ptr<ClassLoaderContext> context =
      ClassLoaderContext::Create("PCL[" + dex1_name + ":" + dex2_name + "]");
  ASSERT_TRUE(context->OpenDexFiles(InstructionSet::kArm, ""));

  std::vector<std::unique_ptr<const DexFile>> dex1 = OpenTestDexFiles("Main");
  std::vector<std::unique_ptr<const DexFile>> dex2 = OpenTestDexFiles("MyClass");
  std::string encoding = context->EncodeContextForOatFile("");
  std::string expected_encoding = "PCL[" + CreateClassPathWithChecksums(dex1) + ":" +
      CreateClassPathWithChecksums(dex2) + "]";
  ASSERT_EQ(expected_encoding, context->EncodeContextForOatFile(""));
}

TEST_F(ClassLoaderContextTest, EncodeInOatFileIMC) {
  jobject class_loader_a = LoadDexInPathClassLoader("Main", nullptr);
  jobject class_loader_b = LoadDexInInMemoryDexClassLoader("MyClass", class_loader_a);

  std::unique_ptr<ClassLoaderContext> context = CreateContextForClassLoader(class_loader_b);
  ASSERT_TRUE(context->OpenDexFiles(InstructionSet::kArm, ""));

  std::vector<std::unique_ptr<const DexFile>> dex1 = OpenTestDexFiles("Main");
  std::vector<std::unique_ptr<const DexFile>> dex2 = OpenTestDexFiles("MyClass");
  ASSERT_EQ(dex2.size(), 1u);

  std::string encoding = context->EncodeContextForOatFile("");
  std::string expected_encoding = "IMC[<unknown>*" + std::to_string(dex2[0]->GetLocationChecksum())
      + "];PCL[" + CreateClassPathWithChecksums(dex1) + "]";
  ASSERT_EQ(expected_encoding, context->EncodeContextForOatFile(""));
}

TEST_F(ClassLoaderContextTest, EncodeForDex2oat) {
  std::string dex1_name = GetTestDexFileName("Main");
  std::string dex2_name = GetTestDexFileName("MultiDex");
  std::unique_ptr<ClassLoaderContext> context =
      ClassLoaderContext::Create("PCL[" + dex1_name + ":" + dex2_name + "]");
  ASSERT_TRUE(context->OpenDexFiles(InstructionSet::kArm, ""));

  std::string encoding = context->EncodeContextForDex2oat("");
  std::string expected_encoding = "PCL[" + dex1_name + ":" + dex2_name + "]";
  ASSERT_EQ(expected_encoding, context->EncodeContextForDex2oat(""));
}

TEST_F(ClassLoaderContextTest, EncodeForDex2oatIMC) {
  jobject class_loader_a = LoadDexInPathClassLoader("Main", nullptr);
  jobject class_loader_b = LoadDexInInMemoryDexClassLoader("MyClass", class_loader_a);

  std::unique_ptr<ClassLoaderContext> context = CreateContextForClassLoader(class_loader_b);
  ASSERT_TRUE(context->OpenDexFiles(InstructionSet::kArm, ""));

  std::string encoding = context->EncodeContextForDex2oat("");
  std::string expected_encoding = "IMC[<unknown>];PCL[" + GetTestDexFileName("Main") + "]";
  ASSERT_EQ(expected_encoding, context->EncodeContextForDex2oat(""));
}

TEST_F(ClassLoaderContextTest, EncodeContextsSinglePath) {
  jobject class_loader = LoadDexInPathClassLoader("Main", nullptr);
  std::unique_ptr<ClassLoaderContext> context =
      CreateContextForClassLoader(class_loader);

  std::map<std::string, std::string> encodings = context->EncodeClassPathContexts("");
  ASSERT_EQ(1u, encodings.size());
  ASSERT_EQ("PCL[]", encodings.at(GetTestDexFileName("Main")));
}

TEST_F(ClassLoaderContextTest, EncodeContextsMultiDex) {
  jobject class_loader = LoadDexInPathClassLoader("MultiDex", nullptr);
  std::unique_ptr<ClassLoaderContext> context =
      CreateContextForClassLoader(class_loader);

  std::map<std::string, std::string> encodings = context->EncodeClassPathContexts("");
  ASSERT_EQ(1u, encodings.size());
  ASSERT_EQ("PCL[]", encodings.at(GetTestDexFileName("MultiDex")));
}

TEST_F(ClassLoaderContextTest, EncodeContextsRepeatedMultiDex) {
  jobject top_class_loader = LoadDexInPathClassLoader("MultiDex", nullptr);
  jobject middle_class_loader =
      LoadDexInPathClassLoader("Main", top_class_loader);
  jobject bottom_class_loader =
      LoadDexInPathClassLoader("MultiDex", middle_class_loader);
  std::unique_ptr<ClassLoaderContext> context =
      CreateContextForClassLoader(bottom_class_loader);

  std::map<std::string, std::string> encodings = context->EncodeClassPathContexts("");
  ASSERT_EQ(1u, encodings.size());

  std::string main_dex_name = GetTestDexFileName("Main");
  std::string multidex_dex_name = GetTestDexFileName("MultiDex");
  ASSERT_EQ(
      "PCL[];PCL[" + main_dex_name + "];PCL[" + multidex_dex_name + "]",
      encodings.at(multidex_dex_name));
}

TEST_F(ClassLoaderContextTest, EncodeContextsSinglePathWithShared) {
  jobject class_loader_a = LoadDexInPathClassLoader("MyClass", nullptr);

  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::ObjectArray<mirror::ClassLoader>> libraries = hs.NewHandle(
    mirror::ObjectArray<mirror::ClassLoader>::Alloc(
        soa.Self(),
        GetClassRoot<mirror::ObjectArray<mirror::ClassLoader>>(),
        1));
  libraries->Set(0, soa.Decode<mirror::ClassLoader>(class_loader_a));

  jobject class_loader_b = LoadDexInPathClassLoader(
      "Main", nullptr, soa.AddLocalReference<jobject>(libraries.Get()));

  std::unique_ptr<ClassLoaderContext> context = CreateContextForClassLoader(class_loader_b);

  std::map<std::string, std::string> encodings = context->EncodeClassPathContexts("");
  ASSERT_EQ(1u, encodings.size());
  ASSERT_EQ(
      "PCL[]{PCL[" + GetTestDexFileName("MyClass") + "]}",
      encodings.at(GetTestDexFileName("Main")));
}

TEST_F(ClassLoaderContextTest, EncodeContextsMultiplePaths) {
  jobject class_loader = LoadDexInPathClassLoader(
      std::vector<std::string>{ "Main", "MultiDex"}, nullptr);

  std::unique_ptr<ClassLoaderContext> context =
      CreateContextForClassLoader(class_loader);

  std::map<std::string, std::string> encodings = context->EncodeClassPathContexts("");
  ASSERT_EQ(2u, encodings.size());
  ASSERT_EQ("PCL[]", encodings.at(GetTestDexFileName("Main")));
  ASSERT_EQ(
      "PCL[" + GetTestDexFileName("Main") + "]", encodings.at(GetTestDexFileName("MultiDex")));
}

TEST_F(ClassLoaderContextTest, EncodeContextsMultiplePathsWithShared) {
  jobject class_loader_a = LoadDexInPathClassLoader("MyClass", nullptr);

  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::ObjectArray<mirror::ClassLoader>> libraries = hs.NewHandle(
    mirror::ObjectArray<mirror::ClassLoader>::Alloc(
        soa.Self(),
        GetClassRoot<mirror::ObjectArray<mirror::ClassLoader>>(),
        1));
  libraries->Set(0, soa.Decode<mirror::ClassLoader>(class_loader_a));

  jobject class_loader_b = LoadDexInPathClassLoader(
      std::vector<std::string> { "Main", "MultiDex" },
      nullptr, soa.AddLocalReference<jobject>(libraries.Get()));

  std::unique_ptr<ClassLoaderContext> context =
      CreateContextForClassLoader(class_loader_b);

  std::map<std::string, std::string> encodings = context->EncodeClassPathContexts("");
  ASSERT_EQ(2u, encodings.size());
  const std::string context_suffix =
      "{PCL[" + GetTestDexFileName("MyClass") + "]}";
  ASSERT_EQ("PCL[]" + context_suffix, encodings.at(GetTestDexFileName("Main")));
  ASSERT_EQ(
      "PCL[" + GetTestDexFileName("Main") + "]" + context_suffix,
      encodings.at(GetTestDexFileName("MultiDex")));
}

TEST_F(ClassLoaderContextTest, EncodeContextsIMC) {
  jobject class_loader_a = LoadDexInPathClassLoader("Main", nullptr);
  jobject class_loader_b =
      LoadDexInInMemoryDexClassLoader("MyClass", class_loader_a);

  std::unique_ptr<ClassLoaderContext> context =
      CreateContextForClassLoader(class_loader_b);

  std::map<std::string, std::string> encodings = context->EncodeClassPathContexts("");
  ASSERT_EQ(1u, encodings.size());
  ASSERT_EQ(
      "IMC[];PCL[" + GetTestDexFileName("Main") + "]",
      encodings.at("<unknown>"));
}

TEST_F(ClassLoaderContextTest, EncodeContextsForSingleDex) {
  jobject class_loader = LoadDexInPathClassLoader("Main", nullptr);
  std::map<std::string, std::string> encodings =
      ClassLoaderContext::EncodeClassPathContextsForClassLoader(class_loader);
  ASSERT_EQ(1u, encodings.size());
  ASSERT_EQ("PCL[]", encodings.at(GetTestDexFileName("Main")));
}

static jobject CreateForeignClassLoader() {
  ScopedObjectAccess soa(Thread::Current());
  JNIEnv* env = soa.Env();

  // We cannot instantiate a ClassLoader directly, so instead we allocate an Object to represent
  // our foreign ClassLoader (this works because the runtime does proper instanceof checks before
  // operating on this object.
  jmethodID ctor = env->GetMethodID(WellKnownClasses::java_lang_Object, "<init>", "()V");
  return env->NewObject(WellKnownClasses::java_lang_Object, ctor);
}

TEST_F(ClassLoaderContextTest, EncodeContextsForUnsupportedBase) {
  std::map<std::string, std::string> empty;
  ASSERT_EQ(
      empty, ClassLoaderContext::EncodeClassPathContextsForClassLoader(CreateForeignClassLoader()));
}

TEST_F(ClassLoaderContextTest, EncodeContextsForUnsupportedChain) {
  jobject class_loader = LoadDexInPathClassLoader("Main", CreateForeignClassLoader());
  std::map<std::string, std::string> encodings =
      ClassLoaderContext::EncodeClassPathContextsForClassLoader(class_loader);
  ASSERT_EQ(1u, encodings.size());
  ASSERT_EQ(
      ClassLoaderContext::kUnsupportedClassLoaderContextEncoding,
      encodings.at(GetTestDexFileName("Main")));
}

TEST_F(ClassLoaderContextTest, EncodeContextsForUnsupportedChainMultiPath) {
  jobject class_loader = LoadDexInPathClassLoader(std::vector<std::string> { "Main", "MyClass" },
                                                  CreateForeignClassLoader());
  std::map<std::string, std::string> encodings =
      ClassLoaderContext::EncodeClassPathContextsForClassLoader(class_loader);
  ASSERT_EQ(2u, encodings.size());
  ASSERT_EQ(
      ClassLoaderContext::kUnsupportedClassLoaderContextEncoding,
      encodings.at(GetTestDexFileName("Main")));
  ASSERT_EQ(
      ClassLoaderContext::kUnsupportedClassLoaderContextEncoding,
      encodings.at(GetTestDexFileName("MyClass")));
}

TEST_F(ClassLoaderContextTest, EncodeContextsForUnsupportedChainMultiDex) {
  jobject class_loader = LoadDexInPathClassLoader("MultiDex", CreateForeignClassLoader());
  std::map<std::string, std::string> encodings =
      ClassLoaderContext::EncodeClassPathContextsForClassLoader(class_loader);
  ASSERT_EQ(1u, encodings.size());
  ASSERT_EQ(
      ClassLoaderContext::kUnsupportedClassLoaderContextEncoding,
      encodings.at(GetTestDexFileName("MultiDex")));
}

TEST_F(ClassLoaderContextTest, IsValidEncoding) {
  ASSERT_TRUE(ClassLoaderContext::IsValidEncoding("PCL[]"));
  ASSERT_TRUE(ClassLoaderContext::IsValidEncoding("PCL[foo.dex]"));
  ASSERT_TRUE(ClassLoaderContext::IsValidEncoding("PCL[foo.dex];PCL[bar.dex]"));
  ASSERT_TRUE(ClassLoaderContext::IsValidEncoding("DLC[];PCL[bar.dex]"));
  ASSERT_TRUE(
      ClassLoaderContext::IsValidEncoding(
        ClassLoaderContext::kUnsupportedClassLoaderContextEncoding));
  ASSERT_FALSE(ClassLoaderContext::IsValidEncoding("not_valid"));
  ASSERT_FALSE(ClassLoaderContext::IsValidEncoding("[]"));
  ASSERT_FALSE(ClassLoaderContext::IsValidEncoding("FCL[]"));
  ASSERT_FALSE(ClassLoaderContext::IsValidEncoding("foo.dex:bar.dex"));
}

// TODO(calin) add a test which creates the context for a class loader together with dex_elements.
TEST_F(ClassLoaderContextTest, CreateContextForClassLoader) {
  // The chain is
  //    ClassLoaderA (PathClassLoader)
  //       ^
  //       |
  //    ClassLoaderB (DelegateLastClassLoader)
  //       ^
  //       |
  //    ClassLoaderC (PathClassLoader)
  //       ^
  //       |
  //    ClassLoaderD (DelegateLastClassLoader)

  jobject class_loader_a = LoadDexInPathClassLoader("ForClassLoaderA", nullptr);
  jobject class_loader_b = LoadDexInDelegateLastClassLoader("ForClassLoaderB", class_loader_a);
  jobject class_loader_c = LoadDexInPathClassLoader("ForClassLoaderC", class_loader_b);
  jobject class_loader_d = LoadDexInDelegateLastClassLoader("ForClassLoaderD", class_loader_c);

  std::unique_ptr<ClassLoaderContext> context = CreateContextForClassLoader(class_loader_d);

  VerifyContextForClassLoader(context.get());
  VerifyContextSize(context.get(), 4);

  VerifyClassLoaderDLCFromTestDex(context.get(), 0, "ForClassLoaderD");
  VerifyClassLoaderPCLFromTestDex(context.get(), 1, "ForClassLoaderC");
  VerifyClassLoaderDLCFromTestDex(context.get(), 2, "ForClassLoaderB");
  VerifyClassLoaderPCLFromTestDex(context.get(), 3, "ForClassLoaderA");
}

TEST_F(ClassLoaderContextTest, CreateContextForClassLoaderIMC) {
  // The chain is
  //    ClassLoaderA (PathClassLoader)
  //       ^
  //       |
  //    ClassLoaderB (InMemoryDexClassLoader)
  //       ^
  //       |
  //    ClassLoaderC (InMemoryDexClassLoader)
  //       ^
  //       |
  //    ClassLoaderD (DelegateLastClassLoader)

  jobject class_loader_a = LoadDexInPathClassLoader("ForClassLoaderA", nullptr);
  jobject class_loader_b = LoadDexInInMemoryDexClassLoader("ForClassLoaderB", class_loader_a);
  jobject class_loader_c = LoadDexInInMemoryDexClassLoader("ForClassLoaderC", class_loader_b);
  jobject class_loader_d = LoadDexInDelegateLastClassLoader("ForClassLoaderD", class_loader_c);

  std::unique_ptr<ClassLoaderContext> context = CreateContextForClassLoader(class_loader_d);

  VerifyContextForClassLoader(context.get());
  VerifyContextSize(context.get(), 4);

  VerifyClassLoaderDLCFromTestDex(context.get(), 0, "ForClassLoaderD");
  VerifyClassLoaderIMCFromTestDex(context.get(), 1, "ForClassLoaderC");
  VerifyClassLoaderIMCFromTestDex(context.get(), 2, "ForClassLoaderB");
  VerifyClassLoaderPCLFromTestDex(context.get(), 3, "ForClassLoaderA");
}

TEST_F(ClassLoaderContextTest, VerifyClassLoaderContextFirstElement) {
  std::string context_spec = "PCL[]";
  std::unique_ptr<ClassLoaderContext> context = ParseContextWithChecksums(context_spec);
  ASSERT_TRUE(context != nullptr);
  PretendContextOpenedDexFiles(context.get());
  // Ensure that the special shared library marks as verified for the first thing in the class path.
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(OatFile::kSpecialSharedLibrary),
            ClassLoaderContext::VerificationResult::kVerifies);
}

TEST_F(ClassLoaderContextTest, VerifyClassLoaderContextMatch) {
  std::string context_spec = "PCL[a.dex*123:b.dex*456];DLC[c.dex*890]";
  std::unique_ptr<ClassLoaderContext> context = ParseContextWithChecksums(context_spec);
  // Pretend that we successfully open the dex files to pass the DCHECKS.
  // (as it's much easier to test all the corner cases without relying on actual dex files).
  PretendContextOpenedDexFiles(context.get());

  VerifyContextSize(context.get(), 2);
  VerifyClassLoaderPCL(context.get(), 0, "a.dex:b.dex");
  VerifyClassLoaderDLC(context.get(), 1, "c.dex");

  ASSERT_EQ(context->VerifyClassLoaderContextMatch(context_spec),
            ClassLoaderContext::VerificationResult::kVerifies);

  std::string wrong_class_loader_type = "PCL[a.dex*123:b.dex*456];PCL[c.dex*890]";
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(wrong_class_loader_type),
            ClassLoaderContext::VerificationResult::kMismatch);

  std::string wrong_class_loader_order = "DLC[c.dex*890];PCL[a.dex*123:b.dex*456]";
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(wrong_class_loader_order),
            ClassLoaderContext::VerificationResult::kMismatch);

  std::string wrong_classpath_order = "PCL[b.dex*456:a.dex*123];DLC[c.dex*890]";
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(wrong_classpath_order),
            ClassLoaderContext::VerificationResult::kMismatch);

  std::string wrong_checksum = "PCL[a.dex*999:b.dex*456];DLC[c.dex*890]";
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(wrong_checksum),
            ClassLoaderContext::VerificationResult::kMismatch);

  std::string wrong_extra_class_loader = "PCL[a.dex*123:b.dex*456];DLC[c.dex*890];PCL[d.dex*321]";
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(wrong_extra_class_loader),
            ClassLoaderContext::VerificationResult::kMismatch);

  std::string wrong_extra_classpath = "PCL[a.dex*123:b.dex*456];DLC[c.dex*890:d.dex*321]";
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(wrong_extra_classpath),
            ClassLoaderContext::VerificationResult::kMismatch);

  std::string wrong_spec = "PCL[a.dex*999:b.dex*456];DLC[";
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(wrong_spec),
            ClassLoaderContext::VerificationResult::kMismatch);
}

TEST_F(ClassLoaderContextTest, VerifyClassLoaderContextWithIMCMatch) {
  std::string context_spec = "PCL[a.dex*123:b.dex*456];DLC[c.dex*890];IMC[<unknown>*111]";
  std::unique_ptr<ClassLoaderContext> context = ParseContextWithChecksums(context_spec);
  // Pretend that we successfully open the dex files to pass the DCHECKS.
  // (as it's much easier to test all the corner cases without relying on actual dex files).
  PretendContextOpenedDexFiles(context.get());

  VerifyContextSize(context.get(), 3);
  VerifyClassLoaderPCL(context.get(), 0, "a.dex:b.dex");
  VerifyClassLoaderDLC(context.get(), 1, "c.dex");
  VerifyClassLoaderIMC(context.get(), 2, "<unknown>");

  ASSERT_EQ(context->VerifyClassLoaderContextMatch(context_spec),
            ClassLoaderContext::VerificationResult::kVerifies);
}

TEST_F(ClassLoaderContextTest, VerifyClassLoaderContextMatchSpecial) {
  std::string context_spec = "&";
  std::unique_ptr<ClassLoaderContext> context = ParseContextWithChecksums(context_spec);
  // Pretend that we successfully open the dex files to pass the DCHECKS.
  // (as it's much easier to test all the corner cases without relying on actual dex files).
  PretendContextOpenedDexFiles(context.get());

  ASSERT_EQ(context->VerifyClassLoaderContextMatch(context_spec),
            ClassLoaderContext::VerificationResult::kForcedToSkipChecks);
}

TEST_F(ClassLoaderContextTest, VerifyClassLoaderContextMatchWithSL) {
  std::string context_spec =
      "PCL[a.dex*123:b.dex*456]{PCL[d.dex*321];PCL[e.dex*654]#PCL[f.dex*098:g.dex*999]}"
      ";DLC[c.dex*890]";
  std::unique_ptr<ClassLoaderContext> context = ParseContextWithChecksums(context_spec);
  // Pretend that we successfully open the dex files to pass the DCHECKS.
  // (as it's much easier to test all the corner cases without relying on actual dex files).
  PretendContextOpenedDexFiles(context.get());

  VerifyContextSize(context.get(), 2);
  VerifyClassLoaderPCL(context.get(), 0, "a.dex:b.dex");
  VerifyClassLoaderDLC(context.get(), 1, "c.dex");
  VerifyClassLoaderSharedLibraryPCL(context.get(), 0, 0, "d.dex");
  VerifyClassLoaderSharedLibraryPCL(context.get(), 0, 1, "f.dex:g.dex");

  ASSERT_EQ(context->VerifyClassLoaderContextMatch(context_spec),
            ClassLoaderContext::VerificationResult::kVerifies);

  std::string wrong_class_loader_type =
      "PCL[a.dex*123:b.dex*456]{DLC[d.dex*321];PCL[e.dex*654]#PCL[f.dex*098:g.dex*999]}"
      ";DLC[c.dex*890]";
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(wrong_class_loader_type),
            ClassLoaderContext::VerificationResult::kMismatch);

  std::string wrong_class_loader_order =
      "PCL[a.dex*123:b.dex*456]{PCL[f.dex#098:g.dex#999}#PCL[d.dex*321];PCL[e.dex*654]}"
      ";DLC[c.dex*890]";
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(wrong_class_loader_order),
            ClassLoaderContext::VerificationResult::kMismatch);

  std::string wrong_classpath_order =
      "PCL[a.dex*123:b.dex*456]{PCL[d.dex*321];PCL[e.dex*654]#PCL[g.dex*999:f.dex*098]}"
      ";DLC[c.dex*890]";
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(wrong_classpath_order),
            ClassLoaderContext::VerificationResult::kMismatch);

  std::string wrong_checksum =
      "PCL[a.dex*123:b.dex*456]{PCL[d.dex*333];PCL[e.dex*654]#PCL[g.dex*999:f.dex*098]}"
      ";DLC[c.dex*890]";
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(wrong_checksum),
            ClassLoaderContext::VerificationResult::kMismatch);

  std::string wrong_extra_class_loader =
      "PCL[a.dex*123:b.dex*456]"
      "{PCL[d.dex*321];PCL[e.dex*654]#PCL[f.dex*098:g.dex*999];PCL[i.dex#444]}"
      ";DLC[c.dex*890]";
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(wrong_extra_class_loader),
            ClassLoaderContext::VerificationResult::kMismatch);

  std::string wrong_extra_classpath =
      "PCL[a.dex*123:b.dex*456]{PCL[d.dex*321:i.dex#444];PCL[e.dex*654]#PCL[f.dex*098:g.dex*999]}"
      ";DLC[c.dex*890]";
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(wrong_extra_classpath),
            ClassLoaderContext::VerificationResult::kMismatch);
}

TEST_F(ClassLoaderContextTest, VerifyClassLoaderContextMatchWithIMCSL) {
  std::string context_spec =
      "IMC[<unknown>*123:<unknown>*456]"
      "{IMC[<unknown>*321];IMC[<unknown>*654]#IMC[<unknown>*098:<unknown>*999]};"
      "DLC[c.dex*890]";
  std::unique_ptr<ClassLoaderContext> context = ParseContextWithChecksums(context_spec);
  // Pretend that we successfully open the dex files to pass the DCHECKS.
  // (as it's much easier to test all the corner cases without relying on actual dex files).
  PretendContextOpenedDexFiles(context.get());

  VerifyContextSize(context.get(), 2);
  VerifyClassLoaderIMC(context.get(), 0, "<unknown>:<unknown>");
  VerifyClassLoaderDLC(context.get(), 1, "c.dex");
  VerifyClassLoaderSharedLibraryIMC(context.get(), 0, 0, "<unknown>");
  VerifyClassLoaderSharedLibraryIMC(context.get(), 0, 1, "<unknown>:<unknown>");

  ASSERT_EQ(context->VerifyClassLoaderContextMatch(context_spec),
            ClassLoaderContext::VerificationResult::kVerifies);
}

TEST_F(ClassLoaderContextTest, VerifyClassLoaderContextMatchAfterEncoding) {
  jobject class_loader_a = LoadDexInPathClassLoader("ForClassLoaderA", nullptr);
  jobject class_loader_b = LoadDexInDelegateLastClassLoader("ForClassLoaderB", class_loader_a);
  jobject class_loader_c = LoadDexInPathClassLoader("ForClassLoaderC", class_loader_b);
  jobject class_loader_d = LoadDexInDelegateLastClassLoader("ForClassLoaderD", class_loader_c);

  std::unique_ptr<ClassLoaderContext> context = CreateContextForClassLoader(class_loader_d);

  std::string context_with_no_base_dir = context->EncodeContextForOatFile("");
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(context_with_no_base_dir),
            ClassLoaderContext::VerificationResult::kVerifies);

  std::string dex_location = GetTestDexFileName("ForClassLoaderA");
  size_t pos = dex_location.rfind('/');
  ASSERT_NE(std::string::npos, pos);
  std::string parent = dex_location.substr(0, pos);

  std::string context_with_base_dir = context->EncodeContextForOatFile(parent);
  ASSERT_NE(context_with_base_dir, context_with_no_base_dir);
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(context_with_base_dir),
            ClassLoaderContext::VerificationResult::kVerifies);
}

TEST_F(ClassLoaderContextTest, VerifyClassLoaderContextMatchAfterEncodingIMC) {
  jobject class_loader_a = LoadDexInPathClassLoader("ForClassLoaderA", nullptr);
  jobject class_loader_b = LoadDexInInMemoryDexClassLoader("ForClassLoaderB", class_loader_a);
  jobject class_loader_c = LoadDexInInMemoryDexClassLoader("ForClassLoaderC", class_loader_b);
  jobject class_loader_d = LoadDexInDelegateLastClassLoader("ForClassLoaderD", class_loader_c);

  std::unique_ptr<ClassLoaderContext> context = CreateContextForClassLoader(class_loader_d);

  std::string context_with_no_base_dir = context->EncodeContextForOatFile("");
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(context_with_no_base_dir),
            ClassLoaderContext::VerificationResult::kVerifies);

  std::string dex_location = GetTestDexFileName("ForClassLoaderA");
  size_t pos = dex_location.rfind('/');
  ASSERT_NE(std::string::npos, pos);
  std::string parent = dex_location.substr(0, pos);

  std::string context_with_base_dir = context->EncodeContextForOatFile(parent);
  ASSERT_NE(context_with_base_dir, context_with_no_base_dir);
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(context_with_base_dir),
            ClassLoaderContext::VerificationResult::kVerifies);
}

TEST_F(ClassLoaderContextTest, VerifyClassLoaderContextMatchAfterEncodingMultidex) {
  jobject class_loader = LoadDexInPathClassLoader("MultiDex", nullptr);

  std::unique_ptr<ClassLoaderContext> context = CreateContextForClassLoader(class_loader);

  std::string context_with_no_base_dir = context->EncodeContextForOatFile("");
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(context_with_no_base_dir),
            ClassLoaderContext::VerificationResult::kVerifies);

  std::string dex_location = GetTestDexFileName("MultiDex");
  size_t pos = dex_location.rfind('/');
  ASSERT_NE(std::string::npos, pos);
  std::string parent = dex_location.substr(0, pos);

  std::string context_with_base_dir = context->EncodeContextForOatFile(parent);
  ASSERT_NE(context_with_base_dir, context_with_no_base_dir);
  ASSERT_EQ(context->VerifyClassLoaderContextMatch(context_with_base_dir),
            ClassLoaderContext::VerificationResult::kVerifies);
}

TEST_F(ClassLoaderContextTest, CreateContextForClassLoaderWithSharedLibraries) {
  jobject class_loader_a = LoadDexInPathClassLoader("ForClassLoaderA", nullptr);

  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::ObjectArray<mirror::ClassLoader>> libraries = hs.NewHandle(
    mirror::ObjectArray<mirror::ClassLoader>::Alloc(
        soa.Self(),
        GetClassRoot<mirror::ObjectArray<mirror::ClassLoader>>(),
        1));
  libraries->Set(0, soa.Decode<mirror::ClassLoader>(class_loader_a));

  jobject class_loader_b = LoadDexInPathClassLoader(
      "ForClassLoaderB", nullptr, soa.AddLocalReference<jobject>(libraries.Get()));

  std::unique_ptr<ClassLoaderContext> context = CreateContextForClassLoader(class_loader_b);
  ASSERT_TRUE(context != nullptr);
  std::vector<std::unique_ptr<const DexFile>> dex_files = OpenTestDexFiles("ForClassLoaderB");
  VerifyClassLoaderPCL(context.get(), 0, dex_files[0]->GetLocation());
  dex_files = OpenTestDexFiles("ForClassLoaderA");
  VerifyClassLoaderSharedLibraryPCL(context.get(), 0, 0, dex_files[0]->GetLocation());

  ASSERT_EQ(context->VerifyClassLoaderContextMatch(context->EncodeContextForOatFile("")),
            ClassLoaderContext::VerificationResult::kVerifies);
}

TEST_F(ClassLoaderContextTest, CheckForDuplicateDexFilesNotFoundSingleCL) {
  jobject class_loader = LoadDexInPathClassLoader("Main", nullptr);

  std::unique_ptr<ClassLoaderContext> context = CreateContextForClassLoader(class_loader);

  std::set<const DexFile*> result = context->CheckForDuplicateDexFiles(
      std::vector<const DexFile*>());
  ASSERT_EQ(0u, result.size());

  std::vector<std::unique_ptr<const DexFile>> dex1 = OpenTestDexFiles("ForClassLoaderA");
  std::vector<const DexFile*> dex1_raw = MakeNonOwningPointerVector(dex1);
  result = context->CheckForDuplicateDexFiles(dex1_raw);
  ASSERT_EQ(0u, result.size());
}

TEST_F(ClassLoaderContextTest, CheckForDuplicateDexFilesFound) {
  jobject class_loader = LoadDexInPathClassLoader(std::vector<std::string> { "Main", "Main" }, nullptr);

  std::unique_ptr<ClassLoaderContext> context = CreateContextForClassLoader(class_loader);

  std::vector<std::unique_ptr<const DexFile>> dex1 = OpenTestDexFiles("Main");
  std::vector<const DexFile*> dex1_raw = MakeNonOwningPointerVector(dex1);
  std::set<const DexFile*> result = context->CheckForDuplicateDexFiles(dex1_raw);
  ASSERT_EQ(1u, result.size()) << context->EncodeContextForOatFile("");
  ASSERT_EQ(dex1_raw[0], *(result.begin()));
}


TEST_F(ClassLoaderContextTest, CheckForDuplicateCrossCLNotFound) {
  jobject class_loader_a = LoadDexInPathClassLoader("ForClassLoaderA", nullptr);
  jobject class_loader_b = LoadDexInInMemoryDexClassLoader("ForClassLoaderB", class_loader_a);

  std::unique_ptr<ClassLoaderContext> context = CreateContextForClassLoader(class_loader_b);

  std::vector<std::unique_ptr<const DexFile>> dex1 = OpenTestDexFiles("ForClassLoaderA");
  std::vector<const DexFile*> dex1_raw = MakeNonOwningPointerVector(dex1);
  std::set<const DexFile*> result = context->CheckForDuplicateDexFiles(dex1_raw);
  ASSERT_EQ(0u, result.size());
}

}  // namespace art
