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

package android.content.type.cts;

import android.content.type.cts.StockAndroidMimeMapFactory;

import org.junit.Before;
import org.junit.Test;

import java.net.FileNameMap;
import java.net.URLConnection;
import java.util.Locale;
import java.util.Objects;
import java.util.TreeMap;
import libcore.content.type.MimeMap;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

/**
 * Tests {@link MimeMap#getDefault()}.
 */
public class MimeMapTest {

    /** Stock Android's default MimeMap. */
    private MimeMap stockAndroidMimeMap;

    /** The platform's actual default MimeMap. */
    private MimeMap mimeMap;

    @Before public void setUp() {
        mimeMap = MimeMap.getDefault();
        // A copy of stock Android's MimeMap.getDefault() from when this test was built,
        // useful for comparing the platform's behavior vs. stock Android's.
        // The resources are placed into the testres/ path by the "mimemap-testing-res.jar" genrule.
        stockAndroidMimeMap = StockAndroidMimeMapFactory.create(
                s -> MimeMapTest.class.getResourceAsStream("/testres/" + s));
    }

    @Test
    public void defaultMap_15715370() {
        assertEquals("audio/flac", mimeMap.guessMimeTypeFromExtension("flac"));
        assertEquals("flac", mimeMap.guessExtensionFromMimeType("audio/flac"));
        assertEquals("flac", mimeMap.guessExtensionFromMimeType("application/x-flac"));
    }

    // https://code.google.com/p/android/issues/detail?id=78909
    @Test public void bug78909() {
        assertEquals("mka", mimeMap.guessExtensionFromMimeType("audio/x-matroska"));
        assertEquals("mkv", mimeMap.guessExtensionFromMimeType("video/x-matroska"));
    }

    @Test public void bug16978217() {
        assertEquals("image/x-ms-bmp", mimeMap.guessMimeTypeFromExtension("bmp"));
        assertEquals("image/x-icon", mimeMap.guessMimeTypeFromExtension("ico"));
        assertEquals("video/mp2ts", mimeMap.guessMimeTypeFromExtension("ts"));
    }

    @Test public void testCommon() {
        assertEquals("audio/mpeg", mimeMap.guessMimeTypeFromExtension("mp3"));
        assertEquals("image/png", mimeMap.guessMimeTypeFromExtension("png"));
        assertEquals("application/zip", mimeMap.guessMimeTypeFromExtension("zip"));

        assertEquals("mp3", mimeMap.guessExtensionFromMimeType("audio/mpeg"));
        assertEquals("png", mimeMap.guessExtensionFromMimeType("image/png"));
        assertEquals("zip", mimeMap.guessExtensionFromMimeType("application/zip"));
    }

    @Test public void bug18390752() {
        assertEquals("jpg", mimeMap.guessExtensionFromMimeType("image/jpeg"));
    }

    @Test public void bug30207891() {
        assertTrue(mimeMap.hasMimeType("IMAGE/PNG"));
        assertTrue(mimeMap.hasMimeType("IMAGE/png"));
        assertFalse(mimeMap.hasMimeType(""));
        assertEquals("png", mimeMap.guessExtensionFromMimeType("IMAGE/PNG"));
        assertEquals("png", mimeMap.guessExtensionFromMimeType("IMAGE/png"));
        assertNull(mimeMap.guessMimeTypeFromExtension(""));
        assertNull(mimeMap.guessMimeTypeFromExtension("doesnotexist"));
        assertTrue(mimeMap.hasExtension("PNG"));
        assertTrue(mimeMap.hasExtension("PnG"));
        assertFalse(mimeMap.hasExtension(""));
        assertFalse(mimeMap.hasExtension(".png"));
        assertEquals("image/png", mimeMap.guessMimeTypeFromExtension("PNG"));
        assertEquals("image/png", mimeMap.guessMimeTypeFromExtension("PnG"));
        assertNull(mimeMap.guessMimeTypeFromExtension(".png"));
        assertNull(mimeMap.guessMimeTypeFromExtension(""));
        assertNull(mimeMap.guessExtensionFromMimeType("doesnotexist"));
    }

    @Test public void bug30793548() {
        assertEquals("video/3gpp", mimeMap.guessMimeTypeFromExtension("3gpp"));
        assertEquals("video/3gpp", mimeMap.guessMimeTypeFromExtension("3gp"));
        assertEquals("video/3gpp2", mimeMap.guessMimeTypeFromExtension("3gpp2"));
        assertEquals("video/3gpp2", mimeMap.guessMimeTypeFromExtension("3g2"));
    }

