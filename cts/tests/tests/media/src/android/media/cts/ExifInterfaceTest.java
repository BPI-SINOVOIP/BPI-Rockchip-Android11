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

package android.media.cts;

import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.ExifInterface;
import android.os.Environment;
import android.os.FileUtils;
import android.os.StrictMode;
import android.platform.test.annotations.AppModeFull;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.test.AndroidTestCase;
import android.util.Log;

import libcore.io.IoUtils;

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.EOFException;
import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;

@NonMediaMainlineTest
@AppModeFull(reason = "Instant apps cannot access the SD card")
public class ExifInterfaceTest extends AndroidTestCase {
    private static final String TAG = ExifInterface.class.getSimpleName();
    private static final boolean VERBOSE = false;  // lots of logging

    private static final double DIFFERENCE_TOLERANCE = .001;

    private static final String EXTERNAL_BASE_DIRECTORY = "/test/images/";

    // This base directory is needed for the files listed below.
    // These files will be available for download in Android O release.
    // Link: https://source.android.com/compatibility/cts/downloads.html#cts-media-files
    private static final String EXIF_BYTE_ORDER_II_JPEG = "image_exif_byte_order_ii.jpg";
    private static final String EXIF_BYTE_ORDER_MM_JPEG = "image_exif_byte_order_mm.jpg";
    private static final String LG_G4_ISO_800_DNG = "lg_g4_iso_800.dng";
    private static final String LG_G4_ISO_800_JPG = "lg_g4_iso_800.jpg";
    private static final String SONY_RX_100_ARW = "sony_rx_100.arw";
    private static final String CANON_G7X_CR2 = "canon_g7x.cr2";
    private static final String FUJI_X20_RAF = "fuji_x20.raf";
    private static final String NIKON_1AW1_NEF = "nikon_1aw1.nef";
    private static final String NIKON_P330_NRW = "nikon_p330.nrw";
    private static final String OLYMPUS_E_PL3_ORF = "olympus_e_pl3.orf";
    private static final String PANASONIC_GM5_RW2 = "panasonic_gm5.rw2";
    private static final String PENTAX_K5_PEF = "pentax_k5.pef";
    private static final String SAMSUNG_NX3000_SRW = "samsung_nx3000.srw";
    private static final String VOLANTIS_JPEG = "volantis.jpg";
    private static final String WEBP_WITH_EXIF = "webp_with_exif.webp";
    private static final String PNG_WITH_EXIF_BYTE_ORDER_II = "png_with_exif_byte_order_ii.png";
    private static final String PNG_WITHOUT_EXIF = "png_without_exif.png";

    private static final String[] EXIF_TAGS = {
            ExifInterface.TAG_MAKE,
            ExifInterface.TAG_MODEL,
            ExifInterface.TAG_F_NUMBER,
            ExifInterface.TAG_DATETIME_ORIGINAL,
            ExifInterface.TAG_EXPOSURE_TIME,
            ExifInterface.TAG_FLASH,
            ExifInterface.TAG_FOCAL_LENGTH,
            ExifInterface.TAG_GPS_ALTITUDE,
            ExifInterface.TAG_GPS_ALTITUDE_REF,
            ExifInterface.TAG_GPS_DATESTAMP,
            ExifInterface.TAG_GPS_LATITUDE,
            ExifInterface.TAG_GPS_LATITUDE_REF,
            ExifInterface.TAG_GPS_LONGITUDE,
            ExifInterface.TAG_GPS_LONGITUDE_REF,
            ExifInterface.TAG_GPS_PROCESSING_METHOD,
            ExifInterface.TAG_GPS_TIMESTAMP,
            ExifInterface.TAG_IMAGE_LENGTH,
            ExifInterface.TAG_IMAGE_WIDTH,
            ExifInterface.TAG_ISO_SPEED_RATINGS,
            ExifInterface.TAG_ORIENTATION,
            ExifInterface.TAG_WHITE_BALANCE
    };

    private static class ExpectedValue {
        // Thumbnail information.
        public final boolean hasThumbnail;
        public final int thumbnailWidth;
        public final int thumbnailHeight;
        public final boolean isThumbnailCompressed;
        public final int thumbnailOffset;
        public final int thumbnailLength;

        // GPS information.
        public final boolean hasLatLong;
        public final float latitude;
        public final int latitudeOffset;
        public final int latitudeLength;
        public final float longitude;
        public final float altitude;

        // Make information
        public final boolean hasMake;
        public final int makeOffset;
        public final int makeLength;
        public final String make;

