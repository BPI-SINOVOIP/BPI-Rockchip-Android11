/*
 *
 * Copyright 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TEEUI_LIBTEEUI_FONT_RENDERING_H_
#define TEEUI_LIBTEEUI_FONT_RENDERING_H_

#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/ftglyph.h>

#include "utils.h"
#include <tuple>

#include <type_traits>

namespace teeui {

template <typename T> struct HandleDelete;

template <typename T, typename Deleter = HandleDelete<T>> class Handle {
  public:
    Handle() : handle_(nullptr) {}
    explicit Handle(T handle) : handle_(handle) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    Handle(Handle&& other) {
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }

    Handle& operator=(Handle&& rhs) {
        if (&rhs != this) {
            auto dummy = handle_;
            handle_ = rhs.handle_;
            rhs.handle_ = dummy;
        }
        return *this;
    }

    ~Handle() {
        if (handle_) Deleter()(handle_);
    }

    operator bool() const { return handle_ != nullptr; }
    T operator*() const { return handle_; }
    const T operator->() const { return handle_; }
    T operator->() { return handle_; }

  private:
    T handle_;
};

#define MAP_HANDLE_DELETER(type, deleter)                                                          \
    template <> struct HandleDelete<type> {                                                        \
        void operator()(type h) { deleter(h); }                                                    \
    }

MAP_HANDLE_DELETER(FT_Face, FT_Done_Face);
MAP_HANDLE_DELETER(FT_Library, FT_Done_FreeType);

/**
 * Important notice. The UTF8Range only works on verified UTF8 encoded strings.
 * E.g. if the string successfully passed through our CBOR formatting (see cbor.h) it is safe to
 * use with UTF8Range. Alternatively, you can call verify() on a new range.
 */
template <typename CharIterator> class UTF8Range {
  public:
    UTF8Range(CharIterator begin, CharIterator end) : begin_(begin), end_(end) {}
    UTF8Range() : begin_{}, end_{begin_} {};
    UTF8Range(const UTF8Range&) = default;
    UTF8Range(UTF8Range&&) = default;
    UTF8Range& operator=(UTF8Range&&) = default;
    UTF8Range& operator=(const UTF8Range&) = default;

    /**
     * Decodes a header byte of a UTF8 sequence. In UTF8 encoding the number of leading ones
     * indicate the length of the UTF8 sequence. Following bytes start with b10 followed by six
     * payload bits. Sequences of length one start with a 0 followed by 7 payload bits.
     */
    static size_t byteCount(char c) {
        if (0x80 & c) {
            /*
             * CLZ - count leading zeroes.
             * __builtin_clz promotes the argument to unsigned int.
             * We invert c to turn leading ones into leading zeroes.
             * We subtract additional leading zeroes due to the type promotion from the result.
             */
            return __builtin_clz((unsigned char)(~c)) - (sizeof(unsigned int) * 8 - 8);
        } else {
            return 1;
        }
    }
    static unsigned long codePoint(CharIterator begin) {
        unsigned long c = (uint8_t)*begin;
        size_t byte_count = byteCount(c);
        if (byte_count == 1) {
            return c;
        } else {
            // multi byte
            unsigned long result = c & ~(0xff << (8 - byte_count));
            ++begin;
            for (size_t i = 1; i < byte_count; ++i) {
                result <<= 6;
                result |= *begin & 0x3f;
                ++begin;
            }
            return result;
        }
    }

    class Iter {
        CharIterator begin_;

      public:
        Iter() : begin_{} {}
        Iter(CharIterator begin) : begin_(begin) {}
        Iter(const Iter& rhs) : begin_(rhs.begin_) {}
        Iter& operator=(const Iter& rhs) {
            begin_ = rhs.begin_;
            return *this;
        }
        CharIterator operator*() const { return begin_; }
        Iter& operator++() {
            begin_ += byteCount(*begin_);
            return *this;
        }
        Iter operator++(int) {
            Iter dummy = *this;
            ++(*this);
            return dummy;
        }
        bool operator==(const Iter& rhs) const { return begin_ == rhs.begin_; }
        bool operator!=(const Iter& rhs) const { return !(*this == rhs); }
        unsigned long codePoint() const { return UTF8Range::codePoint(begin_); }
    };
    Iter begin() const { return Iter(begin_); }
    Iter end() const { return Iter(end_); }
    /*
     * Checks if the range is safe to use. If this returns false, iteration over this range is
     * undefined. It may infinite loop and read out of bounds.
     */
    bool verify() {
        for (auto pos = begin_; pos != end_;) {
            // are we out of sync?
            if ((*pos & 0xc0) == 0x80) return false;
            auto byte_count = byteCount(*pos);
            // did we run out of buffer;
            if (end_ - pos < byte_count) return false;
            // we could check if the non header bytes have the wrong header. While this would
            // be malformed UTF8, it does not impact control flow and is thus not security
            // critical.
            pos += byte_count;
        }
        return true;
    }

  private:
    CharIterator begin_;
    CharIterator end_;
    static_assert(std::is_same<std::remove_reference_t<decltype(*begin_)>, const char>::value,
                  "Iterator must dereference to const char");
    static_assert(
        std::is_convertible<std::remove_reference_t<decltype(end_ - begin_)>, size_t>::value,
        "Iterator arithmetic must evaluate to something that is convertible to size_t");
};

bool isBreakable(unsigned long codePoint);

