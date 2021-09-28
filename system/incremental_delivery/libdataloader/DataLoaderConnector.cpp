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
#define LOG_TAG "incfs-dataloaderconnector"

#include <android-base/logging.h>
#include <fcntl.h>
#include <nativehelper/JNIHelp.h>
#include <sys/stat.h>
#include <utils/Looper.h>

#include <thread>
#include <unordered_map>

#include "JNIHelpers.h"
#include "ManagedDataLoader.h"
#include "dataloader.h"
#include "incfs.h"

namespace {

using namespace android::dataloader;
using namespace std::literals;

using FileId = android::incfs::FileId;
using RawMetadata = android::incfs::RawMetadata;
using UniqueControl = android::incfs::UniqueControl;

struct JniIds {
    struct {
        jint DATA_LOADER_CREATED;
        jint DATA_LOADER_DESTROYED;
        jint DATA_LOADER_STARTED;
        jint DATA_LOADER_STOPPED;
        jint DATA_LOADER_IMAGE_READY;
        jint DATA_LOADER_IMAGE_NOT_READY;
        jint DATA_LOADER_UNAVAILABLE;
        jint DATA_LOADER_UNRECOVERABLE;

        jint DATA_LOADER_TYPE_NONE;
        jint DATA_LOADER_TYPE_STREAMING;
        jint DATA_LOADER_TYPE_INCREMENTAL;

        jint DATA_LOADER_LOCATION_DATA_APP;
        jint DATA_LOADER_LOCATION_MEDIA_OBB;
        jint DATA_LOADER_LOCATION_MEDIA_DATA;
    } constants;

    jmethodID parcelFileDescriptorGetFileDescriptor;

    jfieldID incremental;
    jfieldID service;
    jfieldID callback;

    jfieldID controlCmd;
    jfieldID controlPendingReads;
    jfieldID controlLog;

    jfieldID paramsType;
    jfieldID paramsPackageName;
    jfieldID paramsClassName;
    jfieldID paramsArguments;

    jclass listener;
    jmethodID listenerOnStatusChanged;

    jmethodID callbackControlWriteData;

    jmethodID listGet;
    jmethodID listSize;

    jfieldID installationFileLocation;
    jfieldID installationFileName;
    jfieldID installationFileLengthBytes;
    jfieldID installationFileMetadata;

    jmethodID incrementalServiceConnectorSetStorageParams;

