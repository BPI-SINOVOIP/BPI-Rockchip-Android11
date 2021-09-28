/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <linux/fscrypt.h>

#include <fscrypt/fscrypt.h>

#include <gtest/gtest.h>

using namespace android::fscrypt;

/* modes not supported by upstream kernel, so not in <linux/fscrypt.h> */
#define FSCRYPT_MODE_AES_256_HEH 126
#define FSCRYPT_MODE_PRIVATE 127

const EncryptionOptions TestString(unsigned int first_api_level, const std::string instring,
                                   const std::string outstring) {
    EncryptionOptions options;
    std::string options_string;

    EXPECT_TRUE(ParseOptionsForApiLevel(first_api_level, instring, &options));
    EXPECT_TRUE(OptionsToStringForApiLevel(first_api_level, options, &options_string));
    EXPECT_EQ(outstring, options_string);
    return options;
}

#define TEST_STRING(first_api_level, instring, outstring) \
    SCOPED_TRACE(instring); \
    auto options = TestString(first_api_level, instring, outstring);

TEST(fscrypt, ParseOptions) {
    EncryptionOptions dummy_options;

    std::vector<std::string> defaults = {
            "software",
            "",
            ":",
            "::",
            "aes-256-xts",
            "aes-256-xts:",
            "aes-256-xts::",
            "aes-256-xts:aes-256-cts",
            "aes-256-xts:aes-256-cts:",
            ":aes-256-cts",
            ":aes-256-cts:",
    };
    for (const auto& d : defaults) {
        TEST_STRING(29, d, "aes-256-xts:aes-256-cts:v1");
        EXPECT_EQ(1, options.version);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_XTS, options.contents_mode);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_CTS, options.filenames_mode);
        EXPECT_EQ(FSCRYPT_POLICY_FLAGS_PAD_4, options.flags);
    }
    for (const auto& d : defaults) {
        TEST_STRING(30, d, "aes-256-xts:aes-256-cts:v2");
        EXPECT_TRUE(ParseOptionsForApiLevel(30, d, &dummy_options));
        EXPECT_EQ(2, options.version);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_XTS, options.contents_mode);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_CTS, options.filenames_mode);
        EXPECT_EQ(FSCRYPT_POLICY_FLAGS_PAD_16, options.flags);
    }

    EXPECT_FALSE(ParseOptionsForApiLevel(29, "blah", &dummy_options));
    EXPECT_FALSE(ParseOptionsForApiLevel(30, "blah", &dummy_options));

    {
        TEST_STRING(29, "::v1", "aes-256-xts:aes-256-cts:v1");
        EXPECT_EQ(1, options.version);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_XTS, options.contents_mode);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_CTS, options.filenames_mode);
        EXPECT_EQ(FSCRYPT_POLICY_FLAGS_PAD_4, options.flags);
    }
    {
        TEST_STRING(30, "::v1", "aes-256-xts:aes-256-cts:v1");
        EXPECT_EQ(1, options.version);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_XTS, options.contents_mode);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_CTS, options.filenames_mode);
        EXPECT_EQ(FSCRYPT_POLICY_FLAGS_PAD_16, options.flags);
    }
    {
        TEST_STRING(29, "::v2", "aes-256-xts:aes-256-cts:v2");
        EXPECT_EQ(2, options.version);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_XTS, options.contents_mode);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_CTS, options.filenames_mode);
        EXPECT_EQ(FSCRYPT_POLICY_FLAGS_PAD_16, options.flags);
    }
    {
        TEST_STRING(29, "ice", "ice:aes-256-cts:v1");
        EXPECT_EQ(1, options.version);
        EXPECT_EQ(FSCRYPT_MODE_PRIVATE, options.contents_mode);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_CTS, options.filenames_mode);
        EXPECT_EQ(FSCRYPT_POLICY_FLAGS_PAD_4, options.flags);
    }
    EXPECT_FALSE(ParseOptionsForApiLevel(29, "ice:blah", &dummy_options));

    {
        TEST_STRING(29, "ice:aes-256-cts", "ice:aes-256-cts:v1");
        EXPECT_EQ(1, options.version);
        EXPECT_EQ(FSCRYPT_MODE_PRIVATE, options.contents_mode);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_CTS, options.filenames_mode);
        EXPECT_EQ(FSCRYPT_POLICY_FLAGS_PAD_4, options.flags);
    }

    {
        TEST_STRING(29, "ice:aes-256-heh", "ice:aes-256-heh:v1");
        EXPECT_EQ(1, options.version);
        EXPECT_EQ(FSCRYPT_MODE_PRIVATE, options.contents_mode);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_HEH, options.filenames_mode);
        EXPECT_EQ(FSCRYPT_POLICY_FLAGS_PAD_16, options.flags);
    }
    {
        TEST_STRING(29, "adiantum", "adiantum:adiantum:v1");
        EXPECT_EQ(1, options.version);
        EXPECT_EQ(FSCRYPT_MODE_ADIANTUM, options.contents_mode);
        EXPECT_EQ(FSCRYPT_MODE_ADIANTUM, options.filenames_mode);
        EXPECT_EQ(FSCRYPT_POLICY_FLAGS_PAD_16 | FSCRYPT_POLICY_FLAG_DIRECT_KEY, options.flags);
    }
    {
        TEST_STRING(30, "adiantum", "adiantum:adiantum:v2");
        EXPECT_EQ(2, options.version);
        EXPECT_EQ(FSCRYPT_MODE_ADIANTUM, options.contents_mode);
        EXPECT_EQ(FSCRYPT_MODE_ADIANTUM, options.filenames_mode);
        EXPECT_EQ(FSCRYPT_POLICY_FLAGS_PAD_16 | FSCRYPT_POLICY_FLAG_DIRECT_KEY, options.flags);
    }
    EXPECT_FALSE(ParseOptionsForApiLevel(29, "adiantum:aes-256-cts", &dummy_options));
    EXPECT_FALSE(ParseOptionsForApiLevel(30, "adiantum:aes-256-cts", &dummy_options));
    EXPECT_FALSE(ParseOptionsForApiLevel(29, "aes-256-xts:adiantum", &dummy_options));
    EXPECT_FALSE(ParseOptionsForApiLevel(30, "aes-256-xts:adiantum", &dummy_options));
    {
        TEST_STRING(30, "::inlinecrypt_optimized",
                    "aes-256-xts:aes-256-cts:v2+inlinecrypt_optimized");
        EXPECT_EQ(2, options.version);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_XTS, options.contents_mode);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_CTS, options.filenames_mode);
        EXPECT_EQ(FSCRYPT_POLICY_FLAGS_PAD_16 | FSCRYPT_POLICY_FLAG_IV_INO_LBLK_64, options.flags);
    }
    {
        TEST_STRING(30, "aes-256-xts:aes-256-cts:v2+inlinecrypt_optimized",
                    "aes-256-xts:aes-256-cts:v2+inlinecrypt_optimized");
        EXPECT_EQ(2, options.version);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_XTS, options.contents_mode);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_CTS, options.filenames_mode);
        EXPECT_EQ(FSCRYPT_POLICY_FLAGS_PAD_16 | FSCRYPT_POLICY_FLAG_IV_INO_LBLK_64, options.flags);
    }

    {
        TEST_STRING(30, "::emmc_optimized", "aes-256-xts:aes-256-cts:v2+emmc_optimized");
        EXPECT_EQ(2, options.version);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_XTS, options.contents_mode);
        EXPECT_EQ(FSCRYPT_MODE_AES_256_CTS, options.filenames_mode);
        EXPECT_EQ(FSCRYPT_POLICY_FLAGS_PAD_16 | FSCRYPT_POLICY_FLAG_IV_INO_LBLK_32, options.flags);
    }
    EXPECT_FALSE(
            ParseOptionsForApiLevel(30, "::inlinecrypt_optimized+emmc_optimized", &dummy_options));
    EXPECT_FALSE(ParseOptionsForApiLevel(30, "adiantum::inlinecrypt_optimized", &dummy_options));
    EXPECT_FALSE(ParseOptionsForApiLevel(30, "adiantum::emmc_optimized", &dummy_options));

    EXPECT_FALSE(ParseOptionsForApiLevel(29, "aes-256-xts:aes-256-cts:v2:", &dummy_options));
    EXPECT_FALSE(ParseOptionsForApiLevel(29, "aes-256-xts:aes-256-cts:v2:foo", &dummy_options));
    EXPECT_FALSE(ParseOptionsForApiLevel(29, "aes-256-xts:aes-256-cts:blah", &dummy_options));
    EXPECT_FALSE(ParseOptionsForApiLevel(29, "aes-256-xts:aes-256-cts:vblah", &dummy_options));
    EXPECT_FALSE(ParseOptionsForApiLevel(30, "aes-256-xts:aes-256-cts:v2:", &dummy_options));
    EXPECT_FALSE(ParseOptionsForApiLevel(30, "aes-256-xts:aes-256-cts:v2:foo", &dummy_options));
    EXPECT_FALSE(ParseOptionsForApiLevel(30, "aes-256-xts:aes-256-cts:blah", &dummy_options));
    EXPECT_FALSE(ParseOptionsForApiLevel(30, "aes-256-xts:aes-256-cts:vblah", &dummy_options));
}

TEST(fscrypt, ComparePolicies) {
#define TEST_INEQUALITY(foo, field, value) { \
    auto bar = foo; \
    bar.field = value; \
    EXPECT_NE(foo, bar); \
}
    EncryptionPolicy foo;
    foo.key_raw_ref = "foo";
    EncryptionOptions foo_options;
    foo_options.version = 1;
    foo_options.contents_mode = 1;
    foo_options.filenames_mode = 1;
    foo_options.flags = 1;
    foo_options.use_hw_wrapped_key = true;
    foo.options = foo_options;
    EXPECT_EQ(foo, foo);
    TEST_INEQUALITY(foo, key_raw_ref, "bar");
    TEST_INEQUALITY(foo, options.version, 2);
    TEST_INEQUALITY(foo, options.contents_mode, -1);
    TEST_INEQUALITY(foo, options.filenames_mode, 3);
    TEST_INEQUALITY(foo, options.flags, 0);
    TEST_INEQUALITY(foo, options.use_hw_wrapped_key, false);
}
