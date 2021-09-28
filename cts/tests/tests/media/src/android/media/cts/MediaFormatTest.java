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

package android.media.cts;

import android.media.MediaFormat;
import android.test.AndroidTestCase;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

public class MediaFormatTest extends AndroidTestCase {
    private static ByteBuffer defaultByteBuffer = ByteBuffer.allocateDirect(16);

    private void assertGetByteBuffersThrowClassCastException(
            MediaFormat format, String key, String type) {
        try {
            ByteBuffer value = format.getByteBuffer(key);
            throw new AssertionError("read " + type + " as ByteBuffer " + value);
        } catch (ClassCastException e) {
        }

        try {
            ByteBuffer value = format.getByteBuffer(key, defaultByteBuffer);
            throw new AssertionError("read " + type + " with default as ByteBuffer " + value);
        } catch (ClassCastException e) {
        }
    }

    private void assertGetFloatsThrowClassCastException(
            MediaFormat format, String key, String type) {
        try {
            float value = format.getFloat(key);
            throw new AssertionError("read " + type + " as float " + value);
        } catch (ClassCastException e) {
        }

        try {
            float value = format.getFloat(key, 321.f);
            throw new AssertionError("read " + type + " with default as float " + value);
        } catch (ClassCastException e) {
        }
    }

    private void assertGetIntegersThrowClassCastException(
            MediaFormat format, String key, String type) {
        try {
            int value = format.getInteger(key);
            throw new AssertionError("read " + type + " as int " + value);
        } catch (ClassCastException e) {
        }

        try {
            int value = format.getInteger(key, 123);
            throw new AssertionError("read " + type + " with default as int " + value);
        } catch (ClassCastException e) {
        }
    }

    private void assertGetLongsThrowClassCastException(
            MediaFormat format, String key, String type) {
        try {
            long value = format.getLong(key);
            throw new AssertionError("read " + type + " as long " + value);
        } catch (ClassCastException e) {
        }

        try {
            long value = format.getLong(key, 321L);
            throw new AssertionError("read " + type + " with default as long " + value);
        } catch (ClassCastException e) {
        }
    }

    private void assertGetNumbersThrowClassCastException(
            MediaFormat format, String key, String type) {
        try {
            Number value = format.getNumber(key);
            throw new AssertionError("read " + type + " as Number " + value);
        } catch (ClassCastException e) {
        }

        try {
            Number value = format.getNumber(key, 321.f);
            throw new AssertionError("read " + type + " with default as Number " + value);
        } catch (ClassCastException e) {
        }
    }

    private void assertGetStringsThrowClassCastException(
            MediaFormat format, String key, String type) {
        try {
            String value = format.getString(key);
            throw new AssertionError("read " + type + " as string " + value);
        } catch (ClassCastException e) {
        }

        try {
            String value = format.getString(key, "321");
            throw new AssertionError("read " + type + " with default as string " + value);
        } catch (ClassCastException e) {
        }
    }

    public void testIntegerValue() throws Exception {
        MediaFormat format = new MediaFormat();
        format.setInteger("int", 123);

        assertEquals(123, format.getInteger("int"));

        // We should be able to read int values as numbers.
        assertEquals(123, format.getNumber("int").intValue());
        assertEquals(123, format.getNumber("int", 321).intValue());

        // We should not be able to get an integer value as any other type. Instead,
        // we should receive a ClassCastException
        assertGetByteBuffersThrowClassCastException(format, "int", "int");
        assertGetFloatsThrowClassCastException(format, "int", "int");
        assertGetLongsThrowClassCastException(format, "int", "int");
        assertGetStringsThrowClassCastException(format, "int", "int");

        // We should not have a feature enabled for an integer value
        try {
            boolean value = format.getFeatureEnabled("int");
            throw new AssertionError("read int as feature " + value);
        } catch (IllegalArgumentException e) {
        }

        testSingleKeyRemoval(format, "int", MediaFormat.TYPE_INTEGER);
    }