        // Values.
        public final String model;
        public final float aperture;
        public final String dateTimeOriginal;
        public final float exposureTime;
        public final float flash;
        public final String focalLength;
        public final String gpsAltitude;
        public final String gpsAltitudeRef;
        public final String gpsDatestamp;
        public final String gpsLatitude;
        public final String gpsLatitudeRef;
        public final String gpsLongitude;
        public final String gpsLongitudeRef;
        public final String gpsProcessingMethod;
        public final String gpsTimestamp;
        public final int imageLength;
        public final int imageWidth;
        public final String iso;
        public final int orientation;
        public final int whiteBalance;

        // XMP information.
        public final boolean hasXmp;
        public final int xmpOffset;
        public final int xmpLength;

        private static String getString(TypedArray typedArray, int index) {
            String stringValue = typedArray.getString(index);
            if (stringValue == null || stringValue.equals("")) {
                return null;
            }
            return stringValue.trim();
        }

        public ExpectedValue(TypedArray typedArray) {
            int index = 0;

            // Reads thumbnail information.
            hasThumbnail = typedArray.getBoolean(index++, false);
            thumbnailOffset = typedArray.getInt(index++, -1);
            thumbnailLength = typedArray.getInt(index++, -1);
            thumbnailWidth = typedArray.getInt(index++, 0);
            thumbnailHeight = typedArray.getInt(index++, 0);
            isThumbnailCompressed = typedArray.getBoolean(index++, false);

            // Reads GPS information.
            hasLatLong = typedArray.getBoolean(index++, false);
            latitudeOffset = typedArray.getInt(index++, -1);
            latitudeLength = typedArray.getInt(index++, -1);
            latitude = typedArray.getFloat(index++, 0f);
            longitude = typedArray.getFloat(index++, 0f);
            altitude = typedArray.getFloat(index++, 0f);

            // Reads Make information.
            hasMake = typedArray.getBoolean(index++, false);
            makeOffset = typedArray.getInt(index++, -1);
            makeLength = typedArray.getInt(index++, -1);
            make = getString(typedArray, index++);

            // Reads values.
            model = getString(typedArray, index++);
            aperture = typedArray.getFloat(index++, 0f);
            dateTimeOriginal = getString(typedArray, index++);
            exposureTime = typedArray.getFloat(index++, 0f);
            flash = typedArray.getFloat(index++, 0f);
            focalLength = getString(typedArray, index++);
            gpsAltitude = getString(typedArray, index++);
            gpsAltitudeRef = getString(typedArray, index++);
            gpsDatestamp = getString(typedArray, index++);
            gpsLatitude = getString(typedArray, index++);
            gpsLatitudeRef = getString(typedArray, index++);
            gpsLongitude = getString(typedArray, index++);
            gpsLongitudeRef = getString(typedArray, index++);
            gpsProcessingMethod = getString(typedArray, index++);
            gpsTimestamp = getString(typedArray, index++);
            imageLength = typedArray.getInt(index++, 0);
            imageWidth = typedArray.getInt(index++, 0);
            iso = getString(typedArray, index++);
            orientation = typedArray.getInt(index++, 0);
            whiteBalance = typedArray.getInt(index++, 0);

            // Reads XMP information.
            hasXmp = typedArray.getBoolean(index++, false);
            xmpOffset = typedArray.getInt(index++, 0);
            xmpLength = typedArray.getInt(index++, 0);

            typedArray.recycle();
        }
    }

    private void printExifTagsAndValues(String fileName, ExifInterface exifInterface) {
        // Prints thumbnail information.
        if (exifInterface.hasThumbnail()) {
            byte[] thumbnailBytes = exifInterface.getThumbnailBytes();
            if (thumbnailBytes != null) {
                Log.v(TAG, fileName + " Thumbnail size = " + thumbnailBytes.length);
                Bitmap bitmap = exifInterface.getThumbnailBitmap();
                if (bitmap == null) {
                    Log.e(TAG, fileName + " Corrupted thumbnail!");
                } else {
                    Log.v(TAG, fileName + " Thumbnail size: " + bitmap.getWidth() + ", "
                            + bitmap.getHeight());
                }
            } else {
                Log.e(TAG, fileName + " Unexpected result: No thumbnails were found. "
                        + "A thumbnail is expected.");
            }
        } else {
            if (exifInterface.getThumbnailBytes() != null) {
                Log.e(TAG, fileName + " Unexpected result: A thumbnail was found. "
                        + "No thumbnail is expected.");
            } else {
                Log.v(TAG, fileName + " No thumbnail");
            }
        }

        // Prints GPS information.
        Log.v(TAG, fileName + " Altitude = " + exifInterface.getAltitude(.0));

        float[] latLong = new float[2];
        if (exifInterface.getLatLong(latLong)) {
            Log.v(TAG, fileName + " Latitude = " + latLong[0]);
            Log.v(TAG, fileName + " Longitude = " + latLong[1]);
        } else {
            Log.v(TAG, fileName + " No latlong data");
        }

        // Prints values.
        for (String tagKey : EXIF_TAGS) {
            String tagValue = exifInterface.getAttribute(tagKey);
            Log.v(TAG, fileName + " Key{" + tagKey + "} = '" + tagValue + "'");
        }
    }

