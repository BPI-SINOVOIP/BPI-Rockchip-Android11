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

#include "incfs.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <gtest/gtest.h>
#include <selinux/selinux.h>
#include <sys/select.h>
#include <unistd.h>

#include <optional>
#include <thread>

#include "path.h"

using namespace android::incfs;
using namespace std::literals;

static bool exists(std::string_view path) {
    return access(path.data(), F_OK) == 0;
}

struct ScopedUnmount {
    std::string path_;
    explicit ScopedUnmount(std::string&& path) : path_(std::move(path)) {}
    ~ScopedUnmount() { unmount(path_); }
};

class IncFsTest : public ::testing::Test {
protected:
    virtual void SetUp() {
        tmp_dir_for_mount_.emplace();
        mount_dir_path_ = tmp_dir_for_mount_->path;
        tmp_dir_for_image_.emplace();
        image_dir_path_ = tmp_dir_for_image_->path;
        ASSERT_TRUE(exists(image_dir_path_));
        ASSERT_TRUE(exists(mount_dir_path_));
        if (!enabled()) {
            GTEST_SKIP() << "test not supported: IncFS is not enabled";
        } else {
            control_ =
                    mount(image_dir_path_, mount_dir_path_,
                          MountOptions{.readLogBufferPages = 4,
                                       .defaultReadTimeoutMs = std::chrono::duration_cast<
                                                                       std::chrono::milliseconds>(
                                                                       kDefaultReadTimeout)
                                                                       .count()});
            ASSERT_TRUE(control_.cmd() >= 0) << "Expected >= 0 got " << control_.cmd();
            ASSERT_TRUE(control_.pendingReads() >= 0);
            ASSERT_TRUE(control_.logs() >= 0);
            checkRestoreconResult(mountPath(INCFS_PENDING_READS_FILENAME));
            checkRestoreconResult(mountPath(INCFS_LOG_FILENAME));
        }
    }

    static void checkRestoreconResult(std::string_view path) {
        char* ctx = nullptr;
        ASSERT_NE(-1, getfilecon(path.data(), &ctx));
        ASSERT_EQ("u:object_r:shell_data_file:s0", std::string(ctx));
        freecon(ctx);
    }

    virtual void TearDown() {
        unmount(mount_dir_path_);
        tmp_dir_for_image_.reset();
        tmp_dir_for_mount_.reset();
        EXPECT_FALSE(exists(image_dir_path_));
        EXPECT_FALSE(exists(mount_dir_path_));
    }

    template <class... Paths>
    std::string mountPath(Paths&&... paths) const {
        return path::join(mount_dir_path_, std::forward<Paths>(paths)...);
    }

    static IncFsFileId fileId(uint64_t i) {
        IncFsFileId id = {};
        static_assert(sizeof(id) >= sizeof(i));
        memcpy(&id, &i, sizeof(i));
        return id;
    }

    static IncFsSpan metadata(std::string_view sv) {
        return {.data = sv.data(), .size = IncFsSize(sv.size())};
    }

    int makeFileWithHash(int id) {
        // calculate the required size for two leaf hash blocks
        constexpr auto size =
                (INCFS_DATA_FILE_BLOCK_SIZE / INCFS_MAX_HASH_SIZE + 1) * INCFS_DATA_FILE_BLOCK_SIZE;

        // assemble a signature/hashing data for it
        struct __attribute__((packed)) Signature {
            uint32_t version = INCFS_SIGNATURE_VERSION;
            uint32_t hashingSize = sizeof(hashing);
            struct __attribute__((packed)) Hashing {
                uint32_t algo = INCFS_HASH_TREE_SHA256;
                uint8_t log2Blocksize = 12;
                uint32_t saltSize = 0;
                uint32_t rootHashSize = INCFS_MAX_HASH_SIZE;
                char rootHash[INCFS_MAX_HASH_SIZE] = {};
            } hashing;
            uint32_t signingSize = 0;
        } signature;

        int res = makeFile(control_, mountPath(test_file_name_), 0555, fileId(id),
                           {.size = size,
                            .signature = {.data = (char*)&signature, .size = sizeof(signature)}});
        EXPECT_EQ(0, res);
        return res ? -1 : size;
    }

    static int sizeToPages(int size) {
        return (size + INCFS_DATA_FILE_BLOCK_SIZE - 1) / INCFS_DATA_FILE_BLOCK_SIZE;
    }

