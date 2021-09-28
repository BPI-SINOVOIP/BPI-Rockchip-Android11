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

#include <climits>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <android-base/result.h>

#include "linkerconfig/apex.h"
#include "linkerconfig/apexconfig.h"
#include "linkerconfig/baseconfig.h"
#include "linkerconfig/context.h"
#include "linkerconfig/environment.h"
#include "linkerconfig/legacy.h"
#include "linkerconfig/log.h"
#include "linkerconfig/namespacebuilder.h"
#include "linkerconfig/recovery.h"
#include "linkerconfig/variableloader.h"
#include "linkerconfig/variables.h"

using android::base::ErrnoError;
using android::base::Error;
using android::base::Result;
using android::linkerconfig::contents::Context;
using android::linkerconfig::modules::ApexInfo;
using android::linkerconfig::modules::Configuration;

namespace {
const static struct option program_options[] = {
    {"target", required_argument, 0, 't'},
    {"strict", no_argument, 0, 's'},
#ifndef __ANDROID__
    {"root", required_argument, 0, 'r'},
    {"vndk", required_argument, 0, 'v'},
    {"recovery", no_argument, 0, 'y'},
    {"legacy", no_argument, 0, 'l'},
#endif
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}};

struct ProgramArgs {
  std::string target_directory;
  bool strict;
  std::string root;
  std::string vndk_version;
  bool is_recovery;
  bool is_legacy;
};

[[noreturn]] void PrintUsage(int status = EXIT_SUCCESS) {
  std::cerr << "Usage : linkerconfig [--target <target_directory>]"
               " [--strict]"
#ifndef __ANDROID__
               " --root <root dir>"
               " --vndk <vndk version>"
               " --recovery"
               " --legacy"
#endif
               " [--help]"
            << std::endl;
  exit(status);
}

std::string RealPath(std::string_view path) {
  char resolved_path[PATH_MAX];
  if (realpath(path.data(), resolved_path) != nullptr) {
    return resolved_path;
  }
  PrintUsage(-1);
}

bool ParseArgs(int argc, char* argv[], ProgramArgs* args) {
  int parse_result;
  while ((parse_result = getopt_long(
              argc, argv, "t:sr:v:hyl", program_options, NULL)) != -1) {
    switch (parse_result) {
      case 't':
        args->target_directory = optarg;
        break;
      case 's':
        args->strict = true;
        break;
      case 'r':
        args->root = RealPath(optarg);
        break;
      case 'v':
        args->vndk_version = optarg;
        break;
      case 'y':
        args->is_recovery = true;
        break;
      case 'l':
        args->is_legacy = true;
        break;
      case 'h':
        PrintUsage();
      default:
        return false;
    }
  }

  if (optind < argc) {
    return false;
  }

  return true;
}

void LoadVariables(ProgramArgs args) {
#ifndef __ANDROID__
  if (!args.is_recovery && (args.root == "" || args.vndk_version == "")) {
    PrintUsage();
  }
  android::linkerconfig::modules::Variables::AddValue("ro.vndk.version",
                                                      args.vndk_version);
#endif

  if (!args.is_recovery) {
    android::linkerconfig::generator::LoadVariables(args.root);
  }
}

Result<void> WriteConfigurationToFile(Configuration& conf,
                                      std::string file_path) {
  std::ostream* out = &std::cout;
  std::ofstream file_out;

  if (file_path != "") {
    file_out.open(file_path);
    if (file_out.fail()) {
      return ErrnoError() << "Failed to open file " << file_path;
    }
    out = &file_out;
  }

  android::linkerconfig::modules::ConfigWriter config_writer;

  conf.WriteConfig(config_writer);
  *out << config_writer.ToString();
  if (!out->good()) {
    return ErrnoError() << "Failed to write content to " << file_path;
  }

  return {};
}

Result<void> UpdatePermission([[maybe_unused]] const std::string& file_path) {
#ifdef __ANDROID__
  if (fchmodat(AT_FDCWD,
               file_path.c_str(),
               S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH,
               AT_SYMLINK_NOFOLLOW) < 0) {
    return ErrnoError() << "Failed to update permission of " << file_path;
  }
#endif

  return {};
}

Context GetContext(ProgramArgs args) {
  auto apex_list = android::linkerconfig::modules::ScanActiveApexes(args.root);
  Context ctx;
  for (auto const& apex_item : apex_list) {
    auto apex_info = apex_item.second;
    if (apex_info.has_bin || apex_info.has_lib) {
      ctx.AddApexModule(std::move(apex_info));
    }
  }
  if (args.strict) {
    ctx.SetStrictMode(true);
  }
  android::linkerconfig::contents::RegisterApexNamespaceBuilders(ctx);
  return ctx;
}

