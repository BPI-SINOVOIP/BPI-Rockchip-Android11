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

#ifndef LIBTEXTCLASSIFIER_UTILS_CONTAINER_DOUBLE_ARRAY_TRIE_H_
#define LIBTEXTCLASSIFIER_UTILS_CONTAINER_DOUBLE_ARRAY_TRIE_H_

#include <functional>
#include <vector>

#include "utils/base/endian.h"
#include "utils/base/integral_types.h"
#include "utils/container/string-set.h"
#include "utils/strings/stringpiece.h"

namespace libtextclassifier3 {

// A trie node specifies a node in the tree, either an intermediate node or
// a leaf node.
// A leaf node contains the id as an int of the string match. This id is encoded
// in the lower 31 bits, thus the number of distinct ids is 2^31.
// An intermediate node has an associated label and an offset to it's children.
// The label is encoded in the least significant byte and must match the input
// character during matching.
// We account for endianness when using the node values, as they are serialized
// (in little endian) as bytes in the flatbuffer model.
typedef uint32 TrieNode;

// A memory mappable trie, compatible with Darts::DoubleArray.
class DoubleArrayTrie : public StringSet {
 public:
  // nodes and nodes_length specify the array of the nodes of the trie.
  DoubleArrayTrie(const TrieNode* nodes, const int nodes_length)
      : nodes_(nodes), nodes_length_(nodes_length) {}

  // Find matches that are prefixes of a string.
  bool FindAllPrefixMatches(StringPiece input,
                            std::vector<Match>* matches) const override;
  // Find the longest prefix match of a string.
  bool LongestPrefixMatch(StringPiece input,
                          Match* longest_match) const override;

 private:
  // Returns whether a node as a leaf as a child.
  bool has_leaf(uint32 i) const { return nodes_[i] & 0x100; }

  // Available when a node is a leaf.
  int value(uint32 i) const {
    return static_cast<int>(LittleEndian::ToHost32(nodes_[i]) & 0x7fffffff);
  }

  // Label associated with a node.
  // A leaf node will have the MSB set and thus return an invalid label.
  uint32 label(uint32 i) const {
    return LittleEndian::ToHost32(nodes_[i]) & 0x800000ff;
  }

  // Returns offset to children.
  uint32 offset(uint32 i) const {
    const uint32 node = LittleEndian::ToHost32(nodes_[i]);
    return (node >> 10) << ((node & 0x200) >> 6);
  }

  bool GatherPrefixMatches(StringPiece input,
                           const std::function<void(Match)>& update_fn) const;

  const TrieNode* nodes_;
  const int nodes_length_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_CONTAINER_DOUBLE_ARRAY_TRIE_H_
