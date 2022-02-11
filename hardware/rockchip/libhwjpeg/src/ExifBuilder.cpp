/*
 * Copyright 2021 Rockchip Electronics Co. LTD
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
 *
 * author: kevin.chen@rock-chips.com
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "ExifBuilder"
#include <utils/Log.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ExifBuilder.h"

static const unsigned char ExifHeader[] = {0x45, 0x78, 0x69, 0x66, 0x00, 0x00};

static const struct {
    ExifIfd ifd;
    const char *name;
} ExifIfdTable[] = {
    { EXIF_IFD_0, "0" },
    { EXIF_IFD_1, "1" },
    { EXIF_IFD_EXIF, "EXIF" },
    { EXIF_IFD_GPS, "GPS" },
    { EXIF_IFD_INTEROPERABILITY, "Interoperability" },
};

/*
 * Table of data format types / descriptions and sizes.
 * This table should be sorted in decreasing order of popularity in order
 * to decrease the total average lookup time.
 */
static const struct {
    ExifFormat format;
    const char *name;
    unsigned char size;
} ExifFormatTable[] = {
    { EXIF_FORMAT_SHORT,     "Short",     2 },
    { EXIF_FORMAT_RATIONAL,  "Rational",  8 },
    { EXIF_FORMAT_SRATIONAL, "SRational", 8 },
    { EXIF_FORMAT_UNDEFINED, "Undefined", 1 },
    { EXIF_FORMAT_ASCII,     "ASCII",     1 },
    { EXIF_FORMAT_LONG,      "Long",      4 },
    { EXIF_FORMAT_BYTE,      "Byte",      1 },
    { EXIF_FORMAT_SBYTE,     "SByte",     1 },
    { EXIF_FORMAT_SSHORT,    "SShort",    2 },
    { EXIF_FORMAT_SLONG,     "SLong",     4 },
    { EXIF_FORMAT_FLOAT,     "Float",     4 },
    { EXIF_FORMAT_DOUBLE,    "Double",    8 },
};

static const char* exif_ifd_get_name(ExifIfd ifd)
{
    unsigned int i;

    for (i = 0; ExifIfdTable[i].name; i++)
        if (ExifIfdTable[i].ifd == ifd)
            break;

    return (ExifIfdTable[i].name);
}

static unsigned char exif_format_get_size(ExifFormat format)
{
    unsigned int i;

    for (i = 0; ExifFormatTable[i].size; i++)
        if (ExifFormatTable[i].format == format)
            return ExifFormatTable[i].size;
    return 0;
}

void exif_set_short(unsigned char *b, ExifByteOrder order, uint16_t value)
{
    if (!b) return;
    switch (order) {
    case EXIF_BYTE_ORDER_MOTOROLA:
        b[0] = (unsigned char) (value >> 8);
        b[1] = (unsigned char) value;
        break;
    case EXIF_BYTE_ORDER_INTEL:
        b[0] = (unsigned char) value;
        b[1] = (unsigned char) (value >> 8);
        break;
    }
}

void exif_set_long(unsigned char *b, ExifByteOrder order, uint32_t value)
{
    if (!b) return;
    switch (order) {
    case EXIF_BYTE_ORDER_MOTOROLA:
        b[0] = (unsigned char) (value >> 24);
        b[1] = (unsigned char) (value >> 16);
        b[2] = (unsigned char) (value >> 8);
        b[3] = (unsigned char) value;
        break;
    case EXIF_BYTE_ORDER_INTEL:
        b[3] = (unsigned char) (value >> 24);
        b[2] = (unsigned char) (value >> 16);
        b[1] = (unsigned char) (value >> 8);
        b[0] = (unsigned char) value;
        break;
    }
}

void exif_set_rational(unsigned char *b, ExifByteOrder order, ExifRational value)
{
	if (!b) return;
	exif_set_long(b, order, value.numerator);
	exif_set_long(b + 4, order, value.denominator);
}

void exif_setup_entry(ExifEntry *entry, ExifTag tag, ExifFormat format,
                      unsigned long components, char *data)
{
    int size = exif_format_get_size(format) * components;

    entry->tag = tag;
    entry->format = format;
    entry->components = components;
    entry->data = (unsigned char*)malloc(size);
    memcpy(entry->data, data, size);
}

