/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "simple_philox.h"

#include <android-base/logging.h>

namespace tensorflow {
namespace random {

uint32 SimplePhilox::Skewed(int max_log) {
    CHECK(0 <= max_log && max_log <= 32);
    const int shift = Rand32() % (max_log + 1);
    const uint32 mask = shift == 32 ? ~static_cast<uint32>(0) : (1 << shift) - 1;
    return Rand32() & mask;
}

}  // namespace random
}  // namespace tensorflow
