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

#include "utils/grammar/utils/rules.h"

#include "utils/grammar/rules_generated.h"
#include "utils/grammar/utils/ir.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3::grammar {
namespace {

using ::testing::IsEmpty;
using ::testing::SizeIs;

TEST(SerializeRulesTest, HandlesSimpleRuleSet) {
  Rules rules;

  rules.Add("<verb>", {"buy"});
  rules.Add("<verb>", {"bring"});
  rules.Add("<verb>", {"remind"});
  rules.Add("<reminder>", {"remind", "me", "to", "<verb>"});
  rules.Add("<action>", {"<reminder>"});

  const Ir ir = rules.Finalize();
  RulesSetT frozen_rules;
  ir.Serialize(/*include_debug_information=*/false, &frozen_rules);

  EXPECT_THAT(frozen_rules.rules, SizeIs(1));
  EXPECT_THAT(frozen_rules.lhs, IsEmpty());
  EXPECT_EQ(frozen_rules.terminals,
            std::string("bring\0buy\0me\0remind\0to\0", 23));
  EXPECT_THAT(frozen_rules.rules.front()->binary_rules, SizeIs(3));
  EXPECT_THAT(frozen_rules.rules.front()->unary_rules, SizeIs(1));
}

TEST(SerializeRulesTest, HandlesRulesSetWithCallbacks) {
  Rules rules;
  const CallbackId output = 1;
  const CallbackId filter = 2;
  rules.DefineFilter(filter);

  rules.Add("<verb>", {"buy"});
  rules.Add("<verb>", {"bring"}, output, 0);
  rules.Add("<verb>", {"remind"}, output, 0);
  rules.Add("<reminder>", {"remind", "me", "to", "<verb>"});
  rules.Add("<action>", {"<reminder>"}, filter, 0);

  const Ir ir = rules.Finalize();
  RulesSetT frozen_rules;
  ir.Serialize(/*include_debug_information=*/false, &frozen_rules);

  EXPECT_THAT(frozen_rules.rules, SizeIs(1));
  EXPECT_EQ(frozen_rules.terminals,
            std::string("bring\0buy\0me\0remind\0to\0", 23));

  // We have two identical output calls and one filter call in the rule set
  // definition above.
  EXPECT_THAT(frozen_rules.lhs, SizeIs(2));

  EXPECT_THAT(frozen_rules.rules.front()->binary_rules, SizeIs(3));
  EXPECT_THAT(frozen_rules.rules.front()->unary_rules, SizeIs(1));
}

TEST(SerializeRulesTest, HandlesRulesWithWhitespaceGapLimits) {
  Rules rules;
  rules.Add("<iata>", {"lx"});
  rules.Add("<iata>", {"aa"});
  rules.Add("<flight>", {"<iata>", "<4_digits>"}, kNoCallback, 0,
            /*max_whitespace_gap=*/0);

  const Ir ir = rules.Finalize();
  RulesSetT frozen_rules;
  ir.Serialize(/*include_debug_information=*/false, &frozen_rules);

  EXPECT_THAT(frozen_rules.rules, SizeIs(1));
  EXPECT_EQ(frozen_rules.terminals, std::string("aa\0lx\0", 6));
  EXPECT_THAT(frozen_rules.lhs, SizeIs(1));
}

TEST(SerializeRulesTest, HandlesCaseSensitiveTerminals) {
  Rules rules;
  rules.Add("<iata>", {"LX"}, kNoCallback, 0, /*max_whitespace_gap=*/-1,
            /*case_sensitive=*/true);
  rules.Add("<iata>", {"AA"}, kNoCallback, 0, /*max_whitespace_gap=*/-1,
            /*case_sensitive=*/true);
  rules.Add("<iata>", {"dl"}, kNoCallback, 0, /*max_whitespace_gap=*/-1,
            /*case_sensitive=*/false);
  rules.Add("<flight>", {"<iata>", "<4_digits>"}, kNoCallback, 0,
            /*max_whitespace_gap=*/0);

  const Ir ir = rules.Finalize();
  RulesSetT frozen_rules;
  ir.Serialize(/*include_debug_information=*/false, &frozen_rules);

  EXPECT_THAT(frozen_rules.rules, SizeIs(1));
  EXPECT_EQ(frozen_rules.terminals, std::string("AA\0LX\0dl\0", 9));
  EXPECT_THAT(frozen_rules.lhs, SizeIs(1));
}

TEST(SerializeRulesTest, HandlesMultipleShards) {
  Rules rules(/*num_shards=*/2);
  rules.Add("<iata>", {"LX"}, kNoCallback, 0, /*max_whitespace_gap=*/-1,
            /*case_sensitive=*/true, /*shard=*/0);
  rules.Add("<iata>", {"aa"}, kNoCallback, 0, /*max_whitespace_gap=*/-1,
            /*case_sensitive=*/false, /*shard=*/1);

  const Ir ir = rules.Finalize();
  RulesSetT frozen_rules;
  ir.Serialize(/*include_debug_information=*/false, &frozen_rules);

  EXPECT_THAT(frozen_rules.rules, SizeIs(2));
  EXPECT_EQ(frozen_rules.terminals, std::string("LX\0aa\0", 6));
}

TEST(SerializeRulesTest, HandlesRegexRules) {
  Rules rules;
  rules.AddRegex("<code>", "[A-Z]+");
  rules.AddRegex("<numbers>", "\\d+");
  RulesSetT frozen_rules;
  rules.Finalize().Serialize(/*include_debug_information=*/false,
                             &frozen_rules);
  EXPECT_THAT(frozen_rules.regex_annotator, SizeIs(2));
}

TEST(SerializeRulesTest, HandlesAlias) {
  Rules rules;
  rules.Add("<iata>", {"lx"});
  rules.Add("<iata>", {"aa"});
  rules.Add("<flight>", {"<iata>", "<4_digits>"});
  rules.AddAlias("<flight_number>", "<flight>");

  const Ir ir = rules.Finalize();
  RulesSetT frozen_rules;
  ir.Serialize(/*include_debug_information=*/false, &frozen_rules);

  EXPECT_THAT(frozen_rules.rules, SizeIs(1));
  EXPECT_EQ(frozen_rules.terminals, std::string("aa\0lx\0", 6));
  EXPECT_THAT(frozen_rules.rules.front()->binary_rules, SizeIs(1));

  // Only alias, no rule.
  EXPECT_THAT(frozen_rules.rules.front()->unary_rules, IsEmpty());

  EXPECT_THAT(frozen_rules.lhs, IsEmpty());
}

TEST(SerializeRulesTest, ResolvesAnchorsAndFillers) {
  Rules rules;
  rules.Add("<code>",
            {"<^>", "<filler>", "this", "is", "a", "test", "<filler>", "<$>"});
  const Ir ir = rules.Finalize();
  RulesSetT frozen_rules;
  ir.Serialize(/*include_debug_information=*/false, &frozen_rules);

  EXPECT_THAT(frozen_rules.rules, SizeIs(1));
  EXPECT_EQ(frozen_rules.terminals, std::string("a\0test\0this\0", 12));

  // Expect removal of anchors and fillers in this case.
  // The rule above is equivalent to: <code> ::= this is a test, binarized into
  // <tmp_0> ::= this is
  // <tmp_1> ::= <tmp_0> a
  // <code>  ::= <tmp_1> test
  EXPECT_THAT(frozen_rules.rules.front()->binary_rules, SizeIs(3));

  EXPECT_THAT(frozen_rules.rules.front()->unary_rules, IsEmpty());
  EXPECT_THAT(frozen_rules.lhs, IsEmpty());
}

TEST(SerializeRulesTest, HandlesAnnotations) {
  Rules rules;
  rules.AddAnnotation("phone");
  rules.AddAnnotation("url");
  rules.AddAnnotation("tracking_number");
  const Ir ir = rules.Finalize();
  RulesSetT frozen_rules;
  ir.Serialize(/*include_debug_information=*/false, &frozen_rules);

  EXPECT_THAT(frozen_rules.rules, SizeIs(1));
  EXPECT_THAT(frozen_rules.nonterminals->annotation_nt, SizeIs(3));
  EXPECT_EQ(frozen_rules.nonterminals->annotation_nt[0]->key, "phone");
  EXPECT_EQ(frozen_rules.nonterminals->annotation_nt[1]->key,
            "tracking_number");
  EXPECT_EQ(frozen_rules.nonterminals->annotation_nt[2]->key, "url");
}

}  // namespace
}  // namespace libtextclassifier3::grammar
