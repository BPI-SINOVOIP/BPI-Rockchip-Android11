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

#include "utils/utf8/unilib-common.h"

#include <algorithm>

namespace libtextclassifier3 {
namespace {

#define ARRAYSIZE(a) sizeof(a) / sizeof(*a)

// Derived from http://www.unicode.org/Public/UNIDATA/UnicodeData.txt
// grep -E "Ps" UnicodeData.txt | \
//   sed -rne "s/^([0-9A-Z]{4});.*(PAREN|BRACKET|BRAKCET|BRACE).*/0x\1, /p"
// IMPORTANT: entries with the same offsets in kOpeningBrackets and
//            kClosingBrackets must be counterparts.
constexpr char32 kOpeningBrackets[] = {
    0x0028, 0x005B, 0x007B, 0x0F3C, 0x2045, 0x207D, 0x208D, 0x2329, 0x2768,
    0x276A, 0x276C, 0x2770, 0x2772, 0x2774, 0x27E6, 0x27E8, 0x27EA, 0x27EC,
    0x27EE, 0x2983, 0x2985, 0x2987, 0x2989, 0x298B, 0x298D, 0x298F, 0x2991,
    0x2993, 0x2995, 0x2997, 0x29FC, 0x2E22, 0x2E24, 0x2E26, 0x2E28, 0x3008,
    0x300A, 0x300C, 0x300E, 0x3010, 0x3014, 0x3016, 0x3018, 0x301A, 0xFD3F,
    0xFE17, 0xFE35, 0xFE37, 0xFE39, 0xFE3B, 0xFE3D, 0xFE3F, 0xFE41, 0xFE43,
    0xFE47, 0xFE59, 0xFE5B, 0xFE5D, 0xFF08, 0xFF3B, 0xFF5B, 0xFF5F, 0xFF62};
constexpr int kNumOpeningBrackets = ARRAYSIZE(kOpeningBrackets);

// grep -E "Pe" UnicodeData.txt | \
//   sed -rne "s/^([0-9A-Z]{4});.*(PAREN|BRACKET|BRAKCET|BRACE).*/0x\1, /p"
constexpr char32 kClosingBrackets[] = {
    0x0029, 0x005D, 0x007D, 0x0F3D, 0x2046, 0x207E, 0x208E, 0x232A, 0x2769,
    0x276B, 0x276D, 0x2771, 0x2773, 0x2775, 0x27E7, 0x27E9, 0x27EB, 0x27ED,
    0x27EF, 0x2984, 0x2986, 0x2988, 0x298A, 0x298C, 0x298E, 0x2990, 0x2992,
    0x2994, 0x2996, 0x2998, 0x29FD, 0x2E23, 0x2E25, 0x2E27, 0x2E29, 0x3009,
    0x300B, 0x300D, 0x300F, 0x3011, 0x3015, 0x3017, 0x3019, 0x301B, 0xFD3E,
    0xFE18, 0xFE36, 0xFE38, 0xFE3A, 0xFE3C, 0xFE3E, 0xFE40, 0xFE42, 0xFE44,
    0xFE48, 0xFE5A, 0xFE5C, 0xFE5E, 0xFF09, 0xFF3D, 0xFF5D, 0xFF60, 0xFF63};
constexpr int kNumClosingBrackets = ARRAYSIZE(kClosingBrackets);

// grep -E "WS" UnicodeData.txt | sed -re "s/([0-9A-Z]+);.*/0x\1, /"
constexpr char32 kWhitespaces[] = {
    0x0009,  0x000A,  0x000B,  0x000C,  0x000D,  0x0020,  0x0085,  0x00A0,
    0x1680,  0x2000,  0x2001,  0x2002,  0x2003,  0x2004,  0x2005,  0x2006,
    0x2007,  0x2008,  0x2009,  0x200A,  0x2028,  0x2029,  0x202F,  0x205F,
    0x21C7,  0x21C8,  0x21C9,  0x21CA,  0x21F6,  0x2B31,  0x2B84,  0x2B85,
    0x2B86,  0x2B87,  0x2B94,  0x3000,  0x4DCC,  0x10344, 0x10347, 0x1DA0A,
    0x1DA0B, 0x1DA0C, 0x1DA0D, 0x1DA0E, 0x1DA0F, 0x1DA10, 0x1F4F0, 0x1F500,
    0x1F501, 0x1F502, 0x1F503, 0x1F504, 0x1F5D8, 0x1F5DE};
constexpr int kNumWhitespaces = ARRAYSIZE(kWhitespaces);

// grep -E "Nd" UnicodeData.txt | sed -re "s/([0-9A-Z]+);.*/0x\1, /"
// As the name suggests, these ranges are always 10 codepoints long, so we just
// store the end of the range.
constexpr char32 kDecimalDigitRangesEnd[] = {
    0x0039,  0x0669,  0x06f9,  0x07c9,  0x096f,  0x09ef,  0x0a6f,  0x0aef,
    0x0b6f,  0x0bef,  0x0c6f,  0x0cef,  0x0d6f,  0x0def,  0x0e59,  0x0ed9,
    0x0f29,  0x1049,  0x1099,  0x17e9,  0x1819,  0x194f,  0x19d9,  0x1a89,
    0x1a99,  0x1b59,  0x1bb9,  0x1c49,  0x1c59,  0xa629,  0xa8d9,  0xa909,
    0xa9d9,  0xa9f9,  0xaa59,  0xabf9,  0xff19,  0x104a9, 0x1106f, 0x110f9,
    0x1113f, 0x111d9, 0x112f9, 0x11459, 0x114d9, 0x11659, 0x116c9, 0x11739,
    0x118e9, 0x11c59, 0x11d59, 0x16a69, 0x16b59, 0x1d7ff};
constexpr int kNumDecimalDigitRangesEnd = ARRAYSIZE(kDecimalDigitRangesEnd);

// Visual source: https://en.wikipedia.org/wiki/Latin_script_in_Unicode
// Source https://unicode-search.net/unicode-namesearch.pl?term=letter
// clang-format off
// grep "LATIN " latters.txt | grep -v "TAG LATIN" | grep -v "SQUARED LATIN" | grep -v "CIRCLED LATIN" | grep -v "PARENTHESIZED LATIN" | cut -d'  ' -f1 | cut -d'+' -f2 | sed -re "s/([0-9A-Z]+).*/0x\1, /" | tr -d "\n" NOLINT
// clang-format on
constexpr char32 kLatinLettersRangesStart[] = {0x0041, 0x0061, 0x00C0, 0x00D8,
                                               0x00F8, 0x1D00, 0x2C60, 0xAB30,
                                               0xFF21, 0xFF41};
constexpr int kNumLatinLettersRangesStart = ARRAYSIZE(kLatinLettersRangesStart);
constexpr char32 kLatinLettersRangesEnd[] = {0x005A, 0x007A, 0x00D6, 0x00F7,
                                             0x02A8, 0x1EFF, 0xA7B7, 0xAB64,
                                             0xFF3A, 0xFF5A};
constexpr int kNumLatinLettersRangesEnd = ARRAYSIZE(kLatinLettersRangesEnd);

// Source https://unicode-search.net/unicode-namesearch.pl?term=letter
constexpr char32 kArabicLettersRangesStart[] = {
    0x0620, 0x0641, 0x066E, 0x06EE, 0x0750, 0x08A0, 0xFB50, 0xFDFA, 0xFE80};
constexpr int kNumArabicLettersRangesStart =
    ARRAYSIZE(kArabicLettersRangesStart);
constexpr char32 kArabicLettersRangesEnd[] = {
    0x063F, 0x064A, 0x06D5, 0x06FF, 0x077F, 0x08BD, 0xFBFF, 0xFDFB, 0xFEF4};
constexpr int kNumArabicLettersRangesEnd = ARRAYSIZE(kArabicLettersRangesEnd);

// Source https://unicode-search.net/unicode-namesearch.pl?term=letter
constexpr char32 kCyrillicLettersRangesStart[] = {0x0400, 0x1C80, 0x2DE0,
                                                  0xA640, 0xA674, 0xA680};
constexpr int kNumCyrillicLettersRangesStart =
    ARRAYSIZE(kCyrillicLettersRangesStart);
constexpr char32 kCyrillicLettersRangesEnd[] = {0x052F, 0x1C88, 0x2DFF,
                                                0xA66E, 0xA67B, 0xA69F};
constexpr int kNumCyrillicLettersRangesEnd =
    ARRAYSIZE(kCyrillicLettersRangesEnd);

constexpr char32 kChineseLettersRangesStart[] = {
    0x4E00,  0xF900,  0x2F800, 0xFE30,  0x3400,
    0x20000, 0x2A700, 0x2B740, 0x2B820, 0x2CEB0};
constexpr int kNumChineseLettersRangesStart =
    ARRAYSIZE(kChineseLettersRangesStart);
constexpr char32 kChineseLettersRangesEnd[] = {
    0x9FFF,  0xFAFF,  0x2FA1F, 0xFE4F,  0x4DBF,
    0x2A6DF, 0x2B73F, 0x2B81F, 0x2CEAF, 0x2EBEF};
constexpr int kNumChineseLettersRangesEnd = ARRAYSIZE(kChineseLettersRangesEnd);

// Source https://unicode-search.net/unicode-namesearch.pl?term=letter
// Hiragana and Katakana
constexpr char32 kJapaneseLettersRangesStart[] = {0x3041, 0x30A1, 0x31F0,
                                                  0xFF66};
constexpr int kNumJapaneseLettersRangesStart =
    ARRAYSIZE(kJapaneseLettersRangesStart);
constexpr char32 kJapaneseLettersRangesEnd[] = {0x3096, 0x30FA, 0x31FF, 0xFF9D};
constexpr int kNumJapaneseLettersRangesEnd =
    ARRAYSIZE(kJapaneseLettersRangesEnd);

// Source https://unicode-search.net/unicode-namesearch.pl?term=letter
// Hangul
constexpr char32 kKoreanLettersRangesStart[] = {0x3131, 0xFFA1};
constexpr int kNumKoreanLettersRangesStart =
    ARRAYSIZE(kKoreanLettersRangesStart);
constexpr char32 kKoreanLettersRangesEnd[] = {0x318E, 0xFFDC};
constexpr int kNumKoreanLettersRangesEnd = ARRAYSIZE(kKoreanLettersRangesEnd);

// Source https://unicode-search.net/unicode-namesearch.pl?term=letter
constexpr char32 kThaiLettersRangesStart[] = {0x0E01};
constexpr int kNumThaiLettersRangesStart = ARRAYSIZE(kThaiLettersRangesStart);
constexpr char32 kThaiLettersRangesEnd[] = {0x0E2E};
constexpr int kNumThaiLettersRangesEnd = ARRAYSIZE(kThaiLettersRangesEnd);

// grep -E ";P.;" UnicodeData.txt | sed -re "s/([0-9A-Z]+);.*/0x\1, /"
constexpr char32 kPunctuationRangesStart[] = {
    0x0021,  0x0025,  0x002c,  0x003a,  0x003f,  0x005b,  0x005f,  0x007b,
    0x007d,  0x00a1,  0x00a7,  0x00ab,  0x00b6,  0x00bb,  0x00bf,  0x037e,
    0x0387,  0x055a,  0x0589,  0x05be,  0x05c0,  0x05c3,  0x05c6,  0x05f3,
    0x0609,  0x060c,  0x061b,  0x061e,  0x066a,  0x06d4,  0x0700,  0x07f7,
    0x0830,  0x085e,  0x0964,  0x0970,  0x09fd,  0x0a76,  0x0af0,  0x0c77,
    0x0c84,  0x0df4,  0x0e4f,  0x0e5a,  0x0f04,  0x0f14,  0x0f3a,  0x0f85,
    0x0fd0,  0x0fd9,  0x104a,  0x10fb,  0x1360,  0x1400,  0x166e,  0x169b,
    0x16eb,  0x1735,  0x17d4,  0x17d8,  0x1800,  0x1944,  0x1a1e,  0x1aa0,
    0x1aa8,  0x1b5a,  0x1bfc,  0x1c3b,  0x1c7e,  0x1cc0,  0x1cd3,  0x2010,
    0x2030,  0x2045,  0x2053,  0x207d,  0x208d,  0x2308,  0x2329,  0x2768,
    0x27c5,  0x27e6,  0x2983,  0x29d8,  0x29fc,  0x2cf9,  0x2cfe,  0x2d70,
    0x2e00,  0x2e30,  0x3001,  0x3008,  0x3014,  0x3030,  0x303d,  0x30a0,
    0x30fb,  0xa4fe,  0xa60d,  0xa673,  0xa67e,  0xa6f2,  0xa874,  0xa8ce,
    0xa8f8,  0xa8fc,  0xa92e,  0xa95f,  0xa9c1,  0xa9de,  0xaa5c,  0xaade,
    0xaaf0,  0xabeb,  0xfd3e,  0xfe10,  0xfe30,  0xfe54,  0xfe63,  0xfe68,
    0xfe6a,  0xff01,  0xff05,  0xff0c,  0xff1a,  0xff1f,  0xff3b,  0xff3f,
    0xff5b,  0xff5d,  0xff5f,  0x10100, 0x1039f, 0x103d0, 0x1056f, 0x10857,
    0x1091f, 0x1093f, 0x10a50, 0x10a7f, 0x10af0, 0x10b39, 0x10b99, 0x10f55,
    0x11047, 0x110bb, 0x110be, 0x11140, 0x11174, 0x111c5, 0x111cd, 0x111db,
    0x111dd, 0x11238, 0x112a9, 0x1144b, 0x1145b, 0x1145d, 0x114c6, 0x115c1,
    0x11641, 0x11660, 0x1173c, 0x1183b, 0x119e2, 0x11a3f, 0x11a9a, 0x11a9e,
    0x11c41, 0x11c70, 0x11ef7, 0x11fff, 0x12470, 0x16a6e, 0x16af5, 0x16b37,
    0x16b44, 0x16e97, 0x16fe2, 0x1bc9f, 0x1da87, 0x1e95e};
constexpr int kNumPunctuationRangesStart = ARRAYSIZE(kPunctuationRangesStart);
constexpr char32 kPunctuationRangesEnd[] = {
    0x0023,  0x002a,  0x002f,  0x003b,  0x0040,  0x005d,  0x005f,  0x007b,
    0x007d,  0x00a1,  0x00a7,  0x00ab,  0x00b7,  0x00bb,  0x00bf,  0x037e,
    0x0387,  0x055f,  0x058a,  0x05be,  0x05c0,  0x05c3,  0x05c6,  0x05f4,
    0x060a,  0x060d,  0x061b,  0x061f,  0x066d,  0x06d4,  0x070d,  0x07f9,
    0x083e,  0x085e,  0x0965,  0x0970,  0x09fd,  0x0a76,  0x0af0,  0x0c77,
    0x0c84,  0x0df4,  0x0e4f,  0x0e5b,  0x0f12,  0x0f14,  0x0f3d,  0x0f85,
    0x0fd4,  0x0fda,  0x104f,  0x10fb,  0x1368,  0x1400,  0x166e,  0x169c,
    0x16ed,  0x1736,  0x17d6,  0x17da,  0x180a,  0x1945,  0x1a1f,  0x1aa6,
    0x1aad,  0x1b60,  0x1bff,  0x1c3f,  0x1c7f,  0x1cc7,  0x1cd3,  0x2027,
    0x2043,  0x2051,  0x205e,  0x207e,  0x208e,  0x230b,  0x232a,  0x2775,
    0x27c6,  0x27ef,  0x2998,  0x29db,  0x29fd,  0x2cfc,  0x2cff,  0x2d70,
    0x2e2e,  0x2e4f,  0x3003,  0x3011,  0x301f,  0x3030,  0x303d,  0x30a0,
    0x30fb,  0xa4ff,  0xa60f,  0xa673,  0xa67e,  0xa6f7,  0xa877,  0xa8cf,
    0xa8fa,  0xa8fc,  0xa92f,  0xa95f,  0xa9cd,  0xa9df,  0xaa5f,  0xaadf,
    0xaaf1,  0xabeb,  0xfd3f,  0xfe19,  0xfe52,  0xfe61,  0xfe63,  0xfe68,
    0xfe6b,  0xff03,  0xff0a,  0xff0f,  0xff1b,  0xff20,  0xff3d,  0xff3f,
    0xff5b,  0xff5d,  0xff65,  0x10102, 0x1039f, 0x103d0, 0x1056f, 0x10857,
    0x1091f, 0x1093f, 0x10a58, 0x10a7f, 0x10af6, 0x10b3f, 0x10b9c, 0x10f59,
    0x1104d, 0x110bc, 0x110c1, 0x11143, 0x11175, 0x111c8, 0x111cd, 0x111db,
    0x111df, 0x1123d, 0x112a9, 0x1144f, 0x1145b, 0x1145d, 0x114c6, 0x115d7,
    0x11643, 0x1166c, 0x1173e, 0x1183b, 0x119e2, 0x11a46, 0x11a9c, 0x11aa2,
    0x11c45, 0x11c71, 0x11ef8, 0x11fff, 0x12474, 0x16a6f, 0x16af5, 0x16b3b,
    0x16b44, 0x16e9a, 0x16fe2, 0x1bc9f, 0x1da8b, 0x1e95f};
constexpr int kNumPunctuationRangesEnd = ARRAYSIZE(kPunctuationRangesEnd);

// grep -E "Lu" UnicodeData.txt | sed -re "s/([0-9A-Z]+);.*/0x\1, /"
// There are three common ways in which upper/lower case codepoint ranges
// were introduced: one offs, dense ranges, and ranges that alternate between
// lower and upper case. For the sake of keeping out binary size down, we
// treat each independently.
constexpr char32 kUpperSingles[] = {
    0x01b8, 0x01bc, 0x01c4, 0x01c7, 0x01ca, 0x01f1, 0x0376, 0x037f,
    0x03cf, 0x03f4, 0x03fa, 0x10c7, 0x10cd, 0x2102, 0x2107, 0x2115,
    0x2145, 0x2183, 0x2c72, 0x2c75, 0x2cf2, 0xa7b6};
constexpr int kNumUpperSingles = ARRAYSIZE(kUpperSingles);
constexpr char32 kUpperRanges1Start[] = {
    0x0041, 0x00c0, 0x00d8, 0x0181, 0x018a, 0x018e, 0x0193, 0x0196,
    0x019c, 0x019f, 0x01b2, 0x01f7, 0x023a, 0x023d, 0x0244, 0x0389,
    0x0392, 0x03a3, 0x03d2, 0x03fd, 0x0531, 0x10a0, 0x13a0, 0x1f08,
    0x1f18, 0x1f28, 0x1f38, 0x1f48, 0x1f68, 0x1fb8, 0x1fc8, 0x1fd8,
    0x1fe8, 0x1ff8, 0x210b, 0x2110, 0x2119, 0x212b, 0x2130, 0x213e,
    0x2c00, 0x2c63, 0x2c6e, 0x2c7e, 0xa7ab, 0xa7b0};
constexpr int kNumUpperRanges1Start = ARRAYSIZE(kUpperRanges1Start);
constexpr char32 kUpperRanges1End[] = {
    0x005a, 0x00d6, 0x00de, 0x0182, 0x018b, 0x0191, 0x0194, 0x0198,
    0x019d, 0x01a0, 0x01b3, 0x01f8, 0x023b, 0x023e, 0x0246, 0x038a,
    0x03a1, 0x03ab, 0x03d4, 0x042f, 0x0556, 0x10c5, 0x13f5, 0x1f0f,
    0x1f1d, 0x1f2f, 0x1f3f, 0x1f4d, 0x1f6f, 0x1fbb, 0x1fcb, 0x1fdb,
    0x1fec, 0x1ffb, 0x210d, 0x2112, 0x211d, 0x212d, 0x2133, 0x213f,
    0x2c2e, 0x2c64, 0x2c70, 0x2c80, 0xa7ae, 0xa7b4};
constexpr int kNumUpperRanges1End = ARRAYSIZE(kUpperRanges1End);
constexpr char32 kUpperRanges2Start[] = {
    0x0100, 0x0139, 0x014a, 0x0179, 0x0184, 0x0187, 0x01a2, 0x01a7, 0x01ac,
    0x01af, 0x01b5, 0x01cd, 0x01de, 0x01f4, 0x01fa, 0x0241, 0x0248, 0x0370,
    0x0386, 0x038c, 0x038f, 0x03d8, 0x03f7, 0x0460, 0x048a, 0x04c1, 0x04d0,
    0x1e00, 0x1e9e, 0x1f59, 0x2124, 0x2c60, 0x2c67, 0x2c82, 0x2ceb, 0xa640,
    0xa680, 0xa722, 0xa732, 0xa779, 0xa77e, 0xa78b, 0xa790, 0xa796};
constexpr int kNumUpperRanges2Start = ARRAYSIZE(kUpperRanges2Start);
constexpr char32 kUpperRanges2End[] = {
    0x0136, 0x0147, 0x0178, 0x017d, 0x0186, 0x0189, 0x01a6, 0x01a9, 0x01ae,
    0x01b1, 0x01b7, 0x01db, 0x01ee, 0x01f6, 0x0232, 0x0243, 0x024e, 0x0372,
    0x0388, 0x038e, 0x0391, 0x03ee, 0x03f9, 0x0480, 0x04c0, 0x04cd, 0x052e,
    0x1e94, 0x1efe, 0x1f5f, 0x212a, 0x2c62, 0x2c6d, 0x2ce2, 0x2ced, 0xa66c,
    0xa69a, 0xa72e, 0xa76e, 0xa77d, 0xa786, 0xa78d, 0xa792, 0xa7aa};
constexpr int kNumUpperRanges2End = ARRAYSIZE(kUpperRanges2End);

// grep -E "Ll" UnicodeData.txt | sed -re "s/([0-9A-Z]+);.*/0x\1, /"
constexpr char32 kLowerSingles[] = {
    0x00b5, 0x0188, 0x0192, 0x0195, 0x019e, 0x01b0, 0x01c6, 0x01c9,
    0x01f0, 0x023c, 0x0242, 0x0377, 0x0390, 0x03f5, 0x03f8, 0x1fbe,
    0x210a, 0x2113, 0x212f, 0x2134, 0x2139, 0x214e, 0x2184, 0x2c61,
    0x2ce4, 0x2cf3, 0x2d27, 0x2d2d, 0xa7af, 0xa7c3, 0xa7fa, 0x1d7cb};
constexpr int kNumLowerSingles = ARRAYSIZE(kLowerSingles);
constexpr char32 kLowerRanges1Start[] = {
    0x0061,  0x00df,  0x00f8,  0x017f,  0x018c,  0x0199,  0x01b9,  0x01bd,
    0x0234,  0x023f,  0x0250,  0x0295,  0x037b,  0x03ac,  0x03d0,  0x03d5,
    0x03f0,  0x03fb,  0x0430,  0x0560,  0x10d0,  0x10fd,  0x13f8,  0x1c80,
    0x1d00,  0x1d6b,  0x1d79,  0x1e96,  0x1f00,  0x1f10,  0x1f20,  0x1f30,
    0x1f40,  0x1f50,  0x1f60,  0x1f70,  0x1f80,  0x1f90,  0x1fa0,  0x1fb0,
    0x1fb6,  0x1fc2,  0x1fc6,  0x1fd0,  0x1fd6,  0x1fe0,  0x1ff2,  0x1ff6,
    0x210e,  0x213c,  0x2146,  0x2c30,  0x2c65,  0x2c77,  0x2d00,  0xa730,
    0xa772,  0xa794,  0xab30,  0xab60,  0xab70,  0xfb00,  0xfb13,  0xff41,
    0x10428, 0x104d8, 0x10cc0, 0x118c0, 0x16e60, 0x1d41a, 0x1d44e, 0x1d456,
    0x1d482, 0x1d4b6, 0x1d4be, 0x1d4c5, 0x1d4ea, 0x1d51e, 0x1d552, 0x1d586,
    0x1d5ba, 0x1d5ee, 0x1d622, 0x1d656, 0x1d68a, 0x1d6c2, 0x1d6dc, 0x1d6fc,
    0x1d716, 0x1d736, 0x1d750, 0x1d770, 0x1d78a, 0x1d7aa, 0x1d7c4, 0x1e922};
constexpr int kNumLowerRanges1Start = ARRAYSIZE(kLowerRanges1Start);
constexpr char32 kLowerRanges1End[] = {
    0x007a,  0x00f6,  0x00ff,  0x0180,  0x018d,  0x019b,  0x01ba,  0x01bf,
    0x0239,  0x0240,  0x0293,  0x02af,  0x037d,  0x03ce,  0x03d1,  0x03d7,
    0x03f3,  0x03fc,  0x045f,  0x0588,  0x10fa,  0x10ff,  0x13fd,  0x1c88,
    0x1d2b,  0x1d77,  0x1d9a,  0x1e9d,  0x1f07,  0x1f15,  0x1f27,  0x1f37,
    0x1f45,  0x1f57,  0x1f67,  0x1f7d,  0x1f87,  0x1f97,  0x1fa7,  0x1fb4,
    0x1fb7,  0x1fc4,  0x1fc7,  0x1fd3,  0x1fd7,  0x1fe7,  0x1ff4,  0x1ff7,
    0x210f,  0x213d,  0x2149,  0x2c5e,  0x2c66,  0x2c7b,  0x2d25,  0xa731,
    0xa778,  0xa795,  0xab5a,  0xab67,  0xabbf,  0xfb06,  0xfb17,  0xff5a,
    0x1044f, 0x104fb, 0x10cf2, 0x118df, 0x16e7f, 0x1d433, 0x1d454, 0x1d467,
    0x1d49b, 0x1d4b9, 0x1d4c3, 0x1d4cf, 0x1d503, 0x1d537, 0x1d56b, 0x1d59f,
    0x1d5d3, 0x1d607, 0x1d63b, 0x1d66f, 0x1d6a5, 0x1d6da, 0x1d6e1, 0x1d714,
    0x1d71b, 0x1d74e, 0x1d755, 0x1d788, 0x1d78f, 0x1d7c2, 0x1d7c9, 0x1e943};
constexpr int kNumLowerRanges1End = ARRAYSIZE(kLowerRanges1End);
constexpr char32 kLowerRanges2Start[] = {
    0x0101, 0x0138, 0x0149, 0x017a, 0x0183, 0x01a1, 0x01a8, 0x01ab,
    0x01b4, 0x01cc, 0x01dd, 0x01f3, 0x01f9, 0x0247, 0x0371, 0x03d9,
    0x0461, 0x048b, 0x04c2, 0x04cf, 0x1e01, 0x1e9f, 0x2c68, 0x2c71,
    0x2c74, 0x2c81, 0x2cec, 0xa641, 0xa681, 0xa723, 0xa733, 0xa77a,
    0xa77f, 0xa78c, 0xa791, 0xa797, 0xa7b5, 0x1d4bb};
constexpr int kNumLowerRanges2Start = ARRAYSIZE(kLowerRanges2Start);
constexpr char32 kLowerRanges2End[] = {
    0x0137, 0x0148, 0x0177, 0x017e, 0x0185, 0x01a5, 0x01aa, 0x01ad,
    0x01b6, 0x01dc, 0x01ef, 0x01f5, 0x0233, 0x024f, 0x0373, 0x03ef,
    0x0481, 0x04bf, 0x04ce, 0x052f, 0x1e95, 0x1eff, 0x2c6c, 0x2c73,
    0x2c76, 0x2ce3, 0x2cee, 0xa66d, 0xa69b, 0xa72f, 0xa771, 0xa77c,
    0xa787, 0xa78e, 0xa793, 0xa7a9, 0xa7bf, 0x1d4bd};
constexpr int kNumLowerRanges2End = ARRAYSIZE(kLowerRanges2End);

// grep -E "Lu" UnicodeData.txt | \
//   sed -rne "s/^([0-9A-Z]+);.*;([0-9A-Z]+);$/(0x\1, 0x\2), /p"
// We have two strategies for mapping from upper to lower case. We have single
// character lookups that do not follow a pattern, and ranges for which there
// is a constant codepoint shift.
// Note that these ranges ignore anything that's not an upper case character,
// so when applied to a non-uppercase character the result is incorrect.
constexpr int kToLowerSingles[] = {
    0x0130, 0x0178, 0x0181, 0x0186, 0x018b, 0x018e, 0x018f, 0x0190, 0x0191,
    0x0194, 0x0196, 0x0197, 0x0198, 0x019c, 0x019d, 0x019f, 0x01a6, 0x01a9,
    0x01ae, 0x01b7, 0x01f6, 0x01f7, 0x0220, 0x023a, 0x023d, 0x023e, 0x0243,
    0x0244, 0x0245, 0x037f, 0x0386, 0x038c, 0x03cf, 0x03f4, 0x03f9, 0x04c0,
    0x1e9e, 0x1fec, 0x2126, 0x212a, 0x212b, 0x2132, 0x2183, 0x2c60, 0x2c62,
    0x2c63, 0x2c64, 0x2c6d, 0x2c6e, 0x2c6f, 0x2c70, 0xa77d, 0xa78d, 0xa7aa,
    0xa7ab, 0xa7ac, 0xa7ad, 0xa7ae, 0xa7b0, 0xa7b1, 0xa7b2, 0xa7b3};
constexpr int kNumToLowerSingles = ARRAYSIZE(kToLowerSingles);
constexpr int kToLowerSinglesOffsets[] = {
    -199,   -121,   210,    206,    1,      79,     202,    203,    1,
    207,    211,    209,    1,      211,    213,    214,    218,    218,
    218,    219,    -97,    -56,    -130,   10795,  -163,   10792,  -195,
    69,     71,     116,    38,     64,     8,      -60,    -7,     15,
    -7615,  -7,     -7517,  -8383,  -8262,  28,     1,      1,      -10743,
    -3814,  -10727, -10780, -10749, -10783, -10782, -35332, -42280, -42308,
    -42319, -42315, -42305, -42308, -42258, -42282, -42261, 928};
constexpr int kNumToLowerSinglesOffsets = ARRAYSIZE(kToLowerSinglesOffsets);
constexpr int kToUpperSingles[] = {
    0x00b5, 0x00ff, 0x0131, 0x017f, 0x0180, 0x0195, 0x0199, 0x019a, 0x019e,
    0x01bf, 0x01dd, 0x01f3, 0x0250, 0x0251, 0x0252, 0x0253, 0x0254, 0x0259,
    0x025b, 0x025c, 0x0260, 0x0261, 0x0263, 0x0265, 0x0266, 0x0268, 0x0269,
    0x026a, 0x026b, 0x026c, 0x026f, 0x0271, 0x0272, 0x0275, 0x027d, 0x0280,
    0x0282, 0x0283, 0x0287, 0x0288, 0x0289, 0x028c, 0x0292, 0x029d, 0x029e,
    0x03ac, 0x03c2, 0x03cc, 0x03d0, 0x03d1, 0x03d5, 0x03d6, 0x03d7, 0x03f0,
    0x03f1, 0x03f2, 0x03f3, 0x03f5, 0x04cf, 0x1c80, 0x1c81, 0x1c82, 0x1c85,
    0x1c86, 0x1c87, 0x1c88, 0x1d79, 0x1d7d, 0x1d8e, 0x1e9b, 0x1fb3, 0x1fbe,
    0x1fc3, 0x1fe5, 0x1ff3, 0x214e, 0x2184, 0x2c61, 0x2c65, 0x2c66, 0xa794,
    0xab53};
constexpr int kNumToUpperSingles = ARRAYSIZE(kToUpperSingles);
constexpr int kToUpperSinglesOffsets[] = {
    743,   121,   -232,  -300,  195,   97,    -1,    163,   130,    56,
    -79,   -2,    10783, 10780, 10782, -210,  -206,  -202,  -203,   42319,
    -205,  42315, -207,  42280, 42308, -209,  -211,  42308, 10743,  42305,
    -211,  10749, -213,  -214,  10727, -218,  42307, -218,  42282,  -218,
    -69,   -71,   -219,  42261, 42258, -38,   -31,   -64,   -62,    -57,
    -47,   -54,   -8,    -86,   -80,   7,     -116,  -96,   -15,    -6254,
    -6253, -6244, -6243, -6236, -6181, 35266, 35332, 3814,  35384,  -59,
    9,     -7205, 9,     7,     9,     -28,   -1,    -1,    -10795, -10792,
    48,    -928};
constexpr int kNumToUpperSinglesOffsets = ARRAYSIZE(kToUpperSinglesOffsets);
constexpr int kToLowerRangesStart[] = {
    0x0041, 0x0100, 0x0189, 0x01a0, 0x01b1, 0x01b3, 0x0388,  0x038e,  0x0391,
    0x03d8, 0x03fd, 0x0400, 0x0410, 0x0460, 0x0531, 0x10a0,  0x13a0,  0x13f0,
    0x1e00, 0x1f08, 0x1fba, 0x1fc8, 0x1fd8, 0x1fda, 0x1fe8,  0x1fea,  0x1ff8,
    0x1ffa, 0x2c00, 0x2c67, 0x2c7e, 0x2c80, 0xff21, 0x10400, 0x10c80, 0x118a0};
constexpr int kNumToLowerRangesStart = ARRAYSIZE(kToLowerRangesStart);
constexpr int kToLowerRangesEnd[] = {
    0x00de, 0x0187, 0x019f, 0x01af, 0x01b2, 0x0386, 0x038c,  0x038f,  0x03cf,
    0x03fa, 0x03ff, 0x040f, 0x042f, 0x052e, 0x0556, 0x10cd,  0x13ef,  0x13f5,
    0x1efe, 0x1fb9, 0x1fbb, 0x1fcb, 0x1fd9, 0x1fdb, 0x1fe9,  0x1fec,  0x1ff9,
    0x2183, 0x2c64, 0x2c75, 0x2c7f, 0xa7b6, 0xff3a, 0x104d3, 0x10cb2, 0x118bf};
constexpr int kNumToLowerRangesEnd = ARRAYSIZE(kToLowerRangesEnd);
constexpr int kToLowerRangesOffsets[] = {
    32, 1,    205,  1,    217,   1, 37,     63, 32,  1,   -130, 80,
    32, 1,    48,   7264, 38864, 8, 1,      -8, -74, -86, -8,   -100,
    -8, -112, -128, -126, 48,    1, -10815, 1,  32,  40,  64,   32};
constexpr int kNumToLowerRangesOffsets = ARRAYSIZE(kToLowerRangesOffsets);
constexpr int kToUpperRangesStart[] = {
    0x0061, 0x0101, 0x01c6, 0x01ce, 0x023f,  0x0242,  0x0256, 0x028a,
    0x0371, 0x037b, 0x03ad, 0x03b1, 0x03cd,  0x03d9,  0x0430, 0x0450,
    0x0461, 0x0561, 0x10d0, 0x13f8, 0x1c83,  0x1e01,  0x1f00, 0x1f70,
    0x1f72, 0x1f76, 0x1f78, 0x1f7a, 0x1f7c,  0x1f80,  0x2c30, 0x2c68,
    0x2d00, 0xa641, 0xab70, 0xff41, 0x10428, 0x10cc0, 0x118c0};
constexpr int kNumToUpperRangesStart = ARRAYSIZE(kToUpperRangesStart);
constexpr int kToUpperRangesEnd[] = {
    0x00fe, 0x01bd, 0x01cc, 0x023c, 0x0240,  0x024f,  0x0257, 0x028b,
    0x0377, 0x037d, 0x03af, 0x03cb, 0x03ce,  0x03fb,  0x044f, 0x045f,
    0x052f, 0x0586, 0x10ff, 0x13fd, 0x1c84,  0x1eff,  0x1f67, 0x1f71,
    0x1f75, 0x1f77, 0x1f79, 0x1f7b, 0x1f7d,  0x1fe1,  0x2c5e, 0x2cf3,
    0x2d2d, 0xa7c3, 0xabbf, 0xff5a, 0x104fb, 0x10cf2, 0x16e7f};
constexpr int kNumToUpperRangesEnd = ARRAYSIZE(kToUpperRangesEnd);
constexpr int kToUpperRangesOffsets[]{
    -32, -1,  -2,  -1, 10815, -1,   -205,  -217,  -1,     130, -37, -32, -63,
    -1,  -32, -80, -1, -48,   3008, -8,    -6242, -1,     8,   74,  86,  100,
    128, 112, 126, 8,  -48,   -1,   -7264, -1,    -38864, -32, -40, -64, -32};
constexpr int kNumToUpperRangesOffsets = ARRAYSIZE(kToUpperRangesOffsets);

// Source: https://unicode-search.net/unicode-namesearch.pl?term=PERCENT
constexpr char32 kPercentages[] = {0x0025, 0x066A, 0xFE6A, 0xFF05};
constexpr int kNumPercentages = ARRAYSIZE(kPercentages);

// Source from https://unicode-search.net/unicode-namesearch.pl?term=SLASH
constexpr char32 kSlashes[] = {0x002f, 0x0337, 0x0338, 0x2044, 0x2215, 0xff0f};
constexpr int kNumSlashes = ARRAYSIZE(kSlashes);

// Source: https://unicode-search.net/unicode-namesearch.pl?term=minus
constexpr char32 kMinuses[] = {0x002d, 0x02d7, 0x2212, 0xff0d};
constexpr int kNumMinuses = ARRAYSIZE(kMinuses);

// Source: https://unicode-search.net/unicode-namesearch.pl?term=NUMBER%20SIGN
constexpr char32 kNumberSign[] = {0x0023, 0xfe5f, 0xff03};
constexpr int kNumNumberSign = ARRAYSIZE(kNumberSign);

// Source: https://unicode-search.net/unicode-namesearch.pl?term=period
constexpr char32 kDots[] = {0x002e, 0xfe52, 0xff0e};
constexpr int kNumDots = ARRAYSIZE(kDots);

#undef ARRAYSIZE

static_assert(kNumOpeningBrackets == kNumClosingBrackets,
              "mismatching number of opening and closing brackets");
static_assert(kNumLowerRanges1Start == kNumLowerRanges1End,
              "number of uppercase stride 1 range starts/ends doesn't match");
static_assert(kNumLowerRanges2Start == kNumLowerRanges2End,
              "number of uppercase stride 2 range starts/ends doesn't match");
static_assert(kNumUpperRanges1Start == kNumUpperRanges1End,
              "number of uppercase stride 1 range starts/ends doesn't match");
static_assert(kNumUpperRanges2Start == kNumUpperRanges2End,
              "number of uppercase stride 2 range starts/ends doesn't match");
static_assert(kNumToLowerSingles == kNumToLowerSinglesOffsets,
              "number of to lower singles and offsets doesn't match");
static_assert(kNumToLowerRangesStart == kNumToLowerRangesEnd,
              "mismatching number of range starts/ends for to lower ranges");
static_assert(kNumToLowerRangesStart == kNumToLowerRangesOffsets,
              "number of to lower ranges and offsets doesn't match");
static_assert(kNumToUpperSingles == kNumToUpperSinglesOffsets,
              "number of to upper singles and offsets doesn't match");
static_assert(kNumToUpperRangesStart == kNumToUpperRangesEnd,
              "mismatching number of range starts/ends for to upper ranges");
static_assert(kNumToUpperRangesStart == kNumToUpperRangesOffsets,
              "number of to upper ranges and offsets doesn't match");
static_assert(kNumPunctuationRangesStart == kNumPunctuationRangesEnd,
              "mismatch number of start/ends for punctuation ranges.");
static_assert(kNumLatinLettersRangesStart == kNumLatinLettersRangesEnd,
              "mismatch number of start/ends for letters ranges.");
static_assert(kNumArabicLettersRangesStart == kNumArabicLettersRangesEnd,
              "mismatch number of start/ends for letters ranges.");
static_assert(kNumCyrillicLettersRangesStart == kNumCyrillicLettersRangesEnd,
              "mismatch number of start/ends for letters ranges.");
static_assert(kNumChineseLettersRangesStart == kNumChineseLettersRangesEnd,
              "mismatch number of start/ends for letters ranges.");
static_assert(kNumJapaneseLettersRangesStart == kNumJapaneseLettersRangesEnd,
              "mismatch number of start/ends for letters ranges.");
static_assert(kNumKoreanLettersRangesStart == kNumKoreanLettersRangesEnd,
              "mismatch number of start/ends for letters ranges.");
static_assert(kNumThaiLettersRangesStart == kNumThaiLettersRangesEnd,
              "mismatch number of start/ends for letters ranges.");

constexpr int kNoMatch = -1;

// Returns the index of the element in the array that matched the given
// codepoint, or kNoMatch if the element didn't exist.
// The input array must be in sorted order.
int GetMatchIndex(const char32* array, int array_length, char32 c) {
  const char32* end = array + array_length;
  const auto find_it = std::lower_bound(array, end, c);
  if (find_it != end && *find_it == c) {
    return find_it - array;
  } else {
    return kNoMatch;
  }
}

// Returns the index of the range in the array that overlapped the given
// codepoint, or kNoMatch if no such range existed.
// The input array must be in sorted order.
int GetOverlappingRangeIndex(const char32* arr, int arr_length,
                             int range_length, char32 c) {
  const char32* end = arr + arr_length;
  const auto find_it = std::lower_bound(arr, end, c);
  if (find_it == end) {
    return kNoMatch;
  }
  // The end is inclusive, we so subtract one less than the range length.
  const char32 range_end = *find_it;
  const char32 range_start = range_end - (range_length - 1);
  if (c < range_start || range_end < c) {
    return kNoMatch;
  } else {
    return find_it - arr;
  }
}

// As above, but with explicit codepoint start and end indices for the range.
// The input array must be in sorted order.
int GetOverlappingRangeIndex(const char32* start_arr, const char32* end_arr,
                             int arr_length, int stride, char32 c) {
  const char32* end_arr_end = end_arr + arr_length;
  const auto find_it = std::lower_bound(end_arr, end_arr_end, c);
  if (find_it == end_arr_end) {
    return kNoMatch;
  }
  // Find the corresponding start.
  const int range_index = find_it - end_arr;
  const char32 range_start = start_arr[range_index];
  const char32 range_end = *find_it;
  if (c < range_start || range_end < c) {
    return kNoMatch;
  }
  if ((c - range_start) % stride == 0) {
    return range_index;
  } else {
    return kNoMatch;
  }
}

}  // anonymous namespace

bool IsOpeningBracket(char32 codepoint) {
  return GetMatchIndex(kOpeningBrackets, kNumOpeningBrackets, codepoint) >= 0;
}

bool IsClosingBracket(char32 codepoint) {
  return GetMatchIndex(kClosingBrackets, kNumClosingBrackets, codepoint) >= 0;
}

bool IsWhitespace(char32 codepoint) {
  return GetMatchIndex(kWhitespaces, kNumWhitespaces, codepoint) >= 0;
}

bool IsDigit(char32 codepoint) {
  return GetOverlappingRangeIndex(kDecimalDigitRangesEnd,
                                  kNumDecimalDigitRangesEnd,
                                  /*range_length=*/10, codepoint) >= 0;
}

bool IsLower(char32 codepoint) {
  if (GetMatchIndex(kLowerSingles, kNumLowerSingles, codepoint) >= 0) {
    return true;
  } else if (GetOverlappingRangeIndex(kLowerRanges1Start, kLowerRanges1End,
                                      kNumLowerRanges1Start, /*stride=*/1,
                                      codepoint) >= 0) {
    return true;
  } else if (GetOverlappingRangeIndex(kLowerRanges2Start, kLowerRanges2End,
                                      kNumLowerRanges2Start, /*stride=*/2,
                                      codepoint) >= 0) {
    return true;
  } else {
    return false;
  }
}

bool IsUpper(char32 codepoint) {
  if (GetMatchIndex(kUpperSingles, kNumUpperSingles, codepoint) >= 0) {
    return true;
  } else if (GetOverlappingRangeIndex(kUpperRanges1Start, kUpperRanges1End,
                                      kNumUpperRanges1Start, /*stride=*/1,
                                      codepoint) >= 0) {
    return true;
  } else if (GetOverlappingRangeIndex(kUpperRanges2Start, kUpperRanges2End,
                                      kNumUpperRanges2Start, /*stride=*/2,
                                      codepoint) >= 0) {
    return true;
  } else {
    return false;
  }
}

bool IsPunctuation(char32 codepoint) {
  return (GetOverlappingRangeIndex(
              kPunctuationRangesStart, kPunctuationRangesEnd,
              kNumPunctuationRangesStart, /*stride=*/1, codepoint) >= 0);
}

bool IsPercentage(char32 codepoint) {
  return GetMatchIndex(kPercentages, kNumPercentages, codepoint) >= 0;
}

bool IsSlash(char32 codepoint) {
  return GetMatchIndex(kSlashes, kNumSlashes, codepoint) >= 0;
}

bool IsMinus(char32 codepoint) {
  return GetMatchIndex(kMinuses, kNumMinuses, codepoint) >= 0;
}

bool IsNumberSign(char32 codepoint) {
  return GetMatchIndex(kNumberSign, kNumNumberSign, codepoint) >= 0;
}

bool IsDot(char32 codepoint) {
  return GetMatchIndex(kDots, kNumDots, codepoint) >= 0;
}

bool IsLatinLetter(char32 codepoint) {
  return (GetOverlappingRangeIndex(
              kLatinLettersRangesStart, kLatinLettersRangesEnd,
              kNumLatinLettersRangesStart, /*stride=*/1, codepoint) >= 0);
}

bool IsArabicLetter(char32 codepoint) {
  return (GetOverlappingRangeIndex(
              kArabicLettersRangesStart, kArabicLettersRangesEnd,
              kNumArabicLettersRangesStart, /*stride=*/1, codepoint) >= 0);
}

bool IsCyrillicLetter(char32 codepoint) {
  return (GetOverlappingRangeIndex(
              kCyrillicLettersRangesStart, kCyrillicLettersRangesEnd,
              kNumCyrillicLettersRangesStart, /*stride=*/1, codepoint) >= 0);
}

bool IsChineseLetter(char32 codepoint) {
  return (GetOverlappingRangeIndex(
              kChineseLettersRangesStart, kChineseLettersRangesEnd,
              kNumChineseLettersRangesStart, /*stride=*/1, codepoint) >= 0);
}

bool IsJapaneseLetter(char32 codepoint) {
  return (GetOverlappingRangeIndex(
              kJapaneseLettersRangesStart, kJapaneseLettersRangesEnd,
              kNumJapaneseLettersRangesStart, /*stride=*/1, codepoint) >= 0);
}

bool IsKoreanLetter(char32 codepoint) {
  return (GetOverlappingRangeIndex(
              kKoreanLettersRangesStart, kKoreanLettersRangesEnd,
              kNumKoreanLettersRangesStart, /*stride=*/1, codepoint) >= 0);
}

bool IsThaiLetter(char32 codepoint) {
  return (GetOverlappingRangeIndex(
              kThaiLettersRangesStart, kThaiLettersRangesEnd,
              kNumThaiLettersRangesStart, /*stride=*/1, codepoint) >= 0);
}

bool IsCJTletter(char32 codepoint) {
  return IsJapaneseLetter(codepoint) || IsChineseLetter(codepoint) ||
         IsThaiLetter(codepoint);
}

bool IsLetter(char32 codepoint) {
  return IsLatinLetter(codepoint) || IsArabicLetter(codepoint) ||
         IsCyrillicLetter(codepoint) || IsJapaneseLetter(codepoint) ||
         IsKoreanLetter(codepoint) || IsThaiLetter(codepoint) ||
         IsChineseLetter(codepoint);
}

char32 ToLower(char32 codepoint) {
  // Make sure we still produce output even if the method is called for a
  // codepoint that's not an uppercase character.
  if (!IsUpper(codepoint)) {
    return codepoint;
  }
  const int singles_idx =
      GetMatchIndex(kToLowerSingles, kNumToLowerSingles, codepoint);
  if (singles_idx >= 0) {
    return codepoint + kToLowerSinglesOffsets[singles_idx];
  }
  const int ranges_idx =
      GetOverlappingRangeIndex(kToLowerRangesStart, kToLowerRangesEnd,
                               kNumToLowerRangesStart, /*stride=*/1, codepoint);
  if (ranges_idx >= 0) {
    return codepoint + kToLowerRangesOffsets[ranges_idx];
  }
  return codepoint;
}

char32 ToUpper(char32 codepoint) {
  // Make sure we still produce output even if the method is called for a
  // codepoint that's not an uppercase character.
  if (!IsLower(codepoint)) {
    return codepoint;
  }
  const int singles_idx =
      GetMatchIndex(kToUpperSingles, kNumToUpperSingles, codepoint);
  if (singles_idx >= 0) {
    return codepoint + kToUpperSinglesOffsets[singles_idx];
  }
  const int ranges_idx =
      GetOverlappingRangeIndex(kToUpperRangesStart, kToUpperRangesEnd,
                               kNumToUpperRangesStart, /*stride=*/1, codepoint);
  if (ranges_idx >= 0) {
    return codepoint + kToUpperRangesOffsets[ranges_idx];
  }
  return codepoint;
}

char32 GetPairedBracket(char32 codepoint) {
  const int open_offset =
      GetMatchIndex(kOpeningBrackets, kNumOpeningBrackets, codepoint);
  if (open_offset >= 0) {
    return kClosingBrackets[open_offset];
  }
  const int close_offset =
      GetMatchIndex(kClosingBrackets, kNumClosingBrackets, codepoint);
  if (close_offset >= 0) {
    return kOpeningBrackets[close_offset];
  }
  return codepoint;
}

}  // namespace libtextclassifier3
