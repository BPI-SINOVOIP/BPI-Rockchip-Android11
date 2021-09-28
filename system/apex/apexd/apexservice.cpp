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

#include "apexservice.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/result.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <binder/IPCThreadState.h>
#include <binder/IResultReceiver.h>
#include <binder/IServiceManager.h>
#include <binder/LazyServiceRegistrar.h>
#include <binder/ProcessState.h>
#include <binder/Status.h>
#include <private/android_filesystem_config.h>
#include <utils/String16.h>

#include "apex_file.h"
#include "apex_preinstalled_data.h"
#include "apexd.h"
#include "apexd_session.h"
#include "string_log.h"

#include <android/apex/BnApexService.h>

using android::base::Result;

namespace android {
namespace apex {
namespace binder {
namespace {

using BinderStatus = ::android::binder::Status;

BinderStatus CheckCallerIsRoot(const std::string& name) {
  uid_t uid = IPCThreadState::self()->getCallingUid();
  if (uid != AID_ROOT) {
    std::string msg = "Only root is allowed to call " + name;
    return BinderStatus::fromExceptionCode(BinderStatus::EX_SECURITY,
                                           String8(name.c_str()));
  }
  return BinderStatus::ok();
}

class ApexService : public BnApexService {
 public:
  using BinderStatus = ::android::binder::Status;
  using SessionState = ::apex::proto::SessionState;

  ApexService(){};

  BinderStatus stagePackages(const std::vector<std::string>& paths) override;
  BinderStatus unstagePackages(const std::vector<std::string>& paths) override;
  BinderStatus submitStagedSession(const ApexSessionParams& params,
                                   ApexInfoList* apex_info_list) override;
  BinderStatus markStagedSessionReady(int session_id) override;
  BinderStatus markStagedSessionSuccessful(int session_id) override;
  BinderStatus getSessions(std::vector<ApexSessionInfo>* aidl_return) override;
  BinderStatus getStagedSessionInfo(
      int session_id, ApexSessionInfo* apex_session_info) override;
  BinderStatus activatePackage(const std::string& packagePath) override;
  BinderStatus deactivatePackage(const std::string& packagePath) override;
  BinderStatus getActivePackages(std::vector<ApexInfo>* aidl_return) override;
  BinderStatus getActivePackage(const std::string& packageName,
                                ApexInfo* aidl_return) override;
  BinderStatus getAllPackages(std::vector<ApexInfo>* aidl_return) override;
  BinderStatus preinstallPackages(
      const std::vector<std::string>& paths) override;
  BinderStatus postinstallPackages(
      const std::vector<std::string>& paths) override;
  BinderStatus abortStagedSession(int session_id) override;
  BinderStatus revertActiveSessions() override;
  BinderStatus resumeRevertIfNeeded() override;
  BinderStatus snapshotCeData(int user_id, int rollback_id,
                              const std::string& apex_name,
                              int64_t* _aidl_return) override;
  BinderStatus restoreCeData(int user_id, int rollback_id,
                             const std::string& apex_name) override;
  BinderStatus destroyDeSnapshots(int rollback_id) override;
  BinderStatus destroyCeSnapshotsNotSpecified(
      int user_id, const std::vector<int>& retain_rollback_ids) override;
  BinderStatus remountPackages() override;

  status_t dump(int fd, const Vector<String16>& args) override;

  // Override onTransact so we can handle shellCommand.
  status_t onTransact(uint32_t _aidl_code, const Parcel& _aidl_data,
                      Parcel* _aidl_reply, uint32_t _aidl_flags) override;

