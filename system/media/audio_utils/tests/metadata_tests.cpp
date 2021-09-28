/*
 * Copyright 2020 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "audio_utils_metadata_tests"

#define METADATA_TESTING

#include <audio_utils/Metadata.h>
#include <gtest/gtest.h>
#include <log/log.h>

#include <error.h>
#include <iostream>

using namespace android::audio_utils::metadata;

// Preferred: Key in header - a constexpr which is created by the compiler.
inline constexpr CKey<std::string> ITS_NAME_IS("its_name_is");

// Not preferred: Key which is created at run-time.
inline const Key<std::string> MY_NAME_IS("my_name_is");

// The Metadata table
inline constexpr CKey<Data> TABLE("table");

#ifdef METADATA_TESTING

// Validate recursive typing on "Datum".
inline constexpr CKey<std::vector<Datum>> VECTOR("vector");
inline constexpr CKey<std::pair<Datum, Datum>> PAIR("pair");

// Validate that we move instead of copy.
inline constexpr CKey<MoveCount> MOVE_COUNT("MoveCount");

// Validate recursive container support.
inline constexpr CKey<std::vector<std::vector<std::pair<std::string, short>>>> FUNKY("funky");

// Validate structured binding parceling.
inline constexpr CKey<Arbitrary> ARBITRARY("arbitrary");
#endif

std::string toString(const ByteString &bs) {
    std::stringstream ss;
    ss << "{\n" << std::hex;
    if (bs.size() > 0) {
        for (size_t i = 0; ; ++i) {
            if ((i & 7) == 0) {
                ss << "  ";
            }
            ss << "0x" <<  std::setfill('0') << std::setw(2) << (unsigned)bs[i];
            if (i == bs.size() - 1) {
                break;
            } else if ((i & 7) == 7) {
                ss << ",\n";
            } else {
                ss << ", ";
            }
        }
    }
    ss << "\n}\n";
    return ss.str();
}

TEST(metadata_tests, basic_datum) {
    Datum d;
    d = "abc";
    //ASSERT_EQ("abc", std::any_cast<const char *>(d));
    ASSERT_EQ("abc", std::any_cast<std::string>(d));
    //d = std::vector<int>();

    Datum lore((int32_t) 10);
    d = lore;
    ASSERT_EQ(10, std::any_cast<int32_t>(d));

    // TODO: should we enable Datum to copy from std::any if the types
    // are correct?  The problem is how to signal failure.
    std::any arg = (int)1;
    // Datum invalid = arg; // this doesn't work.

    struct dummy {
        int value = 0;
    };

    // check apply with a void function
    {
        // try to apply with an invalid argument
        int value = 0;

        arg = dummy{}; // not an expected type, apply will fail with false.
        std::any result;

        ASSERT_FALSE(primitive_metadata_types::apply([&](auto *t __unused) {
                value++;
            }, &arg, &result));

        ASSERT_EQ(0, value);              // never invoked.
        ASSERT_FALSE(result.has_value()); // no value returned.

        // try to apply with a valid argument.
        arg = (int)1;

        ASSERT_TRUE(primitive_metadata_types::apply([&](auto *t __unused) {
                value++;
            }, &arg, &result));

        ASSERT_EQ(1, value);              // invoked once.
        ASSERT_FALSE(result.has_value()); // no value returned (function returns void).
    }

    // check apply with a function that returns 2.
    {
        int value = 0;
        arg = (int)1;
        std::any result;

        ASSERT_TRUE(primitive_metadata_types::apply([&](auto *t __unused) {
                value++;
                return (int32_t)2;
            }, &arg, &result));

        ASSERT_EQ(1, value);                          // invoked once.
        ASSERT_EQ(2, std::any_cast<int32_t>(result)); // 2 returned
    }

#ifdef METADATA_TESTING
    // Checks the number of moves versus copies as the datum flows through Data.
    // the counters should increment each time a MoveCount gets copied or
    // moved.

    //  Datum mc = MoveCount();

    Datum mc{MoveCount()};
    ASSERT_TRUE(1 >= std::any_cast<MoveCount>(mc).mMoveCount); // no more than 1 move.
    ASSERT_EQ(0, std::any_cast<MoveCount>(&mc)->mCopyCount);   // no copies
    ASSERT_EQ(1, std::any_cast<MoveCount>(mc).mCopyCount);     // Note: any_cast on value copies.


    // serialize
    ByteString bs;
    ASSERT_TRUE(copyToByteString(mc, bs));
    // deserialize
    size_t idx = 0;
    Datum parceled;
    ASSERT_TRUE(copyFromByteString(&parceled, bs, idx, nullptr /* unknowns */));

    // everything OK with the received data?
    ASSERT_EQ(bs.size(), idx);          // no data left over.
    ASSERT_TRUE(parceled.has_value());  // we have a value.

    // confirm no copies.
    ASSERT_TRUE(2 >= std::any_cast<MoveCount>(&parceled)->mMoveCount); // no more than 2 moves.
    ASSERT_EQ(0, std::any_cast<MoveCount>(&parceled)->mCopyCount);