    @Test public void bug37167977() {
        // https://tools.ietf.org/html/rfc5334#section-10.1
        assertEquals("audio/ogg", mimeMap.guessMimeTypeFromExtension("ogg"));
        assertEquals("audio/ogg", mimeMap.guessMimeTypeFromExtension("oga"));
        assertEquals("audio/ogg", mimeMap.guessMimeTypeFromExtension("spx"));
        assertEquals("video/ogg", mimeMap.guessMimeTypeFromExtension("ogv"));
    }

    @Test public void bug70851634_mimeTypeFromExtension() {
        assertEquals("video/vnd.youtube.yt", mimeMap.guessMimeTypeFromExtension("yt"));
    }

    @Test public void bug70851634_extensionFromMimeType() {
        assertEquals("yt", mimeMap.guessExtensionFromMimeType("video/vnd.youtube.yt"));
        assertEquals("yt", mimeMap.guessExtensionFromMimeType("application/vnd.youtube.yt"));
    }

    @Test public void bug112162449_audio() {
        // According to https://en.wikipedia.org/wiki/M3U#Internet_media_types
        // this is a giant mess, so we pick "audio/x-mpegurl" because a similar
        // playlist format uses "audio/x-scpls".
        assertMimeTypeFromExtension("audio/x-mpegurl", "m3u");
        assertMimeTypeFromExtension("audio/x-mpegurl", "m3u8");
        assertExtensionFromMimeType("m3u", "audio/x-mpegurl");

        assertExtensionFromMimeType("m4a", "audio/mp4");
        assertMimeTypeFromExtension("audio/mpeg", "m4a");

        assertBidirectional("audio/aac", "aac");
    }

    @Test public void bug112162449_video() {
        assertBidirectional("video/x-flv", "flv");
        assertBidirectional("video/quicktime", "mov");
        assertBidirectional("video/mpeg", "mpeg");
    }

    @Test public void bug112162449_image() {
        assertBidirectional("image/heif", "heif");
        assertBidirectional("image/heif-sequence", "heifs");
        assertBidirectional("image/heic", "heic");
        assertBidirectional("image/heic-sequence", "heics");
        assertMimeTypeFromExtension("image/heif", "hif");

        assertBidirectional("image/x-adobe-dng", "dng");
        assertBidirectional("image/x-photoshop", "psd");

        assertBidirectional("image/jp2", "jp2");
        assertMimeTypeFromExtension("image/jp2", "jpg2");
    }

    @Test public void bug120135571_audio() {
        assertMimeTypeFromExtension("audio/mpeg", "m4r");
    }

    @Test public void bug136096979_ota() {
        assertMimeTypeFromExtension("application/vnd.android.ota", "ota");
    }

    @Test public void bug154667531_consistent() {
        // Verify that if developers start from a strongly-typed MIME type, that
        // sending it through a file extension doesn't lose that fidelity. We're
        // only interested in the major MIME types that are relevant to
        // permission models; we're not worried about generic types like
        // "application/x-flac" being mapped to "audio/flac".
        for (String before : mimeMap.mimeTypes()) {
            final String beforeMajor = extractMajorMimeType(before);
            switch (beforeMajor.toLowerCase(Locale.US)) {
                case "audio":
                case "video":
                case "image":
                    final String extension = mimeMap.guessExtensionFromMimeType(before);
                    final String after = mimeMap.guessMimeTypeFromExtension(extension);
                    final String afterMajor = extractMajorMimeType(after);
                    if (!beforeMajor.equalsIgnoreCase(afterMajor)) {
                        fail("Expected " + before + " to map back to " + beforeMajor
                                + "/* after bouncing through file extension, "
                                + "but instead mapped to " + after);
                    }
                    break;
            }
        }
    }

    @Test public void wifiConfig_xml() {
        assertExtensionFromMimeType("xml", "application/x-wifi-config");
        assertMimeTypeFromExtension("text/xml", "xml");
    }

    // http://b/122734564
    @Test public void nonLowercaseMimeType() {
        // A mixed-case mimeType that appears in mime.types; we expect guessMimeTypeFromExtension()
        // to return it in lowercase because MimeMap considers lowercase to be the canonical form.
        String mimeType = "application/vnd.ms-word.document.macroEnabled.12".toLowerCase(Locale.US);
        assertBidirectional(mimeType, "docm");
    }

