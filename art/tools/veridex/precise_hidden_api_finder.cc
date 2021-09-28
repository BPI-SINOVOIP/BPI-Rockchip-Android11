/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "precise_hidden_api_finder.h"

#include "class_filter.h"
#include "dex/class_accessor-inl.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_instruction-inl.h"
#include "dex/dex_file.h"
#include "dex/method_reference.h"
#include "flow_analysis.h"
#include "hidden_api.h"
#include "resolver.h"
#include "veridex.h"

#include <iostream>

namespace art {

void PreciseHiddenApiFinder::RunInternal(
    const std::vector<std::unique_ptr<VeridexResolver>>& resolvers,
    const ClassFilter& class_filter,
    const std::function<void(VeridexResolver*, const ClassAccessor::Method&)>& action) {
  for (const std::unique_ptr<VeridexResolver>& resolver : resolvers) {
    for (ClassAccessor accessor : resolver->GetDexFile().GetClasses()) {
      if (class_filter.Matches(accessor.GetDescriptor())) {
        for (const ClassAccessor::Method& method : accessor.GetMethods()) {
          if (method.GetCodeItem() != nullptr) {
            action(resolver.get(), method);
          }
        }
      }
    }
  }
}

void PreciseHiddenApiFinder::AddUsesAt(const std::vector<ReflectAccessInfo>& accesses,
                                       MethodReference ref) {
  for (const ReflectAccessInfo& info : accesses) {
    if (info.IsConcrete()) {
      concrete_uses_[ref].push_back(info);
    } else {
      abstract_uses_[ref].push_back(info);
    }
  }
}

void PreciseHiddenApiFinder::Run(const std::vector<std::unique_ptr<VeridexResolver>>& resolvers,
                                 const ClassFilter& class_filter) {
  // Collect reflection uses.
  RunInternal(resolvers,
              class_filter,
              [this] (VeridexResolver* resolver, const ClassAccessor::Method& method) {
    FlowAnalysisCollector collector(resolver, method);
    collector.Run();
    AddUsesAt(collector.GetUses(), method.GetReference());
  });

  // For non-final reflection uses, do a limited fixed point calculation over the code to try
  // substituting them with final reflection uses.
  // We limit the number of times we iterate over the code as one run can be long.
  static const int kMaximumIterations = 10;
  uint32_t i = 0;
  while (!abstract_uses_.empty() && (i++ < kMaximumIterations)) {
    // Fetch and clear the worklist.
    std::map<MethodReference, std::vector<ReflectAccessInfo>> current_uses
        = std::move(abstract_uses_);
    RunInternal(resolvers,
                class_filter,
                [this, current_uses] (VeridexResolver* resolver,
                                      const ClassAccessor::Method& method) {
      FlowAnalysisSubstitutor substitutor(resolver, method, current_uses);
      substitutor.Run();
      AddUsesAt(substitutor.GetUses(), method.GetReference());
    });
  }
}

void PreciseHiddenApiFinder::Dump(std::ostream& os, HiddenApiStats* stats) {
  static const char* kPrefix = "       ";
  std::map<std::string, std::vector<MethodReference>> named_uses;
  for (auto& it : concrete_uses_) {
    MethodReference ref = it.first;
    for (const ReflectAccessInfo& info : it.second) {
      std::string cls(info.cls.ToString());
      std::string name(info.name.ToString());
      std::string full_name = cls + "->" + name;
      named_uses[full_name].push_back(ref);
    }
  }

  for (auto& it : named_uses) {
    const std::string& full_name = it.first;
    if (hidden_api_.GetSignatureSource(full_name) != SignatureSource::APP &&
        hidden_api_.ShouldReport(full_name)) {
      stats->reflection_count++;
      hiddenapi::ApiList api_list = hidden_api_.GetApiList(full_name);
      stats->api_counts[api_list.GetIntValue()]++;
      os << "#" << ++stats->count << ": Reflection " << api_list << " " << full_name << " use(s):";
      os << std::endl;
      for (const MethodReference& ref : it.second) {
        os << kPrefix << HiddenApi::GetApiMethodName(ref) << std::endl;
      }
      os << std::endl;
    }
  }
}

}  // namespace art
