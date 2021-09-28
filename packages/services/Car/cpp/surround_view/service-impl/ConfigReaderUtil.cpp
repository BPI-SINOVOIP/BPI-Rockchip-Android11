/*
 * Copyright 2020 The Android Open Source Project
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

#include "ConfigReaderUtil.h"

#include <android-base/logging.h>
#include <tinyxml2.h>
#include <utility>

#include "core_lib.h"

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

using tinyxml2::XML_SUCCESS;
using tinyxml2::XMLElement;

bool ElementHasText(const XMLElement* element) {
    if (element->GetText() == "") {
        LOG(ERROR) << "Expected element to have text: " << element->Name();
        return false;
    }
    return true;
}

bool GetElement(const XMLElement* parent, const char* elementName, XMLElement const** element) {
    *element = parent->FirstChildElement(elementName);
    if (*element == nullptr) {
        LOG(ERROR) << "Expected element '" << elementName << "' in parent '" << parent->Name()
                   << "' not found";
        return false;
    }
    return true;
}

bool ReadValue(const XMLElement* parent, const char* elementName, bool* value) {
    const XMLElement* element = nullptr;
    RETURN_IF_FALSE(GetElement(parent, elementName, &element));
    if (element->QueryBoolText(value) != XML_SUCCESS) {
        LOG(ERROR) << "Failed to read valid boolean value from: " << element->Name();
        return false;
    }
    return true;
}

bool ReadValue(const XMLElement* parent, const char* elementName, std::string* value) {
    const XMLElement* element = nullptr;
    RETURN_IF_FALSE(GetElement(parent, elementName, &element));
    RETURN_IF_FALSE(ElementHasText(element));
    *value = std::string(element->GetText());
    return true;
}

bool ReadValue(const XMLElement* parent, const char* elementName, float* value) {
    const XMLElement* element = nullptr;
    RETURN_IF_FALSE(GetElement(parent, elementName, &element));
    if (element->QueryFloatText(value) != XML_SUCCESS) {
        LOG(ERROR) << "Failed to read valid float value from: " << element->Name();
        return false;
    }
    return true;
}

bool ReadValue(const XMLElement* parent, const char* elementName, int* value) {
    const XMLElement* element = nullptr;
    RETURN_IF_FALSE(GetElement(parent, elementName, &element));
    if (element->QueryIntText(value) != XML_SUCCESS) {
        LOG(ERROR) << "Failed to read valid int value from: " << element->Name();
        return false;
    }
    return true;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
