/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "minikin/Emoji.h"

#include <gtest/gtest.h>
#include <unicode/uchar.h>

namespace minikin {

TEST(EmojiTest, isEmojiTest) {
    EXPECT_TRUE(isEmoji(0x0023));   // NUMBER SIGN
    EXPECT_TRUE(isEmoji(0x0035));   // DIGIT FIVE
    EXPECT_TRUE(isEmoji(0x2640));   // FEMALE SIGN
    EXPECT_TRUE(isEmoji(0x2642));   // MALE SIGN
    EXPECT_TRUE(isEmoji(0x2695));   // STAFF OF AESCULAPIUS
    EXPECT_TRUE(isEmoji(0x1F0CF));  // PLAYING CARD BLACK JOKER
    EXPECT_TRUE(isEmoji(0x1F1E9));  // REGIONAL INDICATOR SYMBOL LETTER D
    EXPECT_TRUE(isEmoji(0x1F6F7));  // SLED
    EXPECT_TRUE(isEmoji(0x1F9E6));  // SOCKS

    // Unicode 12.0
    EXPECT_TRUE(isEmoji(0x1F6D5));  // HINDU TEMPLE
    EXPECT_TRUE(isEmoji(0x1F7E7));  // ORANGE SQUARE
    EXPECT_TRUE(isEmoji(0x1F9CF));  // DEAF PERSON
    EXPECT_TRUE(isEmoji(0x1F9CE));  // PERSON KNEELING
    EXPECT_TRUE(isEmoji(0x1F9A6));  // OTTER
    EXPECT_TRUE(isEmoji(0x1F9A9));  // FLAMINGO
    EXPECT_TRUE(isEmoji(0x1F9C6));  // FALAFEL
    EXPECT_TRUE(isEmoji(0x1F9AA));  // OYSTER
    EXPECT_TRUE(isEmoji(0x1FA82));  // PARACHUTE
    EXPECT_TRUE(isEmoji(0x1FA80));  // YO-YO
    EXPECT_TRUE(isEmoji(0x1FA70));  // BALLET SHOES
    EXPECT_TRUE(isEmoji(0x1FA79));  // ADHESIVE BANDAGE

    // Unicode 13.0
    EXPECT_TRUE(isEmoji(0x1F6D6));  // HUT
    EXPECT_TRUE(isEmoji(0x1F977));  // NINJA
    EXPECT_TRUE(isEmoji(0x1F9A3));  // MAMMOTH
    EXPECT_TRUE(isEmoji(0x1FA74));  // THONG SANDAL
    EXPECT_TRUE(isEmoji(0x1FAC2));  // PEOPLE HUGGING
    EXPECT_TRUE(isEmoji(0x1FAD6));  // TEAPOT

    EXPECT_FALSE(isEmoji(0x0000));   // <control>
    EXPECT_FALSE(isEmoji(0x0061));   // LATIN SMALL LETTER A
    EXPECT_FALSE(isEmoji(0x1F93B));  // MODERN PENTATHLON
    EXPECT_FALSE(isEmoji(0x1F946));  // RIFLE
    EXPECT_FALSE(isEmoji(0x29E3D));  // A han character.
}

TEST(EmojiTest, isEmojiModifierTest) {
    EXPECT_TRUE(isEmojiModifier(0x1F3FB));  // EMOJI MODIFIER FITZPATRICK TYPE-1-2
    EXPECT_TRUE(isEmojiModifier(0x1F3FC));  // EMOJI MODIFIER FITZPATRICK TYPE-3
    EXPECT_TRUE(isEmojiModifier(0x1F3FD));  // EMOJI MODIFIER FITZPATRICK TYPE-4
    EXPECT_TRUE(isEmojiModifier(0x1F3FE));  // EMOJI MODIFIER FITZPATRICK TYPE-5
    EXPECT_TRUE(isEmojiModifier(0x1F3FF));  // EMOJI MODIFIER FITZPATRICK TYPE-6

    EXPECT_FALSE(isEmojiModifier(0x0000));   // <control>
    EXPECT_FALSE(isEmojiModifier(0x1F3FA));  // AMPHORA
    EXPECT_FALSE(isEmojiModifier(0x1F400));  // RAT
    EXPECT_FALSE(isEmojiModifier(0x29E3D));  // A han character.
}

TEST(EmojiTest, isEmojiBaseTest) {
    EXPECT_TRUE(isEmojiBase(0x261D));   // WHITE UP POINTING INDEX
    EXPECT_TRUE(isEmojiBase(0x270D));   // WRITING HAND
    EXPECT_TRUE(isEmojiBase(0x1F385));  // FATHER CHRISTMAS
    EXPECT_TRUE(isEmojiBase(0x1F3C2));  // SNOWBOARDER
    EXPECT_TRUE(isEmojiBase(0x1F3C7));  // HORSE RACING
    EXPECT_TRUE(isEmojiBase(0x1F3CC));  // GOLFER
    EXPECT_TRUE(isEmojiBase(0x1F574));  // MAN IN BUSINESS SUIT LEVITATING
    EXPECT_TRUE(isEmojiBase(0x1F6CC));  // SLEEPING ACCOMMODATION
    EXPECT_TRUE(isEmojiBase(0x1F91D));  // HANDSHAKE (removed from Emoji 4.0, but we need it)
    EXPECT_TRUE(isEmojiBase(0x1F91F));  // I LOVE YOU HAND SIGN
    EXPECT_TRUE(isEmojiBase(0x1F931));  // BREAST-FEEDING
    EXPECT_TRUE(isEmojiBase(0x1F932));  // PALMS UP TOGETHER
    EXPECT_TRUE(isEmojiBase(0x1F93C));  // WRESTLERS (removed from Emoji 4.0, but we need it)
    EXPECT_TRUE(isEmojiBase(0x1F9D1));  // ADULT
    EXPECT_TRUE(isEmojiBase(0x1F9DD));  // ELF

    EXPECT_TRUE(isEmojiBase(0x1F9B5));  // LEG
    EXPECT_TRUE(isEmojiBase(0x1F9B6));  // FOOT
    EXPECT_TRUE(isEmojiBase(0x1F9B8));  // SUPERHERO
    EXPECT_TRUE(isEmojiBase(0x1F9B9));  // SUPERVILLAIN

    // Unicode 12
    EXPECT_TRUE(isEmojiBase(0x1F90F));  // PINCHING HAND
    EXPECT_TRUE(isEmojiBase(0x1F9BB));  // EAR WITH HEARING AID
    EXPECT_TRUE(isEmojiBase(0x1F9CD));  // PERSON STANDING
    EXPECT_TRUE(isEmojiBase(0x1F9CF));  // DEAF PERSON

    // Unicode 13
    EXPECT_TRUE(isEmojiBase(0x1F90C));  // PINCHED FINGERS
    EXPECT_TRUE(isEmojiBase(0x1F977));  // NINJA

    EXPECT_FALSE(isEmojiBase(0x0000));   // <control>
    EXPECT_FALSE(isEmojiBase(0x261C));   // WHITE LEFT POINTING INDEX
    EXPECT_FALSE(isEmojiBase(0x1F384));  // CHRISTMAS TREE
    EXPECT_FALSE(isEmojiBase(0x1F9DE));  // GENIE
    EXPECT_FALSE(isEmojiBase(0x29E3D));  // A han character.
}

TEST(EmojiTest, emojiBidiOverrideTest) {
    EXPECT_EQ(U_RIGHT_TO_LEFT, emojiBidiOverride(nullptr, 0x05D0));  // HEBREW LETTER ALEF
    EXPECT_EQ(U_LEFT_TO_RIGHT,
              emojiBidiOverride(nullptr, 0x1F170));  // NEGATIVE SQUARED LATIN CAPITAL LETTER A
    EXPECT_EQ(U_OTHER_NEUTRAL, emojiBidiOverride(nullptr, 0x1F6F7));  // SLED
    EXPECT_EQ(U_OTHER_NEUTRAL, emojiBidiOverride(nullptr, 0x1F9E6));  // SOCKS
}

}  // namespace minikin