template <typename CharIterator> class UTF8WordRange {
    UTF8Range<CharIterator> range_;

  public:
    UTF8WordRange(CharIterator begin, CharIterator end) : range_(begin, end) {}
    explicit UTF8WordRange(const UTF8Range<CharIterator>& range) : range_(range) {}
    UTF8WordRange() = default;
    UTF8WordRange(const UTF8WordRange&) = default;
    UTF8WordRange(UTF8WordRange&&) = default;
    UTF8WordRange& operator=(UTF8WordRange&&) = default;
    UTF8WordRange& operator=(const UTF8WordRange&) = default;

    using UTF8Iterator = typename UTF8Range<CharIterator>::Iter;
    class Iter {
        UTF8Iterator begin_;
        UTF8Iterator end_;

      public:
        Iter() : begin_{} {}
        Iter(UTF8Iterator begin, UTF8Iterator end) : begin_(begin), end_(end) {}
        Iter(const Iter& rhs) : begin_(rhs.begin_), end_(rhs.end_) {}
        Iter& operator=(const Iter& rhs) {
            begin_ = rhs.begin_;
            end_ = rhs.end_;
            return *this;
        }
        UTF8Iterator operator*() const { return begin_; }
        Iter& operator++() {
            if (begin_ == end_) return *this;
            bool prevBreaking = isBreakable(begin_.codePoint());
            // checkAndUpdate detects edges between breakable and non breakable characters.
            // As a result the iterator stops on the first character of a word or whitespace
            // sequence.
            auto checkAndUpdate = [&](unsigned long cp) {
                bool current = isBreakable(cp);
                bool result = prevBreaking == current;
                prevBreaking = current;
                return result;
            };
            do {
                ++begin_;
            } while (begin_ != end_ && checkAndUpdate(begin_.codePoint()));
            return *this;
        }
        Iter operator++(int) {
            Iter dummy = *this;
            ++(*this);
            return dummy;
        }
        bool operator==(const Iter& rhs) const { return begin_ == rhs.begin_; }
        bool operator!=(const Iter& rhs) const { return !(*this == rhs); }
    };
    Iter begin() const { return Iter(range_.begin(), range_.end()); }
    Iter end() const { return Iter(range_.end(), range_.end()); }
};

class TextContext;

using GlyphIndex = unsigned int;

class TextFace {
    friend TextContext;
    Handle<FT_Face> face_;
    bool hasKerning_ = false;

  public:
    Error setCharSize(signed long char_size, unsigned int dpi);
    Error setCharSizeInPix(pxs size);
    GlyphIndex getCharIndex(unsigned long codePoint);
    Error loadGlyph(GlyphIndex index);
    Error renderGlyph();

    Error drawGlyph(const Vec2d<pxs>& pos, const PixelDrawer& drawPixel) {
        FT_Bitmap* bitmap = &face_->glyph->bitmap;
        uint8_t* rowBuffer = bitmap->buffer;
        Vec2d<pxs> offset{face_->glyph->bitmap_left, -face_->glyph->bitmap_top};
        auto bPos = pos + offset;
        for (unsigned y = 0; y < bitmap->rows; ++y) {
            for (unsigned x = 0; x < bitmap->width; ++x) {
                Color alpha = 0;
                switch (bitmap->pixel_mode) {
                case FT_PIXEL_MODE_GRAY:
                    alpha = rowBuffer[x];
                    alpha *= 256;
                    alpha /= bitmap->num_grays;
                    alpha <<= 24;
                    break;
                case FT_PIXEL_MODE_LCD:
                case FT_PIXEL_MODE_BGRA:
                case FT_PIXEL_MODE_NONE:
                case FT_PIXEL_MODE_LCD_V:
                case FT_PIXEL_MODE_MONO:
                case FT_PIXEL_MODE_GRAY2:
                case FT_PIXEL_MODE_GRAY4:
                default:
                    return Error::UnsupportedPixelFormat;
                }
                if (drawPixel(bPos.x().count() + x, bPos.y().count() + y, alpha)) {
                    return Error::OutOfBoundsDrawing;
                }
            }
            rowBuffer += bitmap->pitch;
        }
        return Error::OK;
    }

    Vec2d<pxs> advance() const;
    Vec2d<pxs> kern(GlyphIndex previous) const;
    optional<Box<pxs>> getGlyphBBox() const;
};

class TextContext {
    Handle<FT_Library> library_;

  public:
    static std::tuple<Error, TextContext> create();

    template <typename Buffer>
    std::tuple<Error, TextFace> loadFace(const Buffer& data, signed long face_index = 0) {
        std::tuple<Error, TextFace> result;
        auto& [rc, tface] = result;
        rc = Error::NotInitialized;
        if (!library_) return result;
        FT_Face face;
        auto error = FT_New_Memory_Face(*library_, data.data(), data.size(), face_index, &face);
        rc = Error::FaceNotLoaded;
        if (error) return result;
        tface.face_ = Handle(face);
        tface.hasKerning_ = FT_HAS_KERNING(face);
        rc = Error::OK;
        return result;
    }
};

std::tuple<Error, Box<pxs>, UTF8Range<const char*>>
findLongestWordSequence(TextFace* face, const UTF8Range<const char*>& text,
                        const Box<pxs>& boundingBox);
Error drawText(TextFace* face, const UTF8Range<const char*>& text, const PixelDrawer& drawPixel,
               PxPoint pen);

}  //  namespace teeui

#endif  // TEEUI_LIBTEEUI_FONT_RENDERING_H_