    // Check that the keys given for lookups in either direction are not case sensitive
    @Test public void caseInsensitiveKeys() {
        String mimeType = mimeMap.guessMimeTypeFromExtension("apk");
        assertNotNull(mimeType);

        assertEquals(mimeType, mimeMap.guessMimeTypeFromExtension("APK"));
        assertEquals(mimeType, mimeMap.guessMimeTypeFromExtension("aPk"));

        assertEquals("apk", mimeMap.guessExtensionFromMimeType(mimeType));
        assertEquals("apk", mimeMap.guessExtensionFromMimeType(
                mimeType.toUpperCase(Locale.US)));
        assertEquals("apk", mimeMap.guessExtensionFromMimeType(
                mimeType.toLowerCase(Locale.US)));
    }

    @Test public void invalidKey_empty() {
        checkInvalidExtension("");
        checkInvalidMimeType("");
    }

    @Test public void invalidKey_null() {
        checkInvalidExtension(null);
        checkInvalidMimeType(null);
    }

    @Test public void invalidKey() {
        checkInvalidMimeType("invalid mime type");
        checkInvalidExtension("invalid extension");
    }

    @Test public void containsAllStockAndroidMappings_mimeToExt() {
        // The minimum expected mimeType -> extension mappings that should be present.
        TreeMap<String, String> expected = new TreeMap<>();
        // The extensions that these mimeTypes are actually mapped to.
        TreeMap<String, String> actual = new TreeMap<>();
        for (String mimeType : stockAndroidMimeMap.mimeTypes()) {
            expected.put(mimeType, stockAndroidMimeMap.guessExtensionFromMimeType(mimeType));
            actual.put(mimeType, mimeMap.guessExtensionFromMimeType(mimeType));
        }
        assertEquals(expected, actual);
    }

    @Test public void containsAllExpectedMappings_extToMime() {
        // The minimum expected extension -> mimeType mappings that should be present.
        TreeMap<String, String> expected = new TreeMap<>();
        // The mimeTypes that these extensions are actually mapped to.
        TreeMap<String, String> actual = new TreeMap<>();
        for (String extension : stockAndroidMimeMap.extensions()) {
            expected.put(extension, stockAndroidMimeMap.guessMimeTypeFromExtension(extension));
            actual.put(extension, mimeMap.guessMimeTypeFromExtension(extension));
        }
        assertEquals(expected, actual);
    }

    /**
     * Checks that MimeTypeMap and URLConnection.getFileNameMap()'s behavior is
     * consistent with MimeMap.getDefault(), i.e. that they are implemented on
     * top of MimeMap.getDefault().
     */
    @Test public void agreesWithPublicApi() {
        android.webkit.MimeTypeMap webkitMap = android.webkit.MimeTypeMap.getSingleton();
        FileNameMap urlConnectionMap = URLConnection.getFileNameMap();

        for (String extension : mimeMap.extensions()) {
            String mimeType = mimeMap.guessMimeTypeFromExtension(extension);
            Objects.requireNonNull(mimeType);
            assertEquals(mimeType, webkitMap.getMimeTypeFromExtension(extension));
            assertTrue(webkitMap.hasExtension(extension));
            assertEquals(mimeType, urlConnectionMap.getContentTypeFor("filename." + extension));
        }

        for (String mimeType : mimeMap.mimeTypes()) {
            String extension = mimeMap.guessExtensionFromMimeType(mimeType);
            Objects.requireNonNull(extension);
            assertEquals(extension, webkitMap.getExtensionFromMimeType(mimeType));
            assertTrue(webkitMap.hasMimeType(mimeType));
        }
    }

    private void checkInvalidExtension(String s) {
        assertFalse(mimeMap.hasExtension(s));
        assertNull(mimeMap.guessMimeTypeFromExtension(s));
    }

    private void checkInvalidMimeType(String s) {
        assertFalse(mimeMap.hasMimeType(s));
        assertNull(mimeMap.guessExtensionFromMimeType(s));
    }

    private void assertMimeTypeFromExtension(String mimeType, String extension) {
        final String actual = mimeMap.guessMimeTypeFromExtension(extension);
        if (!Objects.equals(mimeType, actual)) {
            fail("Expected " + mimeType + " but was " + actual + " for extension " + extension);
        }
    }

    private void assertExtensionFromMimeType(String extension, String mimeType) {
        final String actual = mimeMap.guessExtensionFromMimeType(mimeType);
        if (!Objects.equals(extension, actual)) {
            fail("Expected " + extension + " but was " + actual + " for type " + mimeType);
        }
    }

    private void assertBidirectional(String mimeType, String extension) {
        assertMimeTypeFromExtension(mimeType, extension);
        assertExtensionFromMimeType(extension, mimeType);
    }

    private static String extractMajorMimeType(String mimeType) {
        final int slash = mimeType.indexOf('/');
        return (slash != -1) ? mimeType.substring(0, slash) : mimeType;
    }
}