  status_t shellCommand(int in, int out, int err, const Vector<String16>& args);
};

BinderStatus CheckDebuggable(const std::string& name) {
  if (!::android::base::GetBoolProperty("ro.debuggable", false)) {
    std::string tmp = name + " unavailable";
    return BinderStatus::fromExceptionCode(BinderStatus::EX_SECURITY,
                                           String8(tmp.c_str()));
  }
  return BinderStatus::ok();
}

BinderStatus ApexService::stagePackages(const std::vector<std::string>& paths) {
  BinderStatus debugCheck = CheckDebuggable("stagePackages");
  if (!debugCheck.isOk()) {
    return debugCheck;
  }
  LOG(DEBUG) << "stagePackages() received by ApexService, paths "
             << android::base::Join(paths, ',');

  Result<void> res = ::android::apex::stagePackages(paths);

  if (res.ok()) {
    return BinderStatus::ok();
  }

  // TODO: Get correct binder error status.
  LOG(ERROR) << "Failed to stage " << android::base::Join(paths, ',') << ": "
             << res.error();
  return BinderStatus::fromExceptionCode(
      BinderStatus::EX_ILLEGAL_ARGUMENT,
      String8(res.error().message().c_str()));
}

BinderStatus ApexService::unstagePackages(
    const std::vector<std::string>& paths) {
  Result<void> res = ::android::apex::unstagePackages(paths);
  if (res.ok()) {
    return BinderStatus::ok();
  }

  // TODO: Get correct binder error status.
  LOG(ERROR) << "Failed to unstage " << android::base::Join(paths, ',') << ": "
             << res.error();
  return BinderStatus::fromExceptionCode(
      BinderStatus::EX_ILLEGAL_ARGUMENT,
      String8(res.error().message().c_str()));
}

BinderStatus ApexService::submitStagedSession(const ApexSessionParams& params,
                                              ApexInfoList* apex_info_list) {
  LOG(DEBUG) << "submitStagedSession() received by ApexService, session id "
             << params.sessionId << " child sessions: ["
             << android::base::Join(params.childSessionIds, ',') << "]";

  Result<std::vector<ApexFile>> packages = ::android::apex::submitStagedSession(
      params.sessionId, params.childSessionIds, params.hasRollbackEnabled,
      params.isRollback, params.rollbackId);
  if (!packages.ok()) {
    LOG(ERROR) << "Failed to submit session id " << params.sessionId << ": "
               << packages.error();
    return BinderStatus::fromExceptionCode(
        BinderStatus::EX_SERVICE_SPECIFIC,
        String8(packages.error().message().c_str()));
  }

  for (const auto& package : *packages) {
    ApexInfo out;
    out.moduleName = package.GetManifest().name();
    out.modulePath = package.GetPath();
    out.versionCode = package.GetManifest().version();
    apex_info_list->apexInfos.push_back(out);
  }
  return BinderStatus::ok();
}

BinderStatus ApexService::markStagedSessionReady(int session_id) {
  LOG(DEBUG) << "markStagedSessionReady() received by ApexService, session id "
             << session_id;
  Result<void> success = ::android::apex::markStagedSessionReady(session_id);
  if (!success.ok()) {
    LOG(ERROR) << "Failed to mark session id " << session_id
               << " as ready: " << success.error();
    return BinderStatus::fromExceptionCode(
        BinderStatus::EX_SERVICE_SPECIFIC,
        String8(success.error().message().c_str()));
  }
  return BinderStatus::ok();
}

BinderStatus ApexService::markStagedSessionSuccessful(int session_id) {
  LOG(DEBUG)
      << "markStagedSessionSuccessful() received by ApexService, session id "
      << session_id;
  Result<void> ret = ::android::apex::markStagedSessionSuccessful(session_id);
  if (!ret.ok()) {
    LOG(ERROR) << "Failed to mark session " << session_id
               << " as SUCCESS: " << ret.error();
    return BinderStatus::fromExceptionCode(
        BinderStatus::EX_ILLEGAL_ARGUMENT,
        String8(ret.error().message().c_str()));
  }
  return BinderStatus::ok();
}

static void ClearSessionInfo(ApexSessionInfo* session_info) {
  session_info->sessionId = -1;
  session_info->isUnknown = false;
  session_info->isVerified = false;
  session_info->isStaged = false;
  session_info->isActivated = false;
  session_info->isRevertInProgress = false;
  session_info->isActivationFailed = false;
  session_info->isSuccess = false;
  session_info->isReverted = false;
  session_info->isRevertFailed = false;
}

void convertToApexSessionInfo(const ApexSession& session,
                              ApexSessionInfo* session_info) {
  using SessionState = ::apex::proto::SessionState;

  ClearSessionInfo(session_info);
  session_info->sessionId = session.GetId();
  session_info->crashingNativeProcess = session.GetCrashingNativeProcess();

  switch (session.GetState()) {
    case SessionState::VERIFIED:
      session_info->isVerified = true;
      break;
    case SessionState::STAGED:
      session_info->isStaged = true;
      break;
    case SessionState::ACTIVATED:
      session_info->isActivated = true;
      break;
    case SessionState::ACTIVATION_FAILED:
      session_info->isActivationFailed = true;
      break;
    case SessionState::SUCCESS:
      session_info->isSuccess = true;
      break;
    case SessionState::REVERT_IN_PROGRESS:
      session_info->isRevertInProgress = true;
      break;
    case SessionState::REVERTED:
      session_info->isReverted = true;
      break;
    case SessionState::REVERT_FAILED:
      session_info->isRevertFailed = true;
      break;
    case SessionState::UNKNOWN:
    default:
      session_info->isUnknown = true;
      break;
  }
}

static ApexInfo getApexInfo(const ApexFile& package) {
  ApexInfo out;
  out.moduleName = package.GetManifest().name();
  out.modulePath = package.GetPath();
  out.versionCode = package.GetManifest().version();
  out.versionName = package.GetManifest().versionname();
  out.isFactory = package.IsBuiltin();
  out.isActive = false;
  Result<std::string> preinstalledPath =
      getApexPreinstalledPath(package.GetManifest().name());
  if (preinstalledPath.ok()) {
    out.preinstalledModulePath = *preinstalledPath;
  }
  return out;
}

static std::string toString(const ApexInfo& package) {
  std::string msg = StringLog()
                    << "Module: " << package.moduleName
                    << " Version: " << package.versionCode
                    << " VersionName: " << package.versionName
                    << " Path: " << package.modulePath
                    << " IsActive: " << std::boolalpha << package.isActive
                    << " IsFactory: " << std::boolalpha << package.isFactory
                    << std::endl;
  return msg;
}

BinderStatus ApexService::getSessions(
    std::vector<ApexSessionInfo>* aidl_return) {
  auto sessions = ApexSession::GetSessions();
  for (const auto& session : sessions) {
    ApexSessionInfo sessionInfo;
    convertToApexSessionInfo(session, &sessionInfo);
    aidl_return->push_back(sessionInfo);
  }

  return BinderStatus::ok();
}

BinderStatus ApexService::getStagedSessionInfo(
    int session_id, ApexSessionInfo* apex_session_info) {
  LOG(DEBUG) << "getStagedSessionInfo() received by ApexService, session id "
             << session_id;
  auto session = ApexSession::GetSession(session_id);
  if (!session.ok()) {
    // Unknown session.
    ClearSessionInfo(apex_session_info);
    apex_session_info->isUnknown = true;
    return BinderStatus::ok();
  }

  convertToApexSessionInfo(*session, apex_session_info);

  return BinderStatus::ok();
}

BinderStatus ApexService::activatePackage(const std::string& packagePath) {
  BinderStatus debugCheck = CheckDebuggable("activatePackage");
  if (!debugCheck.isOk()) {
    return debugCheck;
  }

  LOG(DEBUG) << "activatePackage() received by ApexService, path "
             << packagePath;

  Result<void> res = ::android::apex::activatePackage(packagePath);

  if (res.ok()) {
    return BinderStatus::ok();
  }

  // TODO: Get correct binder error status.
  LOG(ERROR) << "Failed to activate " << packagePath << ": " << res.error();
  return BinderStatus::fromExceptionCode(
      BinderStatus::EX_ILLEGAL_ARGUMENT,
      String8(res.error().message().c_str()));
}

BinderStatus ApexService::deactivatePackage(const std::string& packagePath) {
  BinderStatus debugCheck = CheckDebuggable("deactivatePackage");
  if (!debugCheck.isOk()) {
    return debugCheck;
  }

  LOG(DEBUG) << "deactivatePackage() received by ApexService, path "
             << packagePath;

  Result<void> res = ::android::apex::deactivatePackage(packagePath);

  if (res.ok()) {
    return BinderStatus::ok();
  }

  // TODO: Get correct binder error status.
  LOG(ERROR) << "Failed to deactivate " << packagePath << ": " << res.error();
  return BinderStatus::fromExceptionCode(
      BinderStatus::EX_ILLEGAL_ARGUMENT,
      String8(res.error().message().c_str()));
}

BinderStatus ApexService::getActivePackages(
    std::vector<ApexInfo>* aidl_return) {
  auto packages = ::android::apex::getActivePackages();
  for (const auto& package : packages) {
    ApexInfo apexInfo = getApexInfo(package);
    apexInfo.isActive = true;
    aidl_return->push_back(std::move(apexInfo));
  }

  return BinderStatus::ok();
}

BinderStatus ApexService::getActivePackage(const std::string& packageName,
                                           ApexInfo* aidl_return) {
  Result<ApexFile> apex = ::android::apex::getActivePackage(packageName);
  if (apex.ok()) {
    *aidl_return = getApexInfo(*apex);
    aidl_return->isActive = true;
  }
  return BinderStatus::ok();
}

BinderStatus ApexService::getAllPackages(std::vector<ApexInfo>* aidl_return) {
  const auto& active = ::android::apex::getActivePackages();
  const auto& factory = ::android::apex::getFactoryPackages();
  for (const ApexFile& pkg : active) {
    ApexInfo apex_info = getApexInfo(pkg);
    apex_info.isActive = true;
    aidl_return->push_back(std::move(apex_info));
  }
  for (const ApexFile& pkg : factory) {
    const auto& same_path = [&pkg](const auto& o) {
      return o.GetPath() == pkg.GetPath();
    };
    if (std::find_if(active.begin(), active.end(), same_path) == active.end()) {
      aidl_return->push_back(getApexInfo(pkg));
    }
  }
  return BinderStatus::ok();
}

BinderStatus ApexService::preinstallPackages(
    const std::vector<std::string>& paths) {
  BinderStatus debugCheck = CheckDebuggable("preinstallPackages");
  if (!debugCheck.isOk()) {
    return debugCheck;
  }

  Result<void> res = ::android::apex::preinstallPackages(paths);
  if (res.ok()) {
    return BinderStatus::ok();
  }

  // TODO: Get correct binder error status.
  LOG(ERROR) << "Failed to preinstall packages "
             << android::base::Join(paths, ',') << ": " << res.error();
  return BinderStatus::fromExceptionCode(
      BinderStatus::EX_ILLEGAL_ARGUMENT,
      String8(res.error().message().c_str()));
}

BinderStatus ApexService::postinstallPackages(
    const std::vector<std::string>& paths) {
  BinderStatus debugCheck = CheckDebuggable("postinstallPackages");
  if (!debugCheck.isOk()) {
    return debugCheck;
  }

  Result<void> res = ::android::apex::postinstallPackages(paths);
  if (res.ok()) {
    return BinderStatus::ok();
  }

  // TODO: Get correct binder error status.
  LOG(ERROR) << "Failed to postinstall packages "
             << android::base::Join(paths, ',') << ": " << res.error();
  return BinderStatus::fromExceptionCode(
      BinderStatus::EX_ILLEGAL_ARGUMENT,
      String8(res.error().message().c_str()));
}

BinderStatus ApexService::abortStagedSession(int session_id) {
  LOG(DEBUG) << "abortStagedSession() received by ApexService.";
  Result<void> res = ::android::apex::abortStagedSession(session_id);
  if (!res.ok()) {
    return BinderStatus::fromExceptionCode(
        BinderStatus::EX_ILLEGAL_ARGUMENT,
        String8(res.error().message().c_str()));
  }
  return BinderStatus::ok();
}

BinderStatus ApexService::revertActiveSessions() {
  LOG(DEBUG) << "revertActiveSessions() received by ApexService.";
  Result<void> res = ::android::apex::revertActiveSessions("");
  if (!res.ok()) {
    return BinderStatus::fromExceptionCode(
        BinderStatus::EX_ILLEGAL_ARGUMENT,
        String8(res.error().message().c_str()));
  }
  return BinderStatus::ok();
}

BinderStatus ApexService::resumeRevertIfNeeded() {
  BinderStatus debugCheck = CheckDebuggable("resumeRevertIfNeeded");
  if (!debugCheck.isOk()) {
    return debugCheck;
  }

  LOG(DEBUG) << "resumeRevertIfNeeded() received by ApexService.";
  Result<void> res = ::android::apex::resumeRevertIfNeeded();
  if (!res.ok()) {
    return BinderStatus::fromExceptionCode(
        BinderStatus::EX_ILLEGAL_ARGUMENT,
        String8(res.error().message().c_str()));
  }
  return BinderStatus::ok();
}

BinderStatus ApexService::snapshotCeData(int user_id, int rollback_id,
                                         const std::string& apex_name,
                                         int64_t* _aidl_return) {
  LOG(DEBUG) << "snapshotCeData() received by ApexService.";
  Result<ino_t> res =
      ::android::apex::snapshotCeData(user_id, rollback_id, apex_name);
  if (!res.ok()) {
    return BinderStatus::fromExceptionCode(
        BinderStatus::EX_SERVICE_SPECIFIC,
        String8(res.error().message().c_str()));
  }
  *_aidl_return = static_cast<uint64_t>(*res);
  return BinderStatus::ok();
}

BinderStatus ApexService::restoreCeData(int user_id, int rollback_id,
                                        const std::string& apex_name) {
  LOG(DEBUG) << "restoreCeData() received by ApexService.";
  Result<void> res =
      ::android::apex::restoreCeData(user_id, rollback_id, apex_name);
  if (!res.ok()) {
    return BinderStatus::fromExceptionCode(
        BinderStatus::EX_SERVICE_SPECIFIC,
        String8(res.error().message().c_str()));
  }
  return BinderStatus::ok();
}

BinderStatus ApexService::destroyDeSnapshots(int rollback_id) {
  LOG(DEBUG) << "destroyDeSnapshots() received by ApexService.";
  Result<void> res = ::android::apex::destroyDeSnapshots(rollback_id);
  if (!res) {
    return BinderStatus::fromExceptionCode(
        BinderStatus::EX_SERVICE_SPECIFIC,
        String8(res.error().message().c_str()));
  }
  return BinderStatus::ok();
}

BinderStatus ApexService::destroyCeSnapshotsNotSpecified(
    int user_id, const std::vector<int>& retain_rollback_ids) {
  LOG(DEBUG) << "destroyCeSnapshotsNotSpecified() received by ApexService.";
  Result<void> res = ::android::apex::destroyCeSnapshotsNotSpecified(
      user_id, retain_rollback_ids);
  if (!res) {
    return BinderStatus::fromExceptionCode(
        BinderStatus::EX_SERVICE_SPECIFIC,
        String8(res.error().message().c_str()));
  }
  return BinderStatus::ok();
}

BinderStatus ApexService::remountPackages() {
  LOG(DEBUG) << "remountPackages() received by ApexService";
  if (auto debug = CheckDebuggable("remountPackages"); !debug.isOk()) {
    return debug;
  }
  if (auto root = CheckCallerIsRoot("remountPackages"); !root.isOk()) {
    return root;
  }
  if (auto res = ::android::apex::remountPackages(); !res.ok()) {
    return BinderStatus::fromExceptionCode(
        BinderStatus::EX_SERVICE_SPECIFIC,
        String8(res.error().message().c_str()));
  }
  return BinderStatus::ok();
}

status_t ApexService::onTransact(uint32_t _aidl_code, const Parcel& _aidl_data,
                                 Parcel* _aidl_reply, uint32_t _aidl_flags) {
  switch (_aidl_code) {
    case IBinder::SHELL_COMMAND_TRANSACTION: {
      int in = _aidl_data.readFileDescriptor();
      int out = _aidl_data.readFileDescriptor();
      int err = _aidl_data.readFileDescriptor();
      int argc = _aidl_data.readInt32();
      Vector<String16> args;
      for (int i = 0; i < argc && _aidl_data.dataAvail() > 0; i++) {
        args.add(_aidl_data.readString16());
      }
      sp<IBinder> unusedCallback;
      sp<IResultReceiver> resultReceiver;
      status_t status;
      if ((status = _aidl_data.readNullableStrongBinder(&unusedCallback)) != OK)
        return status;
      if ((status = _aidl_data.readNullableStrongBinder(&resultReceiver)) != OK)
        return status;
      status = shellCommand(in, out, err, args);
      if (resultReceiver != nullptr) {
        resultReceiver->send(status);
      }
      return OK;
    }
  }
  return BnApexService::onTransact(_aidl_code, _aidl_data, _aidl_reply,
                                   _aidl_flags);
}
status_t ApexService::dump(int fd, const Vector<String16>& /*args*/) {
  std::vector<ApexInfo> list;
  BinderStatus status = getActivePackages(&list);
  dprintf(fd, "ACTIVE PACKAGES:\n");
  if (!status.isOk()) {
    std::string msg = StringLog() << "Failed to retrieve packages: "
                                  << status.toString8().string() << std::endl;
    dprintf(fd, "%s", msg.c_str());
    return BAD_VALUE;
  } else {
    for (const auto& item : list) {
      std::string msg = toString(item);
      dprintf(fd, "%s", msg.c_str());
    }
  }

  dprintf(fd, "SESSIONS:\n");
  std::vector<ApexSession> sessions = ApexSession::GetSessions();

  for (const auto& session : sessions) {
    std::string child_ids_str = "";
    auto child_ids = session.GetChildSessionIds();
    if (child_ids.size() > 0) {
      child_ids_str = "Child IDs:";
      for (auto childSessionId : session.GetChildSessionIds()) {
        child_ids_str += " " + std::to_string(childSessionId);
      }
    }
    std::string revert_reason = "";
    std::string crashing_native_process = session.GetCrashingNativeProcess();
    if (!crashing_native_process.empty()) {
      revert_reason = " Revert Reason: " + crashing_native_process;
    }
    std::string msg =
        StringLog() << "Session ID: " << session.GetId() << child_ids_str
                    << " State: " << SessionState_State_Name(session.GetState())
                    << revert_reason << std::endl;
    dprintf(fd, "%s", msg.c_str());
  }

  return OK;
}

status_t ApexService::shellCommand(int in, int out, int err,
                                   const Vector<String16>& args) {
  if (in == BAD_TYPE || out == BAD_TYPE || err == BAD_TYPE) {
    return BAD_VALUE;
  }
  auto print_help = [](int fd, const char* prefix = nullptr) {
    StringLog log;
    if (prefix != nullptr) {
      log << prefix << std::endl;
    }
    log << "ApexService:" << std::endl
        << "  help - display this help" << std::endl
        << "  stagePackages [packagePath1] ([packagePath2]...) - stage "
           "multiple packages from the given path"
        << std::endl
        << "  getActivePackage [packageName] - return info for active package "
           "with given name, if present"
        << std::endl
        << "  getAllPackages - return the list of all packages" << std::endl
        << "  getActivePackages - return the list of active packages"
        << std::endl
        << "  activatePackage [packagePath] - activate package from the "
           "given path"
        << std::endl
        << "  deactivatePackage [packagePath] - deactivate package from the "
           "given path"
        << std::endl
        << "  preinstallPackages [packagePath1] ([packagePath2]...) - run "
           "pre-install hooks of the given packages"
        << std::endl
        << "  postinstallPackages [packagePath1] ([packagePath2]...) - run "
           "post-install hooks of the given packages"
        << std::endl
        << "  getStagedSessionInfo [sessionId] - displays information about a "
           "given session previously submitted"
        << std::endl
        << "  submitStagedSession [sessionId] - attempts to submit the "
           "installer session with given id"
        << std::endl
        << "  remountPackages - Force apexd to remount active packages. This "
           "call can be used to speed up development workflow of an APEX "
           "package. Example of usage:\n"
           "    1. adb shell stop\n"
           "    2. adb sync\n"
           "    3. adb shell cmd -w apexservice remountPackages\n"
           "    4. adb shell start\n"
           "\n"
           "Note: APEX package will be successfully remounted only if there "
           "are no alive processes holding a reference to it"
        << std::endl;
    dprintf(fd, "%s", log.operator std::string().c_str());
  };

  if (args.size() == 0) {
    print_help(err, "No command given");
    return BAD_VALUE;
  }

  const String16& cmd = args[0];

  if (cmd == String16("stagePackages")) {
    if (args.size() < 2) {
      print_help(err, "stagePackages requires at least one packagePath");
      return BAD_VALUE;
    }
    std::vector<std::string> pkgs;
    pkgs.reserve(args.size() - 1);
    for (size_t i = 1; i != args.size(); ++i) {
      pkgs.emplace_back(String8(args[i]).string());
    }
    BinderStatus status = stagePackages(pkgs);
    if (status.isOk()) {
      return OK;
    }
    std::string msg = StringLog() << "Failed to stage package(s): "
                                  << status.toString8().string() << std::endl;
    dprintf(err, "%s", msg.c_str());
    return BAD_VALUE;
  }
  if (cmd == String16("getAllPackages")) {
    if (args.size() != 1) {
      print_help(err, "Unrecognized options");
      return BAD_VALUE;
    }
    std::vector<ApexInfo> list;
    BinderStatus status = getAllPackages(&list);
    if (status.isOk()) {
      for (const auto& item : list) {
        std::string msg = toString(item);
        dprintf(out, "%s", msg.c_str());
      }
      return OK;
    }
    std::string msg = StringLog() << "Failed to retrieve packages: "
                                  << status.toString8().string() << std::endl;
    dprintf(err, "%s", msg.c_str());
    return BAD_VALUE;
  }

  if (cmd == String16("getActivePackages")) {
    if (args.size() != 1) {
      print_help(err, "Unrecognized options");
      return BAD_VALUE;
    }
    std::vector<ApexInfo> list;
    BinderStatus status = getActivePackages(&list);
    if (status.isOk()) {
      for (const auto& item : list) {
        std::string msg = toString(item);
        dprintf(out, "%s", msg.c_str());
      }
      return OK;
    }
    std::string msg = StringLog() << "Failed to retrieve packages: "
                                  << status.toString8().string() << std::endl;
    dprintf(err, "%s", msg.c_str());
    return BAD_VALUE;
  }

  if (cmd == String16("getActivePackage")) {
    if (args.size() != 2) {
      print_help(err, "Unrecognized options");
      return BAD_VALUE;
    }

    ApexInfo package;
    BinderStatus status = getActivePackage(String8(args[1]).string(), &package);
    if (status.isOk()) {
      std::string msg = toString(package);
      dprintf(out, "%s", msg.c_str());
      return OK;
    }

    std::string msg = StringLog() << "Failed to fetch active package: "
                                  << String8(args[1]).string()
                                  << ", error: " << status.toString8().string()
                                  << std::endl;
    dprintf(err, "%s", msg.c_str());
    return BAD_VALUE;
  }

  if (cmd == String16("activatePackage")) {
    if (args.size() != 2) {
      print_help(err, "activatePackage requires one packagePath");
      return BAD_VALUE;
    }
    BinderStatus status = activatePackage(String8(args[1]).string());
    if (status.isOk()) {
      return OK;
    }
    std::string msg = StringLog() << "Failed to activate package: "
                                  << status.toString8().string() << std::endl;
    dprintf(err, "%s", msg.c_str());
    return BAD_VALUE;
  }

  if (cmd == String16("deactivatePackage")) {
    if (args.size() != 2) {
      print_help(err, "deactivatePackage requires one packagePath");
      return BAD_VALUE;
    }
    BinderStatus status = deactivatePackage(String8(args[1]).string());
    if (status.isOk()) {
      return OK;
    }
    std::string msg = StringLog() << "Failed to deactivate package: "
                                  << status.toString8().string() << std::endl;
    dprintf(err, "%s", msg.c_str());
    return BAD_VALUE;
  }

  if (cmd == String16("getStagedSessionInfo")) {
    if (args.size() != 2) {
      print_help(err, "getStagedSessionInfo requires one session id");
      return BAD_VALUE;
    }
    int session_id = strtol(String8(args[1]).c_str(), nullptr, 10);
    if (session_id < 0) {
      std::string msg = StringLog()
                        << "Failed to parse session id. Must be an integer.";
      dprintf(err, "%s", msg.c_str());
      return BAD_VALUE;
    }

    ApexSessionInfo session_info;
    BinderStatus status = getStagedSessionInfo(session_id, &session_info);
    if (status.isOk()) {
      std::string revert_reason = "";
      std::string crashing_native_process = session_info.crashingNativeProcess;
      if (!crashing_native_process.empty()) {
        revert_reason = " revertReason: " + crashing_native_process;
      }
      std::string msg = StringLog()
                        << "session_info: "
                        << " isUnknown: " << session_info.isUnknown
                        << " isVerified: " << session_info.isVerified
                        << " isStaged: " << session_info.isStaged
                        << " isActivated: " << session_info.isActivated
                        << " isActivationFailed: "
                        << session_info.isActivationFailed << revert_reason
                        << std::endl;
      dprintf(out, "%s", msg.c_str());
      return OK;
    }
    std::string msg = StringLog() << "Failed to query session: "
                                  << status.toString8().string() << std::endl;
    dprintf(err, "%s", msg.c_str());
    return BAD_VALUE;
  }

  if (cmd == String16("submitStagedSession")) {
    if (args.size() != 2) {
      print_help(err, "submitStagedSession requires one session id");
      return BAD_VALUE;
    }
    int session_id = strtol(String8(args[1]).c_str(), nullptr, 10);
    if (session_id < 0) {
      std::string msg = StringLog()
                        << "Failed to parse session id. Must be an integer.";
      dprintf(err, "%s", msg.c_str());
      return BAD_VALUE;
    }

    ApexInfoList list;
    std::vector<int> empty_child_session_ids;
    ApexSessionParams params;
    params.sessionId = session_id;
    params.childSessionIds = empty_child_session_ids;
    BinderStatus status = submitStagedSession(params, &list);
    if (status.isOk()) {
        for (const auto& item : list.apexInfos) {
          std::string msg = toString(item);
          dprintf(out, "%s", msg.c_str());
        }
      return OK;
    }
    std::string msg = StringLog() << "Failed to submit session: "
                                  << status.toString8().string() << std::endl;
    dprintf(err, "%s", msg.c_str());
    return BAD_VALUE;
  }

  if (cmd == String16("preinstallPackages") ||
      cmd == String16("postinstallPackages")) {
    if (args.size() < 2) {
      print_help(err,
                 "preinstallPackages/postinstallPackages requires at least"
                 " one packagePath");
      return BAD_VALUE;
    }
    std::vector<std::string> pkgs;
    pkgs.reserve(args.size() - 1);
    for (size_t i = 1; i != args.size(); ++i) {
      pkgs.emplace_back(String8(args[i]).string());
    }
    BinderStatus status = cmd == String16("preinstallPackages")
                              ? preinstallPackages(pkgs)
                              : postinstallPackages(pkgs);
    if (status.isOk()) {
      return OK;
    }
    std::string msg = StringLog() << "Failed to pre/postinstall package(s): "
                                  << status.toString8().string() << std::endl;
    dprintf(err, "%s", msg.c_str());
    return BAD_VALUE;
  }

  if (cmd == String16("remountPackages")) {
    BinderStatus status = remountPackages();
    if (status.isOk()) {
      return OK;
    }
    std::string msg = StringLog() << "remountPackages failed: "
                                  << status.toString8().string() << std::endl;
    dprintf(err, "%s", msg.c_str());
    return BAD_VALUE;
  }

  if (cmd == String16("help")) {
    if (args.size() != 1) {
      print_help(err, "Help has no options");
      return BAD_VALUE;
    }
    print_help(out);
    return OK;
  }

  print_help(err);
  return BAD_VALUE;
}

}  // namespace

static constexpr const char* kApexServiceName = "apexservice";

using android::IPCThreadState;
using android::ProcessState;
using android::sp;
using android::binder::LazyServiceRegistrar;

void CreateAndRegisterService() {
  sp<ProcessState> ps(ProcessState::self());

  // Create binder service and register with LazyServiceRegistrar
  sp<ApexService> apexService = new ApexService();
  auto lazyRegistrar = LazyServiceRegistrar::getInstance();
  lazyRegistrar.forcePersist(true);
  lazyRegistrar.registerService(apexService, kApexServiceName);
}

void AllowServiceShutdown() {
  auto lazyRegistrar = LazyServiceRegistrar::getInstance();
  lazyRegistrar.forcePersist(false);
}

void StartThreadPool() {
  sp<ProcessState> ps(ProcessState::self());

  // Start threadpool, wait for IPC
  ps->startThreadPool();
}

void JoinThreadPool() {
  IPCThreadState::self()->joinThreadPool();  // should not return
}

}  // namespace binder
}  // namespace apex
}  // namespace android