    private void assertIntTag(ExifInterface exifInterface, String tag, int expectedValue) {
        int intValue = exifInterface.getAttributeInt(tag, 0);
        assertEquals(expectedValue, intValue);
    }

    private void assertFloatTag(ExifInterface exifInterface, String tag, float expectedValue) {
        double doubleValue = exifInterface.getAttributeDouble(tag, 0.0);
        assertEquals(expectedValue, doubleValue, DIFFERENCE_TOLERANCE);
    }

    private void assertStringTag(ExifInterface exifInterface, String tag, String expectedValue) {
        String stringValue = exifInterface.getAttribute(tag);
        if (stringValue != null) {
            stringValue = stringValue.trim();
        }
        stringValue = (stringValue == "") ? null : stringValue;

        assertEquals(expectedValue, stringValue);
    }

    private void compareWithExpectedValue(ExifInterface exifInterface,
            ExpectedValue expectedValue, String verboseTag, boolean assertRanges) {
        if (VERBOSE) {
            printExifTagsAndValues(verboseTag, exifInterface);
        }
        // Checks a thumbnail image.
        assertEquals(expectedValue.hasThumbnail, exifInterface.hasThumbnail());
        if (expectedValue.hasThumbnail) {
            assertNotNull(exifInterface.getThumbnailRange());
            if (assertRanges) {
                final long[] thumbnailRange = exifInterface.getThumbnailRange();
                assertEquals(expectedValue.thumbnailOffset, thumbnailRange[0]);
                assertEquals(expectedValue.thumbnailLength, thumbnailRange[1]);
            }
            testThumbnail(expectedValue, exifInterface);
        } else {
            assertNull(exifInterface.getThumbnailRange());
            assertNull(exifInterface.getThumbnail());
        }

        // Checks GPS information.
        float[] latLong = new float[2];
        assertEquals(expectedValue.hasLatLong, exifInterface.getLatLong(latLong));
        if (expectedValue.hasLatLong) {
            assertNotNull(exifInterface.getAttributeRange(ExifInterface.TAG_GPS_LATITUDE));
            if (assertRanges) {
                final long[] latitudeRange = exifInterface
                        .getAttributeRange(ExifInterface.TAG_GPS_LATITUDE);
                assertEquals(expectedValue.latitudeOffset, latitudeRange[0]);
                assertEquals(expectedValue.latitudeLength, latitudeRange[1]);
            }
            assertEquals(expectedValue.latitude, latLong[0], DIFFERENCE_TOLERANCE);
            assertEquals(expectedValue.longitude, latLong[1], DIFFERENCE_TOLERANCE);
            assertTrue(exifInterface.hasAttribute(ExifInterface.TAG_GPS_LATITUDE));
            assertTrue(exifInterface.hasAttribute(ExifInterface.TAG_GPS_LONGITUDE));
        } else {
            assertNull(exifInterface.getAttributeRange(ExifInterface.TAG_GPS_LATITUDE));
            assertFalse(exifInterface.hasAttribute(ExifInterface.TAG_GPS_LATITUDE));
            assertFalse(exifInterface.hasAttribute(ExifInterface.TAG_GPS_LONGITUDE));
        }
        assertEquals(expectedValue.altitude, exifInterface.getAltitude(.0), DIFFERENCE_TOLERANCE);

        // Checks Make information.
        String make = exifInterface.getAttribute(ExifInterface.TAG_MAKE);
        assertEquals(expectedValue.hasMake, make != null);
        if (expectedValue.hasMake) {
            assertNotNull(exifInterface.getAttributeRange(ExifInterface.TAG_MAKE));
            if (assertRanges) {
                final long[] makeRange = exifInterface
                        .getAttributeRange(ExifInterface.TAG_MAKE);
                assertEquals(expectedValue.makeOffset, makeRange[0]);
                assertEquals(expectedValue.makeLength, makeRange[1]);
            }
            assertEquals(expectedValue.make, make.trim());
        } else {
            assertNull(exifInterface.getAttributeRange(ExifInterface.TAG_MAKE));
            assertFalse(exifInterface.hasAttribute(ExifInterface.TAG_MAKE));
        }

        // Checks values.
        assertStringTag(exifInterface, ExifInterface.TAG_MAKE, expectedValue.make);
        assertStringTag(exifInterface, ExifInterface.TAG_MODEL, expectedValue.model);
        assertFloatTag(exifInterface, ExifInterface.TAG_F_NUMBER, expectedValue.aperture);
        assertStringTag(exifInterface, ExifInterface.TAG_DATETIME_ORIGINAL,
                expectedValue.dateTimeOriginal);
        assertFloatTag(exifInterface, ExifInterface.TAG_EXPOSURE_TIME, expectedValue.exposureTime);
        assertFloatTag(exifInterface, ExifInterface.TAG_FLASH, expectedValue.flash);
        assertStringTag(exifInterface, ExifInterface.TAG_FOCAL_LENGTH, expectedValue.focalLength);
        assertStringTag(exifInterface, ExifInterface.TAG_GPS_ALTITUDE, expectedValue.gpsAltitude);
        assertStringTag(exifInterface, ExifInterface.TAG_GPS_ALTITUDE_REF,
                expectedValue.gpsAltitudeRef);
        assertStringTag(exifInterface, ExifInterface.TAG_GPS_DATESTAMP, expectedValue.gpsDatestamp);
        assertStringTag(exifInterface, ExifInterface.TAG_GPS_LATITUDE, expectedValue.gpsLatitude);
        assertStringTag(exifInterface, ExifInterface.TAG_GPS_LATITUDE_REF,
                expectedValue.gpsLatitudeRef);
        assertStringTag(exifInterface, ExifInterface.TAG_GPS_LONGITUDE, expectedValue.gpsLongitude);
        assertStringTag(exifInterface, ExifInterface.TAG_GPS_LONGITUDE_REF,
                expectedValue.gpsLongitudeRef);
        assertStringTag(exifInterface, ExifInterface.TAG_GPS_PROCESSING_METHOD,
                expectedValue.gpsProcessingMethod);
        assertStringTag(exifInterface, ExifInterface.TAG_GPS_TIMESTAMP, expectedValue.gpsTimestamp);
        assertIntTag(exifInterface, ExifInterface.TAG_IMAGE_LENGTH, expectedValue.imageLength);
        assertIntTag(exifInterface, ExifInterface.TAG_IMAGE_WIDTH, expectedValue.imageWidth);
        assertStringTag(exifInterface, ExifInterface.TAG_ISO_SPEED_RATINGS, expectedValue.iso);
        assertIntTag(exifInterface, ExifInterface.TAG_ORIENTATION, expectedValue.orientation);
        assertIntTag(exifInterface, ExifInterface.TAG_WHITE_BALANCE, expectedValue.whiteBalance);

        if (expectedValue.hasXmp) {
            assertNotNull(exifInterface.getAttributeRange(ExifInterface.TAG_XMP));
            if (assertRanges) {
                final long[] xmpRange = exifInterface.getAttributeRange(ExifInterface.TAG_XMP);
                assertEquals(expectedValue.xmpOffset, xmpRange[0]);
                assertEquals(expectedValue.xmpLength, xmpRange[1]);
            }
            final String xmp = new String(exifInterface.getAttributeBytes(ExifInterface.TAG_XMP),
                    StandardCharsets.UTF_8);
            // We're only interested in confirming that we were able to extract
            // valid XMP data, which must always include this XML tag; a full
            // XMP parser is beyond the scope of ExifInterface. See XMP
            // Specification Part 1, Section C.2.2 for additional details.
            if (!xmp.contains("<rdf:RDF")) {
                fail("Invalid XMP: " + xmp);
            }
        } else {
            assertNull(exifInterface.getAttributeRange(ExifInterface.TAG_XMP));
        }
    }

