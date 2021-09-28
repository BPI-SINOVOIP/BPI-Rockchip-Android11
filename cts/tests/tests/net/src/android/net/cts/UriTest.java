/*
 * Copyright (C) 2008 The Android Open Source Project
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

package android.net.cts;

import android.content.ContentUris;
import android.net.Uri;
import android.os.Parcel;
import android.test.AndroidTestCase;
import java.io.File;
import java.util.Arrays;
import java.util.ArrayList;

public class UriTest extends AndroidTestCase {
    public void testParcelling() {
        parcelAndUnparcel(Uri.parse("foo:bob%20lee"));
        parcelAndUnparcel(Uri.fromParts("foo", "bob lee", "fragment"));
        parcelAndUnparcel(new Uri.Builder()
             .scheme("http")
            .authority("crazybob.org")
            .path("/rss/")
            .encodedQuery("a=b")
            .fragment("foo")
            .build());
     }

    private void parcelAndUnparcel(Uri u) {
        Parcel p = Parcel.obtain();
        Uri.writeToParcel(p, u);
        p.setDataPosition(0);
        assertEquals(u, Uri.CREATOR.createFromParcel(p));

        p.setDataPosition(0);
        u = u.buildUpon().build();
        Uri.writeToParcel(p, u);
        p.setDataPosition(0);
        assertEquals(u, Uri.CREATOR.createFromParcel(p));
    }

    public void testBuildUpon() {
        Uri u = Uri.parse("bob:lee").buildUpon().scheme("robert").build();
        assertEquals("robert", u.getScheme());
        assertEquals("lee", u.getEncodedSchemeSpecificPart());
        assertEquals("lee", u.getSchemeSpecificPart());
        assertNull(u.getQuery());
        assertNull(u.getPath());
        assertNull(u.getAuthority());
        assertNull(u.getHost());

        Uri a = Uri.fromParts("foo", "bar", "tee");
        Uri b = a.buildUpon().fragment("new").build();
        assertEquals("new", b.getFragment());
        assertEquals("bar", b.getSchemeSpecificPart());
        assertEquals("foo", b.getScheme());
        a = new Uri.Builder()
                .scheme("foo")
                .encodedOpaquePart("bar")
                .fragment("tee")
                .build();
        b = a.buildUpon().fragment("new").build();
        assertEquals("new", b.getFragment());
        assertEquals("bar", b.getSchemeSpecificPart());
        assertEquals("foo", b.getScheme());

        a = Uri.fromParts("scheme", "[2001:db8::dead:e1f]/foo", "bar");
        b = a.buildUpon().fragment("qux").build();
        assertEquals("qux", b.getFragment());
        assertEquals("[2001:db8::dead:e1f]/foo", b.getSchemeSpecificPart());
        assertEquals("scheme", b.getScheme());
    }

    public void testStringUri() {
        assertEquals("bob lee",
                Uri.parse("foo:bob%20lee").getSchemeSpecificPart());
        assertEquals("bob%20lee",
                Uri.parse("foo:bob%20lee").getEncodedSchemeSpecificPart());

        assertEquals("/bob%20lee",
                Uri.parse("foo:/bob%20lee").getEncodedPath());
        assertNull(Uri.parse("foo:bob%20lee").getPath());

        assertEquals("bob%20lee",
                Uri.parse("foo:?bob%20lee").getEncodedQuery());
        assertNull(Uri.parse("foo:bob%20lee").getEncodedQuery());
        assertNull(Uri.parse("foo:bar#?bob%20lee").getQuery());

        assertEquals("bob%20lee",
                Uri.parse("foo:#bob%20lee").getEncodedFragment());

        Uri uri = Uri.parse("http://localhost:42");
        assertEquals("localhost", uri.getHost());
        assertEquals(42, uri.getPort());

        uri = Uri.parse("http://bob@localhost:42");
        assertEquals("bob", uri.getUserInfo());
        assertEquals("localhost", uri.getHost());
        assertEquals(42, uri.getPort());

        uri = Uri.parse("http://bob%20lee@localhost:42");
        assertEquals("bob lee", uri.getUserInfo());
        assertEquals("bob%20lee", uri.getEncodedUserInfo());

        uri = Uri.parse("http://localhost");
        assertEquals("localhost", uri.getHost());
        assertEquals(-1, uri.getPort());

        uri = Uri.parse("http://a:a@example.com:a@example2.com/path");
        assertEquals("a:a@example.com:a@example2.com", uri.getAuthority());
        assertEquals("example2.com", uri.getHost());
        assertEquals(-1, uri.getPort());
        assertEquals("/path", uri.getPath());

        uri = Uri.parse("http://a.foo.com\\.example.com/path");
        assertEquals("a.foo.com", uri.getHost());
        assertEquals(-1, uri.getPort());
        assertEquals("\\.example.com/path", uri.getPath());

        uri = Uri.parse("https://[2001:db8::dead:e1f]/foo");
        assertEquals("[2001:db8::dead:e1f]", uri.getAuthority());
        assertNull(uri.getUserInfo());
        assertEquals("[2001:db8::dead:e1f]", uri.getHost());
        assertEquals(-1, uri.getPort());
        assertEquals("/foo", uri.getPath());
        assertEquals(null, uri.getFragment());
        assertEquals("//[2001:db8::dead:e1f]/foo", uri.getSchemeSpecificPart());

        uri = Uri.parse("https://[2001:db8::dead:e1f]/#foo");
        assertEquals("[2001:db8::dead:e1f]", uri.getAuthority());
        assertNull(uri.getUserInfo());
        assertEquals("[2001:db8::dead:e1f]", uri.getHost());
        assertEquals(-1, uri.getPort());
        assertEquals("/", uri.getPath());
        assertEquals("foo", uri.getFragment());
        assertEquals("//[2001:db8::dead:e1f]/", uri.getSchemeSpecificPart());

        uri = Uri.parse(
                "https://some:user@[2001:db8::dead:e1f]:1234/foo?corge=thud&corge=garp#bar");
        assertEquals("some:user@[2001:db8::dead:e1f]:1234", uri.getAuthority());
        assertEquals("some:user", uri.getUserInfo());
        assertEquals("[2001:db8::dead:e1f]", uri.getHost());
        assertEquals(1234, uri.getPort());
        assertEquals("/foo", uri.getPath());
        assertEquals("bar", uri.getFragment());
        assertEquals("//some:user@[2001:db8::dead:e1f]:1234/foo?corge=thud&corge=garp",
                uri.getSchemeSpecificPart());
        assertEquals("corge=thud&corge=garp", uri.getQuery());
        assertEquals("thud", uri.getQueryParameter("corge"));
        assertEquals(Arrays.asList("thud", "garp"), uri.getQueryParameters("corge"));
    }

    public void testCompareTo() {
        Uri a = Uri.parse("foo:a");
        Uri b = Uri.parse("foo:b");
        Uri b2 = Uri.parse("foo:b");

        assertTrue(a.compareTo(b) < 0);
        assertTrue(b.compareTo(a) > 0);
        assertEquals(0, b.compareTo(b2));
    }

    public void testEqualsAndHashCode() {
        Uri a = Uri.parse("http://crazybob.org/test/?foo=bar#tee");

        Uri b = new Uri.Builder()
                .scheme("http")
                .authority("crazybob.org")
                .path("/test/")
                .encodedQuery("foo=bar")
                .fragment("tee")
                .build();

        // Try alternate builder methods.
        Uri c = new Uri.Builder()
                .scheme("http")
                .encodedAuthority("crazybob.org")
                .encodedPath("/test/")
                .encodedQuery("foo=bar")
                .encodedFragment("tee")
                .build();

        assertFalse(Uri.EMPTY.equals(null));
        assertEquals(a, b);
        assertEquals(b, c);
        assertEquals(c, a);
        assertEquals(a.hashCode(), b.hashCode());
        assertEquals(b.hashCode(), c.hashCode());
    }

    public void testEncodeAndDecode() {
        String encoded = Uri.encode("Bob:/", "/");
        assertEquals(-1, encoded.indexOf(':'));
        assertTrue(encoded.indexOf('/') > -1);
        assertEncodeDecodeRoundtripExact(null);
        assertEncodeDecodeRoundtripExact("");
        assertEncodeDecodeRoundtripExact("Bob");
        assertEncodeDecodeRoundtripExact(":Bob");
        assertEncodeDecodeRoundtripExact("::Bob");
        assertEncodeDecodeRoundtripExact("Bob::Lee");
        assertEncodeDecodeRoundtripExact("Bob:Lee");
        assertEncodeDecodeRoundtripExact("Bob::");
        assertEncodeDecodeRoundtripExact("Bob:");
        assertEncodeDecodeRoundtripExact("::Bob::");
        assertEncodeDecodeRoundtripExact("https:/some:user@[2001:db8::dead:e1f]:1234/foo#bar");
    }

    private static void assertEncodeDecodeRoundtripExact(String s) {
        assertEquals(s, Uri.decode(Uri.encode(s, null)));
    }

    public void testDecode_emptyString_returnsEmptyString() {
        assertEquals("", Uri.decode(""));
    }

    public void testDecode_null_returnsNull() {
        assertNull(Uri.decode(null));
    }

    public void testDecode_wrongHexDigit() {
        // %p in the end.
        assertEquals("ab/$\u0102%\u0840\uFFFD\u0000", Uri.decode("ab%2f$%C4%82%25%e0%a1%80%p"));
    }

    public void testDecode_secondHexDigitWrong() {
        // %1p in the end.
        assertEquals("ab/$\u0102%\u0840\uFFFD\u0001", Uri.decode("ab%2f$%c4%82%25%e0%a1%80%1p"));
    }

    public void testDecode_endsWithPercent_appendsUnknownCharacter() {
        // % in the end.
        assertEquals("ab/$\u0102%\u0840\uFFFD", Uri.decode("ab%2f$%c4%82%25%e0%a1%80%"));
    }

    public void testDecode_plusNotConverted() {
        assertEquals("ab/$\u0102%+\u0840", Uri.decode("ab%2f$%c4%82%25+%e0%a1%80"));
    }

    // Last character needs decoding (make sure we are flushing the buffer with chars to decode).
    public void testDecode_lastCharacter() {
        assertEquals("ab/$\u0102%\u0840", Uri.decode("ab%2f$%c4%82%25%e0%a1%80"));
    }

    // Check that a second row of encoded characters is decoded properly (internal buffers are
    // reset properly).
    public void testDecode_secondRowOfEncoded() {
        assertEquals("ab/$\u0102%\u0840aa\u0840",
                Uri.decode("ab%2f$%c4%82%25%e0%a1%80aa%e0%a1%80"));
    }

    public void testFromFile() {
        File f = new File("/tmp/bob");
        Uri uri = Uri.fromFile(f);
        assertEquals("file:///tmp/bob", uri.toString());
        try {
            Uri.fromFile(null);
            fail("testFile fail");
            } catch (NullPointerException e) {}
    }

    public void testQueryParameters() {
        Uri uri = Uri.parse("content://user");
        assertEquals(null, uri.getQueryParameter("a"));

        uri = uri.buildUpon().appendQueryParameter("a", "b").build();
        assertEquals("b", uri.getQueryParameter("a"));

        uri = uri.buildUpon().appendQueryParameter("a", "b2").build();
        assertEquals(Arrays.asList("b", "b2"), uri.getQueryParameters("a"));

        uri = uri.buildUpon().appendQueryParameter("c", "d").build();
        assertEquals(Arrays.asList("b", "b2"), uri.getQueryParameters("a"));
        assertEquals("d", uri.getQueryParameter("c"));
    }

    public void testPathOperations() {
        Uri uri = Uri.parse("content://user/a/b");

        assertEquals(2, uri.getPathSegments().size());
        assertEquals("a", uri.getPathSegments().get(0));
        assertEquals("b", uri.getPathSegments().get(1));
        assertEquals("b", uri.getLastPathSegment());

        Uri first = uri;
        uri = uri.buildUpon().appendPath("c").build();
        assertEquals(3, uri.getPathSegments().size());
        assertEquals("c", uri.getPathSegments().get(2));
        assertEquals("c", uri.getLastPathSegment());
        assertEquals("content://user/a/b/c", uri.toString());

        uri = ContentUris.withAppendedId(uri, 100);
        assertEquals(4, uri.getPathSegments().size());
        assertEquals("100", uri.getPathSegments().get(3));
        assertEquals("100", uri.getLastPathSegment());
        assertEquals(100, ContentUris.parseId(uri));
        assertEquals("content://user/a/b/c/100", uri.toString());

        // Make sure the original URI is still intact.
        assertEquals(2, first.getPathSegments().size());
        assertEquals("b", first.getLastPathSegment());

        try {
        first.getPathSegments().get(2);
        fail("test path operations");
        } catch (IndexOutOfBoundsException e) {}

        assertEquals(null, Uri.EMPTY.getLastPathSegment());

        Uri withC = Uri.parse("foo:/a/b/").buildUpon().appendPath("c").build();
        assertEquals("/a/b/c", withC.getPath());
    }

    public void testOpaqueUri() {
        Uri uri = Uri.parse("mailto:nobody");
        testOpaqueUri(uri);

        uri = uri.buildUpon().build();
        testOpaqueUri(uri);

        uri = Uri.fromParts("mailto", "nobody", null);
        testOpaqueUri(uri);

        uri = uri.buildUpon().build();
        testOpaqueUri(uri);

        uri = new Uri.Builder()
                .scheme("mailto")
                .opaquePart("nobody")
                .build();
        testOpaqueUri(uri);

        uri = uri.buildUpon().build();
        testOpaqueUri(uri);
    }

    private void testOpaqueUri(Uri uri) {
        assertEquals("mailto", uri.getScheme());
        assertEquals("nobody", uri.getSchemeSpecificPart());
        assertEquals("nobody", uri.getEncodedSchemeSpecificPart());

        assertNull(uri.getFragment());
        assertTrue(uri.isAbsolute());
        assertTrue(uri.isOpaque());
        assertFalse(uri.isRelative());
        assertFalse(uri.isHierarchical());

        assertNull(uri.getAuthority());
        assertNull(uri.getEncodedAuthority());
        assertNull(uri.getPath());
        assertNull(uri.getEncodedPath());
        assertNull(uri.getUserInfo());
        assertNull(uri.getEncodedUserInfo());
        assertNull(uri.getQuery());
        assertNull(uri.getEncodedQuery());
        assertNull(uri.getHost());
        assertEquals(-1, uri.getPort());

        assertTrue(uri.getPathSegments().isEmpty());
        assertNull(uri.getLastPathSegment());

        assertEquals("mailto:nobody", uri.toString());

        Uri withFragment = uri.buildUpon().fragment("top").build();
        assertEquals("mailto:nobody#top", withFragment.toString());
    }

    public void testHierarchicalUris() {
        testHierarchical("http", "google.com", "/p1/p2", "query", "fragment");
        testHierarchical("file", null, "/p1/p2", null, null);
        testHierarchical("content", "contact", "/p1/p2", null, null);
        testHierarchical("http", "google.com", "/p1/p2", null, "fragment");
        testHierarchical("http", "google.com", "", null, "fragment");
        testHierarchical("http", "google.com", "", "query", "fragment");
        testHierarchical("http", "google.com", "", "query", null);
        testHierarchical("http", null, "/", "query", null);
    }

    private static void testHierarchical(String scheme, String authority,
        String path, String query, String fragment) {
        StringBuilder sb = new StringBuilder();

        if (authority != null) {
            sb.append("//").append(authority);
        }
        if (path != null) {
            sb.append(path);
        }
        if (query != null) {
            sb.append('?').append(query);
        }

        String ssp = sb.toString();

        if (scheme != null) {
            sb.insert(0, scheme + ":");
        }
        if (fragment != null) {
            sb.append('#').append(fragment);
        }

        String uriString = sb.toString();

        Uri uri = Uri.parse(uriString);

        // Run these twice to test caching.
        compareHierarchical(
        uriString, ssp, uri, scheme, authority, path, query, fragment);
        compareHierarchical(
        uriString, ssp, uri, scheme, authority, path, query, fragment);

        // Test rebuilt version.
        uri = uri.buildUpon().build();

        // Run these twice to test caching.
        compareHierarchical(
                uriString, ssp, uri, scheme, authority, path, query, fragment);
        compareHierarchical(
                uriString, ssp, uri, scheme, authority, path, query, fragment);

        // The decoded and encoded versions of the inputs are all the same.
        // We'll test the actual encoding decoding separately.

        // Test building with encoded versions.
        Uri built = new Uri.Builder()
            .scheme(scheme)
                .encodedAuthority(authority)
                .encodedPath(path)
                .encodedQuery(query)
                .encodedFragment(fragment)
                .build();

        compareHierarchical(
                uriString, ssp, built, scheme, authority, path, query, fragment);
        compareHierarchical(
                uriString, ssp, built, scheme, authority, path, query, fragment);

        // Test building with decoded versions.
        built = new Uri.Builder()
                .scheme(scheme)
                .authority(authority)
                .path(path)
                .query(query)
                .fragment(fragment)
                .build();

        compareHierarchical(
                uriString, ssp, built, scheme, authority, path, query, fragment);
        compareHierarchical(
                uriString, ssp, built, scheme, authority, path, query, fragment);

        // Rebuild.
        built = built.buildUpon().build();

        compareHierarchical(
                uriString, ssp, built, scheme, authority, path, query, fragment);
        compareHierarchical(
                uriString, ssp, built, scheme, authority, path, query, fragment);
    }

    private static void compareHierarchical(String uriString, String ssp,
        Uri uri,
        String scheme, String authority, String path, String query,
        String fragment) {
        assertEquals(scheme, uri.getScheme());
        assertEquals(authority, uri.getAuthority());
        assertEquals(authority, uri.getEncodedAuthority());
        assertEquals(path, uri.getPath());
        assertEquals(path, uri.getEncodedPath());
        assertEquals(query, uri.getQuery());
        assertEquals(query, uri.getEncodedQuery());
        assertEquals(fragment, uri.getFragment());
        assertEquals(fragment, uri.getEncodedFragment());
        assertEquals(ssp, uri.getSchemeSpecificPart());

        if (scheme != null) {
            assertTrue(uri.isAbsolute());
            assertFalse(uri.isRelative());
        } else {
            assertFalse(uri.isAbsolute());
            assertTrue(uri.isRelative());
        }

        assertFalse(uri.isOpaque());
        assertTrue(uri.isHierarchical());
        assertEquals(uriString, uri.toString());
    }

    public void testNormalizeScheme() {
        assertEquals(Uri.parse(""), Uri.parse("").normalizeScheme());
        assertEquals(Uri.parse("http://www.android.com"),
                Uri.parse("http://www.android.com").normalizeScheme());
        assertEquals(Uri.parse("http://USER@WWW.ANDROID.COM:100/ABOUT?foo=blah@bar=bleh#c"),
                Uri.parse("HTTP://USER@WWW.ANDROID.COM:100/ABOUT?foo=blah@bar=bleh#c")
                        .normalizeScheme());
    }

    public void testToSafeString_tel() {
        checkToSafeString("tel:xxxxxx", "tel:Google");
        checkToSafeString("tel:xxxxxxxxxx", "tel:1234567890");
        checkToSafeString("tEl:xxx.xxx-xxxx", "tEl:123.456-7890");
    }

    public void testToSafeString_sip() {
        checkToSafeString("sip:xxxxxxx@xxxxxxx.xxxxxxxx", "sip:android@android.com:1234");
        checkToSafeString("sIp:xxxxxxx@xxxxxxx.xxx", "sIp:android@android.com");
    }

    public void testToSafeString_sms() {
        checkToSafeString("sms:xxxxxx", "sms:123abc");
        checkToSafeString("smS:xxx.xxx-xxxx", "smS:123.456-7890");
    }

    public void testToSafeString_smsto() {
        checkToSafeString("smsto:xxxxxx", "smsto:123abc");
        checkToSafeString("SMSTo:xxx.xxx-xxxx", "SMSTo:123.456-7890");
    }

    public void testToSafeString_mailto() {
        checkToSafeString("mailto:xxxxxxx@xxxxxxx.xxx", "mailto:android@android.com");
        checkToSafeString("Mailto:xxxxxxx@xxxxxxx.xxxxxxxxxx",
                "Mailto:android@android.com/secret");
    }

    public void testToSafeString_nfc() {
        checkToSafeString("nfc:xxxxxx", "nfc:123abc");
        checkToSafeString("nfc:xxx.xxx-xxxx", "nfc:123.456-7890");
        checkToSafeString("nfc:xxxxxxx@xxxxxxx.xxx", "nfc:android@android.com");
    }

    public void testToSafeString_http() {
        checkToSafeString("http://www.android.com/...", "http://www.android.com");
        checkToSafeString("HTTP://www.android.com/...", "HTTP://www.android.com");
        checkToSafeString("http://www.android.com/...", "http://www.android.com/");
        checkToSafeString("http://www.android.com/...", "http://www.android.com/secretUrl?param");
        checkToSafeString("http://www.android.com/...",
                "http://user:pwd@www.android.com/secretUrl?param");
        checkToSafeString("http://www.android.com/...",
                "http://user@www.android.com/secretUrl?param");
        checkToSafeString("http://www.android.com/...", "http://www.android.com/secretUrl?param");
        checkToSafeString("http:///...", "http:///path?param");
        checkToSafeString("http:///...", "http://");
        checkToSafeString("http://:12345/...", "http://:12345/");
    }

    public void testToSafeString_https() {
        checkToSafeString("https://www.android.com/...", "https://www.android.com/secretUrl?param");
        checkToSafeString("https://www.android.com:8443/...",
                "https://user:pwd@www.android.com:8443/secretUrl?param");
        checkToSafeString("https://www.android.com/...", "https://user:pwd@www.android.com");
        checkToSafeString("Https://www.android.com/...", "Https://user:pwd@www.android.com");
    }

    public void testToSafeString_ftp() {
        checkToSafeString("ftp://ftp.android.com/...", "ftp://ftp.android.com/");
        checkToSafeString("ftP://ftp.android.com/...", "ftP://anonymous@ftp.android.com/");
        checkToSafeString("ftp://ftp.android.com:2121/...",
                "ftp://root:love@ftp.android.com:2121/");
    }

    public void testToSafeString_rtsp() {
        checkToSafeString("rtsp://rtsp.android.com/...", "rtsp://rtsp.android.com/");
        checkToSafeString("rtsp://rtsp.android.com/...", "rtsp://rtsp.android.com/video.mov");
        checkToSafeString("rtsp://rtsp.android.com/...", "rtsp://rtsp.android.com/video.mov?param");
        checkToSafeString("RtsP://rtsp.android.com/...", "RtsP://anonymous@rtsp.android.com/");
        checkToSafeString("rtsp://rtsp.android.com:2121/...",
                "rtsp://username:password@rtsp.android.com:2121/");
    }

    public void testToSafeString_notSupport() {
        checkToSafeString("unsupported://ajkakjah/askdha/secret?secret",
                "unsupported://ajkakjah/askdha/secret?secret");
        checkToSafeString("unsupported:ajkakjah/askdha/secret?secret",
                "unsupported:ajkakjah/askdha/secret?secret");
    }

    private void checkToSafeString(String expectedSafeString, String original) {
        assertEquals(expectedSafeString, Uri.parse(original).toSafeString());
    }
}
