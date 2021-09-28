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

package android.appsecurity.cts.keyrotationtest.utils;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Optional;

/** Provides utility methods that can be used for signature verification. */
public class SignatureUtils {
    private SignatureUtils() {}

    private static final char[] HEX_CHARACTERS = "0123456789abcdef".toCharArray();

    /**
     * Computes the sha256 digest of the provided {@code data}, returning an Optional containing a
     * String representing the hex encoding of the digest.
     *
     * <p>If the SHA-256 MessageDigest instance is not available this method will return an empty
     * Optional.
     */
    public static Optional<String> computeSha256Digest(byte[] data) {
        MessageDigest messageDigest;
        try {
            messageDigest = MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException e) {
            return Optional.empty();
        }
        messageDigest.update(data);
        return Optional.of(toHexString(messageDigest.digest()));
    }

    /** Returns a String representing the hex encoding of the provided {@code data}. */
    public static String toHexString(byte[] data) {
        char[] result = new char[data.length * 2];
        for (int i = 0; i < data.length; i++) {
            int resultIndex = i * 2;
            result[resultIndex] = HEX_CHARACTERS[(data[i] >> 4) & 0x0f];
            result[resultIndex + 1] = HEX_CHARACTERS[data[i] & 0x0f];
        }
        return new String(result);
    }
}

