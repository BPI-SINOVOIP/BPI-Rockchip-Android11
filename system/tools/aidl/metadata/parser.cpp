/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <fstream>
#include <iostream>

#include <json/json.h>

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: aidl_metadata_parser *.json" << std::endl;
    return EXIT_FAILURE;
  }
  const std::string path = argv[1];

  Json::Value root;
  Json::Reader reader;

  std::ifstream stream(path);
  if (!reader.parse(stream, root)) {
    std::cerr << "Failed to read interface metadata file: " << path << std::endl
              << reader.getFormattedErrorMessages() << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "#include <aidl/metadata.h>" << std::endl;
  std::cout << "namespace android {" << std::endl;
  std::cout << "std::vector<AidlInterfaceMetadata> AidlInterfaceMetadata::all() {" << std::endl;
  std::cout << "return std::vector<AidlInterfaceMetadata>{" << std::endl;
  for (const Json::Value& entry : root) {
    std::cout << "AidlInterfaceMetadata{" << std::endl;
    // AIDL interface characters guaranteed to be accepted in C++ string
    std::cout << "std::string(\"" << entry["name"].asString() << "\")," << std::endl;
    std::cout << "std::string(\"" << entry["stability"].asString() << "\")," << std::endl;
    std::cout << "std::vector<std::string>{" << std::endl;
    for (const Json::Value& intf : entry["types"]) {
      std::cout << "std::string(\"" << intf.asString() << "\")," << std::endl;
    }
    std::cout << "}," << std::endl;
    std::cout << "std::vector<std::string>{" << std::endl;
    for (const Json::Value& intf : entry["hashes"]) {
      std::cout << "std::string(\"" << intf.asString() << "\")," << std::endl;
    }
    std::cout << "}," << std::endl;
    std::cout << "}," << std::endl;
  }
  std::cout << "};" << std::endl;
  std::cout << "}" << std::endl;
  std::cout << "}  // namespace android" << std::endl;
  return EXIT_SUCCESS;
}
