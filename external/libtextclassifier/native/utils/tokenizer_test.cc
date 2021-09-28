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

#include "utils/tokenizer.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

using testing::ElementsAreArray;

class TestingTokenizer : public Tokenizer {
 public:
  TestingTokenizer(
      const TokenizationType type, const UniLib* unilib,
      const std::vector<const TokenizationCodepointRange*>& codepoint_ranges,
      const std::vector<const CodepointRange*>&
          internal_tokenizer_codepoint_ranges,
      const bool split_on_script_change,
      const bool icu_preserve_whitespace_tokens,
      const bool preserve_floating_numbers)
      : Tokenizer(type, unilib, codepoint_ranges,
                  internal_tokenizer_codepoint_ranges, split_on_script_change,
                  icu_preserve_whitespace_tokens, preserve_floating_numbers) {}

  using Tokenizer::FindTokenizationRange;
};

class TestingTokenizerProxy {
 public:
  TestingTokenizerProxy(
      TokenizationType type,
      const std::vector<TokenizationCodepointRangeT>& codepoint_range_configs,
      const std::vector<CodepointRangeT>& internal_codepoint_range_configs,
      const bool split_on_script_change,
      const bool icu_preserve_whitespace_tokens,
      const bool preserve_floating_numbers)
      : INIT_UNILIB_FOR_TESTING(unilib_) {
    const int num_configs = codepoint_range_configs.size();
    std::vector<const TokenizationCodepointRange*> configs_fb;
    configs_fb.reserve(num_configs);
    const int num_internal_configs = internal_codepoint_range_configs.size();
    std::vector<const CodepointRange*> internal_configs_fb;
    internal_configs_fb.reserve(num_internal_configs);
    buffers_.reserve(num_configs + num_internal_configs);
    for (int i = 0; i < num_configs; i++) {
      flatbuffers::FlatBufferBuilder builder;
      builder.Finish(CreateTokenizationCodepointRange(
          builder, &codepoint_range_configs[i]));
      buffers_.push_back(builder.Release());
      configs_fb.push_back(flatbuffers::GetRoot<TokenizationCodepointRange>(
          buffers_.back().data()));
    }
    for (int i = 0; i < num_internal_configs; i++) {
      flatbuffers::FlatBufferBuilder builder;
      builder.Finish(
          CreateCodepointRange(builder, &internal_codepoint_range_configs[i]));
      buffers_.push_back(builder.Release());
      internal_configs_fb.push_back(
          flatbuffers::GetRoot<CodepointRange>(buffers_.back().data()));
    }
    tokenizer_ = std::unique_ptr<TestingTokenizer>(new TestingTokenizer(
        type, &unilib_, configs_fb, internal_configs_fb, split_on_script_change,
        icu_preserve_whitespace_tokens, preserve_floating_numbers));
  }

  TokenizationCodepointRange_::Role TestFindTokenizationRole(int c) const {
    const TokenizationCodepointRangeT* range =
        tokenizer_->FindTokenizationRange(c);
    if (range != nullptr) {
      return range->role;
    } else {
      return TokenizationCodepointRange_::Role_DEFAULT_ROLE;
    }
  }

  std::vector<Token> Tokenize(const std::string& utf8_text) const {
    return tokenizer_->Tokenize(utf8_text);
  }

 private:
  UniLib unilib_;
  std::vector<flatbuffers::DetachedBuffer> buffers_;
  std::unique_ptr<TestingTokenizer> tokenizer_;
};

