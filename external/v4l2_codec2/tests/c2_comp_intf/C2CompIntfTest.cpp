// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//#define LOG_NDEBUG 0
#define LOG_TAG "C2CompIntfTest"

#include <C2CompIntfTest.h>

#include <stdio.h>

namespace android {

namespace {

void dumpType(const C2FieldDescriptor::type_t type) {
    switch (type) {
    case C2FieldDescriptor::INT32:
        printf("int32_t");
        break;
    case C2FieldDescriptor::UINT32:
        printf("uint32_t");
        break;
    case C2FieldDescriptor::INT64:
        printf("int64_t");
        break;
    case C2FieldDescriptor::UINT64:
        printf("uint64_t");
        break;
    case C2FieldDescriptor::FLOAT:
        printf("float");
        break;
    default:
        printf("<flex>");
        break;
    }
}

void dumpStruct(const C2StructDescriptor& sd) {
    printf("  struct: { ");
    for (const C2FieldDescriptor& f : sd) {
        printf("%s:", f.name().c_str());
        dumpType(f.type());
        printf(", ");
    }
    printf("}\n");
}

}  // namespace

void C2CompIntfTest::dumpParamDescriptions() {
    std::vector<std::shared_ptr<C2ParamDescriptor>> params;

    ASSERT_EQ(mIntf->querySupportedParams_nb(&params), C2_OK);
    for (const auto& paramDesc : params) {
        printf("name: %s\n", paramDesc->name().c_str());
        printf("  required: %s\n", paramDesc->isRequired() ? "yes" : "no");
        printf("  type: %x\n", paramDesc->index().type());
        std::unique_ptr<C2StructDescriptor> desc{mReflector->describe(paramDesc->index().type())};
        if (desc.get()) dumpStruct(*desc);
    }
}

}  // namespace android