    JniIds(JNIEnv* env) {
        listener = (jclass)env->NewGlobalRef(
                FindClassOrDie(env, "android/content/pm/IDataLoaderStatusListener"));
        listenerOnStatusChanged = GetMethodIDOrDie(env, listener, "onStatusChanged", "(II)V");

        constants.DATA_LOADER_CREATED =
                GetStaticIntFieldValueOrDie(env, listener, "DATA_LOADER_CREATED");
        constants.DATA_LOADER_DESTROYED =
                GetStaticIntFieldValueOrDie(env, listener, "DATA_LOADER_DESTROYED");
        constants.DATA_LOADER_STARTED =
                GetStaticIntFieldValueOrDie(env, listener, "DATA_LOADER_STARTED");
        constants.DATA_LOADER_STOPPED =
                GetStaticIntFieldValueOrDie(env, listener, "DATA_LOADER_STOPPED");
        constants.DATA_LOADER_IMAGE_READY =
                GetStaticIntFieldValueOrDie(env, listener, "DATA_LOADER_IMAGE_READY");
        constants.DATA_LOADER_IMAGE_NOT_READY =
                GetStaticIntFieldValueOrDie(env, listener, "DATA_LOADER_IMAGE_NOT_READY");

        constants.DATA_LOADER_UNAVAILABLE =
                GetStaticIntFieldValueOrDie(env, listener, "DATA_LOADER_UNAVAILABLE");
        constants.DATA_LOADER_UNRECOVERABLE =
                GetStaticIntFieldValueOrDie(env, listener, "DATA_LOADER_UNRECOVERABLE");

        CHECK(constants.DATA_LOADER_UNRECOVERABLE == DATA_LOADER_UNRECOVERABLE);

        auto packageInstaller = (jclass)FindClassOrDie(env, "android/content/pm/PackageInstaller");

        constants.DATA_LOADER_TYPE_NONE =
                GetStaticIntFieldValueOrDie(env, packageInstaller, "DATA_LOADER_TYPE_NONE");
        constants.DATA_LOADER_TYPE_STREAMING =
                GetStaticIntFieldValueOrDie(env, packageInstaller, "DATA_LOADER_TYPE_STREAMING");
        constants.DATA_LOADER_TYPE_INCREMENTAL =
                GetStaticIntFieldValueOrDie(env, packageInstaller, "DATA_LOADER_TYPE_INCREMENTAL");

        CHECK(constants.DATA_LOADER_TYPE_NONE == DATA_LOADER_TYPE_NONE);
        CHECK(constants.DATA_LOADER_TYPE_STREAMING == DATA_LOADER_TYPE_STREAMING);
        CHECK(constants.DATA_LOADER_TYPE_INCREMENTAL == DATA_LOADER_TYPE_INCREMENTAL);

        constants.DATA_LOADER_LOCATION_DATA_APP =
                GetStaticIntFieldValueOrDie(env, packageInstaller, "LOCATION_DATA_APP");
        constants.DATA_LOADER_LOCATION_MEDIA_OBB =
                GetStaticIntFieldValueOrDie(env, packageInstaller, "LOCATION_MEDIA_OBB");
        constants.DATA_LOADER_LOCATION_MEDIA_DATA =
                GetStaticIntFieldValueOrDie(env, packageInstaller, "LOCATION_MEDIA_DATA");

        CHECK(constants.DATA_LOADER_LOCATION_DATA_APP == DATA_LOADER_LOCATION_DATA_APP);
        CHECK(constants.DATA_LOADER_LOCATION_MEDIA_OBB == DATA_LOADER_LOCATION_MEDIA_OBB);
        CHECK(constants.DATA_LOADER_LOCATION_MEDIA_DATA == DATA_LOADER_LOCATION_MEDIA_DATA);

        auto parcelFileDescriptor = FindClassOrDie(env, "android/os/ParcelFileDescriptor");
        parcelFileDescriptorGetFileDescriptor =
                GetMethodIDOrDie(env, parcelFileDescriptor, "getFileDescriptor",
                                 "()Ljava/io/FileDescriptor;");

        auto control = FindClassOrDie(env, "android/content/pm/FileSystemControlParcel");
        incremental =
                GetFieldIDOrDie(env, control, "incremental",
                                "Landroid/os/incremental/IncrementalFileSystemControlParcel;");
        service = GetFieldIDOrDie(env, control, "service",
                                  "Landroid/os/incremental/IIncrementalServiceConnector;");
        callback =
                GetFieldIDOrDie(env, control, "callback",
                                "Landroid/content/pm/IPackageInstallerSessionFileSystemConnector;");

        auto incControl =
                FindClassOrDie(env, "android/os/incremental/IncrementalFileSystemControlParcel");
        controlCmd = GetFieldIDOrDie(env, incControl, "cmd", "Landroid/os/ParcelFileDescriptor;");
        controlPendingReads = GetFieldIDOrDie(env, incControl, "pendingReads",
                                              "Landroid/os/ParcelFileDescriptor;");
        controlLog = GetFieldIDOrDie(env, incControl, "log", "Landroid/os/ParcelFileDescriptor;");

        auto params = FindClassOrDie(env, "android/content/pm/DataLoaderParamsParcel");
        paramsType = GetFieldIDOrDie(env, params, "type", "I");
        paramsPackageName = GetFieldIDOrDie(env, params, "packageName", "Ljava/lang/String;");
        paramsClassName = GetFieldIDOrDie(env, params, "className", "Ljava/lang/String;");
        paramsArguments = GetFieldIDOrDie(env, params, "arguments", "Ljava/lang/String;");

        auto callbackControl =
                FindClassOrDie(env,
                               "android/content/pm/IPackageInstallerSessionFileSystemConnector");
        callbackControlWriteData =
                GetMethodIDOrDie(env, callbackControl, "writeData",
                                 "(Ljava/lang/String;JJLandroid/os/ParcelFileDescriptor;)V");

        auto list = (jclass)FindClassOrDie(env, "java/util/List");
        listGet = GetMethodIDOrDie(env, list, "get", "(I)Ljava/lang/Object;");
        listSize = GetMethodIDOrDie(env, list, "size", "()I");

        auto installationFileParcel =
                (jclass)FindClassOrDie(env, "android/content/pm/InstallationFileParcel");
        installationFileLocation = GetFieldIDOrDie(env, installationFileParcel, "location", "I");
        installationFileName =
                GetFieldIDOrDie(env, installationFileParcel, "name", "Ljava/lang/String;");
        installationFileLengthBytes = GetFieldIDOrDie(env, installationFileParcel, "size", "J");
        installationFileMetadata = GetFieldIDOrDie(env, installationFileParcel, "metadata", "[B");

        auto incrementalServiceConnector =
                FindClassOrDie(env, "android/os/incremental/IIncrementalServiceConnector");
        incrementalServiceConnectorSetStorageParams =
                GetMethodIDOrDie(env, incrementalServiceConnector, "setStorageParams", "(Z)I");
    }
};

JavaVM* getJavaVM(JNIEnv* env) {
    CHECK(env);
    JavaVM* jvm = nullptr;
    env->GetJavaVM(&jvm);
    CHECK(jvm);
    return jvm;
}

const JniIds& jniIds(JNIEnv* env) {
    static const JniIds ids(env);
    return ids;
}

bool reportStatusViaCallback(JNIEnv* env, jobject listener, jint storageId, jint status) {
    if (listener == nullptr) {
        ALOGE("No listener object to talk to IncrementalService. "
              "DataLoaderId=%d, "
              "status=%d",
              storageId, status);
        return false;
    }

    const auto& jni = jniIds(env);

    env->CallVoidMethod(listener, jni.listenerOnStatusChanged, storageId, status);
    ALOGI("Reported status back to IncrementalService. DataLoaderId=%d, "
          "status=%d",
          storageId, status);

    return true;
}

class DataLoaderConnector;
using DataLoaderConnectorPtr = std::shared_ptr<DataLoaderConnector>;
using DataLoaderConnectorsMap = std::unordered_map<int, DataLoaderConnectorPtr>;

struct Globals {
    Globals() {
        managedDataLoaderFactory = new details::DataLoaderFactoryImpl(
                [](auto jvm, auto) { return std::make_unique<ManagedDataLoader>(jvm); });
    }

