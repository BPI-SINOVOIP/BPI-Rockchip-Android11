/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.tv.settings.system.development.audio;

import android.util.Log;

import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ShortBuffer;

/** Contains a static method for writing an audio buffer to a WAV file. */
public class WavWriter {

    private static final String TAG = "WavWriter";

    private static final int HEADER_SIZE = 44;

    /**
     * Writes an audio buffer to a WAV file.
     *
     * @param directory   The directory to store the file in
     * @param audioBuffer The buffer to write to the file
     */
    public static void writeToFile(File directory, ShortBuffer audioBuffer) {
        long timestamp = System.currentTimeMillis();
        File file = new File(directory, String.format("recording_%d.wav", timestamp));

        byte[] data = toLittleEndianBytes(audioBuffer);

        try {
            DataOutputStream stream = new DataOutputStream(
                    new FileOutputStream(file.getAbsolutePath()));
            writeFileHeader(stream, data.length);
            stream.write(data, 0, data.length);
            stream.close();
            Log.i(TAG, String.format("WAV file written to %s", file.getAbsolutePath()));
        } catch (Exception e) {
            Log.e(TAG, String.format("Error writing to file: ", e.toString()));
        }
    }

    /**
     * Writes a WAV header to a file.
     *
     * @param stream   The output stream for the file.
     * @param numBytes The number of (non-header) bytes to be written.
     */
    private static void writeFileHeader(DataOutputStream stream, int numBytes) throws IOException {
        // 1 - 4 "RIFF" Marks the file as a riff file. Characters are each 1 byte long.
        stream.writeBytes("RIFF");
        // 5 - 8 File size (32-bit integer). Number of bytes in the entire file, minus 8.
        stream.write(toLittleEndianBytes(HEADER_SIZE + numBytes - 8));
        // 9 -12 "WAVE" File Type Header. For our purposes, it always equals "WAVE".
        stream.writeBytes("WAVE");
        // 13-16 "fmt " Format chunk marker. Includes trailing null.
        stream.writeBytes("fmt ");
        // 17-20 Length of format data as listed above (16).
        stream.write(toLittleEndianBytes(16), 0, 4);
        // 21-22 Type of format (16-bit integer). 1 is PCM.
        stream.write(toLittleEndianBytes(1), 0, 2);
        // 23-24 Number of channels (16-bit integer).
        stream.write(toLittleEndianBytes(AudioDebug.CHANNELS), 0, 2);
        // 25-28 Sample Rate (32-bit integer). Common values are 44100 (CD), 48000 (DAT).
        stream.write(toLittleEndianBytes(AudioDebug.SAMPLE_RATE), 0, 4);
        // 29-32 (Sample Rate * BitsPerSample * Channels) / 8.
        stream.write(
                toLittleEndianBytes(
                        AudioDebug.SAMPLE_RATE * AudioDebug.BITRATE * AudioDebug.CHANNELS / 8), 0,
                4);
        // 33-34 (BitsPerSample * Channels) / 8.
        //        1 - 8 bit mono; 2 - 8 bit stereo/16 bit mono; 4 - 16 bit stereo
        stream.write(toLittleEndianBytes(AudioDebug.BITRATE * AudioDebug.CHANNELS / 8), 0, 2);
        // 35-36 Bits per sample
        stream.write(toLittleEndianBytes(AudioDebug.BITRATE), 0, 2);
        // 37-40 "data" chunk header. Marks the beginning of the data section.
        stream.writeBytes("data");
        // 41-44 Size of the data section (32-bit integer).
        stream.write(toLittleEndianBytes(numBytes), 0, 4);
    }

    private static byte[] toLittleEndianBytes(ShortBuffer buffer) {
        int numShorts = buffer.position();

        byte[] result = new byte[numShorts * 2];
        for (int shortIdx = 0; shortIdx < numShorts; shortIdx++) {
            short s = buffer.get(shortIdx);
            result[shortIdx * 2] = (byte) s;
            result[shortIdx * 2 + 1] = (byte) (s >> 8);
        }

        return result;
    }

    private static byte[] toLittleEndianBytes(int x) {
        byte[] result = new byte[4];

        result[0] = (byte) x;
        result[1] = (byte) (x >> 8);
        result[2] = (byte) (x >> 16);
        result[3] = (byte) (x >> 24);

        return result;
    }

}
