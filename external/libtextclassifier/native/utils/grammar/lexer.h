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

// This is a lexer that runs off the tokenizer and outputs the tokens to a
// grammar matcher. The tokens it forwards are the same as the ones produced
// by the tokenizer, but possibly further split and normalized (downcased).
// Examples:
//
//    - single character tokens for punctuation (e.g., AddTerminal("?"))
//
//    - a string of letters (e.g., "Foo" -- it calls AddTerminal() on "foo")
//
//    - a string of digits (e.g., AddTerminal("37"))
//
// In addition to the terminal tokens above, it also outputs certain
// special nonterminals:
//
//    - a <token> nonterminal, which it outputs in addition to the
//      regular AddTerminal() call for every token
//
//    - a <digits> nonterminal, which it outputs in addition to
//      the regular AddTerminal() call for each string of digits
//
//    - <N_digits> nonterminals, where N is the length of the string of
//      digits. By default the maximum N that will be output is 20. This
//      may be changed at compile time by kMaxNDigitsLength. For instance,
//      "123" will produce a <3_digits> nonterminal, "1234567" will produce
//      a <7_digits> nonterminal.
//
// It does not output any whitespace.  Instead, whitespace gets absorbed into
// the token that follows them in the text.
// For example, if the text contains:
//
//      ...hello                       there        world...
//              |                      |            |
//              offset=16              39           52
//
// then the output will be:
//
//      "hello" [?, 16)
//      "there" [16, 44)      <-- note "16" NOT "39"
//      "world" [44, ?)       <-- note "44" NOT "52"
//
// This makes it appear to the Matcher as if the tokens are adjacent -- so
// whitespace is simply ignored.
//
// A minor optimization:  We don't bother to output nonterminals if the grammar
// rules don't reference them.

#ifndef LIBTEXTCLASSIFIER_UTILS_GRAMMAR_LEXER_H_
#define LIBTEXTCLASSIFIER_UTILS_GRAMMAR_LEXER_H_

#include "annotator/types.h"
#include "utils/grammar/matcher.h"
#include "utils/grammar/rules_generated.h"
#include "utils/grammar/types.h"
#include "utils/strings/stringpiece.h"
#include "utils/utf8/unicodetext.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3::grammar {

class Lexer {
 public:
  explicit Lexer(const UniLib* unilib, const RulesSet* rules);

  // Processes a tokenized text. Classifies the tokens and feeds them to the
  // matcher.
  // The provided annotations will be fed to the matcher alongside the tokens.
  // NOTE: The `annotations` need to outlive any dependent processing.
  void Process(const UnicodeText& text, const std::vector<Token>& tokens,
               const std::vector<AnnotatedSpan>* annotations,
               Matcher* matcher) const;
  void Process(const UnicodeText& text,
               const std::vector<Token>::const_iterator& begin,
               const std::vector<Token>::const_iterator& end,
               const std::vector<AnnotatedSpan>* annotations,
               Matcher* matcher) const;

 private:
  // A lexical symbol with an identified meaning that represents raw tokens,
  // token categories or predefined text matches.
  // It is the unit fed to the grammar matcher.
  struct Symbol {
    // The type of the lexical symbol.
    enum class Type {
      // A raw token.
      TYPE_TERM,

      // A symbol representing a string of digits.
      TYPE_DIGITS,

      // Punctuation characters.
      TYPE_PUNCTUATION,

      // A predefined match.
      TYPE_MATCH
    };

    explicit Symbol() = default;

    // Constructs a symbol of a given type with an anchor in the text.
    Symbol(const Type type, const CodepointSpan codepoint_span,
           const int match_offset, StringPiece lexeme)
        : type(type),
          codepoint_span(codepoint_span),
          match_offset(match_offset),
          lexeme(lexeme) {}

    // Constructs a symbol from a pre-defined match.
    explicit Symbol(Match* match)
        : type(Type::TYPE_MATCH),
          codepoint_span(match->codepoint_span),
          match_offset(match->match_offset),
          match(match) {}

    // The type of the symbole.
    Type type;

    // The span in the text as codepoint offsets.
    CodepointSpan codepoint_span;

    // The match start offset (including preceding whitespace) as codepoint
    // offset.
    int match_offset;

    // The symbol text value.
    StringPiece lexeme;

    // The predefined match.
    Match* match;
  };

  // Processes a single token: the token is split and classified into symbols.
  void ProcessToken(const StringPiece value, const int prev_token_end,
                    const CodepointSpan codepoint_span,
                    std::vector<Symbol>* symbols) const;

  // Emits a token to the matcher.
  void Emit(const Symbol& symbol, const RulesSet_::Nonterminals* nonterms,
            Matcher* matcher) const;

  // Gets the type of a character.
  Symbol::Type GetSymbolType(const UnicodeText::const_iterator& it) const;

 private:
  struct RegexAnnotator {
    std::unique_ptr<UniLib::RegexPattern> pattern;
    Nonterm nonterm;
  };

  // Uncompress and build the defined regex annotators.
  std::vector<RegexAnnotator> BuildRegexAnnotator(const UniLib& unilib,
                                                  const RulesSet* rules) const;

  const UniLib& unilib_;
  const RulesSet* rules_;
  std::vector<RegexAnnotator> regex_annotators_;
};

}  // namespace libtextclassifier3::grammar

#endif  // LIBTEXTCLASSIFIER_UTILS_GRAMMAR_LEXER_H_
