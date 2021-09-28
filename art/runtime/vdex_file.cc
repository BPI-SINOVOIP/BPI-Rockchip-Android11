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

#include "vdex_file.h"

#include <sys/mman.h>  // For the PROT_* and MAP_* constants.
#include <sys/stat.h>  // for mkdir()

#include <memory>
#include <unordered_set>

#include <android-base/logging.h>

#include "base/bit_utils.h"
#include "base/leb128.h"
#include "base/stl_util.h"
#include "base/systrace.h"
#include "base/unix_file/fd_file.h"
#include "class_linker.h"
#include "class_loader_context.h"
#include "dex/art_dex_file_loader.h"
#include "dex/class_accessor-inl.h"
#include "dex/dex_file_loader.h"
#include "dex_to_dex_decompiler.h"
#include "gc/heap.h"
#include "gc/space/image_space.h"
#include "quicken_info.h"
#include "runtime.h"
#include "verifier/verifier_deps.h"

namespace art {

constexpr uint8_t VdexFile::VerifierDepsHeader::kVdexInvalidMagic[4];
constexpr uint8_t VdexFile::VerifierDepsHeader::kVdexMagic[4];
constexpr uint8_t VdexFile::VerifierDepsHeader::kVerifierDepsVersion[4];
constexpr uint8_t VdexFile::VerifierDepsHeader::kDexSectionVersion[4];
constexpr uint8_t VdexFile::VerifierDepsHeader::kDexSectionVersionEmpty[4];

bool VdexFile::VerifierDepsHeader::IsMagicValid() const {
  return (memcmp(magic_, kVdexMagic, sizeof(kVdexMagic)) == 0);
}

bool VdexFile::VerifierDepsHeader::IsVerifierDepsVersionValid() const {
  return (memcmp(verifier_deps_version_, kVerifierDepsVersion, sizeof(kVerifierDepsVersion)) == 0);
}

bool VdexFile::VerifierDepsHeader::IsDexSectionVersionValid() const {
  return (memcmp(dex_section_version_, kDexSectionVersion, sizeof(kDexSectionVersion)) == 0) ||
      (memcmp(dex_section_version_, kDexSectionVersionEmpty, sizeof(kDexSectionVersionEmpty)) == 0);
}

bool VdexFile::VerifierDepsHeader::HasDexSection() const {
  return (memcmp(dex_section_version_, kDexSectionVersion, sizeof(kDexSectionVersion)) == 0);
}

VdexFile::VerifierDepsHeader::VerifierDepsHeader(uint32_t number_of_dex_files,
                                                 uint32_t verifier_deps_size,
                                                 bool has_dex_section,
                                                 uint32_t bootclasspath_checksums_size,
                                                 uint32_t class_loader_context_size)
    : number_of_dex_files_(number_of_dex_files),
      verifier_deps_size_(verifier_deps_size),
      bootclasspath_checksums_size_(bootclasspath_checksums_size),
      class_loader_context_size_(class_loader_context_size) {
  memcpy(magic_, kVdexMagic, sizeof(kVdexMagic));
  memcpy(verifier_deps_version_, kVerifierDepsVersion, sizeof(kVerifierDepsVersion));
  if (has_dex_section) {
    memcpy(dex_section_version_, kDexSectionVersion, sizeof(kDexSectionVersion));
  } else {
    memcpy(dex_section_version_, kDexSectionVersionEmpty, sizeof(kDexSectionVersionEmpty));
  }
  DCHECK(IsMagicValid());
  DCHECK(IsVerifierDepsVersionValid());
  DCHECK(IsDexSectionVersionValid());
}

VdexFile::DexSectionHeader::DexSectionHeader(uint32_t dex_size,
                                             uint32_t dex_shared_data_size,
                                             uint32_t quickening_info_size)
    : dex_size_(dex_size),
      dex_shared_data_size_(dex_shared_data_size),
      quickening_info_size_(quickening_info_size) {
}

std::unique_ptr<VdexFile> VdexFile::OpenAtAddress(uint8_t* mmap_addr,
                                                  size_t mmap_size,
                                                  bool mmap_reuse,
                                                  const std::string& vdex_filename,
                                                  bool writable,
                                                  bool low_4gb,
                                                  bool unquicken,
                                                  std::string* error_msg) {
  ScopedTrace trace(("VdexFile::OpenAtAddress " + vdex_filename).c_str());
  if (!OS::FileExists(vdex_filename.c_str())) {
    *error_msg = "File " + vdex_filename + " does not exist.";
    return nullptr;
  }

  std::unique_ptr<File> vdex_file;
  if (writable) {
    vdex_file.reset(OS::OpenFileReadWrite(vdex_filename.c_str()));
  } else {
    vdex_file.reset(OS::OpenFileForReading(vdex_filename.c_str()));
  }
  if (vdex_file == nullptr) {
    *error_msg = "Could not open file " + vdex_filename +
                 (writable ? " for read/write" : "for reading");
    return nullptr;
  }

  int64_t vdex_length = vdex_file->GetLength();
  if (vdex_length == -1) {
    *error_msg = "Could not read the length of file " + vdex_filename;
    return nullptr;
  }

  return OpenAtAddress(mmap_addr,
                       mmap_size,
                       mmap_reuse,
                       vdex_file->Fd(),
                       vdex_length,
                       vdex_filename,
                       writable,
                       low_4gb,
                       unquicken,
                       error_msg);
}

std::unique_ptr<VdexFile> VdexFile::OpenAtAddress(uint8_t* mmap_addr,
                                                  size_t mmap_size,
                                                  bool mmap_reuse,
                                                  int file_fd,
                                                  size_t vdex_length,
                                                  const std::string& vdex_filename,
                                                  bool writable,
                                                  bool low_4gb,
                                                  bool unquicken,
                                                  std::string* error_msg) {
  if (mmap_addr != nullptr && mmap_size < vdex_length) {
    LOG(WARNING) << "Insufficient pre-allocated space to mmap vdex.";
    mmap_addr = nullptr;
    mmap_reuse = false;
  }
  CHECK(!mmap_reuse || mmap_addr != nullptr);
  CHECK(!(writable && unquicken)) << "We don't want to be writing unquickened files out to disk!";
  // Start as PROT_WRITE so we can mprotect back to it if we want to.
  MemMap mmap = MemMap::MapFileAtAddress(
      mmap_addr,
      vdex_length,
      PROT_READ | PROT_WRITE,
      writable ? MAP_SHARED : MAP_PRIVATE,
      file_fd,
      /* start= */ 0u,
      low_4gb,
      vdex_filename.c_str(),
      mmap_reuse,
      /* reservation= */ nullptr,
      error_msg);
  if (!mmap.IsValid()) {
    *error_msg = "Failed to mmap file " + vdex_filename + " : " + *error_msg;
    return nullptr;
  }

  std::unique_ptr<VdexFile> vdex(new VdexFile(std::move(mmap)));
  if (!vdex->IsValid()) {
    *error_msg = "Vdex file is not valid";
    return nullptr;
  }

  if (unquicken && vdex->HasDexSection()) {
    std::vector<std::unique_ptr<const DexFile>> unique_ptr_dex_files;
    if (!vdex->OpenAllDexFiles(&unique_ptr_dex_files, error_msg)) {
      return nullptr;
    }
    // TODO: It would be nice to avoid doing the return-instruction stuff but then we end up not
    // being able to tell if we need dequickening later. Instead just get rid of that too.
    vdex->Unquicken(MakeNonOwningPointerVector(unique_ptr_dex_files),
                    /* decompile_return_instruction= */ true);
    // Update the quickening info size to pretend there isn't any.
    size_t offset = vdex->GetDexSectionHeaderOffset();
    reinterpret_cast<DexSectionHeader*>(vdex->mmap_.Begin() + offset)->quickening_info_size_ = 0;
  }

  if (!writable) {
    vdex->AllowWriting(false);
  }

  return vdex;
}

const uint8_t* VdexFile::GetNextDexFileData(const uint8_t* cursor) const {
  DCHECK(cursor == nullptr || (cursor > Begin() && cursor <= End()));
  if (cursor == nullptr) {
    // Beginning of the iteration, return the first dex file if there is one.
    return HasDexSection() ? DexBegin() + sizeof(QuickeningTableOffsetType) : nullptr;
  } else {
    // Fetch the next dex file. Return null if there is none.
    const uint8_t* data = cursor + reinterpret_cast<const DexFile::Header*>(cursor)->file_size_;
    // Dex files are required to be 4 byte aligned. the OatWriter makes sure they are, see
    // OatWriter::SeekToDexFiles.
    data = AlignUp(data, 4);

    return (data == DexEnd()) ? nullptr : data + sizeof(QuickeningTableOffsetType);
  }
}

void VdexFile::AllowWriting(bool val) const {
  CHECK(mmap_.Protect(val ? (PROT_READ | PROT_WRITE) : PROT_READ));
}

bool VdexFile::OpenAllDexFiles(std::vector<std::unique_ptr<const DexFile>>* dex_files,
                               std::string* error_msg) const {
  const ArtDexFileLoader dex_file_loader;
  size_t i = 0;
  for (const uint8_t* dex_file_start = GetNextDexFileData(nullptr);
       dex_file_start != nullptr;
       dex_file_start = GetNextDexFileData(dex_file_start), ++i) {
    size_t size = reinterpret_cast<const DexFile::Header*>(dex_file_start)->file_size_;
    // TODO: Supply the location information for a vdex file.
    static constexpr char kVdexLocation[] = "";
    std::string location = DexFileLoader::GetMultiDexLocation(i, kVdexLocation);
    std::unique_ptr<const DexFile> dex(dex_file_loader.OpenWithDataSection(
        dex_file_start,
        size,
        /*data_base=*/ nullptr,
        /*data_size=*/ 0u,
        location,
        GetLocationChecksum(i),
        /*oat_dex_file=*/ nullptr,
        /*verify=*/ false,
        /*verify_checksum=*/ false,
        error_msg));
    if (dex == nullptr) {
      return false;
    }
    dex_files->push_back(std::move(dex));
  }
  return true;
}

void VdexFile::UnquickenInPlace(bool decompile_return_instruction) const {
  CHECK_NE(mmap_.GetProtect() & PROT_WRITE, 0)
      << "File not mapped writable. Cannot unquicken! " << mmap_;
  if (HasDexSection()) {
    std::vector<std::unique_ptr<const DexFile>> unique_ptr_dex_files;
    std::string error_msg;
    if (!OpenAllDexFiles(&unique_ptr_dex_files, &error_msg)) {
      return;
    }
    Unquicken(MakeNonOwningPointerVector(unique_ptr_dex_files),
              decompile_return_instruction);
    // Update the quickening info size to pretend there isn't any.
    size_t offset = GetDexSectionHeaderOffset();
    reinterpret_cast<DexSectionHeader*>(mmap_.Begin() + offset)->quickening_info_size_ = 0;
  }
}

void VdexFile::Unquicken(const std::vector<const DexFile*>& target_dex_files,
                         bool decompile_return_instruction) const {
  const uint8_t* source_dex = GetNextDexFileData(nullptr);
  for (const DexFile* target_dex : target_dex_files) {
    UnquickenDexFile(*target_dex, source_dex, decompile_return_instruction);
    source_dex = GetNextDexFileData(source_dex);
  }
  DCHECK(source_dex == nullptr);
}

uint32_t VdexFile::GetQuickeningInfoTableOffset(const uint8_t* source_dex_begin) const {
  DCHECK_GE(source_dex_begin, DexBegin());
  DCHECK_LT(source_dex_begin, DexEnd());
  return reinterpret_cast<const QuickeningTableOffsetType*>(source_dex_begin)[-1];
}

CompactOffsetTable::Accessor VdexFile::GetQuickenInfoOffsetTable(
    const uint8_t* source_dex_begin,
    const ArrayRef<const uint8_t>& quickening_info) const {
  // The offset a is in preheader right before the dex file.
  const uint32_t offset = GetQuickeningInfoTableOffset(source_dex_begin);
  return CompactOffsetTable::Accessor(quickening_info.SubArray(offset).data());
}

CompactOffsetTable::Accessor VdexFile::GetQuickenInfoOffsetTable(
    const DexFile& dex_file,
    const ArrayRef<const uint8_t>& quickening_info) const {
  return GetQuickenInfoOffsetTable(dex_file.Begin(), quickening_info);
}

static ArrayRef<const uint8_t> GetQuickeningInfoAt(const ArrayRef<const uint8_t>& quickening_info,
                                                   uint32_t quickening_offset) {
  // Subtract offset of one since 0 represents unused and cannot be in the table.
  ArrayRef<const uint8_t> remaining = quickening_info.SubArray(quickening_offset - 1);
  return remaining.SubArray(0u, QuickenInfoTable::SizeInBytes(remaining));
}

void VdexFile::UnquickenDexFile(const DexFile& target_dex_file,
                                const DexFile& source_dex_file,
                                bool decompile_return_instruction) const {
  UnquickenDexFile(
      target_dex_file, source_dex_file.Begin(), decompile_return_instruction);
}

void VdexFile::UnquickenDexFile(const DexFile& target_dex_file,
                                const uint8_t* source_dex_begin,
                                bool decompile_return_instruction) const {
  ArrayRef<const uint8_t> quickening_info = GetQuickeningInfo();
  if (quickening_info.empty()) {
    // Bail early if there is no quickening info and no need to decompile. This means there is also
    // no RETURN_VOID to decompile since the empty table takes a non zero amount of space.
    return;
  }
  // Make sure to not unquicken the same code item multiple times.
  std::unordered_set<const dex::CodeItem*> unquickened_code_item;
  CompactOffsetTable::Accessor accessor(GetQuickenInfoOffsetTable(source_dex_begin,
                                                                  quickening_info));
  for (ClassAccessor class_accessor : target_dex_file.GetClasses()) {
    for (const ClassAccessor::Method& method : class_accessor.GetMethods()) {
      const dex::CodeItem* code_item = method.GetCodeItem();
      if (code_item != nullptr && unquickened_code_item.emplace(code_item).second) {
        const uint32_t offset = accessor.GetOffset(method.GetIndex());
        // Offset being 0 means not quickened.
        if (offset != 0u) {
          ArrayRef<const uint8_t> quicken_data = GetQuickeningInfoAt(quickening_info, offset);
          optimizer::ArtDecompileDEX(
              target_dex_file,
              *code_item,
              quicken_data,
              decompile_return_instruction);
        }
      }
    }
  }
}

ArrayRef<const uint8_t> VdexFile::GetQuickenedInfoOf(const DexFile& dex_file,
                                                     uint32_t dex_method_idx) const {
  ArrayRef<const uint8_t> quickening_info = GetQuickeningInfo();
  if (quickening_info.empty()) {
    return ArrayRef<const uint8_t>();
  }
  CHECK_LT(dex_method_idx, dex_file.NumMethodIds());
  const uint32_t quickening_offset =
      GetQuickenInfoOffsetTable(dex_file, quickening_info).GetOffset(dex_method_idx);
  if (quickening_offset == 0u) {
    return ArrayRef<const uint8_t>();
  }
  return GetQuickeningInfoAt(quickening_info, quickening_offset);
}

static std::string ComputeBootClassPathChecksumString() {
  Runtime* const runtime = Runtime::Current();
  // Do not include boot image extension checksums, use their dex file checksums instead. Unlike
  // oat files, vdex files do not reference anything in image spaces, so there is no reason why
  // loading or not loading a boot image extension would affect the validity of the vdex file.
  // Note: Update of a boot class path module such as conscrypt invalidates the vdex file anyway.
  ArrayRef<gc::space::ImageSpace* const> image_spaces(runtime->GetHeap()->GetBootImageSpaces());
  size_t boot_image_components =
      image_spaces.empty() ? 0u : image_spaces[0]->GetImageHeader().GetComponentCount();
  return gc::space::ImageSpace::GetBootClassPathChecksums(
          image_spaces.SubArray(/*pos=*/ 0u, boot_image_components),
          ArrayRef<const DexFile* const>(runtime->GetClassLinker()->GetBootClassPath()));
}

static bool CreateDirectories(const std::string& child_path, /* out */ std::string* error_msg) {
  size_t last_slash_pos = child_path.find_last_of('/');
  CHECK_NE(last_slash_pos, std::string::npos) << "Invalid path: " << child_path;
  std::string parent_path = child_path.substr(0, last_slash_pos);
  if (OS::DirectoryExists(parent_path.c_str())) {
    return true;
  } else if (CreateDirectories(parent_path, error_msg)) {
    if (mkdir(parent_path.c_str(), 0700) == 0) {
      return true;
    }
    *error_msg = "Could not create directory " + parent_path;
    return false;
  } else {
    return false;
  }
}

bool VdexFile::WriteToDisk(const std::string& path,
                           const std::vector<const DexFile*>& dex_files,
                           const verifier::VerifierDeps& verifier_deps,
                           const std::string& class_loader_context,
                           std::string* error_msg) {
  std::vector<uint8_t> verifier_deps_data;
  verifier_deps.Encode(dex_files, &verifier_deps_data);

  std::string boot_checksum = ComputeBootClassPathChecksumString();
  DCHECK_NE(boot_checksum, "");

  VdexFile::VerifierDepsHeader deps_header(dex_files.size(),
                                           verifier_deps_data.size(),
                                           /* has_dex_section= */ false,
                                           boot_checksum.size(),
                                           class_loader_context.size());

  if (!CreateDirectories(path, error_msg)) {
    return false;
  }

  std::unique_ptr<File> out(OS::CreateEmptyFileWriteOnly(path.c_str()));
  if (out == nullptr) {
    *error_msg = "Could not open " + path + " for writing";
    return false;
  }

  if (!out->WriteFully(reinterpret_cast<const char*>(&deps_header), sizeof(deps_header))) {
    *error_msg = "Could not write vdex header to " + path;
    out->Unlink();
    return false;
  }

  for (const DexFile* dex_file : dex_files) {
    const uint32_t* checksum_ptr = &dex_file->GetHeader().checksum_;
    static_assert(sizeof(*checksum_ptr) == sizeof(VdexFile::VdexChecksum));
    if (!out->WriteFully(reinterpret_cast<const char*>(checksum_ptr),
                         sizeof(VdexFile::VdexChecksum))) {
      *error_msg = "Could not write dex checksums to " + path;
      out->Unlink();
    return false;
    }
  }

  if (!out->WriteFully(reinterpret_cast<const char*>(verifier_deps_data.data()),
                       verifier_deps_data.size())) {
    *error_msg = "Could not write verifier deps to " + path;
    out->Unlink();
    return false;
  }

  if (!out->WriteFully(boot_checksum.c_str(), boot_checksum.size())) {
    *error_msg = "Could not write boot classpath checksum to " + path;
    out->Unlink();
    return false;
  }

  if (!out->WriteFully(class_loader_context.c_str(), class_loader_context.size())) {
    *error_msg = "Could not write class loader context to " + path;
    out->Unlink();
    return false;
  }

  if (out->FlushClose() != 0) {
    *error_msg = "Could not flush and close " + path;
    out->Unlink();
    return false;
  }

  return true;
}

bool VdexFile::MatchesDexFileChecksums(const std::vector<const DexFile::Header*>& dex_headers)
    const {
  const VerifierDepsHeader& header = GetVerifierDepsHeader();
  if (dex_headers.size() != header.GetNumberOfDexFiles()) {
    LOG(WARNING) << "Mismatch of number of dex files in vdex (expected="
        << header.GetNumberOfDexFiles() << ", actual=" << dex_headers.size() << ")";
    return false;
  }
  const VdexChecksum* checksums = header.GetDexChecksumsArray();
  for (size_t i = 0; i < dex_headers.size(); ++i) {
    if (checksums[i] != dex_headers[i]->checksum_) {
      LOG(WARNING) << "Mismatch of dex file checksum in vdex (index=" << i << ")";
      return false;
    }
  }
  return true;
}

bool VdexFile::MatchesBootClassPathChecksums() const {
  ArrayRef<const uint8_t> data = GetBootClassPathChecksumData();
  std::string vdex(reinterpret_cast<const char*>(data.data()), data.size());
  std::string runtime = ComputeBootClassPathChecksumString();
  if (vdex == runtime) {
    return true;
  } else {
    LOG(WARNING) << "Mismatch of boot class path checksum in vdex (expected="
        << vdex << ", actual=" << runtime << ")";
    return false;
  }
}

bool VdexFile::MatchesClassLoaderContext(const ClassLoaderContext& context) const {
  ArrayRef<const uint8_t> data = GetClassLoaderContextData();
  std::string spec(reinterpret_cast<const char*>(data.data()), data.size());
  ClassLoaderContext::VerificationResult result = context.VerifyClassLoaderContextMatch(spec);
  if (result != ClassLoaderContext::VerificationResult::kMismatch) {
    return true;
  } else {
    LOG(WARNING) << "Mismatch of class loader context in vdex (expected="
        << spec << ", actual=" << context.EncodeContextForOatFile("") << ")";
    return false;
  }
}

}  // namespace art
