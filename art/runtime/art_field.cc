/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "art_field.h"

#include "art_field-inl.h"
#include "base/utils.h"
#include "class_linker-inl.h"
#include "dex/descriptors_names.h"
#include "gc/accounting/card_table-inl.h"
#include "handle_scope.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "well_known_classes.h"

namespace art {

void ArtField::SetOffset(MemberOffset num_bytes) {
  DCHECK(GetDeclaringClass()->IsLoaded() || GetDeclaringClass()->IsErroneous());
  DCHECK_ALIGNED_PARAM(num_bytes.Uint32Value(),
                       Primitive::ComponentSize(GetTypeAsPrimitiveType()));
  // Not called within a transaction.
  offset_ = num_bytes.Uint32Value();
}

ObjPtr<mirror::Class> ArtField::ProxyFindSystemClass(const char* descriptor) {
  DCHECK(IsProxyField());
  ObjPtr<mirror::Class> klass = Runtime::Current()->GetClassLinker()->LookupClass(
      Thread::Current(), descriptor, /* class_loader= */ nullptr);
  DCHECK(klass != nullptr);
  return klass;
}

std::string ArtField::PrettyField(ArtField* f, bool with_type) {
  if (f == nullptr) {
    return "null";
  }
  return f->PrettyField(with_type);
}

std::string ArtField::PrettyField(bool with_type) {
  std::string result;
  if (with_type) {
    result += PrettyDescriptor(GetTypeDescriptor());
    result += ' ';
  }
  std::string temp;
  result += PrettyDescriptor(GetDeclaringClass()->GetDescriptor(&temp));
  result += '.';
  result += GetName();
  return result;
}

void ArtField::GetAccessFlagsDCheck() {
  CHECK(GetDeclaringClass()->IsLoaded() || GetDeclaringClass()->IsErroneous());
}

void ArtField::GetOffsetDCheck() {
  CHECK(GetDeclaringClass()->IsResolved());
}

}  // namespace art