    void writeTestRanges(int id, int size) {
        auto wfd = openForSpecialOps(control_, fileId(id));
        ASSERT_GE(wfd.get(), 0);

        auto lastPage = sizeToPages(size) - 1;

        std::vector<char> data(INCFS_DATA_FILE_BLOCK_SIZE);
        DataBlock blocks[] = {{
                                      .fileFd = wfd.get(),
                                      .pageIndex = 1,
                                      .compression = INCFS_COMPRESSION_KIND_NONE,
                                      .dataSize = (uint32_t)data.size(),
                                      .data = data.data(),
                              },
                              {
                                      .fileFd = wfd.get(),
                                      .pageIndex = 2,
                                      .compression = INCFS_COMPRESSION_KIND_NONE,
                                      .dataSize = (uint32_t)data.size(),
                                      .data = data.data(),
                              },
                              {
                                      .fileFd = wfd.get(),
                                      .pageIndex = 10,
                                      .compression = INCFS_COMPRESSION_KIND_NONE,
                                      .dataSize = (uint32_t)data.size(),
                                      .data = data.data(),
                              },
                              {
                                      .fileFd = wfd.get(),
                                      // last data page
                                      .pageIndex = lastPage,
                                      .compression = INCFS_COMPRESSION_KIND_NONE,
                                      .dataSize = (uint32_t)data.size(),
                                      .data = data.data(),
                              },
                              {
                                      .fileFd = wfd.get(),
                                      // first hash page
                                      .pageIndex = 0,
                                      .compression = INCFS_COMPRESSION_KIND_NONE,
                                      .dataSize = (uint32_t)data.size(),
                                      .kind = INCFS_BLOCK_KIND_HASH,
                                      .data = data.data(),
                              },
                              {
                                      .fileFd = wfd.get(),
                                      .pageIndex = 2,
                                      .compression = INCFS_COMPRESSION_KIND_NONE,
                                      .dataSize = (uint32_t)data.size(),
                                      .kind = INCFS_BLOCK_KIND_HASH,
                                      .data = data.data(),
                              }};
        ASSERT_EQ((int)std::size(blocks), writeBlocks({blocks, std::size(blocks)}));
    }

    std::string mount_dir_path_;
    std::optional<TemporaryDir> tmp_dir_for_mount_;
    std::string image_dir_path_;
    std::optional<TemporaryDir> tmp_dir_for_image_;
    inline static const std::string_view test_file_name_ = "test.txt"sv;
    inline static const std::string_view test_dir_name_ = "test_dir"sv;
    inline static const int test_file_size_ = INCFS_DATA_FILE_BLOCK_SIZE;
    Control control_;
};

TEST_F(IncFsTest, GetIncfsFeatures) {
    ASSERT_NE(features(), none);
}

TEST_F(IncFsTest, FalseIncfsPath) {
    TemporaryDir test_dir;
    ASSERT_FALSE(isIncFsPath(test_dir.path));
}

TEST_F(IncFsTest, TrueIncfsPath) {
    ASSERT_TRUE(isIncFsPath(mount_dir_path_));
}

TEST_F(IncFsTest, TrueIncfsPathForBindMount) {
    TemporaryDir tmp_dir_to_bind;
    ASSERT_EQ(0, makeDir(control_, mountPath(test_dir_name_)));
    ASSERT_EQ(0, bindMount(mountPath(test_dir_name_), tmp_dir_to_bind.path));
    ScopedUnmount su(tmp_dir_to_bind.path);
    ASSERT_TRUE(isIncFsPath(tmp_dir_to_bind.path));
}

TEST_F(IncFsTest, Control) {
    ASSERT_TRUE(control_);
    EXPECT_GE(IncFs_GetControlFd(control_, CMD), 0);
    EXPECT_GE(IncFs_GetControlFd(control_, PENDING_READS), 0);
    EXPECT_GE(IncFs_GetControlFd(control_, LOGS), 0);

    auto fds = control_.releaseFds();
    EXPECT_GE(fds.size(), size_t(3));
    EXPECT_GE(fds[0].get(), 0);
    EXPECT_GE(fds[1].get(), 0);
    EXPECT_GE(fds[2].get(), 0);
    ASSERT_TRUE(control_);
    EXPECT_LT(IncFs_GetControlFd(control_, CMD), 0);
    EXPECT_LT(IncFs_GetControlFd(control_, PENDING_READS), 0);
    EXPECT_LT(IncFs_GetControlFd(control_, LOGS), 0);

    control_.close();
    EXPECT_FALSE(control_);

    auto control = IncFs_CreateControl(fds[0].release(), fds[1].release(), fds[2].release());
    ASSERT_TRUE(control);
    EXPECT_GE(IncFs_GetControlFd(control, CMD), 0);
    EXPECT_GE(IncFs_GetControlFd(control, PENDING_READS), 0);
    EXPECT_GE(IncFs_GetControlFd(control, LOGS), 0);
    IncFsFd rawFds[3];
    EXPECT_EQ(-EINVAL, IncFs_ReleaseControlFds(nullptr, rawFds, 3));
    EXPECT_EQ(-EINVAL, IncFs_ReleaseControlFds(control, nullptr, 3));
    EXPECT_EQ(-ERANGE, IncFs_ReleaseControlFds(control, rawFds, 2));
    EXPECT_EQ(3, IncFs_ReleaseControlFds(control, rawFds, 3));
    EXPECT_GE(rawFds[0], 0);
    EXPECT_GE(rawFds[1], 0);
    EXPECT_GE(rawFds[2], 0);
    ::close(rawFds[0]);
    ::close(rawFds[1]);
    ::close(rawFds[2]);
    IncFs_DeleteControl(control);
}

