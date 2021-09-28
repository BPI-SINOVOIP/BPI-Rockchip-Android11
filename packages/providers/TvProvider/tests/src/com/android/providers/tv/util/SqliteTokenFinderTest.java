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

package com.android.providers.tv.util;

import android.annotation.Nullable;
import android.test.AndroidTestCase;
import android.util.Pair;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.function.Consumer;

/**
 * Tests for {@link SqliteTokenFinder}.
 *
 * Modified from
 * packages/providers/ContactsProvider/tests/src/com/android/providers/contacts/sqlite/SqlCheckerTest.java
 */
public class SqliteTokenFinderTest extends AndroidTestCase {
    private List<Pair<Integer, String>> getTokens(String sql) {
        List<Pair<Integer, String>> tokens = new ArrayList<>();
        SqliteTokenFinder.findTokens(sql, new Consumer<Pair<Integer, String>>() {
            @Override
            public void accept(Pair<Integer, String> pair) {
                tokens.add(pair);
            }
        });
        return tokens;
    }

    private void checkTokens(String sql, Pair<Integer, String>... tokens) {
        final List<Pair<Integer, String>> expected = Arrays.asList(tokens);

        assertEquals(expected, getTokens(sql));
    }

    private void checkTokensRegular(String sql, @Nullable String tokens) {
        List<Pair<Integer, String>> expected = new ArrayList<>();

        if (tokens != null) {
            for (String token : tokens.split(" ")) {
                expected.add(Pair.create(SqliteTokenFinder.TYPE_REGULAR, token));
            }
        }

        assertEquals(expected, getTokens(sql));
    }

    private void assertInvalidSql(String sql, String message) {
        try {
            getTokens(sql);
            fail("Didn't throw Exception");
        } catch (Exception e) {
            assertTrue("Expected " + e.getMessage() + " to contain " + message,
                    e.getMessage().contains(message));
        }
    }

    public void testWhitespaces() {
        checkTokensRegular("  select  \t\r\n a\n\n  ", "select a");
        checkTokensRegular("a b", "a b");
    }

    public void testComment() {
        checkTokensRegular("--\n", null);
        checkTokensRegular("a--\n", "a");
        checkTokensRegular("a--abcdef\n", "a");
        checkTokensRegular("a--abcdef\nx", "a x");
        checkTokensRegular("a--\nx", "a x");
        checkTokensRegular("a--abcdef", "a");
        checkTokensRegular("a--abcdef\ndef--", "a def");

        checkTokensRegular("/**/", null);
        assertInvalidSql("/*", "Unterminated comment");
        assertInvalidSql("/*/", "Unterminated comment");
        assertInvalidSql("/*\n* /*a", "Unterminated comment");
        checkTokensRegular("a/**/", "a");
        checkTokensRegular("/**/b", "b");
        checkTokensRegular("a/**/b", "a b");
        checkTokensRegular("a/* -- \n* /* **/b", "a b");
    }

