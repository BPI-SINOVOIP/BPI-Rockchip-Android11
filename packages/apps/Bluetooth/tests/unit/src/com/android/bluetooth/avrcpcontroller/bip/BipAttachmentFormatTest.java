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

package com.android.bluetooth.avrcpcontroller;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Calendar;
import java.util.Date;
import java.util.TimeZone;

/**
 * A test suite for the BipAttachmentFormat class
 */
@RunWith(AndroidJUnit4.class)
public class BipAttachmentFormatTest {

    private Date makeDate(int month, int day, int year, int hours, int min, int sec, TimeZone tz) {
        Calendar.Builder builder = new Calendar.Builder();

        /* Note that Calendar months are zero-based in Java framework */
        builder.setDate(year, month - 1, day);
        builder.setTimeOfDay(hours, min, sec, 0);
        if (tz != null) builder.setTimeZone(tz);
        return builder.build().getTime();
    }

    private Date makeDate(int month, int day, int year, int hours, int min, int sec) {
        return makeDate(month, day, year, hours, min, sec, null);
    }

    private void testParse(String contentType, String charset, String name, String size,
            String created, String modified, Date expectedCreated, boolean isCreatedUtc,
                Date expectedModified, boolean isModifiedUtc) {
        int expectedSize = (size != null ? Integer.parseInt(size) : -1);
        BipAttachmentFormat attachment = new BipAttachmentFormat(contentType, charset, name,
                size, created, modified);
        Assert.assertEquals(contentType, attachment.getContentType());
        Assert.assertEquals(charset, attachment.getCharset());
        Assert.assertEquals(name, attachment.getName());
        Assert.assertEquals(expectedSize, attachment.getSize());

        if (expectedCreated != null) {
            Assert.assertEquals(expectedCreated, attachment.getCreatedDate().getTime());
            Assert.assertEquals(isCreatedUtc, attachment.getCreatedDate().isUtc());
        } else {
            Assert.assertEquals(null, attachment.getCreatedDate());
        }

        if (expectedModified != null) {
            Assert.assertEquals(expectedModified, attachment.getModifiedDate().getTime());
            Assert.assertEquals(isModifiedUtc, attachment.getModifiedDate().isUtc());
        } else {
            Assert.assertEquals(null, attachment.getModifiedDate());
        }
    }

    private void testCreate(String contentType, String charset, String name, int size,
            Date created, Date modified) {
        BipAttachmentFormat attachment = new BipAttachmentFormat(contentType, charset, name,
                size, created, modified);
        Assert.assertEquals(contentType, attachment.getContentType());
        Assert.assertEquals(charset, attachment.getCharset());
        Assert.assertEquals(name, attachment.getName());
        Assert.assertEquals(size, attachment.getSize());

        if (created != null) {
            Assert.assertEquals(created, attachment.getCreatedDate().getTime());
            Assert.assertTrue(attachment.getCreatedDate().isUtc());
        } else {
            Assert.assertEquals(null, attachment.getCreatedDate());
        }

        if (modified != null) {
            Assert.assertEquals(modified, attachment.getModifiedDate().getTime());
            Assert.assertTrue(attachment.getModifiedDate().isUtc());
        } else {
            Assert.assertEquals(null, attachment.getModifiedDate());
        }
    }

