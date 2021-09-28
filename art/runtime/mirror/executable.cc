/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "executable-inl.h"

#include "art_method-inl.h"
#include "object-inl.h"

namespace art {
namespace mirror {

template <PointerSize kPointerSize, bool kTransactionActive>
bool Executable::CreateFromArtMethod(ArtMethod* method) {
  auto* interface_method = method->GetInterfaceMethodIfProxy(kPointerSize);
  SetArtMethod<kTransactionActive>(method);
  SetFieldObject<kTransactionActive>(DeclaringClassOffset(), method->GetDeclaringClass());
  SetFieldObject<kTransactionActive>(
      DeclaringClassOfOverriddenMethodOffset(), interface_method->GetDeclaringClass());
  SetField32<kTransactionActive>(AccessFlagsOffset(), method->GetAccessFlags());
  SetField32<kTransactionActive>(DexMethodIndexOffset(), method->GetDexMethodIndex());
  return true;
}

template bool Executable::CreateFromArtMethod<PointerSize::k32, false>(ArtMethod* method);
template bool Executable::CreateFromArtMethod<PointerSize::k32, true>(ArtMethod* method);
template bool Executable::CreateFromArtMethod<PointerSize::k64, false>(ArtMethod* method);
template bool Executable::CreateFromArtMethod<PointerSize::k64, true>(ArtMethod* method);

}  // namespace mirror
}  // namespace art