TEST(TokenizerTest, FindTokenizationRange) {
  std::vector<TokenizationCodepointRangeT> configs;
  TokenizationCodepointRangeT* config;

  configs.emplace_back();
  config = &configs.back();
  config->start = 0;
  config->end = 10;
  config->role = TokenizationCodepointRange_::Role_TOKEN_SEPARATOR;

  configs.emplace_back();
  config = &configs.back();
  config->start = 32;
  config->end = 33;
  config->role = TokenizationCodepointRange_::Role_WHITESPACE_SEPARATOR;

  configs.emplace_back();
  config = &configs.back();
  config->start = 1234;
  config->end = 12345;
  config->role = TokenizationCodepointRange_::Role_TOKEN_SEPARATOR;

  TestingTokenizerProxy tokenizer(TokenizationType_INTERNAL_TOKENIZER, configs,
                                  {}, /*split_on_script_change=*/false,
                                  /*icu_preserve_whitespace_tokens=*/false,
                                  /*preserve_floating_numbers=*/false);

  // Test hits to the first group.
  EXPECT_EQ(tokenizer.TestFindTokenizationRole(0),
            TokenizationCodepointRange_::Role_TOKEN_SEPARATOR);
  EXPECT_EQ(tokenizer.TestFindTokenizationRole(5),
            TokenizationCodepointRange_::Role_TOKEN_SEPARATOR);
  EXPECT_EQ(tokenizer.TestFindTokenizationRole(10),
            TokenizationCodepointRange_::Role_DEFAULT_ROLE);

  // Test a hit to the second group.
  EXPECT_EQ(tokenizer.TestFindTokenizationRole(31),
            TokenizationCodepointRange_::Role_DEFAULT_ROLE);
  EXPECT_EQ(tokenizer.TestFindTokenizationRole(32),
            TokenizationCodepointRange_::Role_WHITESPACE_SEPARATOR);
  EXPECT_EQ(tokenizer.TestFindTokenizationRole(33),
            TokenizationCodepointRange_::Role_DEFAULT_ROLE);

  // Test hits to the third group.
  EXPECT_EQ(tokenizer.TestFindTokenizationRole(1233),
            TokenizationCodepointRange_::Role_DEFAULT_ROLE);
  EXPECT_EQ(tokenizer.TestFindTokenizationRole(1234),
            TokenizationCodepointRange_::Role_TOKEN_SEPARATOR);
  EXPECT_EQ(tokenizer.TestFindTokenizationRole(12344),
            TokenizationCodepointRange_::Role_TOKEN_SEPARATOR);
  EXPECT_EQ(tokenizer.TestFindTokenizationRole(12345),
            TokenizationCodepointRange_::Role_DEFAULT_ROLE);

  // Test a hit outside.
  EXPECT_EQ(tokenizer.TestFindTokenizationRole(99),
            TokenizationCodepointRange_::Role_DEFAULT_ROLE);
}

TEST(TokenizerTest, TokenizeOnSpace) {
  std::vector<TokenizationCodepointRangeT> configs;
  TokenizationCodepointRangeT* config;

  configs.emplace_back();
  config = &configs.back();
  // Space character.
  config->start = 32;
  config->end = 33;
  config->role = TokenizationCodepointRange_::Role_WHITESPACE_SEPARATOR;

  TestingTokenizerProxy tokenizer(TokenizationType_INTERNAL_TOKENIZER, configs,
                                  {},
                                  /*split_on_script_change=*/false,
                                  /*icu_preserve_whitespace_tokens=*/false,
                                  /*preserve_floating_numbers=*/false);
  std::vector<Token> tokens = tokenizer.Tokenize("Hello world!");

  EXPECT_THAT(tokens,
              ElementsAreArray({Token("Hello", 0, 5), Token("world!", 6, 12)}));
}