    @Test
    public void testParseAttachment() {
        TimeZone utc = TimeZone.getTimeZone("UTC");
        utc.setRawOffset(0);
        Date date = makeDate(1, 1, 1990, 12, 34, 56);
        Date dateUtc = makeDate(1, 1, 1990, 12, 34, 56, utc);

        // Well defined fields
        testParse("text/plain", "ISO-8859-1", "thisisatextfile.txt", "2048", "19900101T123456",
                "19900101T123456", date, false, date, false);

        // Well defined fields with UTC date
        testParse("text/plain", "ISO-8859-1", "thisisatextfile.txt", "2048", "19900101T123456Z",
                "19900101T123456Z", dateUtc, true, dateUtc, true);

        // Change up the content type and file name
        testParse("audio/basic", "ISO-8859-1", "thisisawavfile.wav", "1024", "19900101T123456",
                "19900101T123456", date, false, date, false);

        // Use a null modified date
        testParse("text/plain", "ISO-8859-1", "thisisatextfile.txt", "2048", "19900101T123456",
                null, date, false, null, false);

        // Use a null created date
        testParse("text/plain", "ISO-8859-1", "thisisatextfile.txt", "2048", null,
                "19900101T123456", null, false, date, false);

        // Use all null dates
        testParse("text/plain", "ISO-8859-1", "thisisatextfile.txt", "123", null, null, null, false,
                null, false);

        // Use a null size
        testParse("text/plain", "ISO-8859-1", "thisisatextfile.txt", null, null, null, null, false,
                null, false);

        // Use a null charset
        testParse("text/plain", null, "thisisatextfile.txt", "2048", null, null, null, false, null,
                false);

        // Use only required fields
        testParse("text/plain", null, "thisisatextfile.txt", null, null, null, null, false, null,
                false);
    }

    @Test(expected = ParseException.class)
    public void testParseNullContentType() {
        testParse(null, "ISO-8859-1", "thisisatextfile.txt", null, null, null, null, false, null,
                false);
    }

    @Test(expected = ParseException.class)
    public void testParseNullName() {
        testParse("text/plain", "ISO-8859-1", null, null, null, null, null, false, null,
                false);
    }

    @Test
    public void testCreateAttachment() {
        TimeZone utc = TimeZone.getTimeZone("UTC");
        utc.setRawOffset(0);
        Date date = makeDate(1, 1, 1990, 12, 34, 56);

        // Create with normal, well defined fields
        testCreate("text/plain", "ISO-8859-1", "thisisatextfile.txt", 2048, date, date);

        // Create with a null charset
        testCreate("text/plain", null, "thisisatextfile.txt", 2048, date, date);

        // Create with "no size"
        testCreate("text/plain", "ISO-8859-1", "thisisatextfile.txt", -1, date, date);

        // Use only required fields
        testCreate("text/plain", null, "thisisatextfile.txt", -1, null, null);
    }

    @Test(expected = NullPointerException.class)
    public void testCreateNullContentType_throwsException() {
        testCreate(null, "ISO-8859-1", "thisisatextfile.txt", 2048, null, null);
    }

    @Test(expected = NullPointerException.class)
    public void testCreateNullName_throwsException() {
        testCreate("text/plain", "ISO-8859-1", null, 2048, null, null);
    }

