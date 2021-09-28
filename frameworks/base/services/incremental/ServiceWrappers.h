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

#pragma once

#include <android-base/unique_fd.h>
#include <android/content/pm/DataLoaderParamsParcel.h>
#include <android/content/pm/FileSystemControlParcel.h>
#include <android/content/pm/IDataLoader.h>
#include <android/content/pm/IDataLoaderStatusListener.h>
#include <binder/IAppOpsCallback.h>
#include <binder/IServiceManager.h>
#include <binder/Status.h>
#include <incfs.h>
#include <jni.h>
#include <utils/Looper.h>

#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace android::incremental {

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Milliseconds = std::chrono::milliseconds;
using Job = std::function<void()>;

// --- Wrapper interfaces ---

using MountId = int32_t;

class VoldServiceWrapper {
public:
    virtual ~VoldServiceWrapper() = default;
    virtual binder::Status mountIncFs(
            const std::string& backingPath, const std::string& targetDir, int32_t flags,
            os::incremental::IncrementalFileSystemControlParcel* result) const = 0;
    virtual binder::Status unmountIncFs(const std::string& dir) const = 0;
    virtual binder::Status bindMount(const std::string& sourceDir,
                                     const std::string& targetDir) const = 0;
    virtual binder::Status setIncFsMountOptions(
            const os::incremental::IncrementalFileSystemControlParcel& control,
            bool enableReadLogs) const = 0;
};

class DataLoaderManagerWrapper {
public:
    virtual ~DataLoaderManagerWrapper() = default;
    virtual binder::Status bindToDataLoader(
            MountId mountId, const content::pm::DataLoaderParamsParcel& params,
            const sp<content::pm::IDataLoaderStatusListener>& listener, bool* result) const = 0;
    virtual binder::Status getDataLoader(MountId mountId,
                                         sp<content::pm::IDataLoader>* result) const = 0;
    virtual binder::Status unbindFromDataLoader(MountId mountId) const = 0;
};

class IncFsWrapper {
public:
    using Control = incfs::Control;
    using FileId = incfs::FileId;
    using ErrorCode = incfs::ErrorCode;
    using WaitResult = incfs::WaitResult;

    using ExistingMountCallback =
            std::function<void(std::string_view root, std::string_view backingDir,
                               std::span<std::pair<std::string_view, std::string_view>> binds)>;

    virtual ~IncFsWrapper() = default;
    virtual void listExistingMounts(const ExistingMountCallback& cb) const = 0;
    virtual Control openMount(std::string_view path) const = 0;
    virtual Control createControl(IncFsFd cmd, IncFsFd pendingReads, IncFsFd logs) const = 0;
    virtual ErrorCode makeFile(const Control& control, std::string_view path, int mode, FileId id,
                               incfs::NewFileParams params) const = 0;
    virtual ErrorCode makeDir(const Control& control, std::string_view path, int mode) const = 0;
    virtual ErrorCode makeDirs(const Control& control, std::string_view path, int mode) const = 0;
    virtual incfs::RawMetadata getMetadata(const Control& control, FileId fileid) const = 0;
    virtual incfs::RawMetadata getMetadata(const Control& control, std::string_view path) const = 0;
    virtual FileId getFileId(const Control& control, std::string_view path) const = 0;
    virtual ErrorCode link(const Control& control, std::string_view from,
                           std::string_view to) const = 0;
    virtual ErrorCode unlink(const Control& control, std::string_view path) const = 0;
    virtual base::unique_fd openForSpecialOps(const Control& control, FileId id) const = 0;
    virtual ErrorCode writeBlocks(std::span<const incfs::DataBlock> blocks) const = 0;
    virtual WaitResult waitForPendingReads(
            const Control& control, std::chrono::milliseconds timeout,
            std::vector<incfs::ReadInfo>* pendingReadsBuffer) const = 0;
};

class AppOpsManagerWrapper {
public:
    virtual ~AppOpsManagerWrapper() = default;
    virtual binder::Status checkPermission(const char* permission, const char* operation,
                                           const char* package) const = 0;
    virtual void startWatchingMode(int32_t op, const String16& packageName, const sp<IAppOpsCallback>& callback) = 0;
    virtual void stopWatchingMode(const sp<IAppOpsCallback>& callback) = 0;
};

class JniWrapper {
public:
    virtual ~JniWrapper() = default;
    virtual void initializeForCurrentThread() const = 0;
};

class LooperWrapper {
public:
    virtual ~LooperWrapper() = default;
    virtual int addFd(int fd, int ident, int events, android::Looper_callbackFunc callback,
                      void* data) = 0;
    virtual int removeFd(int fd) = 0;
    virtual void wake() = 0;
    virtual int pollAll(int timeoutMillis) = 0;
};

class TimedQueueWrapper {
public:
    virtual ~TimedQueueWrapper() = default;
    virtual void addJob(MountId id, Milliseconds after, Job what) = 0;
    virtual void removeJobs(MountId id) = 0;
    virtual void stop() = 0;
};

class ServiceManagerWrapper {
public:
    virtual ~ServiceManagerWrapper() = default;
    virtual std::unique_ptr<VoldServiceWrapper> getVoldService() = 0;
    virtual std::unique_ptr<DataLoaderManagerWrapper> getDataLoaderManager() = 0;
    virtual std::unique_ptr<IncFsWrapper> getIncFs() = 0;
    virtual std::unique_ptr<AppOpsManagerWrapper> getAppOpsManager() = 0;
    virtual std::unique_ptr<JniWrapper> getJni() = 0;
    virtual std::unique_ptr<LooperWrapper> getLooper() = 0;
    virtual std::unique_ptr<TimedQueueWrapper> getTimedQueue() = 0;
};

// --- Real stuff ---

class RealServiceManager : public ServiceManagerWrapper {
public:
    RealServiceManager(sp<IServiceManager> serviceManager, JNIEnv* env);
    ~RealServiceManager() = default;
    std::unique_ptr<VoldServiceWrapper> getVoldService() final;
    std::unique_ptr<DataLoaderManagerWrapper> getDataLoaderManager() final;
    std::unique_ptr<IncFsWrapper> getIncFs() final;
    std::unique_ptr<AppOpsManagerWrapper> getAppOpsManager() final;
    std::unique_ptr<JniWrapper> getJni() final;
    std::unique_ptr<LooperWrapper> getLooper() final;
    std::unique_ptr<TimedQueueWrapper> getTimedQueue() final;

private:
    template <class INTERFACE>
    sp<INTERFACE> getRealService(std::string_view serviceName) const;
    sp<android::IServiceManager> mServiceManager;
    JavaVM* const mJvm;
};

} // namespace android::incremental
