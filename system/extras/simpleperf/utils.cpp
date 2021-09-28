/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "utils.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <string>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <build/version.h>

#include <7zCrc.h>
#include <Xz.h>
#include <XzCrc64.h>

void OneTimeFreeAllocator::Clear() {
  for (auto& p : v_) {
    delete[] p;
  }
  v_.clear();
  cur_ = nullptr;
  end_ = nullptr;
}

const char* OneTimeFreeAllocator::AllocateString(std::string_view s) {
  size_t size = s.size() + 1;
  if (cur_ + size > end_) {
    size_t alloc_size = std::max(size, unit_size_);
    char* p = new char[alloc_size];
    v_.push_back(p);
    cur_ = p;
    end_ = p + alloc_size;
  }
  strcpy(cur_, s.data());
  const char* result = cur_;
  cur_ += size;
  return result;
}


android::base::unique_fd FileHelper::OpenReadOnly(const std::string& filename) {
    int fd = TEMP_FAILURE_RETRY(open(filename.c_str(), O_RDONLY | O_BINARY));
    return android::base::unique_fd(fd);
}

android::base::unique_fd FileHelper::OpenWriteOnly(const std::string& filename) {
    int fd = TEMP_FAILURE_RETRY(open(filename.c_str(), O_WRONLY | O_BINARY | O_CREAT, 0644));
    return android::base::unique_fd(fd);
}

std::unique_ptr<ArchiveHelper> ArchiveHelper::CreateInstance(const std::string& filename) {
  android::base::unique_fd fd = FileHelper::OpenReadOnly(filename);
  if (fd == -1) {
    return nullptr;
  }
  // Simpleperf relies on ArchiveHelper to check if a file is zip file. We expect much more elf
  // files than zip files in a process map. In order to detect invalid zip files fast, we add a
  // check of magic number here. Note that OpenArchiveFd() detects invalid zip files in a thorough
  // way, but it usually needs reading at least 64K file data.
  static const char zip_preamble[] = {0x50, 0x4b, 0x03, 0x04 };
  char buf[4];
  if (!android::base::ReadFully(fd, buf, 4) || memcmp(buf, zip_preamble, 4) != 0) {
    return nullptr;
  }
  if (lseek(fd, 0, SEEK_SET) == -1) {
    return nullptr;
  }
  ZipArchiveHandle handle;
  int result = OpenArchiveFd(fd.release(), filename.c_str(), &handle);
  if (result != 0) {
    LOG(ERROR) << "Failed to open archive " << filename << ": " << ErrorCodeString(result);
    return nullptr;
  }
  return std::unique_ptr<ArchiveHelper>(new ArchiveHelper(handle, filename));
}

ArchiveHelper::~ArchiveHelper() {
  CloseArchive(handle_);
}

bool ArchiveHelper::IterateEntries(
    const std::function<bool(ZipEntry&, const std::string&)>& callback) {
  void* iteration_cookie;
  if (StartIteration(handle_, &iteration_cookie) < 0) {
    LOG(ERROR) << "Failed to iterate " << filename_;
    return false;
  }
  ZipEntry zentry;
  std::string zname;
  int result;
  while ((result = Next(iteration_cookie, &zentry, &zname)) == 0) {
    if (!callback(zentry, zname)) {
      break;
    }
  }
  EndIteration(iteration_cookie);
  if (result == -2) {
    LOG(ERROR) << "Failed to iterate " << filename_;
    return false;
  }
  return true;
}

bool ArchiveHelper::FindEntry(const std::string& name, ZipEntry* entry) {
  int result = ::FindEntry(handle_, name, entry);
  if (result != 0) {
    LOG(ERROR) << "Failed to find " << name << " in " << filename_;
    return false;
  }
  return true;
}

bool ArchiveHelper::GetEntryData(ZipEntry& entry, std::vector<uint8_t>* data) {
  data->resize(entry.uncompressed_length);
  if (ExtractToMemory(handle_, &entry, data->data(), data->size()) != 0) {
    LOG(ERROR) << "Failed to extract entry at " << entry.offset << " in " << filename_;
    return false;
  }
  return true;
}

int ArchiveHelper::GetFd() {
  return GetFileDescriptor(handle_);
}

void PrintIndented(size_t indent, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  printf("%*s", static_cast<int>(indent * 2), "");
  vprintf(fmt, ap);
  va_end(ap);
}

