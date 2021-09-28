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

package android.util.proto.cts;

import static android.util.proto.ProtoOutputStream.FIELD_COUNT_SINGLE;
import static android.util.proto.ProtoOutputStream.FIELD_COUNT_REPEATED;
import static android.util.proto.ProtoOutputStream.FIELD_COUNT_PACKED;
import static android.util.proto.ProtoOutputStream.FIELD_TYPE_DOUBLE;
import static android.util.proto.ProtoOutputStream.FIELD_TYPE_FLOAT;
import static android.util.proto.ProtoOutputStream.FIELD_TYPE_INT64;
import static android.util.proto.ProtoOutputStream.FIELD_TYPE_UINT64;
import static android.util.proto.ProtoOutputStream.FIELD_TYPE_INT32;
import static android.util.proto.ProtoOutputStream.FIELD_TYPE_FIXED64;
import static android.util.proto.ProtoOutputStream.FIELD_TYPE_FIXED32;
import static android.util.proto.ProtoOutputStream.FIELD_TYPE_BOOL;
import static android.util.proto.ProtoOutputStream.FIELD_TYPE_STRING;
import static android.util.proto.ProtoOutputStream.FIELD_TYPE_MESSAGE;
import static android.util.proto.ProtoOutputStream.FIELD_TYPE_BYTES;
import static android.util.proto.ProtoOutputStream.FIELD_TYPE_UINT32;
import static android.util.proto.ProtoOutputStream.FIELD_TYPE_ENUM;
import static android.util.proto.ProtoOutputStream.FIELD_TYPE_SFIXED32;
import static android.util.proto.ProtoOutputStream.FIELD_TYPE_SFIXED64;
import static android.util.proto.ProtoOutputStream.FIELD_TYPE_SINT32;
import static android.util.proto.ProtoOutputStream.FIELD_TYPE_SINT64;
import static android.util.proto.ProtoOutputStream.WIRE_TYPE_VARINT;
import static android.util.proto.ProtoOutputStream.WIRE_TYPE_FIXED64;
import static android.util.proto.ProtoOutputStream.WIRE_TYPE_LENGTH_DELIMITED;
import static android.util.proto.ProtoOutputStream.WIRE_TYPE_START_GROUP;
import static android.util.proto.ProtoOutputStream.WIRE_TYPE_END_GROUP;
import static android.util.proto.ProtoOutputStream.WIRE_TYPE_FIXED32;


import android.util.proto.ProtoOutputStream;

import junit.framework.TestCase;
import org.junit.Assert;

/**
 * Test the debugging methods on the ProtoOutputStream class.
 */
public class DebuggingTest extends TestCase {

    public void testGetWireTypeString() throws Exception {
        Assert.assertEquals("Varint",
                ProtoOutputStream.getWireTypeString(WIRE_TYPE_VARINT));
        Assert.assertEquals("Fixed64",
                ProtoOutputStream.getWireTypeString(WIRE_TYPE_FIXED64));
        Assert.assertEquals("Length Delimited",
                ProtoOutputStream.getWireTypeString(WIRE_TYPE_LENGTH_DELIMITED));
        Assert.assertEquals("Start Group",
                ProtoOutputStream.getWireTypeString(WIRE_TYPE_START_GROUP));
        Assert.assertEquals("End Group",
                ProtoOutputStream.getWireTypeString(WIRE_TYPE_END_GROUP));
        Assert.assertEquals("Fixed32",
                ProtoOutputStream.getWireTypeString(WIRE_TYPE_FIXED32));
    }

    public void testGetFieldCountString() throws Exception {
        Assert.assertEquals("",
                ProtoOutputStream.getFieldCountString(FIELD_COUNT_SINGLE));
        Assert.assertEquals("Repeated",
                ProtoOutputStream.getFieldCountString(FIELD_COUNT_REPEATED));
        Assert.assertEquals("Packed",
                ProtoOutputStream.getFieldCountString(FIELD_COUNT_PACKED));
    }