    DataLoaderFactory* managedDataLoaderFactory = nullptr;
    DataLoaderFactory* dataLoaderFactory = nullptr;

    std::mutex dataLoaderConnectorsLock;
    // id->DataLoader map
    DataLoaderConnectorsMap dataLoaderConnectors GUARDED_BY(dataLoaderConnectorsLock);

    std::atomic_bool stopped;
    std::thread pendingReadsLooperThread;
    std::thread logLooperThread;
    std::vector<ReadInfo> pendingReads;
    std::vector<ReadInfo> pageReads;
};

static Globals& globals() {
    static Globals globals;
    return globals;
}

struct IncFsLooper : public android::Looper {
    IncFsLooper() : Looper(/*allowNonCallbacks=*/false) {}
    ~IncFsLooper() {}
};

static android::Looper& pendingReadsLooper() {
    static IncFsLooper pendingReadsLooper;
    return pendingReadsLooper;
}

static android::Looper& logLooper() {
    static IncFsLooper logLooper;
    return logLooper;
}

struct DataLoaderParamsPair {
    static DataLoaderParamsPair createFromManaged(JNIEnv* env, jobject params);

    const android::dataloader::DataLoaderParams& dataLoaderParams() const {
        return mDataLoaderParams;
    }
    const ::DataLoaderParams& ndkDataLoaderParams() const { return mNDKDataLoaderParams; }

private:
    DataLoaderParamsPair(android::dataloader::DataLoaderParams&& dataLoaderParams);

    android::dataloader::DataLoaderParams mDataLoaderParams;
    ::DataLoaderParams mNDKDataLoaderParams;
};

static constexpr auto kPendingReadsBufferSize = 256;

class DataLoaderConnector : public FilesystemConnector, public StatusListener {
public:
    DataLoaderConnector(JNIEnv* env, jobject service, jint storageId, UniqueControl control,
                        jobject serviceConnector, jobject callbackControl, jobject listener)
          : mJvm(getJavaVM(env)),
            mService(env->NewGlobalRef(service)),
            mServiceConnector(env->NewGlobalRef(serviceConnector)),
            mCallbackControl(env->NewGlobalRef(callbackControl)),
            mListener(env->NewGlobalRef(listener)),
            mStorageId(storageId),
            mControl(std::move(control)) {
        CHECK(mJvm != nullptr);
    }
    DataLoaderConnector(const DataLoaderConnector&) = delete;
    DataLoaderConnector(const DataLoaderConnector&&) = delete;
    virtual ~DataLoaderConnector() {
        JNIEnv* env = GetOrAttachJNIEnvironment(mJvm);

        env->DeleteGlobalRef(mService);
        env->DeleteGlobalRef(mServiceConnector);
        env->DeleteGlobalRef(mCallbackControl);
        env->DeleteGlobalRef(mListener);
    } // to avoid delete-non-virtual-dtor

    bool onCreate(const DataLoaderParamsPair& params, jobject managedParams) {
        CHECK(mDataLoader == nullptr);

        if (auto factory = globals().dataLoaderFactory) {
            // Let's try the non-default first.
            mDataLoader = factory->onCreate(factory, &params.ndkDataLoaderParams(), this, this,
                                            mJvm, mService, managedParams);
            if (checkAndClearJavaException(__func__)) {
                return false;
            }
        }

        if (!mDataLoader) {
            // Didn't work, let's try the default.
            auto factory = globals().managedDataLoaderFactory;
            mDataLoader = factory->onCreate(factory, &params.ndkDataLoaderParams(), this, this,
                                            mJvm, mService, managedParams);
            if (checkAndClearJavaException(__func__)) {
                return false;
            }
        }

        if (!mDataLoader) {
            return false;
        }

        return true;
    }
    bool onStart() {
        CHECK(mDataLoader);
        bool result = mDataLoader->onStart(mDataLoader);
        if (checkAndClearJavaException(__func__)) {
            result = false;
        }
        mRunning = result;
        return result;
    }
    void onStop() {
        CHECK(mDataLoader);

        // Stopping both loopers and waiting for them to exit - we should be able to acquire/release
        // both mutexes.
        mRunning = false;
        std::lock_guard{mPendingReadsLooperBusy}; // NOLINT
        std::lock_guard{mLogLooperBusy}; // NOLINT

        mDataLoader->onStop(mDataLoader);
        checkAndClearJavaException(__func__);
    }
    void onDestroy() {
        CHECK(mDataLoader);
        mDataLoader->onDestroy(mDataLoader);
        checkAndClearJavaException(__func__);
    }