TEST(TokenizerTest, TokenizeOnSpaceAndScriptChange) {
  std::vector<TokenizationCodepointRangeT> configs;
  TokenizationCodepointRangeT* config;

  // Latin.
  configs.emplace_back();
  config = &configs.back();
  config->start = 0;
  config->end = 32;
  config->role = TokenizationCodepointRange_::Role_DEFAULT_ROLE;
  config->script_id = 1;
  configs.emplace_back();
  config = &configs.back();
  config->start = 32;
  config->end = 33;
  config->role = TokenizationCodepointRange_::Role_WHITESPACE_SEPARATOR;
  config->script_id = 1;
  configs.emplace_back();
  config = &configs.back();
  config->start = 33;
  config->end = 0x77F + 1;
  config->role = TokenizationCodepointRange_::Role_DEFAULT_ROLE;
  config->script_id = 1;

  TestingTokenizerProxy tokenizer(TokenizationType_INTERNAL_TOKENIZER, configs,
                                  {},
                                  /*split_on_script_change=*/true,
                                  /*icu_preserve_whitespace_tokens=*/false,
                                  /*preserve_floating_numbers=*/false);
  EXPECT_THAT(tokenizer.Tokenize("앨라배마 주 전화(123) 456-789웹사이트"),
              std::vector<Token>({Token("앨라배마", 0, 4), Token("주", 5, 6),
                                  Token("전화", 7, 10), Token("(123)", 10, 15),
                                  Token("456-789", 16, 23),
                                  Token("웹사이트", 23, 28)}));
}  // namespace