    public void testSingleQuotes() {
        assertInvalidSql("'", "Unterminated quote");
        assertInvalidSql("a'", "Unterminated quote");
        assertInvalidSql("a'''", "Unterminated quote");
        assertInvalidSql("a''' ", "Unterminated quote");
        checkTokens("''", Pair.create(SqliteTokenFinder.TYPE_IN_SINGLE_QUOTES, ""));

        // 2 consecutive quotes inside quotes stands for a quote. e.g.'let''s go' -> let's go
        checkTokens(
                "''''",
                Pair.create(SqliteTokenFinder.TYPE_IN_SINGLE_QUOTES, "\'"));
        checkTokens(
                "a''''b",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_SINGLE_QUOTES, "\'"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
        checkTokens(
                "a' '' 'b",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_SINGLE_QUOTES, " \' "),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
        checkTokens("'abc'", Pair.create(SqliteTokenFinder.TYPE_IN_SINGLE_QUOTES, "abc"));
        checkTokens("'abc\ndef'", Pair.create(SqliteTokenFinder.TYPE_IN_SINGLE_QUOTES, "abc\ndef"));
        checkTokens(
                "a'abc\ndef'",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_SINGLE_QUOTES, "abc\ndef"));
        checkTokens(
                "'abc\ndef'b",
                Pair.create(SqliteTokenFinder.TYPE_IN_SINGLE_QUOTES, "abc\ndef"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
        checkTokens("a'abc\ndef'b",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_SINGLE_QUOTES, "abc\ndef"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
        checkTokens(
                "a'''abc\nd''ef'''b",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_SINGLE_QUOTES, "\'abc\nd\'ef\'"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
    }

    public void testDoubleQuotes() {
        assertInvalidSql("\"", "Unterminated quote");
        assertInvalidSql("a\"", "Unterminated quote");
        assertInvalidSql("a\"\"\"", "Unterminated quote");
        assertInvalidSql("a\"\"\" ", "Unterminated quote");
        checkTokens("\"\"", Pair.create(SqliteTokenFinder.TYPE_IN_DOUBLE_QUOTES, ""));
        checkTokens("\"\"\"\"", Pair.create(SqliteTokenFinder.TYPE_IN_DOUBLE_QUOTES, "\""));
        checkTokens(
                "a\"\"\"\"b",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_DOUBLE_QUOTES, "\""),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
        checkTokens("a\"\t\"\"\t\"b",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_DOUBLE_QUOTES, "\t\"\t"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
        checkTokens("\"abc\"", Pair.create(SqliteTokenFinder.TYPE_IN_DOUBLE_QUOTES, "abc"));
        checkTokens(
                "\"abc\ndef\"",
                Pair.create(SqliteTokenFinder.TYPE_IN_DOUBLE_QUOTES, "abc\ndef"));
        checkTokens(
                "a\"abc\ndef\"",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_DOUBLE_QUOTES, "abc\ndef"));
        checkTokens(
                "\"abc\ndef\"b",
                Pair.create(SqliteTokenFinder.TYPE_IN_DOUBLE_QUOTES, "abc\ndef"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
        checkTokens("a\"abc\ndef\"b",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_DOUBLE_QUOTES, "abc\ndef"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
        checkTokens("a\"\"\"abc\nd\"\"ef\"\"\"b",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_DOUBLE_QUOTES, "\"abc\nd\"ef\""),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
    }

    public void testBackquotes() {
        assertInvalidSql("`", "Unterminated quote");
        assertInvalidSql("a`", "Unterminated quote");
        assertInvalidSql("a```", "Unterminated quote");
        assertInvalidSql("a``` ", "Unterminated quote");
        checkTokens("``", Pair.create(SqliteTokenFinder.TYPE_IN_BACKQUOTES, ""));
        checkTokens("````", Pair.create(SqliteTokenFinder.TYPE_IN_BACKQUOTES, "`"));
        checkTokens(
                "a````b",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_BACKQUOTES, "`"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
        checkTokens(
                "a`\t``\t`b",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_BACKQUOTES, "\t`\t"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
        checkTokens("`abc`", Pair.create(SqliteTokenFinder.TYPE_IN_BACKQUOTES, "abc"));
        checkTokens(
                "`abc\ndef`",
                Pair.create(SqliteTokenFinder.TYPE_IN_BACKQUOTES, "abc\ndef"));
        checkTokens(
                "a`abc\ndef`",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_BACKQUOTES, "abc\ndef"));
        checkTokens(
                "`abc\ndef`b",
                Pair.create(SqliteTokenFinder.TYPE_IN_BACKQUOTES, "abc\ndef"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
        checkTokens(
                "a`abc\ndef`b",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_BACKQUOTES, "abc\ndef"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
        checkTokens(
                "a```abc\nd``ef```b",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_BACKQUOTES, "`abc\nd`ef`"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
    }

    public void testBrackets() {
        assertInvalidSql("[", "Unterminated quote");
        assertInvalidSql("a[", "Unterminated quote");
        assertInvalidSql("a[ ", "Unterminated quote");
        assertInvalidSql("a[[ ", "Unterminated quote");
        checkTokens("[]", Pair.create(SqliteTokenFinder.TYPE_IN_BRACKETS, ""));
        checkTokens("[[]", Pair.create(SqliteTokenFinder.TYPE_IN_BRACKETS, "["));
        checkTokens(
                "a[[]b",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_BRACKETS, "["),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
        checkTokens(
                "a[\t[\t]b",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_BRACKETS, "\t[\t"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
        checkTokens("[abc]", Pair.create(SqliteTokenFinder.TYPE_IN_BRACKETS, "abc"));
        checkTokens(
                "[abc\ndef]",
                Pair.create(SqliteTokenFinder.TYPE_IN_BRACKETS, "abc\ndef"));
        checkTokens(
                "a[abc\ndef]",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_BRACKETS, "abc\ndef"));
        checkTokens(
                "[abc\ndef]b",
                Pair.create(SqliteTokenFinder.TYPE_IN_BRACKETS, "abc\ndef"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
        checkTokens(
                "a[abc\ndef]b",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_BRACKETS, "abc\ndef"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
        checkTokens(
                "a[[abc\nd[ef[]b",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_IN_BRACKETS, "[abc\nd[ef["),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "b"));
    }

    public void testTokens() {
        checkTokensRegular("a,abc,a00b,_1,_123,abcdef", "a abc a00b _1 _123 abcdef");
        checkTokens(
                "a--\nabc/**/a00b''_1'''ABC'''`_123`abc[d]\"e\"f",
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "abc"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "a00b"),
                Pair.create(SqliteTokenFinder.TYPE_IN_SINGLE_QUOTES, ""),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "_1"),
                Pair.create(SqliteTokenFinder.TYPE_IN_SINGLE_QUOTES, "'ABC'"),
                Pair.create(SqliteTokenFinder.TYPE_IN_BACKQUOTES, "_123"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "abc"),
                Pair.create(SqliteTokenFinder.TYPE_IN_BRACKETS, "d"),
                Pair.create(SqliteTokenFinder.TYPE_IN_DOUBLE_QUOTES, "e"),
                Pair.create(SqliteTokenFinder.TYPE_REGULAR, "f"));
    }
}