TEST_F(IncFsTest, MakeDir) {
    const auto dir_path = mountPath(test_dir_name_);
    ASSERT_FALSE(exists(dir_path));
    ASSERT_EQ(makeDir(control_, dir_path), 0);
    ASSERT_TRUE(exists(dir_path));
}

TEST_F(IncFsTest, MakeDirs) {
    const auto dir_path = mountPath(test_dir_name_);
    ASSERT_FALSE(exists(dir_path));
    ASSERT_EQ(makeDirs(control_, dir_path), 0);
    ASSERT_TRUE(exists(dir_path));
    ASSERT_EQ(makeDirs(control_, dir_path), 0);
    auto nested = dir_path + "/couple/more/nested/levels";
    ASSERT_EQ(makeDirs(control_, nested), 0);
    ASSERT_TRUE(exists(nested));
    ASSERT_NE(makeDirs(control_, "/"), 0);
}

TEST_F(IncFsTest, BindMount) {
    {
        TemporaryDir tmp_dir_to_bind;
        ASSERT_EQ(0, makeDir(control_, mountPath(test_dir_name_)));
        ASSERT_EQ(0, bindMount(mountPath(test_dir_name_), tmp_dir_to_bind.path));
        ScopedUnmount su(tmp_dir_to_bind.path);
        const auto test_file = mountPath(test_dir_name_, test_file_name_);
        ASSERT_FALSE(exists(test_file.c_str())) << "Present: " << test_file;
        ASSERT_EQ(0,
                  makeFile(control_, test_file, 0555, fileId(1),
                           {.size = test_file_size_, .metadata = metadata("md")}));
        ASSERT_TRUE(exists(test_file.c_str())) << "Missing: " << test_file;
        const auto file_binded_path = path::join(tmp_dir_to_bind.path, test_file_name_);
        ASSERT_TRUE(exists(file_binded_path.c_str())) << "Missing: " << file_binded_path;
    }

    {
        // Don't allow binding the root
        TemporaryDir tmp_dir_to_bind;
        ASSERT_EQ(-EINVAL, bindMount(mount_dir_path_, tmp_dir_to_bind.path));
    }
}

TEST_F(IncFsTest, Root) {
    ASSERT_EQ(mount_dir_path_, root(control_)) << "Error: " << errno;
}

TEST_F(IncFsTest, RootInvalidControl) {
    const TemporaryFile tmp_file;
    auto control{createControl(tmp_file.fd, -1, -1)};
    ASSERT_EQ("", root(control)) << "Error: " << errno;
}

TEST_F(IncFsTest, Open) {
    Control control = open(mount_dir_path_);
    ASSERT_TRUE(control.cmd() >= 0);
    ASSERT_TRUE(control.pendingReads() >= 0);
    ASSERT_TRUE(control.logs() >= 0);
}

TEST_F(IncFsTest, OpenFail) {
    TemporaryDir tmp_dir_to_bind;
    Control control = open(tmp_dir_to_bind.path);
    ASSERT_TRUE(control.cmd() < 0);
    ASSERT_TRUE(control.pendingReads() < 0);
    ASSERT_TRUE(control.logs() < 0);
}

TEST_F(IncFsTest, MakeFile) {
    ASSERT_EQ(0, makeDir(control_, mountPath(test_dir_name_)));
    const auto file_path = mountPath(test_dir_name_, test_file_name_);
    ASSERT_FALSE(exists(file_path));
    ASSERT_EQ(0,
              makeFile(control_, file_path, 0111, fileId(1),
                       {.size = test_file_size_, .metadata = metadata("md")}));
    struct stat s;
    ASSERT_EQ(0, stat(file_path.c_str(), &s));
    ASSERT_EQ(test_file_size_, (int)s.st_size);
}