void FprintIndented(FILE* fp, size_t indent, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(fp, "%*s", static_cast<int>(indent * 2), "");
  vfprintf(fp, fmt, ap);
  va_end(ap);
}

bool IsPowerOfTwo(uint64_t value) {
  return (value != 0 && ((value & (value - 1)) == 0));
}

std::vector<std::string> GetEntriesInDir(const std::string& dirpath) {
  std::vector<std::string> result;
  DIR* dir = opendir(dirpath.c_str());
  if (dir == nullptr) {
    PLOG(DEBUG) << "can't open dir " << dirpath;
    return result;
  }
  dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    result.push_back(entry->d_name);
  }
  closedir(dir);
  return result;
}

std::vector<std::string> GetSubDirs(const std::string& dirpath) {
  std::vector<std::string> entries = GetEntriesInDir(dirpath);
  std::vector<std::string> result;
  for (size_t i = 0; i < entries.size(); ++i) {
    if (IsDir(dirpath + OS_PATH_SEPARATOR + entries[i])) {
      result.push_back(std::move(entries[i]));
    }
  }
  return result;
}

bool IsDir(const std::string& dirpath) {
  struct stat st;
  if (stat(dirpath.c_str(), &st) == 0) {
    if (S_ISDIR(st.st_mode)) {
      return true;
    }
  }
  return false;
}

bool IsRegularFile(const std::string& filename) {
  struct stat st;
  if (stat(filename.c_str(), &st) == 0) {
    if (S_ISREG(st.st_mode)) {
      return true;
    }
  }
  return false;
}

uint64_t GetFileSize(const std::string& filename) {
  struct stat st;
  if (stat(filename.c_str(), &st) == 0) {
    return static_cast<uint64_t>(st.st_size);
  }
  return 0;
}

bool MkdirWithParents(const std::string& path) {
  size_t prev_end = 0;
  while (prev_end < path.size()) {
    size_t next_end = path.find('/', prev_end + 1);
    if (next_end == std::string::npos) {
      break;
    }
    std::string dir_path = path.substr(0, next_end);
    if (!IsDir(dir_path)) {
#if defined(_WIN32)
      int ret = mkdir(dir_path.c_str());
#else
      int ret = mkdir(dir_path.c_str(), 0755);
#endif
      if (ret != 0) {
        PLOG(ERROR) << "failed to create dir " << dir_path;
        return false;
      }
    }
    prev_end = next_end;
  }
  return true;
}

static void* xz_alloc(ISzAllocPtr, size_t size) {
  return malloc(size);
}

static void xz_free(ISzAllocPtr, void* address) {
  free(address);
}

bool XzDecompress(const std::string& compressed_data, std::string* decompressed_data) {
  ISzAlloc alloc;
  CXzUnpacker state;
  alloc.Alloc = xz_alloc;
  alloc.Free = xz_free;
  XzUnpacker_Construct(&state, &alloc);
  CrcGenerateTable();
  Crc64GenerateTable();
  size_t src_offset = 0;
  size_t dst_offset = 0;
  std::string dst(compressed_data.size(), ' ');

  ECoderStatus status = CODER_STATUS_NOT_FINISHED;
  while (status == CODER_STATUS_NOT_FINISHED) {
    dst.resize(dst.size() * 2);
    size_t src_remaining = compressed_data.size() - src_offset;
    size_t dst_remaining = dst.size() - dst_offset;
    int res = XzUnpacker_Code(&state, reinterpret_cast<Byte*>(&dst[dst_offset]), &dst_remaining,
                              reinterpret_cast<const Byte*>(&compressed_data[src_offset]),
                              &src_remaining, true, CODER_FINISH_ANY, &status);
    if (res != SZ_OK) {
      LOG(ERROR) << "LZMA decompression failed with error " << res;
      XzUnpacker_Free(&state);
      return false;
    }
    src_offset += src_remaining;
    dst_offset += dst_remaining;
  }
  XzUnpacker_Free(&state);
  if (!XzUnpacker_IsStreamWasFinished(&state)) {
    LOG(ERROR) << "LZMA decompresstion failed due to incomplete stream";
    return false;
  }
  dst.resize(dst_offset);
  *decompressed_data = std::move(dst);
  return true;
}