TEST(TokenizerTest, TokenizeComplex) {
  std::vector<TokenizationCodepointRangeT> configs;
  TokenizationCodepointRangeT* config;

  // Source: http://www.unicode.org/Public/10.0.0/ucd/Blocks-10.0.0d1.txt
  // Latin - cyrilic.
  //   0000..007F; Basic Latin
  //   0080..00FF; Latin-1 Supplement
  //   0100..017F; Latin Extended-A
  //   0180..024F; Latin Extended-B
  //   0250..02AF; IPA Extensions
  //   02B0..02FF; Spacing Modifier Letters
  //   0300..036F; Combining Diacritical Marks
  //   0370..03FF; Greek and Coptic
  //   0400..04FF; Cyrillic
  //   0500..052F; Cyrillic Supplement
  //   0530..058F; Armenian
  //   0590..05FF; Hebrew
  //   0600..06FF; Arabic
  //   0700..074F; Syriac
  //   0750..077F; Arabic Supplement
  configs.emplace_back();
  config = &configs.back();
  config->start = 0;
  config->end = 32;
  config->role = TokenizationCodepointRange_::Role_DEFAULT_ROLE;
  configs.emplace_back();
  config = &configs.back();
  config->start = 32;
  config->end = 33;
  config->role = TokenizationCodepointRange_::Role_WHITESPACE_SEPARATOR;
  configs.emplace_back();
  config = &configs.back();
  config->start = 33;
  config->end = 0x77F + 1;
  config->role = TokenizationCodepointRange_::Role_DEFAULT_ROLE;

  // CJK
  // 2E80..2EFF; CJK Radicals Supplement
  // 3000..303F; CJK Symbols and Punctuation
  // 3040..309F; Hiragana
  // 30A0..30FF; Katakana
  // 3100..312F; Bopomofo
  // 3130..318F; Hangul Compatibility Jamo
  // 3190..319F; Kanbun
  // 31A0..31BF; Bopomofo Extended
  // 31C0..31EF; CJK Strokes
  // 31F0..31FF; Katakana Phonetic Extensions
  // 3200..32FF; Enclosed CJK Letters and Months
  // 3300..33FF; CJK Compatibility
  // 3400..4DBF; CJK Unified Ideographs Extension A
  // 4DC0..4DFF; Yijing Hexagram Symbols
  // 4E00..9FFF; CJK Unified Ideographs
  // A000..A48F; Yi Syllables
  // A490..A4CF; Yi Radicals
  // A4D0..A4FF; Lisu
  // A500..A63F; Vai
  // F900..FAFF; CJK Compatibility Ideographs
  // FE30..FE4F; CJK Compatibility Forms
  // 20000..2A6DF; CJK Unified Ideographs Extension B
  // 2A700..2B73F; CJK Unified Ideographs Extension C
  // 2B740..2B81F; CJK Unified Ideographs Extension D
  // 2B820..2CEAF; CJK Unified Ideographs Extension E
  // 2CEB0..2EBEF; CJK Unified Ideographs Extension F
  // 2F800..2FA1F; CJK Compatibility Ideographs Supplement
  configs.emplace_back();
  config = &configs.back();
  config->start = 0x2E80;
  config->end = 0x2EFF + 1;
  config->role = TokenizationCodepointRange_::Role_TOKEN_SEPARATOR;
  configs.emplace_back();
  config = &configs.back();
  config->start = 0x3000;
  config->end = 0xA63F + 1;
  config->role = TokenizationCodepointRange_::Role_TOKEN_SEPARATOR;
  configs.emplace_back();
  config = &configs.back();
  config->start = 0xF900;
  config->end = 0xFAFF + 1;
  config->role = TokenizationCodepointRange_::Role_TOKEN_SEPARATOR;
  configs.emplace_back();
  config = &configs.back();
  config->start = 0xFE30;
  config->end = 0xFE4F + 1;
  config->role = TokenizationCodepointRange_::Role_TOKEN_SEPARATOR;
  configs.emplace_back();
  config = &configs.back();
  config->start = 0x20000;
  config->end = 0x2A6DF + 1;
  config->role = TokenizationCodepointRange_::Role_TOKEN_SEPARATOR;
  configs.emplace_back();
  config = &configs.back();
  config->start = 0x2A700;
  config->end = 0x2B73F + 1;
  config->role = TokenizationCodepointRange_::Role_TOKEN_SEPARATOR;
  configs.emplace_back();
  config = &configs.back();
  config->start = 0x2B740;
  config->end = 0x2B81F + 1;
  config->role = TokenizationCodepointRange_::Role_TOKEN_SEPARATOR;
  configs.emplace_back();
  config = &configs.back();
  config->start = 0x2B820;
  config->end = 0x2CEAF + 1;
  config->role = TokenizationCodepointRange_::Role_TOKEN_SEPARATOR;
  configs.emplace_back();
  config = &configs.back();
  config->start = 0x2CEB0;
  config->end = 0x2EBEF + 1;
  config->role = TokenizationCodepointRange_::Role_TOKEN_SEPARATOR;
  configs.emplace_back();
  config = &configs.back();
  config->start = 0x2F800;
  config->end = 0x2FA1F + 1;
  config->role = TokenizationCodepointRange_::Role_TOKEN_SEPARATOR;

  // Thai.
  // 0E00..0E7F; Thai
  configs.emplace_back();
  config = &configs.back();
  config->start = 0x0E00;
  config->end = 0x0E7F + 1;
  config->role = TokenizationCodepointRange_::Role_TOKEN_SEPARATOR;

  TestingTokenizerProxy tokenizer(TokenizationType_INTERNAL_TOKENIZER, configs,
                                  {},
                                  /*split_on_script_change=*/false,
                                  /*icu_preserve_whitespace_tokens=*/false,
                                  /*preserve_floating_numbers=*/false);
  std::vector<Token> tokens;

  tokens = tokenizer.Tokenize(
      "問少目木輸走猶術権自京門録球変。細開括省用掲情結傍走愛明氷。");
  EXPECT_EQ(tokens.size(), 30);

  tokens = tokenizer.Tokenize("問少目 hello 木輸ยามきゃ");
  // clang-format off
  EXPECT_THAT(
      tokens,
      ElementsAreArray({Token("問", 0, 1),
                        Token("少", 1, 2),
                        Token("目", 2, 3),
                        Token("hello", 4, 9),
                        Token("木", 10, 11),
                        Token("輸", 11, 12),
                        Token("ย", 12, 13),
                        Token("า", 13, 14),
                        Token("ม", 14, 15),
                        Token("き", 15, 16),
                        Token("ゃ", 16, 17)}));
  // clang-format on
}

