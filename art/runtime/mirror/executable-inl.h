/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_MIRROR_EXECUTABLE_INL_H_
#define ART_RUNTIME_MIRROR_EXECUTABLE_INL_H_

#include "executable.h"

#include "object-inl.h"
#include "reflective_value_visitor.h"
#include "verify_object.h"

namespace art {
namespace mirror {

template <bool kTransactionActive,
          bool kCheckTransaction,
          VerifyObjectFlags kVerifyFlags>
inline void Executable::SetArtMethod(ArtMethod* method) {
  SetField64<kTransactionActive, kCheckTransaction, kVerifyFlags>(
      ArtMethodOffset(), reinterpret_cast64<uint64_t>(method));
}

inline ObjPtr<mirror::Class> Executable::GetDeclaringClass() {
  return GetFieldObject<mirror::Class>(DeclaringClassOffset());
}

template<VerifyObjectFlags kVerifiyFlags>
inline void Executable::VisitTarget(ReflectiveValueVisitor* v) {
  HeapReflectiveSourceInfo hrsi(kSourceJavaLangReflectExecutable, this);
  ArtMethod* orig = GetArtMethod<kVerifiyFlags>();
  ArtMethod* new_target = v->VisitMethod(orig, hrsi);
  if (orig != new_target) {
    SetArtMethod(new_target);
    SetDexMethodIndex(new_target->GetDexMethodIndex());
    SetDeclaringClass(new_target->GetDeclaringClass());
    WriteBarrier::ForEveryFieldWrite(this);
  }
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_EXECUTABLE_INL_H_