Configuration GetConfiguration(Context& ctx) {
  if (android::linkerconfig::modules::IsRecoveryMode()) {
    return android::linkerconfig::contents::CreateRecoveryConfiguration(ctx);
  }

  if (android::linkerconfig::modules::IsLegacyDevice()) {
    return android::linkerconfig::contents::CreateLegacyConfiguration(ctx);
  }

  // Use base configuration in default
  return android::linkerconfig::contents::CreateBaseConfiguration(ctx);
}

Result<void> GenerateConfiguration(Configuration config, std::string dir_path,
                                   bool update_permission) {
  std::string file_path = "";
  if (dir_path != "") {
    file_path = dir_path + "/ld.config.txt";
  }

  auto write_config = WriteConfigurationToFile(config, file_path);
  if (!write_config.ok()) {
    return write_config;
  } else if (update_permission && file_path != "") {
    return UpdatePermission(file_path);
  }

  return {};
}

Result<void> GenerateBaseLinkerConfiguration(Context& ctx,
                                             const std::string& dir_path) {
  return GenerateConfiguration(GetConfiguration(ctx), dir_path, true);
}

Result<void> GenerateRecoveryLinkerConfiguration(Context& ctx,
                                                 const std::string& dir_path) {
  return GenerateConfiguration(
      android::linkerconfig::contents::CreateRecoveryConfiguration(ctx),
      dir_path,
      false);
}

Result<void> GenerateLegacyLinkerConfiguration(Context& ctx,
                                               const std::string& dir_path) {
  return GenerateConfiguration(
      android::linkerconfig::contents::CreateLegacyConfiguration(ctx),
      dir_path,
      false);
}

Result<void> GenerateApexConfiguration(
    const std::string& base_dir, android::linkerconfig::contents::Context& ctx,
    const android::linkerconfig::modules::ApexInfo& target_apex) {
  std::string dir_path = base_dir + "/" + target_apex.name;
  if (auto ret = mkdir(dir_path.c_str(), 0755); ret != 0 && errno != EEXIST) {
    return ErrnoError() << "Failed to create directory " << dir_path;
  }

  return GenerateConfiguration(
      android::linkerconfig::contents::CreateApexConfiguration(ctx, target_apex),
      dir_path,
      true);
}

void GenerateApexConfigurations(Context& ctx, const std::string& dir_path) {
  for (auto const& apex_item : ctx.GetApexModules()) {
    if (apex_item.has_bin) {
      auto result = GenerateApexConfiguration(dir_path, ctx, apex_item);
      if (!result.ok()) {
        LOG(WARNING) << result.error();
      }
    }
  }
}

void ExitOnFailure(Result<void> task) {
  if (!task.ok()) {
    LOG(FATAL) << task.error();
    exit(EXIT_FAILURE);
  }
}

#ifdef __ANDROID__
struct CombinedLogger {
  android::base::LogdLogger logd;

  void operator()(android::base::LogId id, android::base::LogSeverity severity,
                  const char* tag, const char* file, unsigned int line,
                  const char* message) {
    logd(id, severity, tag, file, line, message);
    KernelLogger(id, severity, tag, file, line, message);
  }
};
#endif
}  // namespace

int main(int argc, char* argv[]) {
  android::base::InitLogging(argv
#ifdef __ANDROID__
                             ,
                             CombinedLogger()
#endif
  );

  ProgramArgs args = {};

  if (!ParseArgs(argc, argv, &args)) {
    PrintUsage(EXIT_FAILURE);
  }

  LoadVariables(args);
  Context ctx = GetContext(args);

  // when exec'ed from init, this is 0x0077, which makes the subdirectories
  // inaccessible for others. set umask to 0x0022 so that they can be
  // accessible.
  umask(0x0022);

  if (args.is_recovery) {
    ExitOnFailure(
        GenerateRecoveryLinkerConfiguration(ctx, args.target_directory));
  } else if (args.is_legacy) {
    ExitOnFailure(GenerateLegacyLinkerConfiguration(ctx, args.target_directory));
  } else {
    ExitOnFailure(GenerateBaseLinkerConfiguration(ctx, args.target_directory));
    GenerateApexConfigurations(ctx, args.target_directory);
  }

  return EXIT_SUCCESS;
}