TEST_F(IncFsTest, MakeFile0) {
    ASSERT_EQ(0, makeDir(control_, mountPath(test_dir_name_)));
    const auto file_path = mountPath(test_dir_name_, ".info");
    ASSERT_FALSE(exists(file_path));
    ASSERT_EQ(0,
              makeFile(control_, file_path, 0555, fileId(1),
                       {.size = 0, .metadata = metadata("mdsdfhjasdkfas l;jflaskdjf")}));
    struct stat s;
    ASSERT_EQ(0, stat(file_path.c_str(), &s));
    ASSERT_EQ(0, (int)s.st_size);
}

TEST_F(IncFsTest, GetFileId) {
    auto id = fileId(1);
    ASSERT_EQ(0,
              makeFile(control_, mountPath(test_file_name_), 0555, id,
                       {.size = test_file_size_, .metadata = metadata("md")}));
    EXPECT_EQ(id, getFileId(control_, mountPath(test_file_name_))) << "errno = " << errno;
    EXPECT_EQ(kIncFsInvalidFileId, getFileId(control_, test_file_name_));
    EXPECT_EQ(kIncFsInvalidFileId, getFileId(control_, "asdf"));
    EXPECT_EQ(kIncFsInvalidFileId, getFileId({}, mountPath(test_file_name_)));
}

TEST_F(IncFsTest, GetMetaData) {
    const std::string_view md = "abc"sv;
    ASSERT_EQ(0,
              makeFile(control_, mountPath(test_file_name_), 0555, fileId(1),
                       {.size = test_file_size_, .metadata = metadata(md)}));
    {
        const auto raw_metadata = getMetadata(control_, mountPath(test_file_name_));
        ASSERT_NE(0u, raw_metadata.size()) << errno;
        const std::string result(raw_metadata.begin(), raw_metadata.end());
        ASSERT_EQ(md, result);
    }
    {
        const auto raw_metadata = getMetadata(control_, fileId(1));
        ASSERT_NE(0u, raw_metadata.size()) << errno;
        const std::string result(raw_metadata.begin(), raw_metadata.end());
        ASSERT_EQ(md, result);
    }
}

TEST_F(IncFsTest, LinkAndUnlink) {
    ASSERT_EQ(0, makeFile(control_, mountPath(test_file_name_), 0555, fileId(1), {.size = 0}));
    ASSERT_EQ(0, makeDir(control_, mountPath(test_dir_name_)));
    const std::string_view test_file = "test1.txt"sv;
    const auto linked_file_path = mountPath(test_dir_name_, test_file);
    ASSERT_FALSE(exists(linked_file_path));
    ASSERT_EQ(0, link(control_, mountPath(test_file_name_), linked_file_path));
    ASSERT_TRUE(exists(linked_file_path));
    ASSERT_EQ(0, unlink(control_, linked_file_path));
    ASSERT_FALSE(exists(linked_file_path));
}

TEST_F(IncFsTest, WriteBlocksAndPageRead) {
    const auto id = fileId(1);
    ASSERT_TRUE(control_.logs() >= 0);
    ASSERT_EQ(0,
              makeFile(control_, mountPath(test_file_name_), 0555, id, {.size = test_file_size_}));
    auto fd = openForSpecialOps(control_, fileId(1));
    ASSERT_GE(fd.get(), 0);

    std::vector<char> data(INCFS_DATA_FILE_BLOCK_SIZE);
    auto block = DataBlock{
            .fileFd = fd.get(),
            .pageIndex = 0,
            .compression = INCFS_COMPRESSION_KIND_NONE,
            .dataSize = (uint32_t)data.size(),
            .data = data.data(),
    };
    ASSERT_EQ(1, writeBlocks({&block, 1}));

    std::thread wait_page_read_thread([&]() {
        std::vector<ReadInfo> reads;
        ASSERT_EQ(WaitResult::HaveData,
                  waitForPageReads(control_, std::chrono::seconds(5), &reads));
        ASSERT_FALSE(reads.empty());
        ASSERT_EQ(0, memcmp(&id, &reads[0].id, sizeof(id)));
        ASSERT_EQ(0, int(reads[0].block));
    });

    const auto file_path = mountPath(test_file_name_);
    const android::base::unique_fd readFd(open(file_path.c_str(), O_RDONLY | O_CLOEXEC | O_BINARY));
    ASSERT_TRUE(readFd >= 0);
    char buf[INCFS_DATA_FILE_BLOCK_SIZE];
    ASSERT_TRUE(android::base::ReadFully(readFd, buf, sizeof(buf)));
    wait_page_read_thread.join();
}