    private void testExifInterfaceForStandalone(String fileName, int typedArrayResourceId)
            throws IOException {
        ExpectedValue expectedValue = new ExpectedValue(
                getContext().getResources().obtainTypedArray(typedArrayResourceId));

        // Test for reading from external data storage.
        fileName = EXTERNAL_BASE_DIRECTORY + fileName;

        File imageFile = new File(Environment.getExternalStorageDirectory(), fileName);
        String verboseTag = imageFile.getName();

        FileInputStream fis = new FileInputStream(imageFile);
        // Skip the following marker bytes (0xff, 0xd8, 0xff, 0xe1)
        fis.skip(4);
        // Read the value of the length of the exif data
        short length = readShort(fis);
        byte[] exifBytes = new byte[length];
        fis.read(exifBytes);

        ByteArrayInputStream bin = new ByteArrayInputStream(exifBytes);
        ExifInterface exifInterface =
                new ExifInterface(bin, ExifInterface.STREAM_TYPE_EXIF_DATA_ONLY);
        compareWithExpectedValue(exifInterface, expectedValue, verboseTag, true);
    }

    private void testExifInterfaceCommon(String fileName, ExpectedValue expectedValue)
            throws IOException {
        File imageFile = new File(Environment.getExternalStorageDirectory(), fileName);
        String verboseTag = imageFile.getName();

        // Creates via path.
        ExifInterface exifInterface = new ExifInterface(imageFile.getAbsolutePath());
        assertNotNull(exifInterface);
        compareWithExpectedValue(exifInterface, expectedValue, verboseTag, true);

        // Creates via file.
        exifInterface = new ExifInterface(imageFile);
        compareWithExpectedValue(exifInterface, expectedValue, verboseTag, true);

        InputStream in = null;
        // Creates via InputStream.
        try {
            in = new BufferedInputStream(new FileInputStream(imageFile.getAbsolutePath()));
            exifInterface = new ExifInterface(in);
            compareWithExpectedValue(exifInterface, expectedValue, verboseTag, true);
        } finally {
            IoUtils.closeQuietly(in);
        }

        // Creates via FileDescriptor.
        FileDescriptor fd = null;
        try {
            fd = Os.open(imageFile.getAbsolutePath(), OsConstants.O_RDONLY, 0600);
            exifInterface = new ExifInterface(fd);
            compareWithExpectedValue(exifInterface, expectedValue, verboseTag, true);
        } catch (ErrnoException e) {
            throw e.rethrowAsIOException();
        } finally {
            IoUtils.closeQuietly(fd);
        }
    }