#if defined(TC3_TEST_ICU) || defined(__APPLE__)
TEST(TokenizerTest, ICUTokenizeWithWhitespaces) {
  TestingTokenizerProxy tokenizer(TokenizationType_ICU, {}, {},
                                  /*split_on_script_change=*/false,
                                  /*icu_preserve_whitespace_tokens=*/true,
                                  /*preserve_floating_numbers=*/false);
  std::vector<Token> tokens = tokenizer.Tokenize("พระบาท สมเด็จ พระ ปร มิ");
  // clang-format off
  ASSERT_EQ(tokens,
            std::vector<Token>({Token("พระบาท", 0, 6),
                                Token(" ", 6, 7),
                                Token("สมเด็จ", 7, 13),
                                Token(" ", 13, 14),
                                Token("พระ", 14, 17),
                                Token(" ", 17, 18),
                                Token("ปร", 18, 20),
                                Token(" ", 20, 21),
                                Token("มิ", 21, 23)}));
  // clang-format on
}

TEST(TokenizerTest, ICUTokenizePunctuation) {
  TestingTokenizerProxy tokenizer(TokenizationType_ICU, {}, {},
                                  /*split_on_script_change=*/false,
                                  /*icu_preserve_whitespace_tokens=*/true,
                                  /*preserve_floating_numbers=*/false);
  std::vector<Token> tokens =
      tokenizer.Tokenize("The interval is: -(12, 138*)");
  // clang-format off
  ASSERT_EQ(
      tokens,
            std::vector<Token>({Token("The", 0, 3),
                                Token(" ", 3, 4),
                                Token("interval", 4, 12),
                                Token(" ", 12, 13),
                                Token("is", 13, 15),
                                Token(":", 15, 16),
                                Token(" ", 16, 17),
                                Token("-", 17, 18),
                                Token("(", 18, 19),
                                Token("12", 19, 21),
                                Token(",", 21, 22),
                                Token(" ", 22, 23),
                                Token("138", 23, 26),
                                Token("*", 26, 27),
                                Token(")", 27, 28)}));
  // clang-format on
}

TEST(TokenizerTest, ICUTokenizeWithNumbers) {
  TestingTokenizerProxy tokenizer(TokenizationType_ICU, {}, {},
                                  /*split_on_script_change=*/false,
                                  /*icu_preserve_whitespace_tokens=*/true,
                                  /*preserve_floating_numbers=*/false);
  std::vector<Token> tokens = tokenizer.Tokenize("3.1 3﹒2 3．3");
  // clang-format off
  ASSERT_EQ(tokens,
            std::vector<Token>({Token("3.1", 0, 3),
                                Token(" ", 3, 4),
                                Token("3﹒2", 4, 7),
                                Token(" ", 7, 8),
                                Token("3．3", 8, 11)}));
  // clang-format on
}
#endif

#if defined(TC3_TEST_ICU)
TEST(TokenizerTest, ICUTokenize) {
  TestingTokenizerProxy tokenizer(TokenizationType_ICU, {}, {},
                                  /*split_on_script_change=*/false,
                                  /*icu_preserve_whitespace_tokens=*/false,
                                  /*preserve_floating_numbers=*/false);
  std::vector<Token> tokens = tokenizer.Tokenize("พระบาทสมเด็จพระปรมิ");
  // clang-format off
  ASSERT_EQ(tokens,
            std::vector<Token>({Token("พระบาท", 0, 6),
                                Token("สมเด็จ", 6, 12),
                                Token("พระ", 12, 15),
                                Token("ปร", 15, 17),
                                Token("มิ", 17, 19)}));
  // clang-format on
}

