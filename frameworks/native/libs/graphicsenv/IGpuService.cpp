/*
 * Copyright 2019 The Android Open Source Project
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

#define LOG_TAG "GpuService"

#include <graphicsenv/IGpuService.h>

#include <binder/IResultReceiver.h>
#include <binder/Parcel.h>

namespace android {

class BpGpuService : public BpInterface<IGpuService> {
public:
    explicit BpGpuService(const sp<IBinder>& impl) : BpInterface<IGpuService>(impl) {}

    void setGpuStats(const std::string& driverPackageName, const std::string& driverVersionName,
                     uint64_t driverVersionCode, int64_t driverBuildTime,
                     const std::string& appPackageName, const int32_t vulkanVersion,
                     GpuStatsInfo::Driver driver, bool isDriverLoaded,
                     int64_t driverLoadingTime) override {
        Parcel data, reply;
        data.writeInterfaceToken(IGpuService::getInterfaceDescriptor());

        data.writeUtf8AsUtf16(driverPackageName);
        data.writeUtf8AsUtf16(driverVersionName);
        data.writeUint64(driverVersionCode);
        data.writeInt64(driverBuildTime);
        data.writeUtf8AsUtf16(appPackageName);
        data.writeInt32(vulkanVersion);
        data.writeInt32(static_cast<int32_t>(driver));
        data.writeBool(isDriverLoaded);
        data.writeInt64(driverLoadingTime);

        remote()->transact(BnGpuService::SET_GPU_STATS, data, &reply, IBinder::FLAG_ONEWAY);
    }

    void setTargetStats(const std::string& appPackageName, const uint64_t driverVersionCode,
                        const GpuStatsInfo::Stats stats, const uint64_t value) override {
        Parcel data, reply;
        data.writeInterfaceToken(IGpuService::getInterfaceDescriptor());

        data.writeUtf8AsUtf16(appPackageName);
        data.writeUint64(driverVersionCode);
        data.writeInt32(static_cast<int32_t>(stats));
        data.writeUint64(value);

        remote()->transact(BnGpuService::SET_TARGET_STATS, data, &reply, IBinder::FLAG_ONEWAY);
    }

    void setUpdatableDriverPath(const std::string& driverPath) override {
        Parcel data, reply;
        data.writeInterfaceToken(IGpuService::getInterfaceDescriptor());
        data.writeUtf8AsUtf16(driverPath);

        remote()->transact(BnGpuService::SET_UPDATABLE_DRIVER_PATH, data, &reply,
                           IBinder::FLAG_ONEWAY);
    }

    std::string getUpdatableDriverPath() override {
        Parcel data, reply;
        data.writeInterfaceToken(IGpuService::getInterfaceDescriptor());

        status_t error = remote()->transact(BnGpuService::GET_UPDATABLE_DRIVER_PATH, data, &reply);
        std::string driverPath;
        if (error == OK) {
            error = reply.readUtf8FromUtf16(&driverPath);
        }
        return driverPath;
    }
};

IMPLEMENT_META_INTERFACE(GpuService, "android.graphicsenv.IGpuService");

status_t BnGpuService::onTransact(uint32_t code, const Parcel& data, Parcel* reply,
                                  uint32_t flags) {
    ALOGV("onTransact code[0x%X]", code);

    status_t status;
    switch (code) {
        case SET_GPU_STATS: {
            CHECK_INTERFACE(IGpuService, data, reply);

            std::string driverPackageName;
            if ((status = data.readUtf8FromUtf16(&driverPackageName)) != OK) return status;

            std::string driverVersionName;
            if ((status = data.readUtf8FromUtf16(&driverVersionName)) != OK) return status;

            uint64_t driverVersionCode;
            if ((status = data.readUint64(&driverVersionCode)) != OK) return status;

            int64_t driverBuildTime;
            if ((status = data.readInt64(&driverBuildTime)) != OK) return status;

            std::string appPackageName;
            if ((status = data.readUtf8FromUtf16(&appPackageName)) != OK) return status;

            int32_t vulkanVersion;
            if ((status = data.readInt32(&vulkanVersion)) != OK) return status;

            int32_t driver;
            if ((status = data.readInt32(&driver)) != OK) return status;

            bool isDriverLoaded;
            if ((status = data.readBool(&isDriverLoaded)) != OK) return status;

            int64_t driverLoadingTime;
            if ((status = data.readInt64(&driverLoadingTime)) != OK) return status;

            setGpuStats(driverPackageName, driverVersionName, driverVersionCode, driverBuildTime,
                        appPackageName, vulkanVersion, static_cast<GpuStatsInfo::Driver>(driver),
                        isDriverLoaded, driverLoadingTime);

            return OK;
        }
        case SET_TARGET_STATS: {
            CHECK_INTERFACE(IGpuService, data, reply);

            std::string appPackageName;
            if ((status = data.readUtf8FromUtf16(&appPackageName)) != OK) return status;

            uint64_t driverVersionCode;
            if ((status = data.readUint64(&driverVersionCode)) != OK) return status;

            int32_t stats;
            if ((status = data.readInt32(&stats)) != OK) return status;

            uint64_t value;
            if ((status = data.readUint64(&value)) != OK) return status;

            setTargetStats(appPackageName, driverVersionCode,
                           static_cast<GpuStatsInfo::Stats>(stats), value);

            return OK;
        }
        case SET_UPDATABLE_DRIVER_PATH: {
            CHECK_INTERFACE(IGpuService, data, reply);

            std::string driverPath;
            if ((status = data.readUtf8FromUtf16(&driverPath)) != OK) return status;

            setUpdatableDriverPath(driverPath);
            return OK;
        }
        case GET_UPDATABLE_DRIVER_PATH: {
            CHECK_INTERFACE(IGpuService, data, reply);

            std::string driverPath = getUpdatableDriverPath();
            return reply->writeUtf8AsUtf16(driverPath);
        }
        case SHELL_COMMAND_TRANSACTION: {
            int in = data.readFileDescriptor();
            int out = data.readFileDescriptor();
            int err = data.readFileDescriptor();

            std::vector<String16> args;
            data.readString16Vector(&args);

            sp<IBinder> unusedCallback;
            if ((status = data.readNullableStrongBinder(&unusedCallback)) != OK) return status;

            sp<IResultReceiver> resultReceiver;
            if ((status = data.readNullableStrongBinder(&resultReceiver)) != OK) return status;

            status = shellCommand(in, out, err, args);
            if (resultReceiver != nullptr) resultReceiver->send(status);

            return OK;
        }
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

} // namespace android