#endif
}

TEST(metadata_tests, basic_data) {
    Data d;
    d.emplace("int32", (int32_t)1);
    d.emplace("int64", (int64_t)2);
    d.emplace("float", (float)3.1f);
    d.emplace("double", (double)4.11);
    d.emplace("string", "hello");
    d["string2"] = "world";

    // Put with typed keys
    d.put(MY_NAME_IS, "neo");
    d[ITS_NAME_IS] = "spot";

    ASSERT_EQ(1, std::any_cast<int32_t>(d["int32"]));
    ASSERT_EQ(2, std::any_cast<int64_t>(d["int64"]));
    ASSERT_EQ(3.1f, std::any_cast<float>(d["float"]));
    ASSERT_EQ(4.11, std::any_cast<double>(d["double"]));
    ASSERT_EQ("hello", std::any_cast<std::string>(d["string"]));
    ASSERT_EQ("world", std::any_cast<std::string>(d["string2"]));

    // Get with typed keys
    ASSERT_EQ("neo", *d.get_ptr(MY_NAME_IS));
    ASSERT_EQ("spot", *d.get_ptr(ITS_NAME_IS));

    ASSERT_EQ("neo", d[MY_NAME_IS]);
    ASSERT_EQ("spot", d[ITS_NAME_IS]);

    ByteString bs = byteStringFromData(d);
    Data data = dataFromByteString(bs);
    ASSERT_EQ((size_t)8, data.size());

    ASSERT_EQ(1, std::any_cast<int32_t>(data["int32"]));
    ASSERT_EQ(2, std::any_cast<int64_t>(data["int64"]));
    ASSERT_EQ(3.1f, std::any_cast<float>(data["float"]));
    ASSERT_EQ(4.11, std::any_cast<double>(data["double"]));
    ASSERT_EQ("hello", std::any_cast<std::string>(data["string"]));
    ASSERT_EQ("neo", *data.get_ptr(MY_NAME_IS));
    ASSERT_EQ("spot", *data.get_ptr(ITS_NAME_IS));

    data[MY_NAME_IS] = "one";
    ASSERT_EQ("one", data[MY_NAME_IS]);

    // Keys are typed, so this fails to compile.
    // data->put(MY_NAME_IS, 10);

#ifdef METADATA_TESTING
    // Checks the number of moves versus copies as the Datum goes to
    // Data and then parceled and unparceled.
    // The counters should increment each time a MoveCount gets copied or
    // moved.
    {
        Data d2;
        d2[MOVE_COUNT] = MoveCount(); // should be moved.

        ASSERT_TRUE(1 >= d2[MOVE_COUNT].mMoveCount); // no more than one move.
        ASSERT_EQ(0, d2[MOVE_COUNT].mCopyCount);     // no copies

        ByteString bs = byteStringFromData(d2);
        Data d3 = dataFromByteString(bs);

        ASSERT_EQ(0, d3[MOVE_COUNT].mCopyCount);     // no copies
        ASSERT_TRUE(2 >= d3[MOVE_COUNT].mMoveCount); // no more than 2 moves after parceling
    }
#endif
}

TEST(metadata_tests, complex_data) {
    Data small;
    Data big;

    small[MY_NAME_IS] = "abc";
#ifdef METADATA_TESTING
    small[MOVE_COUNT] = MoveCount{};
#endif
    big[TABLE] = small;  // ONE COPY HERE of the MoveCount (embedded in small).

#ifdef METADATA_TESTING
    big[VECTOR] = std::vector<Datum>{small, small};
    big[PAIR] = std::make_pair<Datum, Datum>(small, small);
    ASSERT_EQ(1, big[TABLE][MOVE_COUNT].mCopyCount); // one copy done for small.

    big[FUNKY] = std::vector<std::vector<std::pair<std::string, short>>>{
        {{"a", 1}, {"b", 2}},
        {{"c", 3}, {"d", 4}},
    };

    // struct Arbitrary { int i0; std::vector<int> v1; std::pair<int, int> p2; };
    big[ARBITRARY] = Arbitrary{0, {1, 2, 3}, {4, 5}};
#endif

    // Try round-trip conversion to a ByteString.
    ByteString bs = byteStringFromData(big);
    Data data = dataFromByteString(bs);
#ifdef METADATA_TESTING
    ASSERT_EQ((size_t)5, data.size());
#else
    ASSERT_EQ((size_t)1, data.size());
#endif

    // Nested tables make sense.
    ASSERT_EQ("abc", data[TABLE][MY_NAME_IS]);

#ifdef METADATA_TESTING
    // TODO: Maybe we don't need the vector or the pair.
    ASSERT_EQ("abc", std::any_cast<Data>(data[VECTOR][1])[MY_NAME_IS]);
    ASSERT_EQ("abc", std::any_cast<Data>(data[PAIR].first)[MY_NAME_IS]);
    ASSERT_EQ(1, data[TABLE][MOVE_COUNT].mCopyCount); // no additional copies.

    auto funky = data[FUNKY];
    ASSERT_EQ("a", funky[0][0].first);
    ASSERT_EQ(4, funky[1][1].second);

    auto arbitrary = data[ARBITRARY];
    ASSERT_EQ(0, arbitrary.i0);
    ASSERT_EQ(2, arbitrary.v1[1]);
    ASSERT_EQ(4, arbitrary.p2.first);
#endif
}

