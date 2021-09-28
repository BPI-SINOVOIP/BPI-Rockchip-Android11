/*
 * Copyright 2019 The Android Open Source Project
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

#include "packet/iterator.h"

#include "os/log.h"

namespace bluetooth {
namespace packet {

template <bool little_endian>
Iterator<little_endian>::Iterator(std::forward_list<View> data, size_t offset) {
  data_ = data;
  index_ = offset;
  begin_ = 0;
  end_ = 0;
  for (auto& view : data) {
    end_ += view.size();
  }
}

template <bool little_endian>
Iterator<little_endian> Iterator<little_endian>::operator+(int offset) {
  auto itr(*this);

  return itr += offset;
}

template <bool little_endian>
Iterator<little_endian>& Iterator<little_endian>::operator+=(int offset) {
  index_ += offset;
  return *this;
}

template <bool little_endian>
Iterator<little_endian> Iterator<little_endian>::operator++(int) {
  auto itr(*this);
  index_++;
  return itr;
}

template <bool little_endian>
Iterator<little_endian>& Iterator<little_endian>::operator++() {
  index_++;
  return *this;
}

template <bool little_endian>
Iterator<little_endian> Iterator<little_endian>::operator-(int offset) {
  auto itr(*this);

  return itr -= offset;
}

template <bool little_endian>
int Iterator<little_endian>::operator-(Iterator<little_endian>& itr) {
  return index_ - itr.index_;
}

template <bool little_endian>
Iterator<little_endian>& Iterator<little_endian>::operator-=(int offset) {
  index_ -= offset;

  return *this;
}

template <bool little_endian>
Iterator<little_endian> Iterator<little_endian>::operator--(int) {
  auto itr(*this);
  if (index_ != 0) index_--;

  return itr;
}

template <bool little_endian>
Iterator<little_endian>& Iterator<little_endian>::operator--() {
  if (index_ != 0) index_--;

  return *this;
}

template <bool little_endian>
Iterator<little_endian>& Iterator<little_endian>::operator=(const Iterator<little_endian>& itr) {
  this->data_ = itr.data_;
  this->begin_ = itr.begin_;
  this->end_ = itr.end_;
  this->index_ = itr.index_;
  return *this;
}

template <bool little_endian>
bool Iterator<little_endian>::operator==(const Iterator<little_endian>& itr) const {
  return index_ == itr.index_;
}

template <bool little_endian>
bool Iterator<little_endian>::operator!=(const Iterator<little_endian>& itr) const {
  return !(*this == itr);
}

template <bool little_endian>
bool Iterator<little_endian>::operator<(const Iterator<little_endian>& itr) const {
  return index_ < itr.index_;
}

template <bool little_endian>
bool Iterator<little_endian>::operator>(const Iterator<little_endian>& itr) const {
  return index_ > itr.index_;
}

template <bool little_endian>
bool Iterator<little_endian>::operator<=(const Iterator<little_endian>& itr) const {
  return index_ <= itr.index_;
}

template <bool little_endian>
bool Iterator<little_endian>::operator>=(const Iterator<little_endian>& itr) const {
  return index_ >= itr.index_;
}

template <bool little_endian>
uint8_t Iterator<little_endian>::operator*() const {
  ASSERT_LOG(index_ < end_ && !(begin_ > index_), "Index %zu out of bounds: [%zu,%zu)", index_, begin_, end_);
  size_t index = index_;

  for (auto view : data_) {
    if (index < view.size()) {
      return view[index];
    }
    index -= view.size();
  }
  ASSERT_LOG(false, "Out of fragments searching for index %zu", index_);
  return 0;
}

template <bool little_endian>
size_t Iterator<little_endian>::NumBytesRemaining() const {
  if (end_ > index_ && !(begin_ > index_)) {
    return end_ - index_;
  } else {
    return 0;
  }
}

template <bool little_endian>
Iterator<little_endian> Iterator<little_endian>::Subrange(size_t index, size_t length) const {
  Iterator<little_endian> to_return(*this);
  if (to_return.NumBytesRemaining() > index) {
    to_return.index_ = to_return.index_ + index;
    to_return.begin_ = to_return.index_;
    if (to_return.NumBytesRemaining() >= length) {
      to_return.end_ = to_return.index_ + length;
    }
  } else {
    to_return.end_ = 0;
  }

  return to_return;
}

// Explicit instantiations for both types of Iterators.
template class Iterator<true>;
template class Iterator<false>;
}  // namespace packet
}  // namespace bluetooth
