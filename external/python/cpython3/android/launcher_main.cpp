// Copyright 2017 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <Python.h>
#include <android-base/file.h>
#include <osdefs.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

static_assert(sizeof(double) == SIZEOF_DOUBLE);
static_assert(sizeof(float) == SIZEOF_FLOAT);
static_assert(sizeof(fpos_t) == SIZEOF_FPOS_T);
static_assert(sizeof(int) == SIZEOF_INT);
static_assert(sizeof(long) == SIZEOF_LONG);
static_assert(sizeof(long double) == SIZEOF_LONG_DOUBLE);
static_assert(sizeof(long long) == SIZEOF_LONG_LONG);
static_assert(sizeof(off_t) == SIZEOF_OFF_T);
static_assert(sizeof(pid_t) == SIZEOF_PID_T);
static_assert(sizeof(pthread_key_t) == SIZEOF_PTHREAD_KEY_T);
static_assert(sizeof(pthread_t) == SIZEOF_PTHREAD_T);
static_assert(sizeof(short) == SIZEOF_SHORT);
static_assert(sizeof(size_t) == SIZEOF_SIZE_T);
static_assert(sizeof(time_t) == SIZEOF_TIME_T);
static_assert(sizeof(uintptr_t) == SIZEOF_UINTPTR_T);
static_assert(sizeof(void *) == SIZEOF_VOID_P);
static_assert(sizeof(wchar_t) == SIZEOF_WCHAR_T);
static_assert(sizeof(_Bool) == SIZEOF__BOOL);

// TODO(b/141583221): stop leaks
const char *__asan_default_options() {
    return "detect_leaks=0";
}

int main(int argc, char *argv[]) {
  // PYTHONEXECUTABLE is only used on MacOs X, when the Python interpreter
  // embedded in an application bundle. It is not sure that we have this use case
  // for Android hermetic Python. So remove this environment variable to make
  // our self-contained environment more strict.
  // For user (.py) program, it can access hermetic .par file path through
  // sys.argv[0].
  unsetenv(const_cast<char *>("PYTHONEXECUTABLE"));

  // Resolving absolute path based on argv[0] is not reliable since it may
  // include something unusable, too bad.
  // android::base::GetExecutablePath() also handles for Darwin/Windows.
  std::string executable_path = android::base::GetExecutablePath();
  std::string internal_path = executable_path + "/internal";
  std::string stdlib_path = internal_path + "/stdlib";

  PyStatus status;

  PyConfig config;
  PyConfig_InitPythonConfig(&config);

  wchar_t *path_entry;

  // Ignore PYTHONPATH and PYTHONHOME from the environment. Unless we're not
  // running from inside the zip file, in which case the user may have
  // specified a PYTHONPATH.
#ifdef ANDROID_AUTORUN
  config.use_environment = 0;
  config.module_search_paths_set = 1;
  config.parse_argv = 0;
#endif

  // Set the equivalent of PYTHONHOME internally.
  config.home = Py_DecodeLocale(executable_path.c_str(), NULL);
  if (config.home == NULL) {
    fprintf(stderr, "Unable to parse executable name\n");
    return 1;
  }

#ifdef ANDROID_AUTORUN
  // Execute the __main__.py script inside the zip file attached to our executable.
  config.run_filename = wcsdup(config.home);
#endif

  status = PyConfig_SetBytesArgv(&config, argc, argv);
  if (PyStatus_Exception(status)) {
    goto fail;
  }

  status = PyConfig_Read(&config);
  if (PyStatus_Exception(status)) {
    goto fail;
  }

  path_entry = Py_DecodeLocale(internal_path.c_str(), NULL);
  if (path_entry == NULL) {
    fprintf(stderr, "Unable to parse path\n");
    return 1;
  }

  status = PyWideStringList_Append(&config.module_search_paths, path_entry);
  if (PyStatus_Exception(status)) {
    goto fail;
  }

  path_entry = Py_DecodeLocale(stdlib_path.c_str(), NULL);
  if (path_entry == NULL) {
    fprintf(stderr, "Unable to parse path\n");
    return 1;
  }

  status = PyWideStringList_Append(&config.module_search_paths, path_entry);
  if (PyStatus_Exception(status)) {
    goto fail;
  }

  // Always enable Python "-S" option. We don't need site directories,
  // everything's supposed to be hermetic.
  config.site_import = 0;

  // Always enable Python "-s" option. We don't need user-site directories,
  // everything's supposed to be hermetic.
  config.user_site_directory = 0;

  config.write_bytecode = 0;

  // We've already parsed argv in PyConfig_Read
  config.parse_argv = 0;

  status = Py_InitializeFromConfig(&config);
  if (PyStatus_Exception(status)) {
    goto fail;
  }
  PyConfig_Clear(&config);

  return Py_RunMain();

fail:
  PyConfig_Clear(&config);
  if (PyStatus_IsExit(status)) {
      return status.exitcode;
  }
  Py_ExitStatusException(status);
}