    bool onPrepareImage(const DataLoaderInstallationFiles& addedFiles) {
        CHECK(mDataLoader);
        bool result =
                mDataLoader->onPrepareImage(mDataLoader, addedFiles.data(), addedFiles.size());
        if (checkAndClearJavaException(__func__)) {
            result = false;
        }
        return result;
    }

    int onPendingReadsLooperEvent(std::vector<ReadInfo>& pendingReads) {
        CHECK(mDataLoader);
        std::lock_guard lock{mPendingReadsLooperBusy};
        while (mRunning.load(std::memory_order_relaxed)) {
            pendingReads.resize(kPendingReadsBufferSize);
            if (android::incfs::waitForPendingReads(mControl, 0ms, &pendingReads) !=
                        android::incfs::WaitResult::HaveData ||
                pendingReads.empty()) {
                return 1;
            }
            mDataLoader->onPendingReads(mDataLoader, pendingReads.data(), pendingReads.size());
        }
        return 1;
    }
    int onLogLooperEvent(std::vector<ReadInfo>& pageReads) {
        CHECK(mDataLoader);
        std::lock_guard lock{mLogLooperBusy};
        while (mRunning.load(std::memory_order_relaxed)) {
            pageReads.clear();
            if (android::incfs::waitForPageReads(mControl, 0ms, &pageReads) !=
                        android::incfs::WaitResult::HaveData ||
                pageReads.empty()) {
                return 1;
            }
            mDataLoader->onPageReads(mDataLoader, pageReads.data(), pageReads.size());
        }
        return 1;
    }

    void writeData(jstring name, jlong offsetBytes, jlong lengthBytes, jobject incomingFd) const {
        CHECK(mCallbackControl);
        JNIEnv* env = GetOrAttachJNIEnvironment(mJvm);
        const auto& jni = jniIds(env);
        env->CallVoidMethod(mCallbackControl, jni.callbackControlWriteData, name, offsetBytes,
                            lengthBytes, incomingFd);
    }

    android::incfs::UniqueFd openForSpecialOps(FileId fid) const {
        return android::incfs::openForSpecialOps(mControl, fid);
    }

    int writeBlocks(Span<const IncFsDataBlock> blocks) const {
        return android::incfs::writeBlocks(blocks);
    }

    int getRawMetadata(FileId fid, char buffer[], size_t* bufferSize) const {
        return IncFs_GetMetadataById(mControl, fid, buffer, bufferSize);
    }

    bool setParams(DataLoaderFilesystemParams params) const {
        CHECK(mServiceConnector);
        JNIEnv* env = GetOrAttachJNIEnvironment(mJvm);
        const auto& jni = jniIds(env);
        int result = env->CallIntMethod(mServiceConnector,
                                        jni.incrementalServiceConnectorSetStorageParams,
                                        params.readLogsEnabled);
        if (result != 0) {
            LOG(ERROR) << "setStorageParams failed with error: " << result;
        }
        if (checkAndClearJavaException(__func__)) {
            return false;
        }
        return (result == 0);
    }

    bool reportStatus(DataLoaderStatus status) {
        if (status < DATA_LOADER_FIRST_STATUS || DATA_LOADER_LAST_STATUS < status) {
            ALOGE("Unable to report invalid status. status=%d", status);
            return false;
        }
        JNIEnv* env = GetOrAttachJNIEnvironment(mJvm);
        return reportStatusViaCallback(env, mListener, mStorageId, status);
    }

    bool checkAndClearJavaException(std::string_view method) const {
        JNIEnv* env = GetOrAttachJNIEnvironment(mJvm);

        if (!env->ExceptionCheck()) {
            return false;
        }

        LOG(ERROR) << "Java exception during DataLoader::" << method;
        env->ExceptionDescribe();
        env->ExceptionClear();
        return true;
    }

    const UniqueControl& control() const { return mControl; }
    jobject getListenerLocalRef(JNIEnv* env) const { return env->NewLocalRef(mListener); }

private:
    JavaVM* const mJvm;
    jobject const mService;
    jobject const mServiceConnector;
    jobject const mCallbackControl;
    jobject const mListener;

    jint const mStorageId;
    UniqueControl const mControl;