    private void testExifInterfaceRange(String fileName, ExpectedValue expectedValue)
            throws IOException {
        File imageFile = new File(Environment.getExternalStorageDirectory(), fileName);
        InputStream in = null;
        try {
            in = new BufferedInputStream(new FileInputStream(imageFile.getAbsolutePath()));
            if (expectedValue.hasThumbnail) {
                in.skip(expectedValue.thumbnailOffset);
                byte[] thumbnailBytes = new byte[expectedValue.thumbnailLength];
                if (in.read(thumbnailBytes) != expectedValue.thumbnailLength) {
                    throw new IOException("Failed to read the expected thumbnail length");
                }
                // TODO: Need a way to check uncompressed thumbnail file
                if (expectedValue.isThumbnailCompressed) {
                    Bitmap thumbnailBitmap = BitmapFactory.decodeByteArray(thumbnailBytes, 0,
                            thumbnailBytes.length);
                    assertNotNull(thumbnailBitmap);
                    assertEquals(expectedValue.thumbnailWidth, thumbnailBitmap.getWidth());
                    assertEquals(expectedValue.thumbnailHeight, thumbnailBitmap.getHeight());
                }
            }
            // TODO: Creating a new input stream is a temporary
            //  workaround for BufferedInputStream#mark/reset not working properly for
            //  LG_G4_ISO_800_DNG. Need to investigate cause.
            in = new BufferedInputStream(new FileInputStream(imageFile.getAbsolutePath()));
            if (expectedValue.hasMake) {
                in.skip(expectedValue.makeOffset);
                byte[] makeBytes = new byte[expectedValue.makeLength];
                if (in.read(makeBytes) != expectedValue.makeLength) {
                    throw new IOException("Failed to read the expected make length");
                }
                String makeString = new String(makeBytes);
                // Remove null bytes
                makeString = makeString.replaceAll("\u0000.*", "");
                assertEquals(expectedValue.make, makeString.trim());
            }
            in = new BufferedInputStream(new FileInputStream(imageFile.getAbsolutePath()));
            if (expectedValue.hasXmp) {
                in.skip(expectedValue.xmpOffset);
                byte[] identifierBytes = new byte[expectedValue.xmpLength];
                if (in.read(identifierBytes) != expectedValue.xmpLength) {
                    throw new IOException("Failed to read the expected xmp length");
                }
                final String xmpIdentifier = "<?xpacket begin=";
                assertTrue(new String(identifierBytes, StandardCharsets.UTF_8)
                        .startsWith(xmpIdentifier));
            }
            // TODO: Add code for retrieving raw latitude data using offset and length
        } finally {
            IoUtils.closeQuietly(in);
        }
    }