    @Test
    public void testParsedAttachmentToString() {
        TimeZone utc = TimeZone.getTimeZone("UTC");
        utc.setRawOffset(0);
        Date date = makeDate(1, 1, 1990, 12, 34, 56);
        Date dateUtc = makeDate(1, 1, 1990, 12, 34, 56, utc);
        BipAttachmentFormat attachment = null;

        String expected = "<attachment content-type=\"text/plain\" charset=\"ISO-8859-1\""
                          + " name=\"thisisatextfile.txt\" size=\"2048\""
                          + " created=\"19900101T123456\" modified=\"19900101T123456\" />";

        String expectedUtc = "<attachment content-type=\"text/plain\" charset=\"ISO-8859-1\""
                          + " name=\"thisisatextfile.txt\" size=\"2048\""
                          + " created=\"19900101T123456Z\" modified=\"19900101T123456Z\" />";

        String expectedNoDates = "<attachment content-type=\"text/plain\" charset=\"ISO-8859-1\""
                          + " name=\"thisisatextfile.txt\" size=\"2048\" />";

        String expectedNoSizeNoDates = "<attachment content-type=\"text/plain\""
                          + " charset=\"ISO-8859-1\" name=\"thisisatextfile.txt\" />";

        String expectedNoCharsetNoDates = "<attachment content-type=\"text/plain\""
                          + " name=\"thisisatextfile.txt\" size=\"2048\" />";

        String expectedRequiredOnly = "<attachment content-type=\"text/plain\""
                + " name=\"thisisatextfile.txt\" />";

        // Create by parsing, all fields
        attachment = new BipAttachmentFormat("text/plain", "ISO-8859-1", "thisisatextfile.txt",
                "2048", "19900101T123456", "19900101T123456");
        Assert.assertEquals(expected, attachment.toString());

        // Create by parsing, all fields with utc dates
        attachment = new BipAttachmentFormat("text/plain", "ISO-8859-1", "thisisatextfile.txt",
                "2048", "19900101T123456Z", "19900101T123456Z");
        Assert.assertEquals(expectedUtc, attachment.toString());

        // Create by parsing, no timestamps
        attachment = new BipAttachmentFormat("text/plain", "ISO-8859-1", "thisisatextfile.txt",
                "2048", null, null);
        Assert.assertEquals(expectedNoDates, attachment.toString());

        // Create by parsing, no size, no dates
        attachment = new BipAttachmentFormat("text/plain", "ISO-8859-1", "thisisatextfile.txt",
                null, null, null);
        Assert.assertEquals(expectedNoSizeNoDates, attachment.toString());

        // Create by parsing, no charset, no dates
        attachment = new BipAttachmentFormat("text/plain", null, "thisisatextfile.txt", "2048",
                null, null);
        Assert.assertEquals(expectedNoCharsetNoDates, attachment.toString());

        // Create by parsing, content type only
        attachment = new BipAttachmentFormat("text/plain", null, "thisisatextfile.txt", null, null,
                null);
        Assert.assertEquals(expectedRequiredOnly, attachment.toString());
    }

    @Test
    public void testCreatedAttachmentToString() {
        TimeZone utc = TimeZone.getTimeZone("UTC");
        utc.setRawOffset(0);
        Date date = makeDate(1, 1, 1990, 12, 34, 56, utc);
        BipAttachmentFormat attachment = null;

        String expected = "<attachment content-type=\"text/plain\" charset=\"ISO-8859-1\""
                + " name=\"thisisatextfile.txt\" size=\"2048\""
                + " created=\"19900101T123456Z\" modified=\"19900101T123456Z\" />";

        String expectedNoDates = "<attachment content-type=\"text/plain\" charset=\"ISO-8859-1\""
                + " name=\"thisisatextfile.txt\" size=\"2048\" />";

        String expectedNoSizeNoDates = "<attachment content-type=\"text/plain\""
                + " charset=\"ISO-8859-1\" name=\"thisisatextfile.txt\" />";

        String expectedNoCharsetNoDates = "<attachment content-type=\"text/plain\""
                + " name=\"thisisatextfile.txt\" size=\"2048\" />";

        String expectedRequiredOnly = "<attachment content-type=\"text/plain\""
                + " name=\"thisisatextfile.txt\" />";

        // Create with objects, all fields. Now we Use UTC since all Date objects eventually become
        // UTC anyway and this will be timezone agnostic
        attachment = new BipAttachmentFormat("text/plain", "ISO-8859-1", "thisisatextfile.txt",
                2048, date, date);
        Assert.assertEquals(expected, attachment.toString());

        // Create with objects, no dates
        attachment = new BipAttachmentFormat("text/plain", "ISO-8859-1", "thisisatextfile.txt",
                2048, null, null);
        Assert.assertEquals(expectedNoDates, attachment.toString());

        // Create with objects, no size and no dates
        attachment = new BipAttachmentFormat("text/plain", "ISO-8859-1", "thisisatextfile.txt",
                -1, null, null);
        Assert.assertEquals(expectedNoSizeNoDates, attachment.toString());

        // Create with objects, no charset, no dates
        attachment = new BipAttachmentFormat("text/plain", null, "thisisatextfile.txt", 2048, null,
                null);
        Assert.assertEquals(expectedNoCharsetNoDates, attachment.toString());

        // Create with objects, content type only
        attachment = new BipAttachmentFormat("text/plain", null, "thisisatextfile.txt", -1, null,
                null);
        Assert.assertEquals(expectedRequiredOnly, attachment.toString());
    }
}
