/*
 * Copyright 2018 The Android Open Source Project
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
 *
 */

#include <cstdio>
#include "gtest/gtest.h"
#include <getopt.h>

static bool isGameCoreCertified = false;
static bool doFeatureCheck = true;

bool shouldSkipTest() {
    return doFeatureCheck && !isGameCoreCertified;
}

static const int OPTION_CERTIFIED = 0;
static const int OPTION_FEATURE_CHECK = 1;

int main(int argc, char **argv) {
    printf("Running main() from %s\n", __FILE__);
    testing::InitGoogleTest(&argc, argv);

    static struct option long_options[] = {
        { "gamecore-certified",     required_argument, 0, 0 },  // default to false
        { "gamecore-feature-check", required_argument, 0, 1 },  // default to true
        { 0,                        0,                 0, 0 }
    };
    while(true) {
        int c = getopt_long(argc, argv, "", long_options, nullptr);
        if (c == -1) {
            break;
        }
        switch (c) {
        case OPTION_CERTIFIED:
            std::istringstream(optarg) >> std::boolalpha >> isGameCoreCertified;
            break;
        case OPTION_FEATURE_CHECK:
            std::istringstream(optarg) >> std::boolalpha >> doFeatureCheck;
            break;
        default:
            break;
        }
    }

    return RUN_ALL_TESTS();
}