    private void testSaveAttributes_withFileName(String fileName, ExpectedValue expectedValue)
            throws IOException {
        File srcFile = new File(Environment.getExternalStorageDirectory(), fileName);
        File imageFile = clone(srcFile);
        String verboseTag = imageFile.getName();

        ExifInterface exifInterface = new ExifInterface(imageFile.getAbsolutePath());
        exifInterface.saveAttributes();
        exifInterface = new ExifInterface(imageFile.getAbsolutePath());
        compareWithExpectedValue(exifInterface, expectedValue, verboseTag, false);

        // Test for modifying one attribute.
        String backupValue = exifInterface.getAttribute(ExifInterface.TAG_MAKE);
        exifInterface.setAttribute(ExifInterface.TAG_MAKE, "abc");
        exifInterface.saveAttributes();
        // Check if thumbnail offset and length are properly updated without parsing the data again.
        if (expectedValue.hasThumbnail) {
            testThumbnail(expectedValue, exifInterface);
        }
        exifInterface = new ExifInterface(imageFile.getAbsolutePath());
        assertEquals("abc", exifInterface.getAttribute(ExifInterface.TAG_MAKE));
        // Check if thumbnail bytes can be retrieved from the new thumbnail range.
        if (expectedValue.hasThumbnail) {
            testThumbnail(expectedValue, exifInterface);
        }

        // Restore the backup value.
        exifInterface.setAttribute(ExifInterface.TAG_MAKE, backupValue);
        exifInterface.saveAttributes();
        exifInterface = new ExifInterface(imageFile.getAbsolutePath());
        compareWithExpectedValue(exifInterface, expectedValue, verboseTag, false);
    }

    private void testSaveAttributes_withFileDescriptor(String fileName, ExpectedValue expectedValue)
            throws IOException {
        File srcFile = new File(Environment.getExternalStorageDirectory(), fileName);
        File imageFile = clone(srcFile);
        String verboseTag = imageFile.getName();

        FileDescriptor fd = null;
        try {
            fd = Os.open(imageFile.getAbsolutePath(), OsConstants.O_RDWR, 0600);
            ExifInterface exifInterface = new ExifInterface(fd);
            exifInterface.saveAttributes();
            Os.lseek(fd, 0, OsConstants.SEEK_SET);
            exifInterface = new ExifInterface(fd);
            compareWithExpectedValue(exifInterface, expectedValue, verboseTag, false);

            // Test for modifying one attribute.
            String backupValue = exifInterface.getAttribute(ExifInterface.TAG_MAKE);
            exifInterface.setAttribute(ExifInterface.TAG_MAKE, "abc");
            exifInterface.saveAttributes();
            // Check if thumbnail offset and length are properly updated without parsing the data
            // again.
            if (expectedValue.hasThumbnail) {
                testThumbnail(expectedValue, exifInterface);
            }
            Os.lseek(fd, 0, OsConstants.SEEK_SET);
            exifInterface = new ExifInterface(fd);
            assertEquals("abc", exifInterface.getAttribute(ExifInterface.TAG_MAKE));
            // Check if thumbnail bytes can be retrieved from the new thumbnail range.
            if (expectedValue.hasThumbnail) {
                testThumbnail(expectedValue, exifInterface);
            }

            // Restore the backup value.
            exifInterface.setAttribute(ExifInterface.TAG_MAKE, backupValue);
            exifInterface.saveAttributes();
            Os.lseek(fd, 0, OsConstants.SEEK_SET);
            exifInterface = new ExifInterface(fd);
            compareWithExpectedValue(exifInterface, expectedValue, verboseTag, false);
        } catch (ErrnoException e) {
            throw e.rethrowAsIOException();
        } finally {
            IoUtils.closeQuietly(fd);
        }
    }

    private void testExifInterfaceForReadAndWrite(String fileName, int typedArrayResourceId)
            throws IOException {
        ExpectedValue expectedValue = new ExpectedValue(
                getContext().getResources().obtainTypedArray(typedArrayResourceId));

        // Test for reading from external data storage.
        fileName = EXTERNAL_BASE_DIRECTORY + fileName;
        testExifInterfaceCommon(fileName, expectedValue);

        // Test for checking expected range by retrieving raw data with given offset and length.
        testExifInterfaceRange(fileName, expectedValue);

        // Test for saving attributes.
        testSaveAttributes_withFileName(fileName, expectedValue);
        testSaveAttributes_withFileDescriptor(fileName, expectedValue);
    }