    public void testLongValue() throws Exception {
        MediaFormat format = new MediaFormat();
        format.setLong("long", 9876543210L);

        assertEquals(9876543210L, format.getLong("long"));

        // We should be able to read long values as numbers.
        assertEquals(9876543210L, format.getNumber("long").longValue());
        assertEquals(9876543210L, format.getNumber("long", 9012345678L).longValue());

        // We should not be able to get a long value as any other type. Instead,
        // we should receive a ClassCastException
        assertGetByteBuffersThrowClassCastException(format, "long", "long");
        assertGetFloatsThrowClassCastException(format, "long", "long");
        assertGetIntegersThrowClassCastException(format, "long", "long");
        assertGetStringsThrowClassCastException(format, "long", "long");

        // We should not have a feature enabled for a long value
        try {
            boolean value = format.getFeatureEnabled("long");
            throw new AssertionError("read long as feature " + value);
        } catch (IllegalArgumentException e) {
        }

        testSingleKeyRemoval(format, "long", MediaFormat.TYPE_LONG);
    }

    public void testFloatValue() throws Exception {
        MediaFormat format = new MediaFormat();
        format.setFloat("float", 3.14f);

        assertEquals(3.14f, format.getFloat("float"));

        // We should be able to read float values as numbers.
        assertEquals(3.14f, format.getNumber("float").floatValue());
        assertEquals(3.14f, format.getNumber("float", 2.81f).floatValue());

        // We should not be able to get a float value as any other type. Instead,
        // we should receive a ClassCastException
        assertGetByteBuffersThrowClassCastException(format, "float", "float");
        assertGetIntegersThrowClassCastException(format, "float", "float");
        assertGetLongsThrowClassCastException(format, "float", "float");
        assertGetStringsThrowClassCastException(format, "float", "float");

        // We should not have a feature enabled for a float value
        try {
            boolean value = format.getFeatureEnabled("float");
            throw new AssertionError("read float as feature " + value);
        } catch (IllegalArgumentException e) {
        }

        testSingleKeyRemoval(format, "float", MediaFormat.TYPE_FLOAT);
    }

    public void testStringValue() throws Exception {
        MediaFormat format = new MediaFormat();
        format.setString("string", "value");

        assertEquals("value", format.getString("string"));

        // We should not be able to get a string value as any other type. Instead,
        // we should receive a ClassCastException
        assertGetByteBuffersThrowClassCastException(format, "string", "string");
        assertGetFloatsThrowClassCastException(format, "string", "string");
        assertGetIntegersThrowClassCastException(format, "string", "string");
        assertGetLongsThrowClassCastException(format, "string", "string");
        assertGetNumbersThrowClassCastException(format, "string", "string");

        // We should not have a feature enabled for an integer value
        try {
            boolean value = format.getFeatureEnabled("string");
            throw new AssertionError("read string as feature " + value);
        } catch (IllegalArgumentException e) {
        }

        testSingleKeyRemoval(format, "string", MediaFormat.TYPE_STRING);
    }

    public void testByteBufferValue() throws Exception {
        MediaFormat format = new MediaFormat();
        ByteBuffer buffer = ByteBuffer.allocateDirect(123);
        format.setByteBuffer("buffer", buffer);

        assertEquals(buffer, format.getByteBuffer("buffer"));

        // We should not be able to get a string value as any other type. Instead,
        // we should receive a ClassCastException
        assertGetFloatsThrowClassCastException(format, "buffer", "ByteBuffer");
        assertGetIntegersThrowClassCastException(format, "buffer", "ByteBuffer");
        assertGetLongsThrowClassCastException(format, "buffer", "ByteBuffer");
        assertGetNumbersThrowClassCastException(format, "buffer", "ByteBuffer");
        assertGetStringsThrowClassCastException(format, "buffer", "ByteBuffer");

        // We should not have a feature enabled for an integer value
        try {
            boolean value = format.getFeatureEnabled("buffer");
            throw new AssertionError("read ByteBuffer as feature " + value);
        } catch (IllegalArgumentException e) {
        }

        testSingleKeyRemoval(format, "buffer", MediaFormat.TYPE_BYTE_BUFFER);
    }

    public void testNullStringValue() throws Exception {
        MediaFormat format = new MediaFormat();
        format.setString("null", null);
        testNullOrMissingValue(format, "null");
        testSingleKeyRemoval(format, "null", MediaFormat.TYPE_NULL);
    }

    public void testNullByteBufferValue() throws Exception {
        MediaFormat format = new MediaFormat();
        format.setByteBuffer("null", null);
        testNullOrMissingValue(format, "null");
        testSingleKeyRemoval(format, "null", MediaFormat.TYPE_NULL);
    }

    public void testMissingValue() throws Exception {
        MediaFormat format = new MediaFormat();
        // null values should be handled the same as missing values
        assertEquals(MediaFormat.TYPE_NULL, format.getValueTypeForKey("missing"));
        testNullOrMissingValue(format, "missing");
    }