void exif_setup_rational_entry(ExifEntry *entry, ExifTag tag,
                               ExifFormat format, unsigned long components,
                               ExifByteOrder order, ExifRational *rat)
{
    unsigned long i;
    /* rational format - 8 bytes */
    int size = exif_format_get_size(format) * components;

    entry->tag = tag;
    entry->format = format;
    entry->components = components;
    entry->data = (unsigned char*)malloc(size);

    for (i = 0; i < entry->components; i++) {
        exif_set_rational(entry->data, order, rat[i]);
    }
}

void exif_setup_short_entry(ExifEntry *entry, ExifTag tag, ExifFormat format,
                            unsigned long components, ExifByteOrder order,
                            uint16_t value)
{
    int size = exif_format_get_size(format) * components;

    entry->tag = tag;
    entry->format = format;
    entry->components = components;
    entry->data = (unsigned char*)malloc(size);

    exif_set_short(entry->data, order, value);
}

void exif_setup_long_entry(ExifEntry *entry, ExifTag tag, ExifFormat format,
                           unsigned long components, ExifByteOrder order,
                           uint32_t value)
{
    int size = exif_format_get_size(format) * components;

    entry->tag = tag;
    entry->format = format;
    entry->components = components;
    entry->data = (unsigned char*)malloc(size);

    exif_set_long(entry->data, order, value);
}

void exif_release_entry(ExifEntry *entry)
{
    entry->components = 0;

    if (entry->data) {
        free(entry->data);
        entry->data = NULL;
    }
}

static void exif_save_data_entry(ExifData *edata, ExifEntry *entry,
                                 unsigned char **buf, int *len, int offset)
{
    unsigned int byte_size, ts;
    // offset to entry value
    unsigned int doff;

    if (!edata)
        return;

    /*
     * Each entry is 12 bytes long. The memory for the entry has
     * already been allocated.
     */
    exif_set_short(*buf + 6 + offset + 0, edata->order, entry->tag);
    exif_set_short(*buf + 6 + offset + 2, edata->order, entry->format);
    exif_set_long(*buf + 6 + offset + 4, edata->order, entry->components);

    /*
     * Size - If bigger than 4 bytes, the actual data is not in
     * the entry but somewhere else.
     */
    byte_size = exif_format_get_size(entry->format) * entry->components;
    if (byte_size > 4) {
        ts = *len + byte_size;
        unsigned char *t;
        doff = *len - 6;

        /*
         * According to the TIFF specification,
         * the offset must be an even number. If we need to introduce
         * a padding byte, we set it to 0.
         */
        if (byte_size & 1)
            ts++;

        /* Realloc memery buffer for entry value */
        t = (unsigned char*)realloc(*buf, ts);
        if (!t) {
            ALOGE("failed to realloc buf with size: %d", ts);
            return;
        }

        *buf = t;
        *len = ts;

        exif_set_long(*buf + 6 + offset + 8, edata->order, doff);
        if (byte_size & 1)
            *(*buf + *len - 1) = '\0';
    } else {
        doff = offset + 8;
    }

    /* Write the data. Fill unneeded bytes with 0 */
    if (entry->data) {
        memcpy(*buf + 6 + doff, entry->data, byte_size);
    } else {
        memset(*buf + 6 + doff, 0, byte_size);
    }

    if (byte_size < 4)
        memset(*buf + 6 + doff + byte_size, 0, (4 - byte_size));
}

