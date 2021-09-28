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

#ifndef LIBTEXTCLASSIFIER_UTILS_SENTENCEPIECE_ENCODER_H_
#define LIBTEXTCLASSIFIER_UTILS_SENTENCEPIECE_ENCODER_H_

#include <vector>

#include "utils/base/logging.h"
#include "utils/container/string-set.h"
#include "utils/strings/stringpiece.h"

namespace libtextclassifier3 {

// Encoder to segment/tokenize strings into pieces such that the sum of the
// scores of the pieces used is maximized.
class Encoder {
 public:
  // pieces: the list of valid sentence pieces represented as a string set, e.g.
  //     a trie.
  // num_pieces: the number of pieces in the trie.
  // pieces_scores: the scores of the individual pieces.
  // start_code: code that is used as encoding of the start of input.
  // end_code: code that is used as encoding of the end of input.
  // encoding_offset: value added to the sentence piece ids to make them
  //     not interesecting with start_code and end_code.
  // unknown_code: code that is used for out-of-dictionary characters.
  // unknown_score: the penality score associated with the unknown code.
  Encoder(const StringSet* pieces, const int num_pieces,
          const float* pieces_scores, int start_code = 0, int end_code = 1,
          int encoding_offset = 2, int unknown_code = -1,
          float unknown_score = 0.f)
      : num_pieces_(num_pieces),
        scores_(pieces_scores),
        pieces_(pieces),
        start_code_(start_code),
        end_code_(end_code),
        encoding_offset_(encoding_offset),
        unknown_code_(unknown_code),
        unknown_score_(unknown_score) {}

  // Segment the input so that the total score of the pieces used is maximized.
  // This is a simplified implementation of the general Viterbi algorithm,
  // assuming independence between individual pieces.
  bool Encode(StringPiece normalized_text,
              std::vector<int>* encoded_text) const;

 private:
  // State in the dynamic programming algorithm.
  struct SegmentationEntry {
    // Accumulated score.
    float score;

    // Position before last piece.
    int previous_pos;

    // Last piece used.
    int piece_id;

    // Total number of pieces used.
    int num_pieces;
  };

  const int num_pieces_;
  const float* scores_;
  const StringSet* pieces_;
  const int start_code_;
  const int end_code_;
  const int encoding_offset_;
  const int unknown_code_;
  const int unknown_score_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_SENTENCEPIECE_ENCODER_H_