    public void testGetFieldTypeString() throws Exception {
        Assert.assertEquals("Double",
                ProtoOutputStream.getFieldTypeString(FIELD_TYPE_DOUBLE));
        Assert.assertEquals("Float",
                ProtoOutputStream.getFieldTypeString(FIELD_TYPE_FLOAT));
        Assert.assertEquals("Int64",
                ProtoOutputStream.getFieldTypeString(FIELD_TYPE_INT64));
        Assert.assertEquals("UInt64",
                ProtoOutputStream.getFieldTypeString(FIELD_TYPE_UINT64));
        Assert.assertEquals("Int32",
                ProtoOutputStream.getFieldTypeString(FIELD_TYPE_INT32));
        Assert.assertEquals("Fixed64",
                ProtoOutputStream.getFieldTypeString(FIELD_TYPE_FIXED64));
        Assert.assertEquals("Fixed32",
                ProtoOutputStream.getFieldTypeString(FIELD_TYPE_FIXED32));
        Assert.assertEquals("Bool",
                ProtoOutputStream.getFieldTypeString(FIELD_TYPE_BOOL));
        Assert.assertEquals("String",
                ProtoOutputStream.getFieldTypeString(FIELD_TYPE_STRING));
        Assert.assertEquals("Message",
                ProtoOutputStream.getFieldTypeString(FIELD_TYPE_MESSAGE));
        Assert.assertEquals("Bytes",
                ProtoOutputStream.getFieldTypeString(FIELD_TYPE_BYTES));
        Assert.assertEquals("UInt32",
                ProtoOutputStream.getFieldTypeString(FIELD_TYPE_UINT32));
        Assert.assertEquals("Enum",
                ProtoOutputStream.getFieldTypeString(FIELD_TYPE_ENUM));
        Assert.assertEquals("SFixed32",
                ProtoOutputStream.getFieldTypeString(FIELD_TYPE_SFIXED32));
        Assert.assertEquals("SFixed64",
                ProtoOutputStream.getFieldTypeString(FIELD_TYPE_SFIXED64));
        Assert.assertEquals("SInt32",
                ProtoOutputStream.getFieldTypeString(FIELD_TYPE_SINT32));
        Assert.assertEquals("SInt64",
                ProtoOutputStream.getFieldTypeString(FIELD_TYPE_SINT64));
    }

    private void assertContains(String haystack, String needle) throws Exception {
        if (!haystack.contains(needle)) {
            throw new Exception("Assertion failed: Didn't find '" + needle + "' in: " + haystack);
        }
    }

    private void checkFieldIdString(long count, long type) throws Exception {
        final String str = ProtoOutputStream.getFieldIdString(
                count | type | (((long) 123) & 0x0ffffffffL));
        assertContains(str, ProtoOutputStream.getFieldCountString(count));
        assertContains(str, ProtoOutputStream.getFieldTypeString(type));
    }

    public void testGetFieldIdString() throws Exception {
        checkFieldIdString(FIELD_COUNT_REPEATED, FIELD_TYPE_INT32);
        checkFieldIdString(FIELD_COUNT_PACKED, FIELD_TYPE_INT32);

        checkFieldIdString(FIELD_COUNT_SINGLE, FIELD_TYPE_DOUBLE);
        checkFieldIdString(FIELD_COUNT_SINGLE, FIELD_TYPE_BYTES);
    }

    public void testMakeToken() {
        Assert.assertEquals(0x07L << 61, ProtoOutputStream.makeToken(0xffffffff, false, 0, 0, 0));
        Assert.assertEquals(1L << 60, ProtoOutputStream.makeToken(0, true, 0, 0, 0));
        Assert.assertEquals(0x01ffL << 51, ProtoOutputStream.makeToken(0, false, 0xffffffff, 0, 0));
        Assert.assertEquals(0x07ffffL << 32,
                ProtoOutputStream.makeToken(0, false, 0, 0xffffffff, 0));
        Assert.assertEquals(0x0ffffffffL, ProtoOutputStream.makeToken(0, false, 0, 0, 0xffffffff));
    }

    public void testToken2String() {
        // This function is only for debugging. Just check that it doesn't crash
        ProtoOutputStream.token2String(ProtoOutputStream.makeToken(0xffffffff, false, 0, 0, 0));
        ProtoOutputStream.token2String(0L);
    }
}