    ::DataLoader* mDataLoader = nullptr;

    std::mutex mPendingReadsLooperBusy;
    std::mutex mLogLooperBusy;
    std::atomic<bool> mRunning{false};
};

static int onPendingReadsLooperEvent(int fd, int events, void* data) {
    if (globals().stopped) {
        // No more listeners.
        return 0;
    }
    auto&& dataLoaderConnector = (DataLoaderConnector*)data;
    return dataLoaderConnector->onPendingReadsLooperEvent(globals().pendingReads);
}

static int onLogLooperEvent(int fd, int events, void* data) {
    if (globals().stopped) {
        // No more listeners.
        return 0;
    }
    auto&& dataLoaderConnector = (DataLoaderConnector*)data;
    return dataLoaderConnector->onLogLooperEvent(globals().pageReads);
}

static int createFdFromManaged(JNIEnv* env, jobject pfd) {
    if (!pfd) {
        return -1;
    }

    const auto& jni = jniIds(env);
    auto managedFd = env->CallObjectMethod(pfd, jni.parcelFileDescriptorGetFileDescriptor);
    return fcntl(jniGetFDFromFileDescriptor(env, managedFd), F_DUPFD_CLOEXEC, 0);
}

static jobject createServiceConnector(JNIEnv* env, jobject managedControl) {
    const auto& jni = jniIds(env);
    return env->GetObjectField(managedControl, jni.service);
}

static jobject createCallbackControl(JNIEnv* env, jobject managedControl) {
    const auto& jni = jniIds(env);
    return env->GetObjectField(managedControl, jni.callback);
}

static UniqueControl createIncFsControlFromManaged(JNIEnv* env, jobject managedControl) {
    const auto& jni = jniIds(env);
    auto managedIncControl = env->GetObjectField(managedControl, jni.incremental);
    if (!managedIncControl) {
        return UniqueControl();
    }
    auto cmd = createFdFromManaged(env, env->GetObjectField(managedIncControl, jni.controlCmd));
    auto pr = createFdFromManaged(env,
                                  env->GetObjectField(managedIncControl, jni.controlPendingReads));
    auto log = createFdFromManaged(env, env->GetObjectField(managedIncControl, jni.controlLog));
    return android::incfs::createControl(cmd, pr, log);
}

DataLoaderParamsPair::DataLoaderParamsPair(android::dataloader::DataLoaderParams&& dataLoaderParams)
      : mDataLoaderParams(std::move(dataLoaderParams)) {
    mNDKDataLoaderParams.type = mDataLoaderParams.type();
    mNDKDataLoaderParams.packageName = mDataLoaderParams.packageName().c_str();
    mNDKDataLoaderParams.className = mDataLoaderParams.className().c_str();
    mNDKDataLoaderParams.arguments = mDataLoaderParams.arguments().c_str();
}

DataLoaderParamsPair DataLoaderParamsPair::createFromManaged(JNIEnv* env, jobject managedParams) {
    const auto& jni = jniIds(env);

    const DataLoaderType type = (DataLoaderType)env->GetIntField(managedParams, jni.paramsType);

    std::string packageName(
            env->GetStringUTFChars((jstring)env->GetObjectField(managedParams,
                                                                jni.paramsPackageName),
                                   nullptr));
    std::string className(
            env->GetStringUTFChars((jstring)env->GetObjectField(managedParams, jni.paramsClassName),
                                   nullptr));
    std::string arguments(
            env->GetStringUTFChars((jstring)env->GetObjectField(managedParams, jni.paramsArguments),
                                   nullptr));

    return DataLoaderParamsPair(android::dataloader::DataLoaderParams(type, std::move(packageName),
                                                                      std::move(className),
                                                                      std::move(arguments)));
}

static void pendingReadsLooperThread() {
    constexpr auto kTimeoutMsecs = 60 * 1000;
    while (!globals().stopped) {
        pendingReadsLooper().pollAll(kTimeoutMsecs);
    }
}

static void logLooperThread() {
    constexpr auto kTimeoutMsecs = 60 * 1000;
    while (!globals().stopped) {
        logLooper().pollAll(kTimeoutMsecs);
    }
}

static std::string pathFromFd(int fd) {
    static constexpr char fdNameFormat[] = "/proc/self/fd/%d";
    char fdNameBuffer[NELEM(fdNameFormat) + 11 + 1]; // max int length + '\0'
    snprintf(fdNameBuffer, NELEM(fdNameBuffer), fdNameFormat, fd);

    std::string res;
    // lstat() is supposed to return us exactly the needed buffer size, but
    // somehow it may also return a smaller (but still >0) st_size field.
    // That's why let's only use it for the initial estimate.
    struct stat st = {};
    if (::lstat(fdNameBuffer, &st) || st.st_size == 0) {
        st.st_size = PATH_MAX;
    }
    auto bufSize = st.st_size;
    for (;;) {
        res.resize(bufSize + 1, '\0');
        auto size = ::readlink(fdNameBuffer, &res[0], res.size());
        if (size < 0) {
            return {};
        }
        if (size > bufSize) {
            // File got renamed in between lstat() and readlink() calls? Retry.
            bufSize *= 2;
            continue;
        }
        res.resize(size);
        return res;
    }
}

} // namespace

