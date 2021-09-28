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

#include "utils/grammar/match.h"

#include <algorithm>
#include <stack>

namespace libtextclassifier3::grammar {

void Traverse(const Match* root,
              const std::function<bool(const Match*)>& node_fn) {
  std::stack<const Match*> open;
  open.push(root);

  while (!open.empty()) {
    const Match* node = open.top();
    open.pop();
    if (!node_fn(node) || node->IsLeaf()) {
      continue;
    }
    open.push(node->rhs2);
    if (node->rhs1 != nullptr) {
      open.push(node->rhs1);
    }
  }
}

const Match* SelectFirst(const Match* root,
                         const std::function<bool(const Match*)>& pred_fn) {
  std::stack<const Match*> open;
  open.push(root);

  while (!open.empty()) {
    const Match* node = open.top();
    open.pop();
    if (pred_fn(node)) {
      return node;
    }
    if (node->IsLeaf()) {
      continue;
    }
    open.push(node->rhs2);
    if (node->rhs1 != nullptr) {
      open.push(node->rhs1);
    }
  }

  return nullptr;
}

std::vector<const Match*> SelectAll(
    const Match* root, const std::function<bool(const Match*)>& pred_fn) {
  std::vector<const Match*> result;
  Traverse(root, [&result, pred_fn](const Match* node) {
    if (pred_fn(node)) {
      result.push_back(node);
    }
    return true;
  });
  return result;
}

}  // namespace libtextclassifier3::grammar
