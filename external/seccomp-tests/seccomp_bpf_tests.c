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

#include "seccomp_bpf_tests.h"

#include <android/log.h>
#include <string.h>

static const char TAG[] = "SeccompBpfTest-Native";

/* Forward declare from seccomp_bpf_tests.c. */
struct __test_metadata {
	const char *name;
	void (*fn)(struct __test_metadata *);
	int termsig;
	int passed;
	int trigger; /* extra handler after the evaluation */
	struct __test_metadata *prev, *next;
};
extern struct __test_metadata* get_seccomp_test_list();
extern void __run_test(struct __test_metadata*);

int run_seccomp_test(const char* name) {
    for (struct __test_metadata* t = get_seccomp_test_list(); t; t = t->next) {
        if (strcmp(t->name, name) == 0) {
            __android_log_print(ANDROID_LOG_INFO, TAG, "Start: %s", t->name);
            __run_test(t);
            __android_log_print(ANDROID_LOG_INFO, TAG, "%s: %s",
                t->passed ? "PASS" : "FAIL", t->name);
            return t->passed;
        }
    }
    return 0;
}