static std::map<std::string, android::base::LogSeverity> log_severity_map = {
    {"verbose", android::base::VERBOSE},
    {"debug", android::base::DEBUG},
    {"info", android::base::INFO},
    {"warning", android::base::WARNING},
    {"error", android::base::ERROR},
    {"fatal", android::base::FATAL},
};
bool GetLogSeverity(const std::string& name, android::base::LogSeverity* severity) {
  auto it = log_severity_map.find(name);
  if (it != log_severity_map.end()) {
    *severity = it->second;
    return true;
  }
  return false;
}

std::string GetLogSeverityName() {
  android::base::LogSeverity severity = android::base::GetMinimumLogSeverity();
  for (auto& pair : log_severity_map) {
    if (severity == pair.second) {
      return pair.first;
    }
  }
  return "info";
}

bool IsRoot() {
  static int is_root = -1;
  if (is_root == -1) {
#if defined(__linux__)
    is_root = (getuid() == 0) ? 1 : 0;
#else
    is_root = 0;
#endif
  }
  return is_root == 1;
}

bool ProcessKernelSymbols(std::string& symbol_data,
                          const std::function<bool(const KernelSymbol&)>& callback) {
  char* p = &symbol_data[0];
  char* data_end = p + symbol_data.size();
  while (p < data_end) {
    char* line_end = strchr(p, '\n');
    if (line_end != nullptr) {
      *line_end = '\0';
    }
    size_t line_size = (line_end != nullptr) ? (line_end - p) : (data_end - p);
    // Parse line like: ffffffffa005c4e4 d __warned.41698       [libsas]
    char name[line_size];
    char module[line_size];
    strcpy(module, "");

    KernelSymbol symbol;
    int ret = sscanf(p, "%" PRIx64 " %c %s%s", &symbol.addr, &symbol.type, name, module);
    if (line_end != nullptr) {
      *line_end = '\n';
      p = line_end + 1;
    } else {
      p = data_end;
    }
    if (ret >= 3) {
      symbol.name = name;
      size_t module_len = strlen(module);
      if (module_len > 2 && module[0] == '[' && module[module_len - 1] == ']') {
        module[module_len - 1] = '\0';
        symbol.module = &module[1];
      } else {
        symbol.module = nullptr;
      }

      if (callback(symbol)) {
        return true;
      }
    }
  }
  return false;
}

size_t GetPageSize() {
#if defined(__linux__)
  return sysconf(_SC_PAGE_SIZE);
#else
  return 4096;
#endif
}

uint64_t ConvertBytesToValue(const char* bytes, uint32_t size) {
  if (size > 8) {
    LOG(FATAL) << "unexpected size " << size << " in ConvertBytesToValue";
  }
  uint64_t result = 0;
  int shift = 0;
  for (uint32_t i = 0; i < size; ++i) {
    uint64_t tmp = static_cast<unsigned char>(bytes[i]);
    result |= tmp << shift;
    shift += 8;
  }
  return result;
}

timeval SecondToTimeval(double time_in_sec) {
  timeval tv;
  tv.tv_sec = static_cast<time_t>(time_in_sec);
  tv.tv_usec = static_cast<int>((time_in_sec - tv.tv_sec) * 1000000);
  return tv;
}

constexpr int SIMPLEPERF_VERSION = 1;

std::string GetSimpleperfVersion() {
  return android::base::StringPrintf("%d.build.%s", SIMPLEPERF_VERSION,
                                     android::build::GetBuildNumber().c_str());
}

std::vector<int> GetCpusFromString(const std::string& s) {
  std::set<int> cpu_set;
  bool have_dash = false;
  const char* p = s.c_str();
  char* endp;
  int last_cpu;
  int cpu;
  // Parse line like: 0,1-3, 5, 7-8
  while ((cpu = static_cast<int>(strtol(p, &endp, 10))) != 0 || endp != p) {
    if (have_dash && !cpu_set.empty()) {
      for (int t = last_cpu + 1; t < cpu; ++t) {
        cpu_set.insert(t);
      }
    }
    have_dash = false;
    cpu_set.insert(cpu);
    last_cpu = cpu;
    p = endp;
    while (!isdigit(*p) && *p != '\0') {
      if (*p == '-') {
        have_dash = true;
      }
      ++p;
    }
  }
  return std::vector<int>(cpu_set.begin(), cpu_set.end());
}
