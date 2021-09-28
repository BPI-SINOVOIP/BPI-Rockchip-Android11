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

// This is a compilation test only, to see that types are generated as
// expected.

#include <hidl2aidl/BnBar.h>
#include <hidl2aidl/BnFoo.h>
#include <hidl2aidl/BpBar.h>
#include <hidl2aidl/BpFoo.h>
#include <hidl2aidl/IBar.h>
#include <hidl2aidl/IBarInner.h>
#include <hidl2aidl/IFoo.h>
#include <hidl2aidl/IFooBigStruct.h>
#include <hidl2aidl/OnlyIn10.h>
#include <hidl2aidl/OnlyIn11.h>
#include <hidl2aidl/Outer.h>
#include <hidl2aidl/OuterInner.h>
#include <hidl2aidl/OverrideMe.h>
#include <hidl2aidl/Value.h>
#include <hidl2aidl2/BnFoo.h>
#include <hidl2aidl2/BpFoo.h>
#include <hidl2aidl2/IFoo.h>

using android::sp;
using android::String16;
using android::binder::Status;

void testIFoo(const sp<hidl2aidl::IFoo>& foo) {
    Status status1 = foo->someBar(String16());
    (void)status1;
    String16 f;
    Status status2 = foo->oneOutput(&f);
    (void)status2;
}

void testIBar(const sp<hidl2aidl::IBar>& bar) {
    String16 out;
    Status status1 = bar->someBar(String16(), 3, &out);
    (void)status1;
    hidl2aidl::IBarInner inner;
    inner.a = 3;
    Status status2 = bar->extraMethod(inner);
    (void)status2;
}

static_assert(static_cast<int>(hidl2aidl::Value::A) == 3);
static_assert(static_cast<int>(hidl2aidl::Value::B) == 7);
static_assert(static_cast<int>(hidl2aidl::Value::C) == 8);
static_assert(static_cast<int>(hidl2aidl::Value::D) == 9);
static_assert(static_cast<int>(hidl2aidl::Value::E) == 27);
static_assert(static_cast<int>(hidl2aidl::Value::F) == 28);

void testIFoo2(const sp<hidl2aidl2::IFoo>& foo) {
    Status status = foo->someFoo(3);
    (void)status;
}