// DO NOT CHANGE THIS after R, but add a new test.
TEST(metadata_tests, compatibility_R) {
    Data d;
    d.emplace("i32", (int32_t)1);
    d.emplace("i64", (int64_t)2);
    d.emplace("float", (float)3.1f);
    d.emplace("double", (double)4.11);
    Data s;
    s.emplace("string", "hello");
    d.emplace("data", s);

    ByteString bs = byteStringFromData(d);
    ALOGD("%s", toString(bs).c_str());

    // Since we use a map instead of a hashmap
    // layout order of elements is precisely defined.
    ByteString reference = {
        0x05, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x64, 0x61, 0x74, 0x61, 0x06, 0x00, 0x00, 0x00,
        0x1f, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x06, 0x00, 0x00, 0x00, 0x73, 0x74, 0x72, 0x69,
        0x6e, 0x67, 0x05, 0x00, 0x00, 0x00, 0x09, 0x00,
        0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x68, 0x65,
        0x6c, 0x6c, 0x6f, 0x06, 0x00, 0x00, 0x00, 0x64,
        0x6f, 0x75, 0x62, 0x6c, 0x65, 0x04, 0x00, 0x00,
        0x00, 0x08, 0x00, 0x00, 0x00, 0x71, 0x3d, 0x0a,
        0xd7, 0xa3, 0x70, 0x10, 0x40, 0x05, 0x00, 0x00,
        0x00, 0x66, 0x6c, 0x6f, 0x61, 0x74, 0x03, 0x00,
        0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x66, 0x66,
        0x46, 0x40, 0x03, 0x00, 0x00, 0x00, 0x69, 0x33,
        0x32, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
        0x00, 0x69, 0x36, 0x34, 0x02, 0x00, 0x00, 0x00,
        0x08, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    ASSERT_EQ(reference, bs);

    Data decoded = dataFromByteString(bs);

    // TODO: data equality.
    // ASSERT_EQ(decoded, d);

    ASSERT_EQ(1, std::any_cast<int32_t>(decoded["i32"]));
    ASSERT_EQ(2, std::any_cast<int64_t>(decoded["i64"]));
    ASSERT_EQ(3.1f, std::any_cast<float>(decoded["float"]));
    ASSERT_EQ(4.11, std::any_cast<double>(decoded["double"]));
    Data decoded_s = std::any_cast<Data>(decoded["data"]);

    ASSERT_EQ("hello", std::any_cast<std::string>(s["string"]));

    {
        ByteString unknownData = reference;
        unknownData[12] = 0xff;
        Data decoded2 = dataFromByteString(unknownData);
        ASSERT_EQ((size_t)0, decoded2.size());

        ByteStringUnknowns unknowns;
        Data decoded3 = dataFromByteString(unknownData, &unknowns);
        ASSERT_EQ((size_t)4, decoded3.size());
        ASSERT_EQ((size_t)1, unknowns.size());
        ASSERT_EQ((unsigned)0xff, unknowns[0]);
    }

    {
        ByteString unknownDouble = reference;
        ASSERT_EQ(0x4, unknownDouble[0x3d]);
        unknownDouble[0x3d] = 0xfe;
        Data decoded2 = dataFromByteString(unknownDouble);
        ASSERT_EQ((size_t)0, decoded2.size());

        ByteStringUnknowns unknowns;
        Data decoded3 = dataFromByteString(unknownDouble, &unknowns);
        ASSERT_EQ((size_t)4, decoded3.size());
        ASSERT_EQ((size_t)1, unknowns.size());
        ASSERT_EQ((unsigned)0xfe, unknowns[0]);
    }
};

TEST(metadata_tests, bytestring_examples) {
    ByteString bs;

    copyToByteString((int32_t)123, bs);
    ALOGD("123 -> %s", toString(bs).c_str());
    const ByteString ref1{ 0x7b, 0x00, 0x00, 0x00 };
    ASSERT_EQ(ref1, bs);

    bs.clear();
    // for copyToByteString use std::string instead of char array.
    copyToByteString(std::string("hi"), bs);
    ALOGD("\"hi\" -> %s", toString(bs).c_str());
    const ByteString ref2{ 0x02, 0x00, 0x00, 0x00, 0x68, 0x69 };
    ASSERT_EQ(ref2, bs);

    bs.clear();
    Data d;
    d.emplace("hello", "world");
    d.emplace("value", (int32_t)1000);
    copyToByteString(d, bs);
    ALOGD("{{\"hello\", \"world\"}, {\"value\", 1000}} -> %s", toString(bs).c_str());
    const ByteString ref3{
        0x02, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
        0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x05, 0x00, 0x00,
        0x00, 0x09, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00,
        0x00, 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x05, 0x00,
        0x00, 0x00, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x01,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xe8,
        0x03, 0x00, 0x00};
    ASSERT_EQ(ref3, bs);
};

// Test C API
TEST(metadata_tests, c) {
    audio_metadata_t *metadata = audio_metadata_create();
    Data d;
    d.emplace("i32", (int32_t)1);
    d.emplace("i64", (int64_t)2);
    d.emplace("float", (float)3.1f);
    d.emplace("double", (double)4.11);
    Data s;
    s.emplace("string", "hello");
    d.emplace("data", s);

    audio_metadata_put(metadata, "i32", (int32_t)1);
    audio_metadata_put(metadata, "i64", (int64_t)2);
    audio_metadata_put(metadata, "float", (float)3.1f);
    audio_metadata_put(metadata, "double", (double)4.11);
    audio_metadata_t *data = audio_metadata_create();
    audio_metadata_put(data, "string", "hello");
    audio_metadata_put(metadata, "data", data);
    audio_metadata_destroy(data);

    int32_t i32Val;
    int64_t i64Val;
    float floatVal;
    double doubleVal;
    char *strVal = nullptr;
    audio_metadata_t *dataVal = nullptr;
    ASSERT_EQ(0, audio_metadata_get(metadata, "i32", &i32Val));
    ASSERT_EQ(1, i32Val);
    ASSERT_EQ(0, audio_metadata_get(metadata, "i64", &i64Val));
    ASSERT_EQ(2, i64Val);
    ASSERT_EQ(0, audio_metadata_get(metadata, "float", &floatVal));
    ASSERT_EQ(3.1f, floatVal);
    ASSERT_EQ(0, audio_metadata_get(metadata, "double", &doubleVal));
    ASSERT_EQ(4.11, doubleVal);
    ASSERT_EQ(0, audio_metadata_get(metadata, "data", &dataVal));
    ASSERT_NE(dataVal, nullptr);
    ASSERT_EQ(0, audio_metadata_get(dataVal, "string", &strVal));
    ASSERT_EQ(0, strcmp("hello", strVal));
    free(strVal);
    audio_metadata_destroy(dataVal);
    dataVal = nullptr;
    ASSERT_EQ(-ENOENT, audio_metadata_get(metadata, "non_exist_key", &i32Val));
    audio_metadata_t *nullMetadata = nullptr;
    ASSERT_EQ(-EINVAL, audio_metadata_get(nullMetadata, "i32", &i32Val));
    char *nullKey = nullptr;
    ASSERT_EQ(-EINVAL, audio_metadata_get(metadata, nullKey, &i32Val));
    int *nullI32Val = nullptr;
    ASSERT_EQ(-EINVAL, audio_metadata_get(metadata, "i32", nullI32Val));

    uint8_t *bs = nullptr;
    size_t length = byte_string_from_audio_metadata(metadata, &bs);
    ASSERT_EQ(byteStringFromData(d).size(), ByteString(bs, length).size());
    audio_metadata_t *metadataFromBs = audio_metadata_from_byte_string(bs, length);
    free(bs);
    bs = nullptr;
    length = byte_string_from_audio_metadata(metadataFromBs, &bs);
    ASSERT_EQ(byteStringFromData(d), ByteString(bs, length));
    free(bs);
    bs = nullptr;
    audio_metadata_destroy(metadataFromBs);
    ASSERT_EQ(-EINVAL, byte_string_from_audio_metadata(nullMetadata, &bs));
    uint8_t **nullBs = nullptr;
    ASSERT_EQ(-EINVAL, byte_string_from_audio_metadata(metadata, nullBs));

    ASSERT_EQ(1, audio_metadata_erase(metadata, "data"));
    audio_metadata_get(metadata, "data", dataVal);
    ASSERT_EQ(nullptr, dataVal);
    ASSERT_EQ(0, audio_metadata_erase(metadata, "data"));
    ASSERT_EQ(-EINVAL, audio_metadata_erase(nullMetadata, "key"));
    ASSERT_EQ(-EINVAL, audio_metadata_erase(metadata, nullKey));

    audio_metadata_destroy(metadata);
};
