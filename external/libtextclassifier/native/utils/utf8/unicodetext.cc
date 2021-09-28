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

#include "utils/utf8/unicodetext.h"

#include <string.h>

#include <algorithm>

#include "utils/base/logging.h"
#include "utils/strings/utf8.h"

namespace libtextclassifier3 {

// *************** Data representation **********
// Note: the copy constructor is undefined.

UnicodeText::Repr& UnicodeText::Repr::operator=(Repr&& src) {
  if (ours_ && data_) delete[] data_;
  data_ = src.data_;
  size_ = src.size_;
  capacity_ = src.capacity_;
  ours_ = src.ours_;
  src.ours_ = false;
  return *this;
}

void UnicodeText::Repr::PointTo(const char* data, int size) {
  if (ours_ && data_) delete[] data_;  // If we owned the old buffer, free it.
  data_ = const_cast<char*>(data);
  size_ = size;
  capacity_ = size;
  ours_ = false;
}

void UnicodeText::Repr::Copy(const char* data, int size) {
  resize(size);
  memcpy(data_, data, size);
}

void UnicodeText::Repr::resize(int new_size) {
  if (new_size == 0) {
    clear();
  } else {
    if (!ours_ || new_size > capacity_) reserve(new_size);
    // Clear the memory in the expanded part.
    if (size_ < new_size) memset(data_ + size_, 0, new_size - size_);
    size_ = new_size;
    ours_ = true;
  }
}

void UnicodeText::Repr::reserve(int new_capacity) {
  // If there's already enough capacity, and we're an owner, do nothing.
  if (capacity_ >= new_capacity && ours_) return;

  // Otherwise, allocate a new buffer.
  capacity_ = std::max(new_capacity, (3 * capacity_) / 2 + 20);
  char* new_data = new char[capacity_];

  // If there is an old buffer, copy it into the new buffer.
  if (data_) {
    memcpy(new_data, data_, size_);
    if (ours_) delete[] data_;  // If we owned the old buffer, free it.
  }
  data_ = new_data;
  ours_ = true;  // We own the new buffer.
  // size_ is unchanged.
}

void UnicodeText::Repr::append(const char* bytes, int byte_length) {
  reserve(size_ + byte_length);
  memcpy(data_ + size_, bytes, byte_length);
  size_ += byte_length;
}

void UnicodeText::Repr::clear() {
  if (ours_) delete[] data_;
  data_ = nullptr;
  size_ = capacity_ = 0;
  ours_ = true;
}

// *************** UnicodeText ******************

UnicodeText::UnicodeText() {}

UnicodeText::UnicodeText(const UnicodeText& src, bool do_copy) {
  if (do_copy) {
    Copy(src);
  } else {
    repr_.PointTo(src.repr_.data_, src.repr_.size_);
  }
}

UnicodeText& UnicodeText::operator=(UnicodeText&& src) {
  this->repr_ = std::move(src.repr_);
  return *this;
}

UnicodeText& UnicodeText::Copy(const UnicodeText& src) {
  repr_.Copy(src.repr_.data_, src.repr_.size_);
  return *this;
}

UnicodeText& UnicodeText::PointToUTF8(const char* buffer, int byte_length) {
  repr_.PointTo(buffer, byte_length);
  return *this;
}

UnicodeText& UnicodeText::CopyUTF8(const char* buffer, int byte_length) {
  repr_.Copy(buffer, byte_length);
  return *this;
}

UnicodeText& UnicodeText::AppendUTF8(const char* utf8, int len) {
  repr_.append(utf8, len);
  return *this;
}

const char* UnicodeText::data() const { return repr_.data_; }

int UnicodeText::size_bytes() const { return repr_.size_; }

namespace {

enum {
  RuneError = 0xFFFD,  // Decoding error in UTF.
  RuneMax = 0x10FFFF,  // Maximum rune value.
};

int runetochar(const char32 rune, char* dest) {
  // Convert to unsigned for range check.
  uint32 c;

  // 1 char 00-7F
  c = rune;
  if (c <= 0x7F) {
    dest[0] = static_cast<char>(c);
    return 1;
  }

  // 2 char 0080-07FF
  if (c <= 0x07FF) {
    dest[0] = 0xC0 | static_cast<char>(c >> 1 * 6);
    dest[1] = 0x80 | (c & 0x3F);
    return 2;
  }

  // Range check
  if (c > RuneMax) {
    c = RuneError;
  }

  // 3 char 0800-FFFF
  if (c <= 0xFFFF) {
    dest[0] = 0xE0 | static_cast<char>(c >> 2 * 6);
    dest[1] = 0x80 | ((c >> 1 * 6) & 0x3F);
    dest[2] = 0x80 | (c & 0x3F);
    return 3;
  }

  // 4 char 10000-1FFFFF
  dest[0] = 0xF0 | static_cast<char>(c >> 3 * 6);
  dest[1] = 0x80 | ((c >> 2 * 6) & 0x3F);
  dest[2] = 0x80 | ((c >> 1 * 6) & 0x3F);
  dest[3] = 0x80 | (c & 0x3F);
  return 4;
}

}  // namespace

UnicodeText& UnicodeText::push_back(char32 ch) {
  char str[4];
  int char_len = runetochar(ch, str);
  repr_.append(str, char_len);
  return *this;
}

void UnicodeText::clear() { repr_.clear(); }

int UnicodeText::size_codepoints() const {
  return std::distance(begin(), end());
}

bool UnicodeText::empty() const { return size_bytes() == 0; }

bool UnicodeText::is_valid() const {
  return IsValidUTF8(repr_.data_, repr_.size_);
}

bool UnicodeText::operator==(const UnicodeText& other) const {
  if (repr_.size_ != other.repr_.size_) {
    return false;
  }
  return memcmp(repr_.data_, other.repr_.data_, repr_.size_) == 0;
}

std::string UnicodeText::ToUTF8String() const {
  return UTF8Substring(begin(), end());
}

std::string UnicodeText::UTF8Substring(int begin_codepoint,
                                       int end_codepoint) const {
  auto span_begin = begin();
  std::advance(span_begin, begin_codepoint);
  auto span_end = span_begin;
  std::advance(span_end, end_codepoint - begin_codepoint);
  return UTF8Substring(span_begin, span_end);
}

std::string UnicodeText::UTF8Substring(const const_iterator& it_begin,
                                       const const_iterator& it_end) {
  return std::string(it_begin.it_, it_end.it_ - it_begin.it_);
}

UnicodeText UnicodeText::Substring(const UnicodeText& text, int begin_codepoint,
                                   int end_codepoint, bool do_copy) {
  auto it_begin = text.begin();
  std::advance(it_begin, begin_codepoint);
  auto it_end = text.begin();
  std::advance(it_end, end_codepoint);

  return Substring(it_begin, it_end, do_copy);
}

UnicodeText UnicodeText::Substring(const const_iterator& it_begin,
                                   const const_iterator& it_end, bool do_copy) {
  if (do_copy) {
    UnicodeText result;
    result.repr_.Copy(it_begin.it_, it_end.it_ - it_begin.it_);
    return result;
  } else {
    UnicodeText result;
    result.repr_.PointTo(it_begin.it_, it_end.it_ - it_begin.it_);
    return result;
  }
}

UnicodeText::~UnicodeText() {}

// ******************* UnicodeText::const_iterator *********************

// The implementation of const_iterator would be nicer if it
// inherited from boost::iterator_facade
// (http://boost.org/libs/iterator/doc/iterator_facade.html).

UnicodeText::const_iterator::const_iterator() : it_(nullptr) {}

UnicodeText::const_iterator& UnicodeText::const_iterator::operator=(
    const const_iterator& other) {
  if (&other != this) it_ = other.it_;
  return *this;
}

UnicodeText::const_iterator UnicodeText::begin() const {
  return const_iterator(repr_.data_);
}

UnicodeText::const_iterator UnicodeText::end() const {
  return const_iterator(repr_.data_ + repr_.size_);
}

bool operator<(const UnicodeText::const_iterator& lhs,
               const UnicodeText::const_iterator& rhs) {
  return lhs.it_ < rhs.it_;
}

char32 UnicodeText::const_iterator::operator*() const {
  // (We could call chartorune here, but that does some
  // error-checking, and we're guaranteed that our data is valid
  // UTF-8. Also, we expect this routine to be called very often. So
  // for speed, we do the calculation ourselves.)
  return ValidCharToRune(it_);
}

UnicodeText::const_iterator& UnicodeText::const_iterator::operator++() {
  it_ += GetNumBytesForUTF8Char(it_);
  return *this;
}

UnicodeText::const_iterator& UnicodeText::const_iterator::operator--() {
  while (IsTrailByte(*--it_)) {
  }
  return *this;
}

UnicodeText UTF8ToUnicodeText(const char* utf8_buf, int len, bool do_copy) {
  UnicodeText t;
  if (do_copy) {
    t.CopyUTF8(utf8_buf, len);
  } else {
    t.PointToUTF8(utf8_buf, len);
  }
  return t;
}

UnicodeText UTF8ToUnicodeText(const char* utf8_buf, bool do_copy) {
  return UTF8ToUnicodeText(utf8_buf, strlen(utf8_buf), do_copy);
}

UnicodeText UTF8ToUnicodeText(const std::string& str, bool do_copy) {
  return UTF8ToUnicodeText(str.data(), str.size(), do_copy);
}

UnicodeText UTF8ToUnicodeText(StringPiece str, bool do_copy) {
  return UTF8ToUnicodeText(str.data(), str.size(), do_copy);
}

}  // namespace libtextclassifier3
