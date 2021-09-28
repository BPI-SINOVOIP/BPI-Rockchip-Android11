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

#include <dirent.h>
#include <string>

#include <android-base/properties.h>
#include <android-base/strings.h>
#include "utility/ValidateXml.h"

using std::string_literals::operator""s;

static void get_files_in_dirs(const char* dir_path, std::vector<std::string>& files) {
    DIR* d;
    struct dirent* de;

    d = opendir(dir_path);
    if (d == nullptr) {
        return;
    }

    while ((de = readdir(d))) {
        if (de->d_type != DT_REG) {
            continue;
        }
        files.push_back(de->d_name);
    }
    closedir(d);
}

TEST(CheckConfig, halManifestValidation) {
    if (android::base::GetIntProperty("ro.product.first_api_level", INT64_MAX) <= 28) {
        GTEST_SKIP();
    }

    RecordProperty("description",
                   "Verify that the hal manifest file "
                   "is valid according to the schema");

    constexpr const char* xsd = "/data/local/tmp/hal_manifest.xsd";

    // There may be compatibility matrices in .../etc/vintf. Manifests are only loaded from
    // manifest.xml and manifest_*.xml, so only check those.
    std::vector<const char*> vintf_locations = {"/vendor/etc/vintf", "/odm/etc/vintf"};
    for (const char* dir_path : vintf_locations) {
        std::vector<std::string> files;
        get_files_in_dirs(dir_path, files);
        for (std::string file_name : files) {
            if (android::base::StartsWith(file_name, "manifest")) {
                EXPECT_VALID_XML((dir_path + "/"s + file_name).c_str(), xsd);
            }
        }
    }

    // .../etc/vintf/manifest should only contain manifest fragments, so all of them must match the
    // schema.
    std::vector<const char*> fragment_locations = {"/vendor/etc/vintf/manifest",
                                                   "/odm/etc/vintf/manifest"};
    for (const char* dir_path : fragment_locations) {
        std::vector<std::string> files;
        get_files_in_dirs(dir_path, files);
        for (std::string file_name : files) {
            EXPECT_VALID_XML((dir_path + "/"s + file_name).c_str(), xsd);
        }
    }
}
