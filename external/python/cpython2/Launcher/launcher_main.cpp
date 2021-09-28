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
#include <stdio.h>
#include <stdlib.h>
#include <string>

int main(int argc, char *argv[]) {
  // PYTHONEXECUTABLE is only used on MacOs X, when the Python interpreter
  // embedded in an application bundle. It is not sure that we have this use case
  // for Android hermetic Python. So remove this environment variable to make
  // our self-contained environment more strict.
  // For user (.py) program, it can access hermetic .par file path through
  // sys.argv[0].
  unsetenv(const_cast<char *>("PYTHONEXECUTABLE"));

  // Always enable Python "-s" option. We don't need user-site directories,
  // everything's supposed to be hermetic.
  Py_NoUserSiteDirectory = 1;

  // Ignore PYTHONPATH and PYTHONHOME from the environment. Unless we're not
  // running from inside the zip file, in which case the user may have
  // specified a PYTHONPATH.
#ifdef ANDROID_AUTORUN
  Py_IgnoreEnvironmentFlag = 1;
#endif

  Py_DontWriteBytecodeFlag = 1;

  // Resolving absolute path based on argv[0] is not reliable since it may
  // include something unusable, too bad.
  // android::base::GetExecutablePath() also handles for Darwin/Windows.
  std::string executable_path = android::base::GetExecutablePath();

  // Set the equivalent of PYTHONHOME internally.
  Py_SetPythonHome(strdup(executable_path.c_str()));

#ifdef ANDROID_AUTORUN
  argc += 1;
  char **new_argv = reinterpret_cast<char**>(calloc(argc, sizeof(*argv)));

  // Inject the path to our binary into argv[1] so the Py_Main won't parse any
  // other options, and will execute the __main__.py script inside the zip file
  // attached to our executable.
  new_argv[0] = argv[0];
  new_argv[1] = strdup(executable_path.c_str());
  for (int i = 1; i < argc - 1; i++) {
    new_argv[i+1] = argv[i];
  }
  argv = new_argv;
#endif

  return Py_Main(argc, argv);
}
