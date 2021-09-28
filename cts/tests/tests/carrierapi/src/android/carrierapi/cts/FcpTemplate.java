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

package android.carrierapi.cts;

import static android.carrierapi.cts.IccUtils.hexStringToBytes;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import javax.annotation.Nonnull;

/**
 * Class for representing a File Control Parameters (FCP) Template object. TS 101 220
 *
 * A correctly formatted FCP Template will be in the format:
 * | 1 byte: BER tag (0x62) | 1 byte: length of TLVs |...TLV objects...| 2 bytes: status |
 */
public class FcpTemplate {

    // FCP Template label
    public static final int BER_TAG_FCP_TEMPLATE = 0x62;

    // FCP Template BER-TLV Tag tags. TS 101 220 Section 7.2
    public static final int FILE_SIZE_DATA = 0x80;
    public static final int FILE_SIZE_TOTAL = 0x81;
    public static final int FILE_DESCRIPTOR = 0x82;
    public static final int FILE_IDENTIFIER = 0x83;
    public static final int DF_NAME = 0x84;
    public static final int PROPRIETARY = 0x85;
    public static final int SFI_SUPPORT = 0x88;
    public static final int LIFE_CYCLE_STATUS = 0x8A;
    public static final int SECURITY_ATTR_REFERENCE_FORMAT = 0x8B;
    public static final int SECURITY_ATTR_COMPACT_FORMAT = 0x8C;
    public static final int SECURITY_ATTR_TEMPLATE_EXPANDED_FORMAT = 0xAB;
    public static final int PROPRIETARY_TEMPLATE = 0xA5;
    public static final int PIN_STATUS_DATA_OBJECT = 0xC6;

    private final List<Tlv> tlvs;
    private final String status;

    private FcpTemplate(@Nonnull List<Tlv> tlvs, @Nonnull String status) {
        this.tlvs = tlvs;
        this.status = status;
    }

    public List<Tlv> getTlvs() {
        return tlvs;
    }

    public String getStatus() {
        return status;
    }

    /**
     * Parses and returns a FcpTemplate for the given {@code fcpResponse}
     *
     * @param fcpResponse The Hex String response for a given Status APDU command. Expected to be in
     * the format: | 1 byte: BER tag | 1 byte: length of TLVs |...TLV objects...| 2 bytes: status |
     *
     * @return a FcpTemplate for the given hex String
     *
     * @throws FcpTemplateParseException for non-FCP inputs or inputs of the wrong length (encoded
     *                                   length does not match actual length)
     */
    public static FcpTemplate parseFcpTemplate(@Nonnull String fcpResponse) {
        final List<Tlv> tlvObjects = new ArrayList<>();

        // Expected FcpResponse format:
        // | 1 byte: BER tag | 1 byte: length of TLVs | ...TLV objects... | 2 bytes: status |
        byte[] data = hexStringToBytes(fcpResponse);
        int responseLength = data.length;
        // don't count BER tag, length byte, or status bytes
        int payloadLength = responseLength - 4;
        // data[0]: Response tag
        // data[1]: TLV data object length in bytes. Assumes that length is < 128 bytes
        if (data[0] != BER_TAG_FCP_TEMPLATE || data[1] != payloadLength) {
            throw new FcpTemplateParseException("Improperly formatted fcpResponse: " + fcpResponse);
        }

        int index = 2;
        while (index < responseLength - 2) { // don't need to process the 2 byte status-word footer
            // TLV data object format per TS 101 220:
            // | 1 byte: tag | 1 byte: length | 'length' bytes: value |
            int tag = data[index++] & 0xFF;
            int length = data[index++] & 0xFF; // assumes that length is < 128 bytes.
            String value = fcpResponse.substring(index * 2, (index + length) * 2);
            tlvObjects .add(new Tlv(tag, length, value));
            index += length;
        }

        String status = fcpResponse.substring(fcpResponse.length() - 4);
        return new FcpTemplate(tlvObjects , status);
    }

    /**
     * Represents a Tag-Length-Value object. TS 101 220 Section 2
     */
    public static class Tlv {

        private final int tag;
        private final int length;
        private final String value;

        public Tlv(int tag, int length, @Nonnull String value) {
            this.tag = tag;
            this.length = length;
            this.value = value;
        }

        public int getTag() {
            return tag;
        }

        public int getLength() {
            return length;
        }

        public String getValue() {
            return value;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) {
                return true;
            }
            if (o == null || getClass() != o.getClass()) {
                return false;
            }
            Tlv tlv = (Tlv) o;
            return tag == tlv.tag &&
                    length == tlv.length &&
                    value.equals(tlv.value);
        }

        @Override
        public int hashCode() {
            return Objects.hash(tag, length, value);
        }

        @Override
        public String toString() {
            return tag + "(" + length + "):" + value;
        }
    }

    private static final class FcpTemplateParseException extends RuntimeException {

        public FcpTemplateParseException(String message) {
            super(message);
        }
    }
}