    private void testExifInterfaceForRead(String fileName, int typedArrayResourceId)
            throws IOException {
        ExpectedValue expectedValue = new ExpectedValue(
                getContext().getResources().obtainTypedArray(typedArrayResourceId));

        // Test for reading from external data storage.
        fileName = EXTERNAL_BASE_DIRECTORY + fileName;
        testExifInterfaceCommon(fileName, expectedValue);

        // Test for checking expected range by retrieving raw data with given offset and length.
        testExifInterfaceRange(fileName, expectedValue);
    }

    private void testThumbnail(ExpectedValue expectedValue, ExifInterface exifInterface) {
        byte[] thumbnail = exifInterface.getThumbnailBytes();
        // TODO: Add support for testing validity of uncompressed thumbnails
        if (expectedValue.isThumbnailCompressed) {
            Bitmap thumbnailBitmap = BitmapFactory.decodeByteArray(thumbnail, 0,
                    thumbnail.length);
            assertNotNull(thumbnailBitmap);
            assertEquals(expectedValue.thumbnailWidth, thumbnailBitmap.getWidth());
            assertEquals(expectedValue.thumbnailHeight, thumbnailBitmap.getHeight());
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder()
                .detectUnbufferedIo()
                .penaltyDeath()
                .build());
    }

    public void testReadExifDataFromExifByteOrderIIJpeg() throws Throwable {
        testExifInterfaceForReadAndWrite(EXIF_BYTE_ORDER_II_JPEG, R.array.exifbyteorderii_jpg);
    }

    public void testReadExifDataFromExifByteOrderMMJpeg() throws Throwable {
        testExifInterfaceForReadAndWrite(EXIF_BYTE_ORDER_MM_JPEG, R.array.exifbyteordermm_jpg);
    }

    public void testReadExifDataFromLgG4Iso800Dng() throws Throwable {
        testExifInterfaceForRead(LG_G4_ISO_800_DNG, R.array.lg_g4_iso_800_dng);
    }

    public void testReadExifDataFromLgG4Iso800Jpg() throws Throwable {
        stageFile(R.raw.lg_g4_iso_800, new File(Environment.getExternalStorageDirectory(),
                EXTERNAL_BASE_DIRECTORY + LG_G4_ISO_800_JPG));
        testExifInterfaceForReadAndWrite(LG_G4_ISO_800_JPG, R.array.lg_g4_iso_800_jpg);
    }

    public void testDoNotFailOnCorruptedImage() throws Throwable {
        // To keep the compatibility with old versions of ExifInterface, even on a corrupted image,
        // it shouldn't raise any exceptions except an IOException when unable to open a file.
        byte[] bytes = new byte[1024];
        try {
            new ExifInterface(new ByteArrayInputStream(bytes));
            // Always success
        } catch (IOException e) {
            fail("Should not reach here!");
        }
    }

    public void testReadExifDataFromVolantisJpg() throws Throwable {
        // Test if it is possible to parse the volantis generated JPEG smoothly.
        testExifInterfaceForReadAndWrite(VOLANTIS_JPEG, R.array.volantis_jpg);
    }

    public void testReadExifDataFromSonyRX100Arw() throws Throwable {
        testExifInterfaceForRead(SONY_RX_100_ARW, R.array.sony_rx_100_arw);
    }

    public void testReadExifDataFromCanonG7XCr2() throws Throwable {
        testExifInterfaceForRead(CANON_G7X_CR2, R.array.canon_g7x_cr2);
    }

    public void testReadExifDataFromFujiX20Raf() throws Throwable {
        testExifInterfaceForRead(FUJI_X20_RAF, R.array.fuji_x20_raf);
    }

    public void testReadExifDataFromNikon1AW1Nef() throws Throwable {
        testExifInterfaceForRead(NIKON_1AW1_NEF, R.array.nikon_1aw1_nef);
    }

    public void testReadExifDataFromNikonP330Nrw() throws Throwable {
        testExifInterfaceForRead(NIKON_P330_NRW, R.array.nikon_p330_nrw);
    }

    public void testReadExifDataFromOlympusEPL3Orf() throws Throwable {
        testExifInterfaceForRead(OLYMPUS_E_PL3_ORF, R.array.olympus_e_pl3_orf);
    }

    public void testReadExifDataFromPanasonicGM5Rw2() throws Throwable {
        testExifInterfaceForRead(PANASONIC_GM5_RW2, R.array.panasonic_gm5_rw2);
    }

    public void testReadExifDataFromPentaxK5Pef() throws Throwable {
        testExifInterfaceForRead(PENTAX_K5_PEF, R.array.pentax_k5_pef);
    }

