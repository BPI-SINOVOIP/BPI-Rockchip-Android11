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

#ifndef LIBTEXTCLASSIFIER_UTILS_STRINGS_STRINGPIECE_H_
#define LIBTEXTCLASSIFIER_UTILS_STRINGS_STRINGPIECE_H_

#include <cstddef>
#include <string>

#include "utils/base/logging.h"

namespace libtextclassifier3 {

// Read-only "view" of a piece of data.  Does not own the underlying data.
class StringPiece {
 public:
  static constexpr size_t npos = static_cast<size_t>(-1);

  StringPiece() : StringPiece(nullptr, 0) {}

  StringPiece(const char* str)  // NOLINT(runtime/explicit)
      : start_(str), size_(str == nullptr ? 0 : strlen(str)) {}

  StringPiece(const char* start, size_t size) : start_(start), size_(size) {}

  // Intentionally no "explicit" keyword: in function calls, we want strings to
  // be converted to StringPiece implicitly.
  StringPiece(const std::string& s)  // NOLINT(runtime/explicit)
      : StringPiece(s.data(), s.size()) {}

  StringPiece(const std::string& s, int offset, int len)
      : StringPiece(s.data() + offset, len) {}

  char operator[](size_t i) const { return start_[i]; }

  // Returns start address of underlying data.
  const char* data() const { return start_; }

  // Returns number of bytes of underlying data.
  size_t size() const { return size_; }
  size_t length() const { return size_; }

  bool empty() const { return size_ == 0; }

  // Returns a std::string containing a copy of the underlying data.
  std::string ToString() const { return std::string(data(), size()); }

  // Returns whether string ends with a given suffix.
  bool EndsWith(StringPiece suffix) const {
    return suffix.empty() || (size_ >= suffix.size() &&
                              memcmp(start_ + (size_ - suffix.size()),
                                     suffix.data(), suffix.size()) == 0);
  }

  // Returns whether the string begins with a given prefix.
  bool StartsWith(StringPiece prefix) const {
    return prefix.empty() ||
           (size_ >= prefix.size() &&
            memcmp(start_, prefix.data(), prefix.size()) == 0);
  }

  bool Equals(StringPiece other) const {
    return size() == other.size() && memcmp(start_, other.data(), size_) == 0;
  }

  // Removes the first `n` characters from the string piece. Note that the
  // underlying string is not changed, only the view.
  void RemovePrefix(int n) {
    TC3_CHECK_LE(n, size_);
    start_ += n;
    size_ -= n;
  }

  // Removes the last `n` characters from the string piece. Note that the
  // underlying string is not changed, only the view.
  void RemoveSuffix(int n) {
    TC3_CHECK_LE(n, size_);
    size_ -= n;
  }

  // Finds the first occurrence of the substring `s` within the `StringPiece`,
  // returning the position of the first character's match, or `npos` if no
  // match was found.
  // Here
  // - c is the char to search for in the StringPiece
  // - pos is the position at which to start the search.
  size_t find(char c, size_t pos = 0) const noexcept {
    if (empty() || pos >= size_) {
      return npos;
    }
    const char* result =
        static_cast<const char*>(memchr(start_ + pos, c, size_ - pos));
    return result != nullptr ? result - start_ : npos;
  }

  size_t find(StringPiece s, size_t pos = 0) const noexcept {
    if (empty() || pos >= size_) {
      if (empty() && pos == 0 && s.empty()) {
        return 0;
      }
      return npos;
    }
    const char* result = memmatch(start_ + pos, size_ - pos, s.start_, s.size_);
    return result ? result - start_ : npos;
  }

 private:
  const char* memmatch(const char* phaystack, size_t haylen,
                       const char* pneedle, size_t neelen) const {
    if (0 == neelen) {
      return phaystack;  // Even if haylen is 0.
    }
    if (haylen < neelen) {
      return nullptr;
    }

    const char* match;
    const char* hayend = phaystack + haylen - neelen + 1;
    while ((match = static_cast<const char*>(
                memchr(phaystack, pneedle[0], hayend - phaystack)))) {
      if (memcmp(match, pneedle, neelen) == 0) {
        return match;
      } else {
        phaystack = match + 1;
      }
    }
    return nullptr;
  }

  const char* start_;  // Not owned.
  size_t size_;
};

inline bool EndsWith(StringPiece text, StringPiece suffix) {
  return text.EndsWith(suffix);
}

inline bool StartsWith(StringPiece text, StringPiece prefix) {
  return text.StartsWith(prefix);
}

inline bool ConsumePrefix(StringPiece* text, StringPiece prefix) {
  if (!text->StartsWith(prefix)) {
    return false;
  }
  text->RemovePrefix(prefix.size());
  return true;
}

inline bool ConsumeSuffix(StringPiece* text, StringPiece suffix) {
  if (!text->EndsWith(suffix)) {
    return false;
  }
  text->RemoveSuffix(suffix.size());
  return true;
}

inline logging::LoggingStringStream& operator<<(
    logging::LoggingStringStream& stream, StringPiece message) {
  stream.message.append(message.data(), message.size());
  return stream;
}

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_STRINGS_STRINGPIECE_H_
