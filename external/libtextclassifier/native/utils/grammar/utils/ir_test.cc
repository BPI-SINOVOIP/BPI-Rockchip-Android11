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

#include "utils/grammar/utils/ir.h"

#include "utils/grammar/rules_generated.h"
#include "utils/grammar/types.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3::grammar {
namespace {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::SizeIs;

TEST(IrTest, HandlesSharingWithTerminalRules) {
  Ir ir;

  // <t1> ::= the
  const Nonterm t1 = ir.Add(kUnassignedNonterm, "the");

  // <t2> ::= quick
  const Nonterm t2 = ir.Add(kUnassignedNonterm, "quick");

  // <t3> ::= quick    -- should share with <t2>
  const Nonterm t3 = ir.Add(kUnassignedNonterm, "quick");

  // <t4> ::= quick    -- specify unshareable <t4>
  // <t4> ::= brown
  const Nonterm t4_unshareable = ir.AddUnshareableNonterminal();
  ir.Add(t4_unshareable, "quick");
  ir.Add(t4_unshareable, "brown");

  // <t5> ::= brown    -- should not be shared with <t4>
  const Nonterm t5 = ir.Add(kUnassignedNonterm, "brown");

  // <t6> ::= brown    -- specify unshareable <t6>
  const Nonterm t6_unshareable = ir.AddUnshareableNonterminal();
  ir.Add(t6_unshareable, "brown");

  // <t7> ::= brown    -- should share with <t5>
  const Nonterm t7 = ir.Add(kUnassignedNonterm, "brown");

  EXPECT_THAT(t1, Ne(kUnassignedNonterm));
  EXPECT_THAT(t2, Ne(kUnassignedNonterm));
  EXPECT_THAT(t1, Ne(t2));
  EXPECT_THAT(t2, Eq(t3));
  EXPECT_THAT(t4_unshareable, Ne(kUnassignedNonterm));
  EXPECT_THAT(t4_unshareable, Ne(t3));
  EXPECT_THAT(t4_unshareable, Ne(t5));
  EXPECT_THAT(t6_unshareable, Ne(kUnassignedNonterm));
  EXPECT_THAT(t6_unshareable, Ne(t4_unshareable));
  EXPECT_THAT(t6_unshareable, Ne(t5));
  EXPECT_THAT(t7, Eq(t5));
}

TEST(IrTest, HandlesSharingWithNonterminalRules) {
  Ir ir;

  // Setup a few terminal rules.
  const std::vector<Nonterm> rhs = {
      ir.Add(kUnassignedNonterm, "the"), ir.Add(kUnassignedNonterm, "quick"),
      ir.Add(kUnassignedNonterm, "brown"), ir.Add(kUnassignedNonterm, "fox")};

  // Check for proper sharing using nonterminal rules.
  for (int rhs_length = 1; rhs_length <= rhs.size(); rhs_length++) {
    std::vector<Nonterm> rhs_truncated = rhs;
    rhs_truncated.resize(rhs_length);
    const Nonterm nt_u = ir.AddUnshareableNonterminal();
    ir.Add(nt_u, rhs_truncated);
    const Nonterm nt_1 = ir.Add(kUnassignedNonterm, rhs_truncated);
    const Nonterm nt_2 = ir.Add(kUnassignedNonterm, rhs_truncated);

    EXPECT_THAT(nt_1, Eq(nt_2));
    EXPECT_THAT(nt_1, Ne(nt_u));
  }
}

TEST(IrTest, HandlesSharingWithCallbacksWithSameParameters) {
  // Test sharing in the presence of callbacks.
  constexpr CallbackId kOutput1 = 1;
  constexpr CallbackId kOutput2 = 2;
  constexpr CallbackId kFilter1 = 3;
  constexpr CallbackId kFilter2 = 4;
  Ir ir(/*filters=*/{kFilter1, kFilter2});

  const Nonterm x1 = ir.Add(kUnassignedNonterm, "hello");
  const Nonterm x2 =
      ir.Add(Ir::Lhs{kUnassignedNonterm, {kOutput1, 0}}, "hello");
  const Nonterm x3 =
      ir.Add(Ir::Lhs{kUnassignedNonterm, {kFilter1, 0}}, "hello");
  const Nonterm x4 =
      ir.Add(Ir::Lhs{kUnassignedNonterm, {kOutput2, 0}}, "hello");
  const Nonterm x5 =
      ir.Add(Ir::Lhs{kUnassignedNonterm, {kFilter2, 0}}, "hello");

  // Duplicate entry.
  const Nonterm x6 =
      ir.Add(Ir::Lhs{kUnassignedNonterm, {kOutput2, 0}}, "hello");

  EXPECT_THAT(x2, Eq(x1));
  EXPECT_THAT(x3, Ne(x1));
  EXPECT_THAT(x4, Eq(x1));
  EXPECT_THAT(x5, Ne(x1));
  EXPECT_THAT(x5, Ne(x3));
  EXPECT_THAT(x6, Ne(x3));
}

TEST(IrTest, HandlesSharingWithCallbacksWithDifferentParameters) {
  // Test sharing in the presence of callbacks.
  constexpr CallbackId kOutput = 1;
  constexpr CallbackId kFilter = 2;
  Ir ir(/*filters=*/{kFilter});

  const Nonterm x1 = ir.Add(Ir::Lhs{kUnassignedNonterm, {kOutput, 0}}, "world");
  const Nonterm x2 = ir.Add(Ir::Lhs{kUnassignedNonterm, {kOutput, 1}}, "world");
  const Nonterm x3 = ir.Add(Ir::Lhs{kUnassignedNonterm, {kFilter, 0}}, "world");
  const Nonterm x4 = ir.Add(Ir::Lhs{kUnassignedNonterm, {kFilter, 1}}, "world");

  EXPECT_THAT(x2, Eq(x1));
  EXPECT_THAT(x3, Ne(x1));
  EXPECT_THAT(x4, Ne(x1));
  EXPECT_THAT(x4, Ne(x3));
}

TEST(IrTest, SerializesRulesToFlatbufferFormat) {
  constexpr CallbackId kOutput = 1;
  Ir ir;
  const Nonterm verb = ir.AddUnshareableNonterminal();
  ir.Add(verb, "buy");
  ir.Add(Ir::Lhs{verb, {kOutput}}, "bring");
  ir.Add(verb, "upbring");
  ir.Add(verb, "remind");
  const Nonterm set_reminder = ir.AddUnshareableNonterminal();
  ir.Add(set_reminder,
         std::vector<Nonterm>{ir.Add(kUnassignedNonterm, "remind"),
                              ir.Add(kUnassignedNonterm, "me"),
                              ir.Add(kUnassignedNonterm, "to"), verb});
  const Nonterm action = ir.AddUnshareableNonterminal();
  ir.Add(action, set_reminder);
  RulesSetT rules;
  ir.Serialize(/*include_debug_information=*/false, &rules);

  EXPECT_THAT(rules.rules, SizeIs(1));

  // Only one rule uses a callback, the rest will be encoded directly.
  EXPECT_THAT(rules.lhs, SizeIs(1));
  EXPECT_THAT(rules.lhs.front().callback_id(), kOutput);

  // 6 distinct terminals: "buy", "upbring", "bring", "remind", "me" and "to".
  EXPECT_THAT(rules.rules.front()->lowercase_terminal_rules->terminal_offsets,
              SizeIs(6));
  EXPECT_THAT(rules.rules.front()->terminal_rules->terminal_offsets, IsEmpty());

  // As "bring" is a suffix of "upbring" it is expected to be suffix merged in
  // the string pool
  EXPECT_THAT(rules.terminals,
              Eq(std::string("buy\0me\0remind\0to\0upbring\0", 25)));

  EXPECT_THAT(rules.rules.front()->binary_rules, SizeIs(3));

  // One unary rule: <action> ::= <set_reminder>
  EXPECT_THAT(rules.rules.front()->unary_rules, SizeIs(1));
}

TEST(IrTest, HandlesRulesSharding) {
  Ir ir(/*filters=*/{}, /*num_shards=*/2);
  const Nonterm verb = ir.AddUnshareableNonterminal();
  const Nonterm set_reminder = ir.AddUnshareableNonterminal();

  // Shard 0: en
  ir.Add(verb, "buy");
  ir.Add(verb, "bring");
  ir.Add(verb, "remind");
  ir.Add(set_reminder,
         std::vector<Nonterm>{ir.Add(kUnassignedNonterm, "remind"),
                              ir.Add(kUnassignedNonterm, "me"),
                              ir.Add(kUnassignedNonterm, "to"), verb});

  // Shard 1: de
  ir.Add(verb, "kaufen", /*case_sensitive=*/false, /*shard=*/1);
  ir.Add(verb, "bringen", /*case_sensitive=*/false, /*shard=*/1);
  ir.Add(verb, "erinnern", /*case_sensitive=*/false, /*shard=*/1);
  ir.Add(set_reminder,
         std::vector<Nonterm>{ir.Add(kUnassignedNonterm, "erinnere",
                                     /*case_sensitive=*/false, /*shard=*/1),
                              ir.Add(kUnassignedNonterm, "mich",
                                     /*case_sensitive=*/false, /*shard=*/1),
                              ir.Add(kUnassignedNonterm, "zu",
                                     /*case_sensitive=*/false, /*shard=*/1),
                              verb},
         /*shard=*/1);

  // Test that terminal strings are correctly merged into the shared
  // string pool.
  RulesSetT rules;
  ir.Serialize(/*include_debug_information=*/false, &rules);

  EXPECT_THAT(rules.rules, SizeIs(2));

  // 5 distinct terminals: "buy", "bring", "remind", "me" and "to".
  EXPECT_THAT(rules.rules[0]->lowercase_terminal_rules->terminal_offsets,
              SizeIs(5));
  EXPECT_THAT(rules.rules[0]->terminal_rules->terminal_offsets, IsEmpty());

  // 6 distinct terminals: "kaufen", "bringen", "erinnern", "erinnere", "mich"
  // and "zu".
  EXPECT_THAT(rules.rules[1]->lowercase_terminal_rules->terminal_offsets,
              SizeIs(6));
  EXPECT_THAT(rules.rules[1]->terminal_rules->terminal_offsets, IsEmpty());

  EXPECT_THAT(rules.terminals,
              Eq(std::string("bring\0bringen\0buy\0erinnere\0erinnern\0kaufen\0"
                             "me\0mich\0remind\0to\0zu\0",
                             64)));

  EXPECT_THAT(rules.rules[0]->binary_rules, SizeIs(3));
  EXPECT_THAT(rules.rules[1]->binary_rules, SizeIs(3));
}

}  // namespace
}  // namespace libtextclassifier3::grammar
