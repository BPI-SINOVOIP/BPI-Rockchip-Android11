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

#ifndef SURROUND_VIEW_SERVICE_IMPL_CONFIGREADERUTIL_H_
#define SURROUND_VIEW_SERVICE_IMPL_CONFIGREADERUTIL_H_

#include <tinyxml2.h>
#include <sstream>
#include <string>

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

// Macro returning false if condition evaluates to false.
#define RETURN_IF_FALSE(cond) \
    do {                      \
        if (!(cond)) {        \
            return false;     \
        }                     \
    } while (0)

// Returns true if element has text.
bool ElementHasText(const tinyxml2::XMLElement* element);

// Gets a xml element from the parent element, returns false if not found.
bool GetElement(const tinyxml2::XMLElement* parent, const char* elementName,
                tinyxml2::XMLElement const** element);

// Reads a boolean value from a element, returns false if not found.
bool ReadValue(const tinyxml2::XMLElement* parent, const char* elementName, bool* value);

// Reads a string value from a element, returns false if not found.
bool ReadValue(const tinyxml2::XMLElement* parent, const char* elementName, std::string* value);

// Reads a float value from a element, returns false if not found.
bool ReadValue(const tinyxml2::XMLElement* parent, const char* elementName, float* value);

// Reads a int value from a element, returns false if not found.
bool ReadValue(const tinyxml2::XMLElement* parent, const char* elementName, int* value);

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif  // SURROUND_VIEW_SERVICE_IMPL_CONFIGREADERUTIL_H_
