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
 * limitations under the License
 */

package com.android.providers.tv.util;


import android.annotation.Nullable;

import android.util.Pair;
import java.util.function.Consumer;

/**
 * Simple SQL parser to check statements for usage of prohibited/sensitive fields. Modified from
 * packages/providers/ContactsProvider/src/com/android/providers/contacts/sqlite/SqlChecker.java
 */
public class SqliteTokenFinder {
    public static final int TYPE_REGULAR = 0;
    public static final int TYPE_IN_SINGLE_QUOTES = 1;
    public static final int TYPE_IN_DOUBLE_QUOTES = 2;
    public static final int TYPE_IN_BACKQUOTES = 3;
    public static final int TYPE_IN_BRACKETS = 4;

    private static boolean isAlpha(char ch) {
        return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z') || (ch == '_');
    }

    private static boolean isNum(char ch) {
        return ('0' <= ch && ch <= '9');
    }

    private static boolean isAlNum(char ch) {
        return isAlpha(ch) || isNum(ch);
    }

    private static boolean isAnyOf(char ch, String set) {
        return set.indexOf(ch) >= 0;
    }

    private static char peek(String s, int index) {
        return index < s.length() ? s.charAt(index) : '\0';
    }

    /**
     * SQL Tokenizer specialized to extract tokens from SQL (snippets).
     *
     * Based on sqlite3GetToken() in tokenzie.c in SQLite.
     *
     * Source for v3.8.6 (which android uses): http://www.sqlite.org/src/artifact/ae45399d6252b4d7
     * (Latest source as of now: http://www.sqlite.org/src/artifact/78c8085bc7af1922)
     *
     * Also draft spec: http://www.sqlite.org/draft/tokenreq.html
     *
     * @param sql the SQL clause to be tokenized.
     * @param checker the {@link Consumer} to check each token. The input of the checker is a pair
     *                of token type and the token.
     */
    public static void findTokens(@Nullable String sql, Consumer<Pair<Integer, String>> checker) {
        if (sql == null) {
            return;
        }
        int pos = 0;
        final int len = sql.length();
        while (pos < len) {
            final char ch = peek(sql, pos);

            // Regular token.
            if (isAlpha(ch)) {
                final int start = pos;
                pos++;
                while (isAlNum(peek(sql, pos))) {
                    pos++;
                }
                final int end = pos;

                final String token = sql.substring(start, end);
                checker.accept(Pair.create(TYPE_REGULAR, token));

                continue;
            }

            // Handle quoted tokens
            if (isAnyOf(ch, "'\"`")) {
                final int quoteStart = pos;
                pos++;

                for (;;) {
                    pos = sql.indexOf(ch, pos);
                    if (pos < 0) {
                        throw new IllegalArgumentException("Unterminated quote in" + sql);
                    }
                    if (peek(sql, pos + 1) != ch) {
                        break;
                    }
                    // Quoted quote char -- e.g. "abc""def" is a single string.
                    pos += 2;
                }
                final int quoteEnd = pos;
                pos++;

                // Extract the token
                String token = sql.substring(quoteStart + 1, quoteEnd);
                // Unquote if needed. i.e. "aa""bb" -> aa"bb
                if (token.indexOf(ch) >= 0) {
                    token = token.replaceAll(String.valueOf(ch) + ch, String.valueOf(ch));
                }
                int type = TYPE_REGULAR;
                switch (ch) {
                    case '\'':
                        type = TYPE_IN_SINGLE_QUOTES;
                        break;
                    case '\"':
                        type = TYPE_IN_DOUBLE_QUOTES;
                        break;
                    case '`':
                        type = TYPE_IN_BACKQUOTES;
                        break;
                }
                checker.accept(Pair.create(type, token));
                continue;
            }
            // Handle tokens enclosed in [...]
            if (ch == '[') {
                final int quoteStart = pos;
                pos++;

                pos = sql.indexOf(']', pos);
                if (pos < 0) {
                    throw new IllegalArgumentException("Unterminated quote in" + sql);
                }
                final int quoteEnd = pos;
                pos++;

                final String token = sql.substring(quoteStart + 1, quoteEnd);

                checker.accept(Pair.create(TYPE_IN_BRACKETS, token));
                continue;
            }

            // Detect comments.
            if (ch == '-' && peek(sql, pos + 1) == '-') {
                pos += 2;
                pos = sql.indexOf('\n', pos);
                if (pos < 0) {
                    // strings ending in an inline comment.
                    break;
                }
                pos++;

                continue;
            }
            if (ch == '/' && peek(sql, pos + 1) == '*') {
                pos += 2;
                pos = sql.indexOf("*/", pos);
                if (pos < 0) {
                    throw new IllegalArgumentException("Unterminated comment in" + sql);
                }
                pos += 2;

                continue;
            }

            // For this purpose, we can simply ignore other characters.
            // (Note it doesn't handle the X'' literal properly and reports this X as a token,
            // but that should be fine...)
            pos++;
        }
    }
}