    private void testSingleKeyRemoval(
            MediaFormat format, String key, @MediaFormat.Type int type) {
        assertEquals(type, format.getValueTypeForKey(key));
        assertTrue(format.containsKey(key));

        Set<String> keySet = format.getKeys();
        assertEquals(1, keySet.size());
        assertTrue(keySet.contains(key));

        format.removeKey(key);
        assertFalse(format.containsKey(key));

        // test that keySet is connected to the format
        assertFalse(keySet.contains(key));
        assertEquals(0, keySet.size());
    }

    private static Set<String> asSet(String ... values) {
        return new HashSet<>(Arrays.asList(values));
    }

    private void assertStringSetEquals(Set<String> set, String ... expected_members) {
        Set<String> expected = new HashSet<>(Arrays.asList(expected_members));
        assertEquals(expected, set);
    }

    public void testKeySetContainsAndRemove() {
        MediaFormat format = new MediaFormat();
        format.setInteger("int", 123);
        format.setLong("long", 9876543210L);
        format.setFloat("float", 321.f);
        format.setFeatureEnabled("int", true);
        format.setFeatureEnabled("long", false);
        format.setFeatureEnabled("float", true);
        format.setFeatureEnabled("string", false);

        Set<String> keySet = format.getKeys();
        // test size and contains
        assertEquals(3, keySet.size());
        assertTrue(keySet.contains("int"));
        assertTrue(keySet.contains("long"));
        assertTrue(keySet.contains("float"));
        assertFalse(keySet.contains("string"));
        assertStringSetEquals(keySet, "int", "long", "float");

        // test adding an element
        format.setString("string", "value");

        // test that key set reflects format change in size and contains
        assertEquals(4, keySet.size());
        assertTrue(keySet.contains("int"));
        assertTrue(keySet.contains("long"));
        assertTrue(keySet.contains("float"));
        assertTrue(keySet.contains("string"));

        // test iterator
        {
            Set<String> toFind = asSet("int", "long", "float", "string");
            Iterator<String> it = keySet.iterator();
            while (it.hasNext()) {
                String k = it.next();
                assertTrue(toFind.remove(k));
            }
            assertEquals(0, toFind.size());
        }

        // remove via format
        format.removeKey("float");

        // test that key set reflects format change in size and contains
        assertEquals(3, keySet.size());
        assertTrue(keySet.contains("int"));
        assertTrue(keySet.contains("long"));
        assertFalse(keySet.contains("float"));
        assertTrue(keySet.contains("string"));

        // re-test iterator after removal
        {
            Set<String> toFind = asSet("int", "long", "string");
            for (String k : keySet) {
                assertTrue(toFind.remove(k));
            }
            assertEquals(0, toFind.size());
        }

        // test remove
        keySet.remove("long");
        assertEquals(2, keySet.size());
        assertTrue(keySet.contains("int"));
        assertFalse(keySet.contains("long"));
        assertFalse(keySet.contains("float"));
        assertTrue(keySet.contains("string"));
        assertTrue(format.containsKey("int"));
        assertFalse(format.containsKey("long"));
        assertFalse(format.containsKey("float"));
        assertTrue(format.containsKey("string"));

        // test iterator by its interface as well as its remove
        {
            Set<String> toFind = asSet("int", "string");
            Iterator<String> it = keySet.iterator();
            while (it.hasNext()) {
                String k = it.next();
                assertTrue(toFind.remove(k));
                if (k.equals("int")) {
                    it.remove();
                }
            }
            assertEquals(0, toFind.size());
        }

        // test that removing via iterator also removes from format
        assertEquals(1, keySet.size());
        assertFalse(keySet.contains("int"));
        assertFalse(keySet.contains("long"));
        assertFalse(keySet.contains("float"));
        assertTrue(keySet.contains("string"));
        assertFalse(format.containsKey("int"));
        assertFalse(format.containsKey("long"));
        assertFalse(format.containsKey("float"));
        assertTrue(format.containsKey("string"));

        // verify that features are still present
        assertTrue(format.getFeatureEnabled("int"));
        assertFalse(format.getFeatureEnabled("long"));
        assertTrue(format.getFeatureEnabled("float"));
        assertFalse(format.getFeatureEnabled("string"));
    }