static bool exif_save_data_content(ExifData *edata, ExifContent *ifd,
                                   unsigned char **buf, int *len, int offset)
{
    int j, n_ptr = 0, n_thumb = 0;
    ExifIfd i;
    // the new number bytes of raw EXIF data
    unsigned char *t;
    int ts;

    if (!edata || !ifd || !buf || !len) {
        ALOGE("found invalid input param.");
        return false;
    }

    for (j = 0; j < EXIF_IFD_COUNT; j++) {
        if (ifd == &edata->ifd[j])
            break;
    }

    if (j == EXIF_IFD_COUNT)
        return false; /* input error */

    i = (ExifIfd)j;

    /* Check if we need some extra entries for pointers or the thumbnail. */
    switch (i) {
    case EXIF_IFD_0:
        /*
         * The pointer to IFD_EXIF is in IFD_0. The pointer to
         * IFD_INTEROPERABILITY is in IFD_EXIF.
         */
        if (edata->ifd[EXIF_IFD_EXIF].entry_count ||
            edata->ifd[EXIF_IFD_INTEROPERABILITY].entry_count)
            n_ptr++;

        /* The pointer to IFD_GPS is in IFD_0. */
        if (edata->ifd[EXIF_IFD_GPS].entry_count)
            n_ptr++;
        break;
    case EXIF_IFD_1:
        if (edata->thumb_size)
            n_thumb = 2;
        break;
    case EXIF_IFD_EXIF:
        if (edata->ifd[EXIF_IFD_INTEROPERABILITY].entry_count)
            n_ptr++;
        break;
    default:
        break;
    }

    /*
     * Allocate enough memory for all entries
     * and the number of entries.
     */
    ts = *len + (2 + (ifd->entry_count + n_ptr + n_thumb) * 12 + 4);
    t = (unsigned char*)realloc(*buf, ts);
    if (!t) {
        ALOGE("failed to realloc buf with size: %d", ts);
        return false;
    }

    *buf = t;
    *len = ts;

    /* Save the number of entries */
    exif_set_short(*buf + 6 + offset, edata->order,
                   (ifd->entry_count + n_ptr + n_thumb));
    offset += 2;

    ALOGV("saving %i entries (IFD '%s', offset: %i)...", ifd->entry_count,
          exif_ifd_get_name(i), offset);

    for (j = 0; j < ifd->entry_count; j++) {
        if (ifd->entries[j].components) {
            exif_save_data_entry(edata, &ifd->entries[j],
                                 buf, len, offset + 12 * j);
        }
    }

    offset += 12 * ifd->entry_count;

    /* Now save special entries. */
    switch (i) {
    case EXIF_IFD_0:
        /*
         * The pointer to IFD_EXIF is in IFD_0.
         * However, the pointer to IFD_INTEROPERABILITY is in IFD_EXIF,
         * therefore, if IFD_INTEROPERABILITY is not empty, we need
         * IFD_EXIF even if latter is empty.
         */
        if (edata->ifd[EXIF_IFD_EXIF].entry_count ||
            edata->ifd[EXIF_IFD_INTEROPERABILITY].entry_count) {
            exif_set_short(*buf + 6 + offset + 0,
                           edata->order, EXIF_TAG_EXIF_IFD_POINTER);
            exif_set_short(*buf + 6 + offset + 2,
                           edata->order, EXIF_FORMAT_LONG);
            exif_set_long(*buf + 6 + offset + 4, edata->order, 1);
            /* set offset to IFD_EXIF */
            exif_set_long(*buf + 6 + offset + 8, edata->order, *len - 6);
            exif_save_data_content(edata, &edata->ifd[EXIF_IFD_EXIF],
                                   buf, len, *len - 6);
            offset += 12;
        }

        /* The pointer to IFD_GPS is in IFD_0, too. */
        if (edata->ifd[EXIF_IFD_GPS].entry_count) {
            exif_set_short(*buf + 6 + offset + 0,
                           edata->order, EXIF_TAG_GPS_INFO_IFD_POINTER);
            exif_set_short(*buf + 6 + offset + 2,
                           edata->order, EXIF_FORMAT_LONG);
            exif_set_long(*buf + 6 + offset + 4, edata->order, 1);
            exif_set_long(*buf + 6 + offset + 8, edata->order, *len - 6);
            exif_save_data_content(edata, &edata->ifd[EXIF_IFD_GPS],
                                   buf, len, *len - 6);
            offset += 12;
        }
        break;
    case EXIF_IFD_EXIF:
        /*
         * The pointer to IFD_INTEROPERABILITY is in IFD_EXIF.
         * See note above.
         */
        if (edata->ifd[EXIF_IFD_INTEROPERABILITY].entry_count) {
            exif_set_short(*buf + 6 + offset + 0, edata->order,
                           EXIF_TAG_INTEROPERABILITY_IFD_POINTER);
            exif_set_short(*buf + 6 + offset + 2, edata->order,
                           EXIF_FORMAT_LONG);
            exif_set_long(*buf + 6 + offset + 4, edata->order, 1);
            exif_set_long(*buf + 6 + offset + 8, edata->order, *len - 6);
            exif_save_data_content(edata, &edata->ifd[EXIF_IFD_INTEROPERABILITY],
                                   buf, len, *len - 6);
            offset += 12;
        }
        break;
    case EXIF_IFD_1:
        /* Information about the thumbnail (if any) is saved in IFD_1. */
        if (edata->thumb_size) {
            int addZeroLen = 0;

            ts = *len + edata->thumb_size;

            /* Rockchip only, offset that send to vpu must be aligned 16 */
            if (((ts + 6 - 20) & 15) != 0) {
                addZeroLen = 16 - ((ts + 6 - 20) & 15);
                ts += addZeroLen;
            }

            t = (unsigned char*)realloc(*buf, ts);
            if (!t) {
                ALOGE("failed to realloc buf with thumb_size: %d", ts);
                return false;
            }

            *buf = t;
            *len = ts;

            if (addZeroLen != 0)
                memset(*buf + *len - edata->thumb_size - addZeroLen, 0, addZeroLen);

            memcpy(*buf + *len - edata->thumb_size,
                   edata->thumb_data, edata->thumb_size);

            /* EXIF_TAG_JPEG_INTERCHANGE_FORMAT */
            exif_set_short(*buf + 6 + offset + 0,
                           edata->order, EXIF_TAG_JPEG_INTERCHANGE_FORMAT);
            exif_set_short(*buf + 6 + offset + 2,
                           edata->order, EXIF_FORMAT_LONG);
            exif_set_long(*buf + 6 + offset + 4, edata->order, 1);
            exif_set_long(*buf + 6 + offset + 8,
                          edata->order, *len - edata->thumb_size - 6);
            offset += 12;

            /* EXIF_TAG_JPEG_INTERCHANGE_FORMAT_LENGTH */
            exif_set_short(*buf + 6 + offset + 0,
                           edata->order, EXIF_TAG_JPEG_INTERCHANGE_FORMAT_LENGTH);
            exif_set_short(*buf + 6 + offset + 2, edata->order, EXIF_FORMAT_LONG);
            exif_set_long(*buf + 6 + offset + 4, edata->order, 1);
            exif_set_long(*buf + 6 + offset + 8, edata->order, edata->thumb_size);
            offset += 12;
        }
        break;
    default:
        break;
    }

    /* Correctly terminate the directory */
    if (i == EXIF_IFD_0 && (edata->ifd[EXIF_IFD_1].entry_count
        || edata->thumb_size)) {
        /* We are saving IFD 0. Tell where IFD 1 starts and save IFD 1.*/
        exif_set_long(*buf + 6 + offset, edata->order, *len - 6);
        exif_save_data_content(edata, &edata->ifd[EXIF_IFD_1],
                               buf, len, *len - 6);
    } else {
        exif_set_long (*buf + 6 + offset, edata->order, 0);
    }

    return true;
}