TEST_F(IncFsTest, WaitForPendingReads) {
    const auto id = fileId(1);
    ASSERT_EQ(0,
              makeFile(control_, mountPath(test_file_name_), 0555, id, {.size = test_file_size_}));

    std::thread wait_pending_read_thread([&]() {
        std::vector<ReadInfo> pending_reads;
        ASSERT_EQ(WaitResult::HaveData,
                  waitForPendingReads(control_, std::chrono::seconds(10), &pending_reads));
        ASSERT_GT(pending_reads.size(), 0u);
        ASSERT_EQ(0, memcmp(&id, &pending_reads[0].id, sizeof(id)));
        ASSERT_EQ(0, (int)pending_reads[0].block);

        auto fd = openForSpecialOps(control_, fileId(1));
        ASSERT_GE(fd.get(), 0);

        std::vector<char> data(INCFS_DATA_FILE_BLOCK_SIZE);
        auto block = DataBlock{
                .fileFd = fd.get(),
                .pageIndex = 0,
                .compression = INCFS_COMPRESSION_KIND_NONE,
                .dataSize = (uint32_t)data.size(),
                .data = data.data(),
        };
        ASSERT_EQ(1, writeBlocks({&block, 1}));
    });

    const auto file_path = mountPath(test_file_name_);
    const android::base::unique_fd fd(open(file_path.c_str(), O_RDONLY | O_CLOEXEC | O_BINARY));
    ASSERT_GE(fd.get(), 0);
    char buf[INCFS_DATA_FILE_BLOCK_SIZE];
    ASSERT_TRUE(android::base::ReadFully(fd, buf, sizeof(buf)));
    wait_pending_read_thread.join();
}

TEST_F(IncFsTest, GetFilledRangesBad) {
    EXPECT_EQ(-EBADF, IncFs_GetFilledRanges(-1, {}, nullptr));
    EXPECT_EQ(-EINVAL, IncFs_GetFilledRanges(0, {}, nullptr));
    EXPECT_EQ(-EINVAL, IncFs_GetFilledRangesStartingFrom(0, -1, {}, nullptr));

    makeFileWithHash(1);
    const android::base::unique_fd readFd(
            open(mountPath(test_file_name_).c_str(), O_RDONLY | O_CLOEXEC | O_BINARY));
    ASSERT_GE(readFd.get(), 0);

    char buffer[1024];
    IncFsFilledRanges res;
    EXPECT_EQ(-EPERM, IncFs_GetFilledRanges(readFd.get(), {buffer, std::size(buffer)}, &res));
}