TEST(TokenizerTest, MixedTokenize) {
  std::vector<TokenizationCodepointRangeT> configs;
  TokenizationCodepointRangeT* config;

  configs.emplace_back();
  config = &configs.back();
  config->start = 32;
  config->end = 33;
  config->role = TokenizationCodepointRange_::Role_WHITESPACE_SEPARATOR;

  std::vector<CodepointRangeT> internal_configs;
  CodepointRangeT* interal_config;

  internal_configs.emplace_back();
  interal_config = &internal_configs.back();
  interal_config->start = 0;
  interal_config->end = 128;

  internal_configs.emplace_back();
  interal_config = &internal_configs.back();
  interal_config->start = 128;
  interal_config->end = 256;

  internal_configs.emplace_back();
  interal_config = &internal_configs.back();
  interal_config->start = 256;
  interal_config->end = 384;

  internal_configs.emplace_back();
  interal_config = &internal_configs.back();
  interal_config->start = 384;
  interal_config->end = 592;

  TestingTokenizerProxy tokenizer(TokenizationType_MIXED, configs,
                                  internal_configs,
                                  /*split_on_script_change=*/false,
                                  /*icu_preserve_whitespace_tokens=*/false,
                                  /*preserve_floating_numbers=*/false);

  std::vector<Token> tokens = tokenizer.Tokenize(
      "こんにちはJapanese-ląnguagę text 你好世界 http://www.google.com/");
  ASSERT_EQ(
      tokens,
      // clang-format off
      std::vector<Token>({Token("こんにちは", 0, 5),
                          Token("Japanese-ląnguagę", 5, 22),
                          Token("text", 23, 27),
                          Token("你好", 28, 30),
                          Token("世界", 30, 32),
                          Token("http://www.google.com/", 33, 55)}));
  // clang-format on
}

TEST(TokenizerTest, InternalTokenizeOnScriptChange) {
  std::vector<TokenizationCodepointRangeT> configs;
  TokenizationCodepointRangeT* config;

  configs.emplace_back();
  config = &configs.back();
  config->start = 0;
  config->end = 256;
  config->role = TokenizationCodepointRange_::Role_DEFAULT_ROLE;

  {
    TestingTokenizerProxy tokenizer(TokenizationType_INTERNAL_TOKENIZER,
                                    configs, {},
                                    /*split_on_script_change=*/false,
                                    /*icu_preserve_whitespace_tokens=*/false,
                                    /*preserve_floating_numbers=*/false);

    EXPECT_EQ(tokenizer.Tokenize("앨라배마123웹사이트"),
              std::vector<Token>({Token("앨라배마123웹사이트", 0, 11)}));
  }

  {
    TestingTokenizerProxy tokenizer(TokenizationType_INTERNAL_TOKENIZER,
                                    configs, {},
                                    /*split_on_script_change=*/true,
                                    /*icu_preserve_whitespace_tokens=*/false,
                                    /*preserve_floating_numbers=*/false);
    EXPECT_EQ(tokenizer.Tokenize("앨라배마123웹사이트"),
              std::vector<Token>({Token("앨라배마", 0, 4), Token("123", 4, 7),
                                  Token("웹사이트", 7, 11)}));
  }
}
#endif

TEST(TokenizerTest, LetterDigitTokenize) {
  TestingTokenizerProxy tokenizer(TokenizationType_LETTER_DIGIT, {}, {},
                                  /*split_on_script_change=*/false,
                                  /*icu_preserve_whitespace_tokens=*/false,
                                  /*preserve_floating_numbers=*/true);
  std::vector<Token> tokens = tokenizer.Tokenize("7% -3.14 68.9#? 7% $99 .18.");
  ASSERT_EQ(tokens,
            std::vector<Token>(
                {Token("7", 0, 1), Token("%", 1, 2), Token(" ", 2, 3),
                 Token("-", 3, 4), Token("3.14", 4, 8), Token(" ", 8, 9),
                 Token("68.9", 9, 13), Token("#", 13, 14), Token("?", 14, 15),
                 Token(" ", 15, 16), Token("7", 16, 17), Token("%", 17, 18),
                 Token(" ", 18, 19), Token("$", 19, 20), Token("99", 20, 22),
                 Token(" ", 22, 23), Token(".", 23, 24), Token("18", 24, 26),
                 Token(".", 26, 27)}));
}

