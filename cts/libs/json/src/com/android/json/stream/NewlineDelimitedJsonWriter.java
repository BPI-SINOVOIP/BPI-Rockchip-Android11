/*
 * Copyright (C) 2010 The Android Open Source Project
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

package com.android.json.stream;

import java.io.IOException;
import java.io.Writer;

/**
 * Writes a JSON (<a href="http://www.ietf.org/rfc/rfc4627.txt">RFC 4627</a>) encoded value to a
 * stream, one token at a time. The stream includes both literal values (strings, numbers, booleans
 * and nulls) as well as the begin and end delimiters of objects and arrays.
 *
 * <h3>Encoding JSON</h3>
 *
 * To encode your data as JSON, create a new {@code JsonWriter}. Each JSON document must contain one
 * top-level array or object. Call methods on the writer as you walk the structure's contents,
 * nesting arrays and objects as necessary:
 *
 * <ul>
 *   <li>To write <strong>arrays</strong>, first call {@link #beginArray()}. Write each of the
 *       array's elements with the appropriate {@link #value} methods or by nesting other arrays and
 *       objects. Finally close the array using {@link #endArray()}.
 *   <li>To write <strong>objects</strong>, first call {@link #beginObject()}. Write each of the
 *       object's properties by alternating calls to {@link #name} with the property's value. Write
 *       property values with the appropriate {@link #value} method or by nesting other objects or
 *       arrays. Finally close the object using {@link #endObject()}.
 * </ul>
 *
 * <h3>Example</h3>
 *
 * Suppose we'd like to encode a stream of messages such as the following:
 *
 * <pre>{@code
 * [
 *   {
 *     "id": 912345678901,
 *     "text": "How do I write JSON on Android?",
 *     "geo": null,
 *     "user": {
 *       "name": "android_newb",
 *       "followers_count": 41
 *      }
 *   },
 *   {
 *     "id": 912345678902,
 *     "text": "@android_newb just use android.util.JsonWriter!",
 *     "geo": [50.454722, -104.606667],
 *     "user": {
 *       "name": "jesse",
 *       "followers_count": 2
 *     }
 *   }
 * ]
 * }</pre>
 *
 * This code encodes the above structure:
 *
 * <pre>{@code
 * public void writeJsonStream(OutputStream out, List<Message> messages) throws IOException {
 *   JsonWriter writer = new JsonWriter(new OutputStreamWriter(out, "UTF-8"));
 *   writer.setIndent("  ");
 *   writeMessagesArray(writer, messages);
 *   writer.close();
 * }
 *
 * public void writeMessagesArray(JsonWriter writer, List<Message> messages) throws IOException {
 *   writer.beginArray();
 *   for (Message message : messages) {
 *     writeMessage(writer, message);
 *   }
 *   writer.endArray();
 * }
 *
 * public void writeMessage(JsonWriter writer, Message message) throws IOException {
 *   writer.beginObject();
 *   writer.name("id").value(message.getId());
 *   writer.name("text").value(message.getText());
 *   if (message.getGeo() != null) {
 *     writer.name("geo");
 *     writeDoublesArray(writer, message.getGeo());
 *   } else {
 *     writer.name("geo").nullValue();
 *   }
 *   writer.name("user");
 *   writeUser(writer, message.getUser());
 *   writer.endObject();
 * }
 *
 * public void writeUser(JsonWriter writer, User user) throws IOException {
 *   writer.beginObject();
 *   writer.name("name").value(user.getName());
 *   writer.name("followers_count").value(user.getFollowersCount());
 *   writer.endObject();
 * }
 *
 * public void writeDoublesArray(JsonWriter writer, List<Double> doubles) throws IOException {
 *   writer.beginArray();
 *   for (Double value : doubles) {
 *     writer.value(value);
 *   }
 *   writer.endArray();
 * }
 * }</pre>
 *
 * <p>Each {@code JsonWriter} may be used to write a single JSON stream. Instances of this class are
 * not thread safe. Calls that would result in a malformed JSON string will fail with an {@link
 * IllegalStateException}.
 */
public class NewlineDelimitedJsonWriter extends JsonWriter {
    private static final String NEW_LINE = "\n";
    /**
     * Creates a new instance that writes a JSON-encoded stream to {@code out}. For best
     * performance, ensure {@link Writer} is buffered; wrapping in {@link java.io.BufferedWriter
     * BufferedWriter} if necessary.
     */
    public NewlineDelimitedJsonWriter(Writer out) {
        super(out);
    }

    /**
     * Sets the indentation string is no opt for newline-delimited JSON
     *
     * @param indent a string containing only whitespace.
     */
    @Override
    public void setIndent(String indent) {
        // no opt on Indent
        super.setIndent(null);
    }

    /** Sets newline-delimited for a JSON object. */
    public void newlineDelimited() throws IOException {
        mOut.write(NEW_LINE);
    }

    /**
     * Flushes and closes this writer and the underlying {@link Writer}.
     *
     * @throws IOException if the JSON document is incomplete.
     */
    public void close() throws IOException {
        mOut.close();
    }

    /**
     * Inserts any necessary separators and whitespace before a literal value, inline array, or
     * inline object. Also adjusts the stack to expect either a closing bracket or another element.
     *
     * @param root true if the value is a new array or object, the two values permitted as top-level
     *     elements.
     */
    @Override
    protected void beforeValue(boolean root) throws IOException {
        switch (peek()) {
            case EMPTY_DOCUMENT: // first in document
                if (!root) {
                    throw new IllegalStateException("JSON must start with an array or an object.");
                }
                break;

            case EMPTY_ARRAY: // first in array
                replaceTop(JsonScope.NONEMPTY_ARRAY);
                newline();
                break;

            case NONEMPTY_ARRAY: // another in array
                mOut.append(',');
                newline();
                break;

            case DANGLING_NAME: // value for name
                mOut.append(mSeparator);
                replaceTop(JsonScope.NONEMPTY_OBJECT);
                break;

            default:
                throw new IllegalStateException("Nesting problem: " + mStack);
        }
    }
}