void DataLoader_Initialize(struct ::DataLoaderFactory* factory) {
    CHECK(factory) << "DataLoader factory is invalid.";
    globals().dataLoaderFactory = factory;
}

void DataLoader_FilesystemConnector_writeData(DataLoaderFilesystemConnectorPtr ifs, jstring name,
                                              jlong offsetBytes, jlong lengthBytes,
                                              jobject incomingFd) {
    auto connector = static_cast<DataLoaderConnector*>(ifs);
    return connector->writeData(name, offsetBytes, lengthBytes, incomingFd);
}

int DataLoader_FilesystemConnector_openForSpecialOps(DataLoaderFilesystemConnectorPtr ifs,
                                                     IncFsFileId fid) {
    auto connector = static_cast<DataLoaderConnector*>(ifs);
    return connector->openForSpecialOps(fid).release();
}

int DataLoader_FilesystemConnector_writeBlocks(DataLoaderFilesystemConnectorPtr ifs,
                                               const IncFsDataBlock blocks[], int blocksCount) {
    auto connector = static_cast<DataLoaderConnector*>(ifs);
    return connector->writeBlocks({blocks, static_cast<size_t>(blocksCount)});
}

int DataLoader_FilesystemConnector_getRawMetadata(DataLoaderFilesystemConnectorPtr ifs,
                                                  IncFsFileId fid, char buffer[],
                                                  size_t* bufferSize) {
    auto connector = static_cast<DataLoaderConnector*>(ifs);
    return connector->getRawMetadata(fid, buffer, bufferSize);
}

bool DataLoader_FilesystemConnector_setParams(DataLoaderFilesystemConnectorPtr ifs,
                                              DataLoaderFilesystemParams params) {
    auto connector = static_cast<DataLoaderConnector*>(ifs);
    return connector->setParams(params);
}

int DataLoader_StatusListener_reportStatus(DataLoaderStatusListenerPtr listener,
                                           DataLoaderStatus status) {
    auto connector = static_cast<DataLoaderConnector*>(listener);
    return connector->reportStatus(status);
}

bool DataLoaderService_OnCreate(JNIEnv* env, jobject service, jint storageId, jobject control,
                                jobject params, jobject listener) {
    {
        std::lock_guard lock{globals().dataLoaderConnectorsLock};
        auto dlIt = globals().dataLoaderConnectors.find(storageId);
        if (dlIt != globals().dataLoaderConnectors.end()) {
            ALOGI("id(%d): already exist, skipping creation.", storageId);
            return true;
        }
    }
    auto nativeControl = createIncFsControlFromManaged(env, control);
    ALOGI("DataLoader::create1 cmd: %d|%s", nativeControl.cmd(),
          pathFromFd(nativeControl.cmd()).c_str());
    ALOGI("DataLoader::create1 pendingReads: %d|%s", nativeControl.pendingReads(),
          pathFromFd(nativeControl.pendingReads()).c_str());
    ALOGI("DataLoader::create1 log: %d|%s", nativeControl.logs(),
          pathFromFd(nativeControl.logs()).c_str());

    auto nativeParams = DataLoaderParamsPair::createFromManaged(env, params);
    ALOGI("DataLoader::create2: %d|%s|%s|%s", nativeParams.dataLoaderParams().type(),
          nativeParams.dataLoaderParams().packageName().c_str(),
          nativeParams.dataLoaderParams().className().c_str(),
          nativeParams.dataLoaderParams().arguments().c_str());

    auto serviceConnector = createServiceConnector(env, control);
    auto callbackControl = createCallbackControl(env, control);

    auto reportUnavailable = [env, storageId](jobject listener) {
        const auto& jni = jniIds(env);
        reportStatusViaCallback(env, listener, storageId, jni.constants.DATA_LOADER_UNAVAILABLE);
    };
    // By default, it's disabled. Need to assign listener to enable.
    std::unique_ptr<_jobject, decltype(reportUnavailable)>
            reportUnavailableOnExit(nullptr, reportUnavailable);

    auto dataLoaderConnector =
            std::make_unique<DataLoaderConnector>(env, service, storageId, std::move(nativeControl),
                                                  serviceConnector, callbackControl, listener);
    {
        std::lock_guard lock{globals().dataLoaderConnectorsLock};
        auto [dlIt, dlInserted] =
                globals().dataLoaderConnectors.try_emplace(storageId,
                                                           std::move(dataLoaderConnector));
        if (!dlInserted) {
            ALOGE("id(%d): already exist, skipping creation.", storageId);
            return false;
        }
        if (!dlIt->second->onCreate(nativeParams, params)) {
            globals().dataLoaderConnectors.erase(dlIt);
            // Enable the reporter.
            reportUnavailableOnExit.reset(listener);
            return false;
        }
    }

    const auto& jni = jniIds(env);
    reportStatusViaCallback(env, listener, storageId, jni.constants.DATA_LOADER_CREATED);

    return true;
}

