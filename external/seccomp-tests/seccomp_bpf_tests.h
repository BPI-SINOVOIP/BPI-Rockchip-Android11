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


/**
 * This header provides access to the Seccomp-BPF kernel test suite,
 * for use by CTS.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Runs a Seccomp kernel test named |name|. Returns 1 if the test passed
 * and 0 if the test failed.
 */
int run_seccomp_test(const char* name);

#ifdef __cplusplus
}
#endif