    public void testFeatureKeySetContainsAndRemove() {
        MediaFormat format = new MediaFormat();
        format.setInteger("int", 123);
        format.setLong("long", 9876543210L);
        format.setFloat("float", 321.f);
        format.setString("string", "value");
        format.setFeatureEnabled("int", true);
        format.setFeatureEnabled("long", false);
        format.setFeatureEnabled("float", true);

        Set<String> featureSet = format.getFeatures();
        // test size and contains
        assertEquals(3, featureSet.size());
        assertTrue(featureSet.contains("int"));
        assertTrue(featureSet.contains("long"));
        assertTrue(featureSet.contains("float"));
        assertFalse(featureSet.contains("string"));
        assertStringSetEquals(featureSet, "int", "long", "float");

        // test adding an element
        format.setFeatureEnabled("string", false);

        // test that key set reflects format change in size and contains
        assertEquals(4, featureSet.size());
        assertTrue(featureSet.contains("int"));
        assertTrue(featureSet.contains("long"));
        assertTrue(featureSet.contains("float"));
        assertTrue(featureSet.contains("string"));

        // test iterator
        {
            Set<String> toFind = asSet("int", "long", "float", "string");
            Iterator<String> it = featureSet.iterator();
            while (it.hasNext()) {
                String k = it.next();
                assertTrue(toFind.remove(k));
            }
            assertEquals(0, toFind.size());
        }

        // test that features cannot be removed via format as keys, even though for backward
        // compatibility, they can be accessed as integers and can be found via containsKey
        format.removeKey("feature-float");
        assertEquals(4, featureSet.size());
        assertTrue(featureSet.contains("float"));

        format.removeFeature("float");

        // TODO: deprecate this usage
        assertEquals(1, format.getInteger("feature-int"));
        assertEquals(0, format.getInteger("feature-long"));
        assertTrue(format.containsKey("feature-int"));

        // Along these lines also verify that this is not true for the getKeys() set
        assertFalse(format.getKeys().contains("feature-int"));

        // Also verify that getKeys() cannot be used to remove a feature
        assertFalse(format.getKeys().remove("feature-int"));

        // test that key set reflects format change in size and contains
        assertEquals(3, featureSet.size());
        assertTrue(featureSet.contains("int"));
        assertTrue(featureSet.contains("long"));
        assertFalse(featureSet.contains("float"));
        assertTrue(featureSet.contains("string"));

        // re-test iterator after removal
        {
            Set<String> toFind = asSet("int", "long", "string");
            for (String k : featureSet) {
                assertTrue(toFind.remove(k));
            }
            assertEquals(0, toFind.size());
        }

        // test remove via set
        featureSet.remove("long");
        assertEquals(2, featureSet.size());
        assertTrue(featureSet.contains("int"));
        assertFalse(featureSet.contains("long"));
        assertFalse(featureSet.contains("float"));
        assertTrue(featureSet.contains("string"));

        assertTrue(format.containsFeature("int"));
        assertFalse(format.containsFeature("long"));
        assertFalse(format.containsFeature("float"));
        assertTrue(format.containsFeature("string"));

        assertTrue(format.getFeatureEnabled("int"));
        try {
            format.getFeatureEnabled("long");
            fail("should not contain feature long");
        } catch (IllegalArgumentException e) {}
        try {
            format.getFeatureEnabled("float");
            fail("should not contain feature float");
        } catch (IllegalArgumentException e) {}
        assertFalse(format.getFeatureEnabled("string"));

        // test iterator by its interface as well as its remove
        {
            Set<String> toFind = asSet("int", "string");
            Iterator<String> it = featureSet.iterator();
            while (it.hasNext()) {
                String k = it.next();
                assertTrue(toFind.remove(k));
                if (k.equals("int")) {
                    it.remove();
                }
            }
            assertEquals(0, toFind.size());
        }

        // test that removing via iterator also removes from format
        assertEquals(1, featureSet.size());
        assertFalse(featureSet.contains("int"));
        assertFalse(featureSet.contains("long"));
        assertFalse(featureSet.contains("float"));
        assertTrue(featureSet.contains("string"));

        assertFalse(format.containsFeature("int"));
        assertFalse(format.containsFeature("long"));
        assertFalse(format.containsFeature("float"));
        assertTrue(format.containsFeature("string"));

        try {
            format.getFeatureEnabled("int");
            fail("should not contain feature int");
        } catch (IllegalArgumentException e) {}
        try {
            format.getFeatureEnabled("long");
            fail("should not contain feature long");
        } catch (IllegalArgumentException e) {}
        try {
            format.getFeatureEnabled("float");
            fail("should not contain feature float");
        } catch (IllegalArgumentException e) {}
        assertFalse(format.getFeatureEnabled("string"));

        // verify that keys are still present
        assertEquals(123, format.getInteger("int"));
        assertEquals(9876543210L, format.getLong("long"));
        assertEquals(321.f, format.getFloat("float"));
        assertEquals("value", format.getString("string"));
    }