TEST(TokenizerTest, LetterDigitTokenizeUnicode) {
  TestingTokenizerProxy tokenizer(TokenizationType_LETTER_DIGIT, {}, {},
                                  /*split_on_script_change=*/false,
                                  /*icu_preserve_whitespace_tokens=*/false,
                                  /*preserve_floating_numbers=*/true);
  std::vector<Token> tokens = tokenizer.Tokenize("２ pércént ３パーセント");
  ASSERT_EQ(tokens, std::vector<Token>({Token("２", 0, 1), Token(" ", 1, 2),
                                        Token("pércént", 2, 9),
                                        Token(" ", 9, 10), Token("３", 10, 11),
                                        Token("パーセント", 11, 16)}));
}

TEST(TokenizerTest, LetterDigitTokenizeWithDots) {
  TestingTokenizerProxy tokenizer(TokenizationType_LETTER_DIGIT, {}, {},
                                  /*split_on_script_change=*/false,
                                  /*icu_preserve_whitespace_tokens=*/false,
                                  /*preserve_floating_numbers=*/true);
  std::vector<Token> tokens = tokenizer.Tokenize("3 3﹒2 3．3%");
  ASSERT_EQ(tokens,
            std::vector<Token>({Token("3", 0, 1), Token(" ", 1, 2),
                                Token("3﹒2", 2, 5), Token(" ", 5, 6),
                                Token("3．3", 6, 9), Token("%", 9, 10)}));
}

TEST(TokenizerTest, LetterDigitTokenizeDoNotPreserveFloatingNumbers) {
  TestingTokenizerProxy tokenizer(TokenizationType_LETTER_DIGIT, {}, {},
                                  /*split_on_script_change=*/false,
                                  /*icu_preserve_whitespace_tokens=*/false,
                                  /*preserve_floating_numbers=*/false);
  std::vector<Token> tokens = tokenizer.Tokenize("15.12.2019 january's 3.2");
  ASSERT_EQ(tokens,
            std::vector<Token>(
                {Token("15", 0, 2), Token(".", 2, 3), Token("12", 3, 5),
                 Token(".", 5, 6), Token("2019", 6, 10), Token(" ", 10, 11),
                 Token("january", 11, 18), Token("'", 18, 19),
                 Token("s", 19, 20), Token(" ", 20, 21), Token("3", 21, 22),
                 Token(".", 22, 23), Token("2", 23, 24)}));
}

TEST(TokenizerTest, LetterDigitTokenizeStrangeStringFloatingNumbers) {
  TestingTokenizerProxy tokenizer(TokenizationType_LETTER_DIGIT, {}, {},
                                  /*split_on_script_change=*/false,
                                  /*icu_preserve_whitespace_tokens=*/false,
                                  /*preserve_floating_numbers=*/false);
  std::vector<Token> tokens = tokenizer.Tokenize("The+2345++the +íí+");
  ASSERT_EQ(tokens,
            std::vector<Token>({Token("The", 0, 3), Token("+", 3, 4),
                                Token("2345", 4, 8), Token("+", 8, 9),
                                Token("+", 9, 10), Token("the", 10, 13),
                                Token(" ", 13, 14), Token("+", 14, 15),
                                Token("íí", 15, 17), Token("+", 17, 18)}));
}

TEST(TokenizerTest, LetterDigitTokenizeWhitespcesInSameToken) {
  TestingTokenizerProxy tokenizer(TokenizationType_LETTER_DIGIT, {}, {},
                                  /*split_on_script_change=*/false,
                                  /*icu_preserve_whitespace_tokens=*/false,
                                  /*preserve_floating_numbers=*/false);
  std::vector<Token> tokens = tokenizer.Tokenize("2 3  4   5");
  ASSERT_EQ(tokens, std::vector<Token>({Token("2", 0, 1), Token(" ", 1, 2),
                                        Token("3", 2, 3), Token("  ", 3, 5),
                                        Token("4", 5, 6), Token("   ", 6, 9),
                                        Token("5", 9, 10)}));
}

}  // namespace
}  // namespace libtextclassifier3