bool exif_general_build(ExifData *edata, unsigned char **buf, int *len)
{
    if (!edata || !buf || !len) {
        ALOGE("found invalid input param.");
        return false;
    }

    // Set to intel byte order default.
    edata->order = EXIF_BYTE_ORDER_INTEL;

    /* Header */
    *len = 14;
    *buf = (unsigned char*)malloc(*len);
    if (!*buf)  {
        *len = 0;
        ALOGE("failed to alloc exif raw buf.");
        return false;
    }

    memcpy(*buf, ExifHeader, 6);

    /* Order (offset 6) */
    if (edata->order == EXIF_BYTE_ORDER_INTEL) {
        memcpy(*buf + 6, "II", 2);
    } else {
        memcpy(*buf + 6, "MM", 2);
    }

    /* TIFF version flag (2 bytes, offset 8) */
    exif_set_short(*buf + 8, edata->order, 0x002a);

    /*
     * IFD 0 offset (4 bytes, offset 10).
     * We will start 8 bytes after the EXIF header (2 bytes for order,
     * another 2 for version, and 4 bytes for the IFD 0 offset).
     */
    exif_set_long(*buf + 10, edata->order, 8);

    ALOGV("Saving IFDs...");
    if (exif_save_data_content(edata, &edata->ifd[EXIF_IFD_0],
                               buf, len, *len - 6)) {
        ALOGV("Saved %d bytes EXIF data.", *len);
        return true;
    }

    return false;
}