    private void testNullOrMissingValue(MediaFormat format, String key) throws Exception {
        // We should not be able to get a string value as any primitive type. Instead,
        // we should receive a NullPointerException
        try {
            int value = format.getInteger(key);
            throw new AssertionError("read " + key + " as int " + value);
        } catch (NullPointerException e) {
        }

        try {
            long value = format.getLong(key);
            throw new AssertionError("read " + key + " as long " + value);
        } catch (NullPointerException e) {
        }

        try {
            float value = format.getFloat(key);
            throw new AssertionError("read " + key + " as float " + value);
        } catch (NullPointerException e) {
        }

        // We should get null for all object types (ByteBuffer, Number, String)
        assertNull(format.getNumber(key));
        assertNull(format.getString(key));
        assertNull(format.getByteBuffer(key));

        // We should get the default value for all getters with default
        assertEquals(123, format.getInteger(key, 123));
        assertEquals(321L, format.getLong(key, 321L));
        assertEquals(321.f, format.getFloat(key, 321.f));
        assertEquals(321.f, format.getNumber(key, 321.f));
        assertEquals("value", format.getString(key, "value"));
        assertEquals(defaultByteBuffer, format.getByteBuffer(key, defaultByteBuffer));

        // We should not have a feature enabled for a null value
        try {
            boolean value = format.getFeatureEnabled(key);
            throw new AssertionError("read " + key + " as feature " + value);
        } catch (IllegalArgumentException e) {
        }
    }

    public void testMediaFormatConstructors() {
        MediaFormat format;
        {
            format = MediaFormat.createAudioFormat(MediaFormat.MIMETYPE_AUDIO_AAC, 48000, 6);
            assertEquals(MediaFormat.MIMETYPE_AUDIO_AAC, format.getString(MediaFormat.KEY_MIME));
            assertEquals(48000, format.getInteger(MediaFormat.KEY_SAMPLE_RATE));
            assertEquals(6, format.getInteger(MediaFormat.KEY_CHANNEL_COUNT));
            assertEquals(3, format.getKeys().size());
            assertEquals(0, format.getFeatures().size());
        }

        {
            format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, 1920, 1080);
            assertEquals(MediaFormat.MIMETYPE_VIDEO_AVC, format.getString(MediaFormat.KEY_MIME));
            assertEquals(1920, format.getInteger(MediaFormat.KEY_WIDTH));
            assertEquals(1080, format.getInteger(MediaFormat.KEY_HEIGHT));
            assertEquals(3, format.getKeys().size());
            assertEquals(0, format.getFeatures().size());
        }

        {
            format = MediaFormat.createSubtitleFormat(MediaFormat.MIMETYPE_TEXT_VTT, "und");
            assertEquals(MediaFormat.MIMETYPE_TEXT_VTT, format.getString(MediaFormat.KEY_MIME));
            assertEquals("und", format.getString(MediaFormat.KEY_LANGUAGE));
            assertEquals(2, format.getKeys().size());
            assertEquals(0, format.getFeatures().size());

            format.setFeatureEnabled("feature1", false);

            // also test dup
            MediaFormat other = new MediaFormat(format);
            format.setString(MediaFormat.KEY_LANGUAGE, "un");
            other.setInteger(MediaFormat.KEY_IS_DEFAULT, 1);
            other.setFeatureEnabled("feature1", true);

            assertEquals(MediaFormat.MIMETYPE_TEXT_VTT, format.getString(MediaFormat.KEY_MIME));
            assertEquals("un", format.getString(MediaFormat.KEY_LANGUAGE));
            assertEquals(2, format.getKeys().size());
            assertFalse(format.getFeatureEnabled("feature1"));
            assertEquals(1, format.getFeatures().size());

            assertEquals(MediaFormat.MIMETYPE_TEXT_VTT, other.getString(MediaFormat.KEY_MIME));
            assertEquals("und", other.getString(MediaFormat.KEY_LANGUAGE));
            assertEquals(1, other.getInteger(MediaFormat.KEY_IS_DEFAULT));
            assertEquals(3, other.getKeys().size());
            assertTrue(other.getFeatureEnabled("feature1"));
            assertEquals(1, other.getFeatures().size());
        }
    }
}
