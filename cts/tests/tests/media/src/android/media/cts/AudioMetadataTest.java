/*
 * Copyright (C) 2020 The Android Open Source Project
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

import static org.junit.Assert.*;
import static org.testng.Assert.assertThrows;

import android.media.AudioFormat;
import android.media.AudioMetadata;
import android.media.AudioMetadataMap;
import android.media.AudioMetadataReadMap;
import android.util.Log;
import androidx.test.runner.AndroidJUnit4;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

@NonMediaMainlineTest
@RunWith(AndroidJUnit4.class)
public class AudioMetadataTest {

    // Trial keys to test with.
    private static final AudioMetadata.Key<Integer>
        KEY_INTEGER = AudioMetadata.createKey("integer", Integer.class);
    private static final AudioMetadata.Key<Number>
        KEY_NUMBER = AudioMetadata.createKey("number", Number.class);
    private static final AudioMetadata.Key<String>
        KEY_STRING = AudioMetadata.createKey("string", String.class);
    private static final AudioMetadata.Key<Long>
        KEY_LONG = AudioMetadata.createKey("long", Long.class);
    private static final AudioMetadata.Key<Float>
        KEY_FLOAT = AudioMetadata.createKey("float", Float.class);
    private static final AudioMetadata.Key<Double>
        KEY_DOUBLE = AudioMetadata.createKey("double", Double.class);
    private static final AudioMetadata.Key<AudioMetadata.BaseMap>
        KEY_BASE_MAP = AudioMetadata.createKey("data", AudioMetadata.BaseMap.class);

    @Test
    public void testKey() throws Exception {
        assertEquals("integer", KEY_INTEGER.getName());
        assertEquals("number", KEY_NUMBER.getName());
        assertEquals("string", KEY_STRING.getName());

        assertEquals(Integer.class, KEY_INTEGER.getValueClass());
        assertEquals(Number.class, KEY_NUMBER.getValueClass());
        assertEquals(String.class, KEY_STRING.getValueClass());
    }

    @Test
    public void testMap() throws Exception {
        final AudioMetadataMap audioMetadata = AudioMetadata.createMap();

        int ivalue;
        String svalue;

        audioMetadata.set(KEY_INTEGER, 10);
        ivalue = audioMetadata.get(KEY_INTEGER);
        assertEquals(10, ivalue);

        // Because the get is typed, the following cannot compile.
        // audioMetadata.set(KEY_INTEGER, "abc");
        // String svalue = audioMetadata.get(KEY_INTEGER);

        assertEquals(1, audioMetadata.size());

        audioMetadata.set(KEY_STRING, "abc");
        svalue = audioMetadata.get(KEY_STRING);
        assertEquals("abc", svalue);

        // Because the set is typed, the following cannot compile
        // audioMetadata.set(KEY_STRING, 10);
        // ivalue = audioMetadata.get(KEY_STRING);

        assertEquals(2, audioMetadata.size());
        assertTrue(audioMetadata.containsKey(KEY_STRING));
        // We should be able to remove the string
        svalue = audioMetadata.remove(KEY_STRING);
        assertEquals("abc", svalue);

        assertEquals(1, audioMetadata.size());
        assertFalse(audioMetadata.containsKey(KEY_STRING));
        assertTrue(audioMetadata.containsKey(KEY_INTEGER));

        // Try a generic Number.
        Number nvalue;
        audioMetadata.set(KEY_NUMBER, 2.125f);
        nvalue = audioMetadata.get(KEY_NUMBER);
        assertEquals(2.125f, nvalue.floatValue(), 0.f);

        // Verify we handle null properly.
        assertThrows(NullPointerException.class,
            () -> { audioMetadata.get(null); }
        );
        assertThrows(NullPointerException.class,
            () -> { audioMetadata.set(null, 1); }
        );
        assertThrows(NullPointerException.class,
            () -> { audioMetadata.set(KEY_NUMBER, null); }
        );

        // check creating a map from another map.
        assertEquals(audioMetadata, audioMetadata.dup());
    }

    @Test
    public void testFormatKeys() throws Exception {
        final AudioMetadataMap audioMetadata = AudioMetadata.createMap();
        audioMetadata.set(AudioMetadata.Format.KEY_ATMOS_PRESENT, true);
        audioMetadata.set(AudioMetadata.Format.KEY_AUDIO_ENCODING, AudioFormat.ENCODING_MP3);
        audioMetadata.set(AudioMetadata.Format.KEY_BIT_RATE, 64000);
        audioMetadata.set(AudioMetadata.Format.KEY_BIT_WIDTH, 16);
        audioMetadata.set(AudioMetadata.Format.KEY_CHANNEL_MASK, AudioFormat.CHANNEL_OUT_STEREO);
        audioMetadata.set(AudioMetadata.Format.KEY_MIME, "audio/mp3");
        audioMetadata.set(AudioMetadata.Format.KEY_SAMPLE_RATE, 48000);

        assertEquals(64000, (int)audioMetadata.get(AudioMetadata.Format.KEY_BIT_RATE));
        assertEquals(AudioFormat.CHANNEL_OUT_STEREO,
                (int)audioMetadata.get(AudioMetadata.Format.KEY_CHANNEL_MASK));
        assertEquals("audio/mp3", (String)audioMetadata.get(AudioMetadata.Format.KEY_MIME));
        assertEquals(48000, (int)audioMetadata.get(AudioMetadata.Format.KEY_SAMPLE_RATE));
        assertEquals(16, (int)audioMetadata.get(AudioMetadata.Format.KEY_BIT_WIDTH));
        assertEquals(true, (boolean)audioMetadata.get(AudioMetadata.Format.KEY_ATMOS_PRESENT));
        assertEquals(AudioFormat.ENCODING_MP3,
                (int)audioMetadata.get(AudioMetadata.Format.KEY_AUDIO_ENCODING));

        // Additional test to ensure we can survive parceling
        testPackingAndUnpacking((AudioMetadata.BaseMap)audioMetadata);
    }

    // Vendor keys created by direct override of the AudioMetadata interface.
    private static final AudioMetadata.Key<Integer>
        KEY_VENDOR_INTEGER = new AudioMetadata.Key<Integer>() {
        @Override
        public String getName() {
            return "vendor.integerData";
        }

        @Override
        public Class<Integer> getValueClass() {
            return Integer.class;  // matches Class<Integer>
        }
    };

    private static final AudioMetadata.Key<Double>
        KEY_VENDOR_DOUBLE = new AudioMetadata.Key<Double>() {
        @Override
        public String getName() {
            return "vendor.doubleData";
        }

        @Override
        public Class<Double> getValueClass() {
            return Double.class;  // matches Class<Double>
        }
    };

    private static final AudioMetadata.Key<String>
            KEY_VENDOR_STRING = new AudioMetadata.Key<String>() {
        @Override
        public String getName() {
            return "vendor.stringData";
        }

        @Override
        public Class<String> getValueClass() {
            return String.class;  // matches Class<String>
        }
    };

    @Test
    public void testVendorKeys() {
        final AudioMetadataMap audioMetadata = AudioMetadata.createMap();

        audioMetadata.set(KEY_VENDOR_INTEGER, 10);
        final int ivalue = audioMetadata.get(KEY_VENDOR_INTEGER);
        assertEquals(10, ivalue);

        audioMetadata.set(KEY_VENDOR_DOUBLE, 11.5);
        final double dvalue = audioMetadata.get(KEY_VENDOR_DOUBLE);
        assertEquals(11.5, dvalue, 0. /* epsilon */);

        audioMetadata.set(KEY_VENDOR_STRING, "alphabet");
        final String svalue = audioMetadata.get(KEY_VENDOR_STRING);
        assertEquals("alphabet", svalue);
    }

    // The byte buffer here is generated by audio_utils::metadata::byteStringFromData(Data).
    // Data data = {
    //     "integer": (int32_t) 1,
    //     "long": (int64_t) 2,
    //     "float": (float) 3.1f,
    //     "double": (double) 4.11,
    //     "data": (Data) {
    //         "string": (std::string) "hello",
    //     }
    // }
    // Use to test compatibility of packing/unpacking audio metadata.
    // DO NOT CHANGE after R
    private static final byte[] BYTE_BUFFER_REFERENCE = new byte[] {
            // Number of items
            0x05, 0x00, 0x00, 0x00,
            // Length of 1st key
            0x04, 0x00, 0x00, 0x00,
            // Payload of 1st key
            0x64, 0x61, 0x74, 0x61,
            // Data type of 1st value
            0x06, 0x00, 0x00, 0x00,
            // Length of 1st value
            0x1f, 0x00, 0x00, 0x00,
            // Payload of 1st value
            0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
            0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x05, 0x00,
            0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x05, 0x00,
            0x00, 0x00, 0x68, 0x65, 0x6c, 0x6c, 0x6f,
            // Length of 2nd key
            0x06, 0x00, 0x00, 0x00,
            // Payload of 2nd key
            0x64, 0x6f, 0x75, 0x62, 0x6c, 0x65,
            // Data type of 2nd value
            0x04, 0x00, 0x00, 0x00,
            // Length of 2nd value
            0x08, 0x00, 0x00, 0x00,
            // Payload of 2nd value
            0x71, 0x3d, 0x0a, (byte) 0xd7, (byte) 0xa3, 0x70, 0x10, 0x40,
            // Length of 3rd key
            0x05, 0x00, 0x00, 0x00,
            // Payload of 3rd key
            0x66, 0x6c, 0x6f, 0x61, 0x74,
            // Data type of 3rd value
            0x03, 0x00, 0x00, 0x00,
            // Length of 3rd value
            0x04, 0x00, 0x00, 0x00,
            // Payload of 3rd value
            0x66, 0x66, 0x46, 0x40,
            // Length of 4th key
            0x07, 0x00, 0x00, 0x00,
            // Payload of 4th key
            0x69, 0x6e, 0x74, 0x65, 0x67, 0x65, 0x72,
            // Data type of 4th value
            0x01, 0x00, 0x00, 0x00,
            // Length of 4th value
            0x04, 0x00, 0x00, 0x00,
            // Payload of 4th value
            0x01, 0x00, 0x00, 0x00,
            // Length of 5th key
            0x04, 0x00, 0x00, 0x00,
            // Payload of 5th key
            0x6c, 0x6f, 0x6e, 0x67,
            // Data type of 5th value
            0x02, 0x00, 0x00, 0x00,
            // Length of 5th value
            0x08, 0x00, 0x00, 0x00,
            // Payload of 5th value
            0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    // AUDIO_METADATA_REFERENCE corresponds to BYTE_BUFFER_REFERENCE.
    // DO NOT CHANGE after R
    private static final AudioMetadata.BaseMap AUDIO_METADATA_REFERENCE =
            new AudioMetadata.BaseMap() {{
                set(KEY_INTEGER, 1);
                set(KEY_LONG, (long) 2);
                set(KEY_FLOAT, 3.1f);
                set(KEY_DOUBLE, 4.11);
                set(KEY_BASE_MAP, new AudioMetadata.BaseMap() {{
                    set(KEY_STRING, "hello");
                }});
            }};

    // Position of data type for first value in reference buffer.
    // Will be used to set an invalid data type for test.
    private static final int FIRST_VALUE_DATA_TYPE_POSITION_IN_REFERENCE = 12;
    private static final byte INVALID_DATA_TYPE = (byte) 0xff;

    @Test
    public void testCompatibilityR() {
        ByteBuffer bufferPackedAtJava = AudioMetadata.toByteBuffer(
                AUDIO_METADATA_REFERENCE, ByteOrder.nativeOrder());
        assertNotNull(bufferPackedAtJava);
        ByteBuffer buffer = nativeGetByteBuffer(bufferPackedAtJava, bufferPackedAtJava.limit());
        assertNotNull(buffer);
        ByteBuffer refBuffer = ByteBuffer.allocate(BYTE_BUFFER_REFERENCE.length);
        refBuffer.put(BYTE_BUFFER_REFERENCE);
        refBuffer.position(0);
        assertEquals(buffer, refBuffer);
    }

    @Test
    public void testUnpackingByteBuffer() throws Exception {
        testPackingAndUnpacking(AUDIO_METADATA_REFERENCE);
    }

    @Test
    public void testUnpackingInvalidBuffer() throws Exception {
        ByteBuffer buffer = ByteBuffer.allocate(BYTE_BUFFER_REFERENCE.length);
        buffer.put(BYTE_BUFFER_REFERENCE);
        buffer.order(ByteOrder.nativeOrder());

        // Manually modify the buffer to create an invalid buffer with an invalid data type,
        // a null should be returned when unpacking.
        buffer.position(FIRST_VALUE_DATA_TYPE_POSITION_IN_REFERENCE);
        buffer.put(INVALID_DATA_TYPE);
        buffer.position(0);
        AudioMetadata.BaseMap metadataFromByteBuffer = AudioMetadata.fromByteBuffer(buffer);
        assertNull(metadataFromByteBuffer);
    }

    private static void testPackingAndUnpacking(AudioMetadata.BaseMap audioMetadata) {
        ByteBuffer bufferPackedAtJava = AudioMetadata.toByteBuffer(
                audioMetadata, ByteOrder.nativeOrder());
        assertNotNull(bufferPackedAtJava);
        ByteBuffer buffer = nativeGetByteBuffer(bufferPackedAtJava, bufferPackedAtJava.limit());
        assertNotNull(buffer);
        buffer.order(ByteOrder.nativeOrder());

        AudioMetadata.BaseMap metadataFromByteBuffer = AudioMetadata.fromByteBuffer(buffer);
        assertNotNull(metadataFromByteBuffer);
        assertEquals(metadataFromByteBuffer, audioMetadata);
    }

    static {
        System.loadLibrary("audio_jni");
    }

    /**
     * Passing a ByteBuffer that contains audio metadata. In native, the buffer will be
     * unpacked as native audio metadata and then reconstructed as ByteBuffer back to Java.
     * This is aimed at testing round trip (un)packing logic.
     */
    private static native ByteBuffer nativeGetByteBuffer(ByteBuffer buffer, int sizeInBytes);
}
