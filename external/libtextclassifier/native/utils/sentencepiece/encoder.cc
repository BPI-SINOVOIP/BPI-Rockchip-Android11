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

#include "utils/sentencepiece/encoder.h"

namespace libtextclassifier3 {

bool Encoder::Encode(StringPiece normalized_text,
                     std::vector<int>* encoded_text) const {
  const int len = normalized_text.size();
  if (len <= 0) {
    *encoded_text = {start_code_, end_code_};
    return true;
  }
  // We use `previous_pos` to indicate whether a dynamic programming state was
  // reachable.
  std::vector<SegmentationEntry> segmentation(
      len + 1, {/*score=*/0, /*previous_pos=*/-1, /*piece_id=*/-1,
                /*num_pieces=*/0});
  for (int i = 0; i < len; i++) {
    // State couldn't be reached.
    if (i > 0 && segmentation[i].previous_pos < 0) {
      // Advance position.
      normalized_text.RemovePrefix(1);
      continue;
    }
    // Check whether we can use the unknown token.
    if (unknown_code_ >= 0) {
      const int pos = i + 1;
      const float unknown_penalty = segmentation[i].score + unknown_score_;
      if (segmentation[pos].previous_pos < 0 ||
          segmentation[pos].score < unknown_penalty) {
        // Merge multiple unknown tokens into one.
        if (segmentation[i].piece_id == unknown_code_) {
          segmentation[pos] = {/*score=*/unknown_penalty,
                               /*previous_pos=*/segmentation[i].previous_pos,
                               /*piece_id=*/unknown_code_,
                               /*num_pieces=*/segmentation[i].num_pieces};
        } else {
          segmentation[pos] = {/*score=*/unknown_penalty,
                               /*previous_pos=*/i,
                               /*piece_id=*/unknown_code_,
                               /*num_pieces=*/segmentation[i].num_pieces + 1};
        }
      }
    }
    std::vector<StringSet::Match> matches;
    if (!pieces_->FindAllPrefixMatches(normalized_text, &matches)) {
      TC3_LOG(ERROR)
          << "Couldn't successfully gather prefix sentence piece matches.";
      return false;
    }
    for (const auto& match : matches) {
      TC3_CHECK(match.id >= 0 && match.id < num_pieces_);
      const int pos = i + match.match_length;
      const float candidate_score = segmentation[i].score + scores_[match.id];
      if (segmentation[pos].previous_pos < 0 ||
          segmentation[pos].score < candidate_score) {
        segmentation[pos] = {/*score=*/candidate_score, /*previous_pos=*/i,
                             /*piece_id=*/match.id + encoding_offset_,
                             /*num_pieces=*/segmentation[i].num_pieces + 1};
      }
    }
    // Advance position.
    normalized_text.RemovePrefix(1);
  }
  if (segmentation[len].num_pieces <= 0) {
    *encoded_text = {start_code_, end_code_};
    return true;
  }
  const int num_pieces = segmentation[len].num_pieces;
  encoded_text->resize(num_pieces + 2);
  (*encoded_text)[num_pieces + 1] = end_code_;
  int pos = len;
  for (int i = num_pieces; i > 0; i--) {
    (*encoded_text)[i] = segmentation[pos].piece_id;
    pos = segmentation[pos].previous_pos;
  }
  (*encoded_text)[0] = start_code_;
  return true;
}

}  // namespace libtextclassifier3