bool DataLoaderService_OnStart(JNIEnv* env, jint storageId) {
    auto destroyAndReportUnavailable = [env, storageId](jobject listener) {
        // Because of the MT the installer can call commit and recreate/restart dataLoader before
        // system server has a change to destroy it.
        DataLoaderService_OnDestroy(env, storageId);
        const auto& jni = jniIds(env);
        reportStatusViaCallback(env, listener, storageId, jni.constants.DATA_LOADER_UNAVAILABLE);
    };
    // By default, it's disabled. Need to assign listener to enable.
    std::unique_ptr<_jobject, decltype(destroyAndReportUnavailable)>
            destroyAndReportUnavailableOnExit(nullptr, destroyAndReportUnavailable);

    const UniqueControl* control;
    jobject listener;
    DataLoaderConnectorPtr dataLoaderConnector;
    {
        std::lock_guard lock{globals().dataLoaderConnectorsLock};
        auto dlIt = globals().dataLoaderConnectors.find(storageId);
        if (dlIt == globals().dataLoaderConnectors.end()) {
            ALOGE("Failed to start id(%d): not found", storageId);
            return false;
        }

        listener = dlIt->second->getListenerLocalRef(env);

        dataLoaderConnector = dlIt->second;
        if (!dataLoaderConnector->onStart()) {
            ALOGE("Failed to start id(%d): onStart returned false", storageId);
            destroyAndReportUnavailableOnExit.reset(listener);
            return false;
        }

        control = &(dataLoaderConnector->control());

        // Create loopers while we are under lock.
        if (control->pendingReads() >= 0 && !globals().pendingReadsLooperThread.joinable()) {
            pendingReadsLooper();
            globals().pendingReadsLooperThread = std::thread(&pendingReadsLooperThread);
        }
        if (control->logs() >= 0 && !globals().logLooperThread.joinable()) {
            logLooper();
            globals().logLooperThread = std::thread(&logLooperThread);
        }
    }

    if (control->pendingReads() >= 0) {
        pendingReadsLooper().addFd(control->pendingReads(), android::Looper::POLL_CALLBACK,
                                   android::Looper::EVENT_INPUT, &onPendingReadsLooperEvent,
                                   dataLoaderConnector.get());
        pendingReadsLooper().wake();
    }

    if (control->logs() >= 0) {
        logLooper().addFd(control->logs(), android::Looper::POLL_CALLBACK,
                          android::Looper::EVENT_INPUT, &onLogLooperEvent,
                          dataLoaderConnector.get());
        logLooper().wake();
    }

    const auto& jni = jniIds(env);
    reportStatusViaCallback(env, listener, storageId, jni.constants.DATA_LOADER_STARTED);

    return true;
}

jobject DataLoaderService_OnStop_NoStatus(JNIEnv* env, jint storageId) {
    const UniqueControl* control;
    {
        std::lock_guard lock{globals().dataLoaderConnectorsLock};
        auto dlIt = globals().dataLoaderConnectors.find(storageId);
        if (dlIt == globals().dataLoaderConnectors.end()) {
            return nullptr;
        }
        control = &(dlIt->second->control());
    }

    if (control->pendingReads() >= 0) {
        pendingReadsLooper().removeFd(control->pendingReads());
        pendingReadsLooper().wake();
    }
    if (control->logs() >= 0) {
        logLooper().removeFd(control->logs());
        logLooper().wake();
    }

    jobject listener = nullptr;
    {
        std::lock_guard lock{globals().dataLoaderConnectorsLock};
        auto dlIt = globals().dataLoaderConnectors.find(storageId);
        if (dlIt == globals().dataLoaderConnectors.end()) {
            ALOGI("Failed to stop id(%d): not found", storageId);
            return nullptr;
        }

        listener = dlIt->second->getListenerLocalRef(env);

        auto&& dataLoaderConnector = dlIt->second;
        dataLoaderConnector->onStop();
    }
    return listener;
}

bool DataLoaderService_OnStop(JNIEnv* env, jint storageId) {
    auto listener = DataLoaderService_OnStop_NoStatus(env, storageId);
    if (listener == nullptr) {
        ALOGI("Failed to stop id(%d): not found", storageId);
        return true;
    }

    const auto& jni = jniIds(env);
    reportStatusViaCallback(env, listener, storageId, jni.constants.DATA_LOADER_STOPPED);

    return true;
}