TEST_F(IncFsTest, GetFilledRanges) {
    ASSERT_EQ(0,
              makeFile(control_, mountPath(test_file_name_), 0555, fileId(1),
                       {.size = 4 * INCFS_DATA_FILE_BLOCK_SIZE}));
    char buffer[1024];
    const auto bufferSpan = IncFsSpan{.data = buffer, .size = std::size(buffer)};

    auto fd = openForSpecialOps(control_, fileId(1));
    ASSERT_GE(fd.get(), 0);

    IncFsFilledRanges filledRanges;
    EXPECT_EQ(0, IncFs_GetFilledRanges(fd.get(), IncFsSpan{}, &filledRanges));
    EXPECT_EQ(0, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(0, IncFs_GetFilledRanges(fd.get(), bufferSpan, &filledRanges));
    EXPECT_EQ(0, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(0, IncFs_GetFilledRangesStartingFrom(fd.get(), 0, bufferSpan, &filledRanges));
    EXPECT_EQ(0, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(0, IncFs_GetFilledRangesStartingFrom(fd.get(), 1, bufferSpan, &filledRanges));
    EXPECT_EQ(0, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(0, IncFs_GetFilledRangesStartingFrom(fd.get(), 30, bufferSpan, &filledRanges));
    EXPECT_EQ(0, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(-ENODATA, IncFs_IsFullyLoaded(fd.get()));

    // write one block
    std::vector<char> data(INCFS_DATA_FILE_BLOCK_SIZE);
    auto block = DataBlock{
            .fileFd = fd.get(),
            .pageIndex = 0,
            .compression = INCFS_COMPRESSION_KIND_NONE,
            .dataSize = (uint32_t)data.size(),
            .data = data.data(),
    };
    ASSERT_EQ(1, writeBlocks({&block, 1}));

    EXPECT_EQ(0, IncFs_GetFilledRanges(fd.get(), bufferSpan, &filledRanges));
    ASSERT_EQ(1, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.dataRanges[0].begin);
    EXPECT_EQ(1, filledRanges.dataRanges[0].end);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(0, IncFs_GetFilledRangesStartingFrom(fd.get(), 0, bufferSpan, &filledRanges));
    ASSERT_EQ(1, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.dataRanges[0].begin);
    EXPECT_EQ(1, filledRanges.dataRanges[0].end);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(0, IncFs_GetFilledRangesStartingFrom(fd.get(), 1, bufferSpan, &filledRanges));
    EXPECT_EQ(0, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(0, IncFs_GetFilledRangesStartingFrom(fd.get(), 30, bufferSpan, &filledRanges));
    EXPECT_EQ(0, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(-ENODATA, IncFs_IsFullyLoaded(fd.get()));

    // append one more block next to the first one
    block.pageIndex = 1;
    ASSERT_EQ(1, writeBlocks({&block, 1}));

    EXPECT_EQ(0, IncFs_GetFilledRanges(fd.get(), bufferSpan, &filledRanges));
    ASSERT_EQ(1, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.dataRanges[0].begin);
    EXPECT_EQ(2, filledRanges.dataRanges[0].end);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(0, IncFs_GetFilledRangesStartingFrom(fd.get(), 0, bufferSpan, &filledRanges));
    ASSERT_EQ(1, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.dataRanges[0].begin);
    EXPECT_EQ(2, filledRanges.dataRanges[0].end);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(0, IncFs_GetFilledRangesStartingFrom(fd.get(), 1, bufferSpan, &filledRanges));
    ASSERT_EQ(1, filledRanges.dataRangesCount);
    EXPECT_EQ(1, filledRanges.dataRanges[0].begin);
    EXPECT_EQ(2, filledRanges.dataRanges[0].end);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(0, IncFs_GetFilledRangesStartingFrom(fd.get(), 30, bufferSpan, &filledRanges));
    EXPECT_EQ(0, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(-ENODATA, IncFs_IsFullyLoaded(fd.get()));

    // now create a gap between filled blocks
    block.pageIndex = 3;
    ASSERT_EQ(1, writeBlocks({&block, 1}));

    EXPECT_EQ(0, IncFs_GetFilledRanges(fd.get(), bufferSpan, &filledRanges));
    ASSERT_EQ(2, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.dataRanges[0].begin);
    EXPECT_EQ(2, filledRanges.dataRanges[0].end);
    EXPECT_EQ(3, filledRanges.dataRanges[1].begin);
    EXPECT_EQ(4, filledRanges.dataRanges[1].end);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(0, IncFs_GetFilledRangesStartingFrom(fd.get(), 0, bufferSpan, &filledRanges));
    ASSERT_EQ(2, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.dataRanges[0].begin);
    EXPECT_EQ(2, filledRanges.dataRanges[0].end);
    EXPECT_EQ(3, filledRanges.dataRanges[1].begin);
    EXPECT_EQ(4, filledRanges.dataRanges[1].end);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(0, IncFs_GetFilledRangesStartingFrom(fd.get(), 1, bufferSpan, &filledRanges));
    ASSERT_EQ(2, filledRanges.dataRangesCount);
    EXPECT_EQ(1, filledRanges.dataRanges[0].begin);
    EXPECT_EQ(2, filledRanges.dataRanges[0].end);
    EXPECT_EQ(3, filledRanges.dataRanges[1].begin);
    EXPECT_EQ(4, filledRanges.dataRanges[1].end);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(0, IncFs_GetFilledRangesStartingFrom(fd.get(), 2, bufferSpan, &filledRanges));
    ASSERT_EQ(1, filledRanges.dataRangesCount);
    EXPECT_EQ(3, filledRanges.dataRanges[0].begin);
    EXPECT_EQ(4, filledRanges.dataRanges[0].end);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(0, IncFs_GetFilledRangesStartingFrom(fd.get(), 30, bufferSpan, &filledRanges));
    EXPECT_EQ(0, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(-ENODATA, IncFs_IsFullyLoaded(fd.get()));

    // at last fill the whole file and make sure we report it as having a single range
    block.pageIndex = 2;
    ASSERT_EQ(1, writeBlocks({&block, 1}));

    EXPECT_EQ(0, IncFs_GetFilledRanges(fd.get(), bufferSpan, &filledRanges));
    ASSERT_EQ(1, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.dataRanges[0].begin);
    EXPECT_EQ(4, filledRanges.dataRanges[0].end);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(0, IncFs_GetFilledRangesStartingFrom(fd.get(), 0, bufferSpan, &filledRanges));
    ASSERT_EQ(1, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.dataRanges[0].begin);
    EXPECT_EQ(4, filledRanges.dataRanges[0].end);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(0, IncFs_GetFilledRangesStartingFrom(fd.get(), 1, bufferSpan, &filledRanges));
    ASSERT_EQ(1, filledRanges.dataRangesCount);
    EXPECT_EQ(1, filledRanges.dataRanges[0].begin);
    EXPECT_EQ(4, filledRanges.dataRanges[0].end);

    EXPECT_EQ(0, IncFs_GetFilledRangesStartingFrom(fd.get(), 30, bufferSpan, &filledRanges));
    EXPECT_EQ(0, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.hashRangesCount);

    EXPECT_EQ(0, IncFs_IsFullyLoaded(fd.get()));
}

TEST_F(IncFsTest, GetFilledRangesSmallBuffer) {
    ASSERT_EQ(0,
              makeFile(control_, mountPath(test_file_name_), 0555, fileId(1),
                       {.size = 5 * INCFS_DATA_FILE_BLOCK_SIZE}));
    char buffer[1024];

    auto fd = openForSpecialOps(control_, fileId(1));
    ASSERT_GE(fd.get(), 0);

    std::vector<char> data(INCFS_DATA_FILE_BLOCK_SIZE);
    DataBlock blocks[] = {DataBlock{
                                  .fileFd = fd.get(),
                                  .pageIndex = 0,
                                  .compression = INCFS_COMPRESSION_KIND_NONE,
                                  .dataSize = (uint32_t)data.size(),
                                  .data = data.data(),
                          },
                          DataBlock{
                                  .fileFd = fd.get(),
                                  .pageIndex = 2,
                                  .compression = INCFS_COMPRESSION_KIND_NONE,
                                  .dataSize = (uint32_t)data.size(),
                                  .data = data.data(),
                          },
                          DataBlock{
                                  .fileFd = fd.get(),
                                  .pageIndex = 4,
                                  .compression = INCFS_COMPRESSION_KIND_NONE,
                                  .dataSize = (uint32_t)data.size(),
                                  .data = data.data(),
                          }};
    ASSERT_EQ(3, writeBlocks({blocks, 3}));

    IncFsSpan bufferSpan = {.data = buffer, .size = sizeof(IncFsBlockRange)};
    IncFsFilledRanges filledRanges;
    EXPECT_EQ(-ERANGE, IncFs_GetFilledRanges(fd.get(), bufferSpan, &filledRanges));
    ASSERT_EQ(1, filledRanges.dataRangesCount);
    EXPECT_EQ(0, filledRanges.dataRanges[0].begin);
    EXPECT_EQ(1, filledRanges.dataRanges[0].end);
    EXPECT_EQ(0, filledRanges.hashRangesCount);
    EXPECT_EQ(2, filledRanges.endIndex);

    EXPECT_EQ(-ERANGE,
              IncFs_GetFilledRangesStartingFrom(fd.get(), filledRanges.endIndex, bufferSpan,
                                                &filledRanges));
    ASSERT_EQ(1, filledRanges.dataRangesCount);
    EXPECT_EQ(2, filledRanges.dataRanges[0].begin);
    EXPECT_EQ(3, filledRanges.dataRanges[0].end);
    EXPECT_EQ(0, filledRanges.hashRangesCount);
    EXPECT_EQ(4, filledRanges.endIndex);

    EXPECT_EQ(0,
              IncFs_GetFilledRangesStartingFrom(fd.get(), filledRanges.endIndex, bufferSpan,
                                                &filledRanges));
    ASSERT_EQ(1, filledRanges.dataRangesCount);
    EXPECT_EQ(4, filledRanges.dataRanges[0].begin);
    EXPECT_EQ(5, filledRanges.dataRanges[0].end);
    EXPECT_EQ(0, filledRanges.hashRangesCount);
    EXPECT_EQ(5, filledRanges.endIndex);
}

TEST_F(IncFsTest, GetFilledRangesWithHashes) {
    auto size = makeFileWithHash(1);
    ASSERT_GT(size, 0);
    ASSERT_NO_FATAL_FAILURE(writeTestRanges(1, size));

    auto fd = openForSpecialOps(control_, fileId(1));
    ASSERT_GE(fd.get(), 0);

    char buffer[1024];
    IncFsSpan bufferSpan = {.data = buffer, .size = sizeof(buffer)};
    IncFsFilledRanges filledRanges;
    EXPECT_EQ(0, IncFs_GetFilledRanges(fd.get(), bufferSpan, &filledRanges));
    ASSERT_EQ(3, filledRanges.dataRangesCount);
    auto lastPage = sizeToPages(size) - 1;
    EXPECT_EQ(lastPage, filledRanges.dataRanges[2].begin);
    EXPECT_EQ(lastPage + 1, filledRanges.dataRanges[2].end);
    EXPECT_EQ(2, filledRanges.hashRangesCount);
    EXPECT_EQ(0, filledRanges.hashRanges[0].begin);
    EXPECT_EQ(1, filledRanges.hashRanges[0].end);
    EXPECT_EQ(2, filledRanges.hashRanges[1].begin);
    EXPECT_EQ(3, filledRanges.hashRanges[1].end);
    EXPECT_EQ(sizeToPages(size) + 3, filledRanges.endIndex);
}

TEST_F(IncFsTest, GetFilledRangesCpp) {
    auto size = makeFileWithHash(1);
    ASSERT_GT(size, 0);
    ASSERT_NO_FATAL_FAILURE(writeTestRanges(1, size));

    auto fd = openForSpecialOps(control_, fileId(1));
    ASSERT_GE(fd.get(), 0);

    // simply get all ranges
    auto [res, ranges] = getFilledRanges(fd.get());
    EXPECT_EQ(res, 0);
    EXPECT_EQ(size_t(5), ranges.totalSize());
    ASSERT_EQ(size_t(3), ranges.dataRanges().size());
    auto lastPage = sizeToPages(size) - 1;
    EXPECT_EQ(lastPage, ranges.dataRanges()[2].begin);
    EXPECT_EQ(size_t(1), ranges.dataRanges()[2].size());
    ASSERT_EQ(size_t(2), ranges.hashRanges().size());
    EXPECT_EQ(0, ranges.hashRanges()[0].begin);
    EXPECT_EQ(size_t(1), ranges.hashRanges()[0].size());
    EXPECT_EQ(2, ranges.hashRanges()[1].begin);
    EXPECT_EQ(size_t(1), ranges.hashRanges()[1].size());

    // now check how buffer size limiting works.
    FilledRanges::RangeBuffer buf(ranges.totalSize() - 1);
    auto [res2, ranges2] = getFilledRanges(fd.get(), std::move(buf));
    ASSERT_EQ(-ERANGE, res2);
    EXPECT_EQ(ranges.totalSize() - 1, ranges2.totalSize());
    ASSERT_EQ(size_t(3), ranges2.dataRanges().size());
    ASSERT_EQ(size_t(1), ranges2.hashRanges().size());
    EXPECT_EQ(0, ranges2.hashRanges()[0].begin);
    EXPECT_EQ(size_t(1), ranges2.hashRanges()[0].size());

    // and now check the resumption from the previous result
    auto [res3, ranges3] = getFilledRanges(fd.get(), std::move(ranges2));
    ASSERT_EQ(0, res3);
    EXPECT_EQ(ranges.totalSize(), ranges3.totalSize());
    ASSERT_EQ(size_t(3), ranges3.dataRanges().size());
    ASSERT_EQ(size_t(2), ranges3.hashRanges().size());
    EXPECT_EQ(0, ranges3.hashRanges()[0].begin);
    EXPECT_EQ(size_t(1), ranges3.hashRanges()[0].size());
    EXPECT_EQ(2, ranges3.hashRanges()[1].begin);
    EXPECT_EQ(size_t(1), ranges3.hashRanges()[1].size());

    EXPECT_EQ(LoadingState::MissingBlocks, isFullyLoaded(fd.get()));

    {
        std::vector<char> data(INCFS_DATA_FILE_BLOCK_SIZE);
        DataBlock block = {.fileFd = fd.get(),
                           .pageIndex = 1,
                           .compression = INCFS_COMPRESSION_KIND_NONE,
                           .dataSize = (uint32_t)data.size(),
                           .data = data.data()};
        for (auto i = 0; i != sizeToPages(size); ++i) {
            block.pageIndex = i;
            ASSERT_EQ(1, writeBlocks({&block, 1}));
        }
        block.kind = INCFS_BLOCK_KIND_HASH;
        for (auto i = 0; i != 3; ++i) {
            block.pageIndex = i;
            ASSERT_EQ(1, writeBlocks({&block, 1}));
        }
    }
    EXPECT_EQ(LoadingState::Full, isFullyLoaded(fd.get()));
}