    public void testReadExifDataFromSamsungNX3000Srw() throws Throwable {
        testExifInterfaceForRead(SAMSUNG_NX3000_SRW, R.array.samsung_nx3000_srw);
    }

    public void testStandaloneDataForRead() throws Throwable {
        testExifInterfaceForStandalone(EXIF_BYTE_ORDER_II_JPEG, R.array.exifbyteorderii_standalone);
        testExifInterfaceForStandalone(EXIF_BYTE_ORDER_MM_JPEG, R.array.exifbyteordermm_standalone);
    }

    public void testExifByteOrderIIPngForReadAndWrite() throws Throwable {
        stageFile(R.raw.png_with_exif_byte_order_ii, new File(Environment.getExternalStorageDirectory(),
                EXTERNAL_BASE_DIRECTORY + PNG_WITH_EXIF_BYTE_ORDER_II));
        testExifInterfaceForRead(PNG_WITH_EXIF_BYTE_ORDER_II, R.array.exifbyteorderii_png);
    }

    public void testExifByteOrderIIWebpForRead() throws Throwable {
        stageFile(R.raw.webp_with_exif, new File(Environment.getExternalStorageDirectory(),
                EXTERNAL_BASE_DIRECTORY + WEBP_WITH_EXIF));
        testExifInterfaceForRead(WEBP_WITH_EXIF, R.array.exifbyteorderii_webp);
    }

    public void testPngWithoutExifForWrite() throws Throwable {
        stageFile(R.raw.png_without_exif, new File(Environment.getExternalStorageDirectory(),
                EXTERNAL_BASE_DIRECTORY + PNG_WITHOUT_EXIF));

        // Do we need to clone this file?
        File imageFile = new File(Environment.getExternalStorageDirectory(),
                EXTERNAL_BASE_DIRECTORY + PNG_WITHOUT_EXIF);
        ExifInterface exifInterface = new ExifInterface(imageFile.getAbsolutePath());
        exifInterface.setAttribute(ExifInterface.TAG_MAKE, "abc");
        exifInterface.saveAttributes();
        exifInterface = new ExifInterface(imageFile.getAbsolutePath());
        String make = exifInterface.getAttribute(ExifInterface.TAG_MAKE);
        assertEquals("abc", make);
    }

    public void testSetDateTime() throws IOException {
        final String dateTimeValue = "2017:02:02 22:22:22";
        final String dateTimeOriginalValue = "2017:01:01 11:11:11";

        File srcFile = new File(Environment.getExternalStorageDirectory(),
                EXTERNAL_BASE_DIRECTORY + EXIF_BYTE_ORDER_II_JPEG);
        File imageFile = clone(srcFile);

        FileUtils.copyFileOrThrow(srcFile, imageFile);
        ExifInterface exif = new ExifInterface(imageFile.getAbsolutePath());
        exif.setAttribute(ExifInterface.TAG_DATETIME, dateTimeValue);
        exif.setAttribute(ExifInterface.TAG_DATETIME_ORIGINAL, dateTimeOriginalValue);
        exif.saveAttributes();

        // Check that the DATETIME value is not overwritten by DATETIME_ORIGINAL's value.
        exif = new ExifInterface(imageFile.getAbsolutePath());
        assertEquals(dateTimeValue, exif.getAttribute(ExifInterface.TAG_DATETIME));
        assertEquals(dateTimeOriginalValue, exif.getAttribute(ExifInterface.TAG_DATETIME_ORIGINAL));

        // Now remove the DATETIME value.
        exif.setAttribute(ExifInterface.TAG_DATETIME, null);
        exif.saveAttributes();

        // When the DATETIME has no value, then it should be set to DATETIME_ORIGINAL's value.
        exif = new ExifInterface(imageFile.getAbsolutePath());
        assertEquals(dateTimeOriginalValue, exif.getAttribute(ExifInterface.TAG_DATETIME));
        imageFile.delete();
    }

    private void stageFile(int resId, File file) throws IOException {
        try (InputStream source = getContext().getResources().openRawResource(resId);
                OutputStream target = new FileOutputStream(file)) {
            FileUtils.copy(source, target);
        }
    }

    private static File clone(File original) throws IOException {
        final File cloned = new File(original.getParentFile(),
                "cts_" + System.nanoTime() + "_" + original.getName());
        FileUtils.copyFileOrThrow(original, cloned);
        return cloned;
    }

    private short readShort(InputStream is) throws IOException {
        int ch1 = is.read();
        int ch2 = is.read();
        if ((ch1 | ch2) < 0) {
            throw new EOFException();
        }
        return (short) ((ch1 << 8) + (ch2));
    }
}