jobject DataLoaderService_OnDestroy_NoStatus(JNIEnv* env, jint storageId) {
    jobject listener = DataLoaderService_OnStop_NoStatus(env, storageId);
    if (!listener) {
        return nullptr;
    }

    {
        std::lock_guard lock{globals().dataLoaderConnectorsLock};
        auto dlIt = globals().dataLoaderConnectors.find(storageId);
        if (dlIt == globals().dataLoaderConnectors.end()) {
            return nullptr;
        }

        auto&& dataLoaderConnector = dlIt->second;
        dataLoaderConnector->onDestroy();
        globals().dataLoaderConnectors.erase(dlIt);
    }

    return listener;
}

bool DataLoaderService_OnDestroy(JNIEnv* env, jint storageId) {
    jobject listener = DataLoaderService_OnDestroy_NoStatus(env, storageId);
    if (!listener) {
        ALOGI("Failed to remove id(%d): not found", storageId);
        return true;
    }

    const auto& jni = jniIds(env);
    reportStatusViaCallback(env, listener, storageId, jni.constants.DATA_LOADER_DESTROYED);

    return true;
}

struct DataLoaderInstallationFilesPair {
    static DataLoaderInstallationFilesPair createFromManaged(JNIEnv* env, jobjectArray jfiles);

    using Files = std::vector<android::dataloader::DataLoaderInstallationFile>;
    const Files& files() const { return mFiles; }

    using NDKFiles = std::vector<::DataLoaderInstallationFile>;
    const NDKFiles& ndkFiles() const { return mNDKFiles; }

private:
    DataLoaderInstallationFilesPair(Files&& files);

    Files mFiles;
    NDKFiles mNDKFiles;
};

DataLoaderInstallationFilesPair DataLoaderInstallationFilesPair::createFromManaged(
        JNIEnv* env, jobjectArray jfiles) {
    const auto& jni = jniIds(env);

    // jfiles is a Java array of InstallationFileParcel
    auto size = env->GetArrayLength(jfiles);
    DataLoaderInstallationFilesPair::Files files;
    files.reserve(size);

    for (int i = 0; i < size; ++i) {
        jobject jfile = env->GetObjectArrayElement(jfiles, i);

        DataLoaderLocation location =
                (DataLoaderLocation)env->GetIntField(jfile, jni.installationFileLocation);
        std::string name =
                env->GetStringUTFChars((jstring)env->GetObjectField(jfile,
                                                                    jni.installationFileName),
                                       nullptr);
        IncFsSize size = env->GetLongField(jfile, jni.installationFileLengthBytes);

        auto jmetadataBytes = (jbyteArray)env->GetObjectField(jfile, jni.installationFileMetadata);
        auto metadataElements = env->GetByteArrayElements(jmetadataBytes, nullptr);
        auto metadataLength = env->GetArrayLength(jmetadataBytes);
        RawMetadata metadata(metadataElements, metadataElements + metadataLength);
        env->ReleaseByteArrayElements(jmetadataBytes, metadataElements, 0);

        files.emplace_back(location, std::move(name), size, std::move(metadata));
    }

    return DataLoaderInstallationFilesPair(std::move(files));
}

DataLoaderInstallationFilesPair::DataLoaderInstallationFilesPair(Files&& files)
      : mFiles(std::move(files)) {
    const auto size = mFiles.size();
    mNDKFiles.resize(size);
    for (size_t i = 0; i < size; ++i) {
        const auto& file = mFiles[i];
        auto&& ndkFile = mNDKFiles[i];

        ndkFile.location = file.location();
        ndkFile.name = file.name().c_str();
        ndkFile.size = file.size();
        ndkFile.metadata.data = file.metadata().data();
        ndkFile.metadata.size = file.metadata().size();
    }
}

bool DataLoaderService_OnPrepareImage(JNIEnv* env, jint storageId, jobjectArray addedFiles,
                                      jobjectArray removedFiles) {
    jobject listener;
    DataLoaderConnectorPtr dataLoaderConnector;
    {
        std::lock_guard lock{globals().dataLoaderConnectorsLock};
        auto dlIt = globals().dataLoaderConnectors.find(storageId);
        if (dlIt == globals().dataLoaderConnectors.end()) {
            ALOGE("Failed to handle onPrepareImage for id(%d): not found", storageId);
            return false;
        }
        listener = dlIt->second->getListenerLocalRef(env);
        dataLoaderConnector = dlIt->second;
    }

    auto addedFilesPair = DataLoaderInstallationFilesPair::createFromManaged(env, addedFiles);
    bool result = dataLoaderConnector->onPrepareImage(addedFilesPair.ndkFiles());

    const auto& jni = jniIds(env);
    reportStatusViaCallback(env, listener, storageId,
                            result ? jni.constants.DATA_LOADER_IMAGE_READY
                                   : jni.constants.DATA_LOADER_IMAGE_NOT_READY);

    return result;
}
