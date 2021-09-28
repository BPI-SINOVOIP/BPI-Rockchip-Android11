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

#include <aidl/hidl2aidl/BnBar.h>
#include <aidl/hidl2aidl/BnFoo.h>
#include <aidl/hidl2aidl/BpBar.h>
#include <aidl/hidl2aidl/BpFoo.h>
#include <aidl/hidl2aidl/IBar.h>
#include <aidl/hidl2aidl/IBarInner.h>
#include <aidl/hidl2aidl/IFoo.h>
#include <aidl/hidl2aidl/IFooBigStruct.h>
#include <aidl/hidl2aidl/OnlyIn10.h>
#include <aidl/hidl2aidl/OnlyIn11.h>
#include <aidl/hidl2aidl/Outer.h>
#include <aidl/hidl2aidl/OuterInner.h>
#include <aidl/hidl2aidl/OverrideMe.h>
#include <aidl/hidl2aidl/Value.h>
#include <aidl/hidl2aidl2/BnFoo.h>
#include <aidl/hidl2aidl2/BpFoo.h>
#include <aidl/hidl2aidl2/IFoo.h>

void testIFoo(const std::shared_ptr<aidl::hidl2aidl::IFoo>& foo) {
    ndk::ScopedAStatus status1 = foo->someBar(std::string());
    (void)status1;
    std::string f;
    ndk::ScopedAStatus status2 = foo->oneOutput(&f);
    (void)status2;
}

void testIBar(const std::shared_ptr<aidl::hidl2aidl::IBar>& bar) {
    std::string out;
    ndk::ScopedAStatus status1 = bar->someBar(std::string(), 3, &out);
    (void)status1;
    aidl::hidl2aidl::IBarInner inner;
    inner.a = 3;
    ndk::ScopedAStatus status2 = bar->extraMethod(inner);
    (void)status2;
}

static_assert(static_cast<int>(aidl::hidl2aidl::Value::A) == 3);
static_assert(static_cast<int>(aidl::hidl2aidl::Value::B) == 7);
static_assert(static_cast<int>(aidl::hidl2aidl::Value::C) == 8);
static_assert(static_cast<int>(aidl::hidl2aidl::Value::D) == 9);
static_assert(static_cast<int>(aidl::hidl2aidl::Value::E) == 27);
static_assert(static_cast<int>(aidl::hidl2aidl::Value::F) == 28);

void testIFoo2(const std::shared_ptr<aidl::hidl2aidl2::IFoo>& foo) {
    ndk::ScopedAStatus status = foo->someFoo(3);
    (void)status;
}
