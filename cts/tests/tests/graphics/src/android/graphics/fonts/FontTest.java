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

package android.graphics.fonts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.content.res.Resources.NotFoundException;
import android.graphics.cts.R;
import android.os.ParcelFileDescriptor;
import android.util.Log;
import android.util.Pair;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.util.ArrayList;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class FontTest {
    private static final String TAG = "FontFileUtilTest";
    private static final String CACHE_FILE_PREFIX = ".font";

    /**
     * Create new temporary file.
     *
     * Caller must delete the file after used.
     */
    private static File getTempFile() {
        Context ctx = InstrumentationRegistry.getTargetContext();
        final String prefix = CACHE_FILE_PREFIX;
        for (int i = 0; i < 100; ++i) {
            final File file = new File(ctx.getCacheDir(), prefix + i);
            try {
                if (file.createNewFile()) {
                    return file;
                }
            } catch (IOException e) {
                // ignore. Try next file.
            }
        }
        return null;
    }

    private static ByteBuffer mmap(AssetManager am, String path) {
        File file = getTempFile();
        try (InputStream is = am.open(path)) {
            if (!copyToFile(file, is)) {
                return null;
            }
            return mmap(file);
        } catch (IOException e) {
            Log.e(TAG, "Failed to open assets");
            return null;
        } finally {
            file.delete();
        }
    }

    private static ByteBuffer mmap(File file) {
        try (FileInputStream fis = new FileInputStream(file)) {
            FileChannel channel = fis.getChannel();
            return channel.map(FileChannel.MapMode.READ_ONLY, 0, channel.size());
        } catch (IOException e) {
            return null;
        }
    }

    private static boolean copyToFile(File file, InputStream is) {
        return copyToFile(file, is, null, null);
    }

    private static boolean copyToFile(File file, InputStream is, byte[] prepend, byte[] append) {
        try (FileOutputStream os = new FileOutputStream(file, false)) {
            byte[] buffer = new byte[1024];
            int readLen;
            if (prepend != null) {
                os.write(prepend, 0, prepend.length);
            }
            while ((readLen = is.read(buffer)) != -1) {
                os.write(buffer, 0, readLen);
            }
            if (append != null) {
                os.write(append, 0, append.length);
            }
            return true;
        } catch (IOException e) {
            Log.e(TAG, "Error copying resource contents to temp file: " + e.getMessage());
            return false;
        }
    }

    @Test
    public void testBuilder_buffer() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;
            String path = FontTestUtil.getFontPathFromStyle(weight, italic);

            ByteBuffer buffer = mmap(am, path);

            Font font = new Font.Builder(buffer).build();
            assertEquals(path, weight, font.getStyle().getWeight());
            assertEquals(path, slant, font.getStyle().getSlant());
            assertEquals(path, 0, font.getTtcIndex());
            assertNull(path, font.getAxes());
            assertNotNull(font.getBuffer());
            assertNull(font.getFile());
        }
    }

    @Test
    public void testBuilder_buffer_ttc() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;
            String path = FontTestUtil.getTtcFontFileInAsset();

            ByteBuffer buffer = mmap(am, path);
            int ttcIndex = FontTestUtil.getTtcIndexFromStyle(weight, italic);

            Font font = new Font.Builder(buffer).setTtcIndex(ttcIndex).build();
            assertEquals(path, weight, font.getStyle().getWeight());
            assertEquals(path, slant, font.getStyle().getSlant());
            assertEquals(path, ttcIndex, font.getTtcIndex());
            assertNull(path, font.getAxes());
            assertNotNull(font.getBuffer());
            assertNull(font.getFile());
        }
    }

    @Test
    public void testBuilder_buffer_vf() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getVFFontInAsset();
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            ByteBuffer buffer = mmap(am, path);
            FontVariationAxis[] axes = FontVariationAxis.fromFontVariationSettings(
                    FontTestUtil.getVarSettingsFromStyle(weight, italic));

            Font font = new Font.Builder(buffer).setFontVariationSettings(axes).build();
            assertEquals(path, weight, font.getStyle().getWeight());
            assertEquals(path, slant, font.getStyle().getSlant());
            assertEquals(path, 0, font.getTtcIndex());
            assertEquals(path, axes, font.getAxes());
            assertNotNull(font.getBuffer());
            assertNull(font.getFile());
        }
    }

    @Test
    public void testBuilder_buffer_override() throws IOException {
        int customWeight = 350;
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getFontPathFromStyle(weight, italic);
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            ByteBuffer buffer = mmap(am, path);

            Font font = new Font.Builder(buffer).setWeight(customWeight).build();
            assertEquals(path, customWeight, font.getStyle().getWeight());
            assertEquals(path, slant, font.getStyle().getSlant());
            assertEquals(path, 0, font.getTtcIndex());
            assertNull(path, font.getAxes());
            assertNotNull(font.getBuffer());
            assertNull(font.getFile());
        }

        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getFontPathFromStyle(weight, italic);

            ByteBuffer buffer = mmap(am, path);

            Font font = new Font.Builder(buffer).setSlant(FontStyle.FONT_SLANT_ITALIC).build();
            assertEquals(path, weight, font.getStyle().getWeight());
            assertEquals(path, FontStyle.FONT_SLANT_ITALIC, font.getStyle().getSlant());
            assertEquals(path, 0, font.getTtcIndex());
            assertNull(path, font.getAxes());
            assertNotNull(font.getBuffer());
            assertNull(font.getFile());
        }
    }

    @Test(expected = NullPointerException.class)
    public void testBuilder_buffer_invalid_null() throws IOException {
        ByteBuffer buf = null;
        new Font.Builder(buf);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testBuilder_buffer_invalid_not_direct() throws IOException {
        ByteBuffer buf = ByteBuffer.allocate(1024);
        new Font.Builder(buf);
    }

    @Test
    public void testBuilder_file() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getFontPathFromStyle(weight, italic);
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            File file = getTempFile();
            try (InputStream is = am.open(path)) {
                assertTrue(copyToFile(file, is));

                Font font = new Font.Builder(file).build();
                assertEquals(path, weight, font.getStyle().getWeight());
                assertEquals(path, slant, font.getStyle().getSlant());
                assertEquals(path, 0, font.getTtcIndex());
                assertNull(path, font.getAxes());
                assertNotNull(font.getBuffer());
                assertNotNull(font.getFile());
            } finally {
                file.delete();
            }
        }
    }

    @Test
    public void testBuilder_file_ttc() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getTtcFontFileInAsset();
            int ttcIndex = FontTestUtil.getTtcIndexFromStyle(weight, italic);
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            File file = getTempFile();
            try (InputStream is = am.open(path)) {
                assertTrue(copyToFile(file, is));

                Font font = new Font.Builder(file).setTtcIndex(ttcIndex).build();
                assertEquals(path, weight, font.getStyle().getWeight());
                assertEquals(path, slant, font.getStyle().getSlant());
                assertEquals(path, ttcIndex, font.getTtcIndex());
                assertNull(path, font.getAxes());
                assertNotNull(font.getBuffer());
                assertNotNull(font.getFile());
            } finally {
                file.delete();
            }
        }
    }

    @Test
    public void testBuilder_file_vf() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getVFFontInAsset();
            FontVariationAxis[] axes = FontVariationAxis.fromFontVariationSettings(
                    FontTestUtil.getVarSettingsFromStyle(weight, italic));
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            File file = getTempFile();
            try (InputStream is = am.open(path)) {
                assertTrue(copyToFile(file, is));

                Font font = new Font.Builder(file).setFontVariationSettings(axes).build();
                assertEquals(path, weight, font.getStyle().getWeight());
                assertEquals(path, slant, font.getStyle().getSlant());
                assertEquals(path, 0, font.getTtcIndex());
                assertEquals(path, axes, font.getAxes());
                assertNotNull(font.getBuffer());
                assertNotNull(font.getFile());
            } finally {
                file.delete();
            }
        }
    }

    @Test
    public void testBuilder_file_override() throws IOException {
        int customWeight = 350;
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getFontPathFromStyle(weight, italic);
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            File file = getTempFile();
            try (InputStream is = am.open(path)) {
                assertTrue(copyToFile(file, is));

                Font font = new Font.Builder(file).setWeight(customWeight).build();
                assertEquals(path, customWeight, font.getStyle().getWeight());
                assertEquals(path, slant, font.getStyle().getSlant());
                assertEquals(path, 0, font.getTtcIndex());
                assertNull(path, font.getAxes());
                assertNotNull(font.getBuffer());
                assertNotNull(font.getFile());
            } finally {
                file.delete();
            }
        }
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getFontPathFromStyle(weight, italic);

            File file = getTempFile();
            try (InputStream is = am.open(path)) {
                assertTrue(copyToFile(file, is));

                Font font = new Font.Builder(file).setSlant(FontStyle.FONT_SLANT_ITALIC).build();
                assertEquals(path, weight, font.getStyle().getWeight());
                assertEquals(path, FontStyle.FONT_SLANT_ITALIC, font.getStyle().getSlant());
                assertEquals(path, 0, font.getTtcIndex());
                assertNull(path, font.getAxes());
                assertNotNull(font.getBuffer());
                assertNotNull(font.getFile());
            } finally {
                file.delete();
            }
        }
    }

    @Test(expected = NullPointerException.class)
    public void testBuilder_file_invalid_null_file() throws IOException {
        File file = null;
        new Font.Builder(file);
    }

    @Test(expected = IOException.class)
    public void testBuilder_file_invalid_not_found_file() throws IOException {
        File file = new File("/no/such/file");
        new Font.Builder(file).build();
    }

    @Test
    public void testBuilder_fd() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getFontPathFromStyle(weight, italic);
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            File file = getTempFile();
            try (InputStream is = am.open(path)) {
                assertTrue(copyToFile(file, is));

                try (ParcelFileDescriptor fd = ParcelFileDescriptor.open(file,
                        ParcelFileDescriptor.MODE_READ_ONLY)) {
                    Font font = new Font.Builder(fd).build();
                    assertEquals(path, weight, font.getStyle().getWeight());
                    assertEquals(path, slant, font.getStyle().getSlant());
                    assertEquals(path, 0, font.getTtcIndex());
                    assertNull(path, font.getAxes());
                    assertNotNull(font.getBuffer());
                    assertNull(font.getFile());
                }
            } finally {
                file.delete();
            }
        }
    }

    @Test
    public void testBuilder_fd_ttc() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getTtcFontFileInAsset();
            int ttcIndex = FontTestUtil.getTtcIndexFromStyle(weight, italic);
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            File file = getTempFile();
            try (InputStream is = am.open(path)) {
                assertTrue(copyToFile(file, is));

                try (ParcelFileDescriptor fd = ParcelFileDescriptor.open(file,
                        ParcelFileDescriptor.MODE_READ_ONLY)) {
                    Font font = new Font.Builder(fd).setTtcIndex(ttcIndex).build();
                    assertEquals(path, weight, font.getStyle().getWeight());
                    assertEquals(path, slant, font.getStyle().getSlant());
                    assertEquals(path, ttcIndex, font.getTtcIndex());
                    assertNull(path, font.getAxes());
                    assertNotNull(font.getBuffer());
                    assertNull(font.getFile());
                }
            } finally {
                file.delete();
            }
        }
    }

    @Test
    public void testBuilder_fd_vf() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getVFFontInAsset();
            FontVariationAxis[] axes = FontVariationAxis.fromFontVariationSettings(
                    FontTestUtil.getVarSettingsFromStyle(weight, italic));
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            File file = getTempFile();
            try (InputStream is = am.open(path)) {
                assertTrue(copyToFile(file, is));

                try (ParcelFileDescriptor fd = ParcelFileDescriptor.open(file,
                        ParcelFileDescriptor.MODE_READ_ONLY)) {
                    Font font = new Font.Builder(fd).setFontVariationSettings(axes)
                            .build();
                    assertEquals(path, weight, font.getStyle().getWeight());
                    assertEquals(path, slant, font.getStyle().getSlant());
                    assertEquals(path, 0, font.getTtcIndex());
                    assertEquals(path, axes, font.getAxes());
                    assertNotNull(font.getBuffer());
                    assertNull(font.getFile());
                }
            } finally {
                file.delete();
            }
        }
    }

    @Test
    public void testBuilder_fd_override() throws IOException {
        int customWeight = 350;
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getFontPathFromStyle(weight, italic);
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            File file = getTempFile();
            try (InputStream is = am.open(path)) {
                assertTrue(copyToFile(file, is));

                try (ParcelFileDescriptor fd = ParcelFileDescriptor.open(file,
                        ParcelFileDescriptor.MODE_READ_ONLY)) {
                    Font font = new Font.Builder(fd).setWeight(customWeight).build();
                    assertEquals(path, customWeight, font.getStyle().getWeight());
                    assertEquals(path, slant, font.getStyle().getSlant());
                    assertEquals(path, 0, font.getTtcIndex());
                    assertNull(path, font.getAxes());
                    assertNotNull(font.getBuffer());
                    assertNull(font.getFile());
                }
            } finally {
                file.delete();
            }
        }
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getFontPathFromStyle(weight, italic);
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            File file = getTempFile();
            try (InputStream is = am.open(path)) {
                assertTrue(copyToFile(file, is));

                try (ParcelFileDescriptor fd = ParcelFileDescriptor.open(file,
                        ParcelFileDescriptor.MODE_READ_ONLY)) {
                    Font font = new Font.Builder(fd).setSlant(
                            FontStyle.FONT_SLANT_ITALIC).build();
                    assertEquals(path, weight, font.getStyle().getWeight());
                    assertEquals(path, FontStyle.FONT_SLANT_ITALIC, font.getStyle().getSlant());
                    assertEquals(path, 0, font.getTtcIndex());
                    assertNull(path, font.getAxes());
                    assertNotNull(font.getBuffer());
                    assertNull(font.getFile());
                }
            } finally {
                file.delete();
            }
        }
    }

    @Test
    public void testBuilder_fd_subdata() throws IOException {
        byte[] dummy = { (byte) 0xde, (byte) 0xad, (byte) 0xbe, (byte) 0xef };
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getFontPathFromStyle(weight, italic);
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            File file = getTempFile();
            try (InputStream is = am.open(path)) {
                assertTrue(copyToFile(file, is, dummy, dummy));

                try (ParcelFileDescriptor fd = ParcelFileDescriptor.open(file,
                        ParcelFileDescriptor.MODE_READ_ONLY)) {
                    Font font = new Font.Builder(
                            fd, dummy.length, file.length() - dummy.length * 2).build();
                    assertEquals(path, weight, font.getStyle().getWeight());
                    assertEquals(path, slant, font.getStyle().getSlant());
                    assertEquals(path, 0, font.getTtcIndex());
                    assertNull(path, font.getAxes());
                    assertNotNull(font.getBuffer());
                    assertNull(font.getFile());
                }
            } finally {
                file.delete();
            }
        }
    }

    @Test
    public void testBuilder_fd_subdata_ttc() throws IOException {
        byte[] dummy = { (byte) 0xde, (byte) 0xad, (byte) 0xbe, (byte) 0xef };
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getTtcFontFileInAsset();
            int ttcIndex = FontTestUtil.getTtcIndexFromStyle(weight, italic);
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            File file = getTempFile();
            try (InputStream is = am.open(path)) {
                assertTrue(copyToFile(file, is, dummy, dummy));

                try (ParcelFileDescriptor fd = ParcelFileDescriptor.open(file,
                        ParcelFileDescriptor.MODE_READ_ONLY)) {
                    Font font = new Font.Builder(
                            fd, dummy.length, file.length() - dummy.length * 2)
                            .setTtcIndex(ttcIndex).build();
                    assertEquals(path, weight, font.getStyle().getWeight());
                    assertEquals(path, slant, font.getStyle().getSlant());
                    assertEquals(path, ttcIndex, font.getTtcIndex());
                    assertNull(path, font.getAxes());
                    assertNotNull(font.getBuffer());
                    assertNull(font.getFile());
                }
            } finally {
                file.delete();
            }
        }
    }

    @Test
    public void testBuilder_fd_subdata_vf() throws IOException {
        byte[] dummy = { (byte) 0xde, (byte) 0xad, (byte) 0xbe, (byte) 0xef };
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getVFFontInAsset();
            FontVariationAxis[] axes = FontVariationAxis.fromFontVariationSettings(
                    FontTestUtil.getVarSettingsFromStyle(weight, italic));
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            File file = getTempFile();
            try (InputStream is = am.open(path)) {
                assertTrue(copyToFile(file, is, dummy, dummy));

                try (ParcelFileDescriptor fd = ParcelFileDescriptor.open(file,
                        ParcelFileDescriptor.MODE_READ_ONLY)) {
                    Font font = new Font.Builder(
                            fd, dummy.length, file.length() - dummy.length * 2)
                            .setFontVariationSettings(axes).build();
                    assertEquals(path, weight, font.getStyle().getWeight());
                    assertEquals(path, slant, font.getStyle().getSlant());
                    assertEquals(path, 0, font.getTtcIndex());
                    assertEquals(path, axes, font.getAxes());
                    assertNotNull(font.getBuffer());
                    assertNull(font.getFile());
                }
            } finally {
                file.delete();
            }
        }
    }

    @Test
    public void testBuilder_fd_subdata_override() throws IOException {
        int customWeight = 350;
        byte[] dummy = { (byte) 0xde, (byte) 0xad, (byte) 0xbe, (byte) 0xef };
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getFontPathFromStyle(weight, italic);
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            File file = getTempFile();
            try (InputStream is = am.open(path)) {
                assertTrue(copyToFile(file, is, dummy, dummy));

                try (ParcelFileDescriptor fd = ParcelFileDescriptor.open(file,
                        ParcelFileDescriptor.MODE_READ_ONLY)) {
                    Font font = new Font.Builder(
                            fd, dummy.length, file.length() - dummy.length * 2)
                            .setWeight(customWeight).build();
                    assertEquals(path, customWeight, font.getStyle().getWeight());
                    assertEquals(path, slant, font.getStyle().getSlant());
                    assertEquals(path, 0, font.getTtcIndex());
                    assertNull(path, font.getAxes());
                    assertNotNull(font.getBuffer());
                    assertNull(font.getFile());
                }
            } finally {
                file.delete();
            }
        }

        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getFontPathFromStyle(weight, italic);

            File file = getTempFile();
            try (InputStream is = am.open(path)) {
                assertTrue(copyToFile(file, is, dummy, dummy));

                try (ParcelFileDescriptor fd = ParcelFileDescriptor.open(file,
                        ParcelFileDescriptor.MODE_READ_ONLY)) {
                    Font font = new Font.Builder(
                            fd, dummy.length, file.length() - dummy.length * 2)
                            .setSlant(FontStyle.FONT_SLANT_ITALIC).build();
                    assertEquals(path, weight, font.getStyle().getWeight());
                    assertEquals(path, FontStyle.FONT_SLANT_ITALIC, font.getStyle().getSlant());
                    assertEquals(path, 0, font.getTtcIndex());
                    assertNull(path, font.getAxes());
                    assertNotNull(font.getBuffer());
                    assertNull(font.getFile());
                }
            } finally {
                file.delete();
            }
        }
    }

    @Test(expected = NullPointerException.class)
    public void testBuilder_fd_invalid_null() throws IOException {
        ParcelFileDescriptor fd = null;
        new Font.Builder(fd);
    }

    @Test(expected = NullPointerException.class)
    public void testBuilder_fd_subadata_invalid_null() throws IOException {
        ParcelFileDescriptor fd = null;
        new Font.Builder(fd, 0, -1);
    }

    @Test(expected = IOException.class)
    public void testBuilder_fd_subadata_invalid_invalid_size() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        String path = FontTestUtil.getFontPathFromStyle(400, false);

        File file = getTempFile();
        try (InputStream is = am.open(path)) {
            assertTrue(copyToFile(file, is));
            try (ParcelFileDescriptor fd = ParcelFileDescriptor.open(file,
                    ParcelFileDescriptor.MODE_READ_ONLY)) {
                new Font.Builder(fd, 0, Integer.MAX_VALUE).build();
            }
        }
    }

    @Test(expected = IOException.class)
    public void testBuilder_fd_subadata_invalid_invalid_offset() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        String path = FontTestUtil.getFontPathFromStyle(400, false);

        File file = getTempFile();
        try (InputStream is = am.open(path)) {
            assertTrue(copyToFile(file, is));
            try (ParcelFileDescriptor fd = ParcelFileDescriptor.open(file,
                    ParcelFileDescriptor.MODE_READ_ONLY)) {
                new Font.Builder(fd, Integer.MAX_VALUE, file.length()).build();
            }
        }
    }

    @Test
    public void testBuilder_asset() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getFontPathFromStyle(weight, italic);
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            Font font = new Font.Builder(am, path).build();
            assertEquals(path, weight, font.getStyle().getWeight());
            assertEquals(path, slant, font.getStyle().getSlant());
            assertEquals(path, 0, font.getTtcIndex());
            assertNull(path, font.getAxes());
            assertNotNull(font.getBuffer());
            assertNull(font.getFile());
        }
    }

    @Test
    public void testBuilder_asset_ttc() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getTtcFontFileInAsset();
            int ttcIndex = FontTestUtil.getTtcIndexFromStyle(weight, italic);
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            Font font = new Font.Builder(am, path).setTtcIndex(ttcIndex).build();
            assertEquals(path, weight, font.getStyle().getWeight());
            assertEquals(path, slant, font.getStyle().getSlant());
            assertEquals(path, ttcIndex, font.getTtcIndex());
            assertNull(path, font.getAxes());
            assertNotNull(font.getBuffer());
            assertNull(font.getFile());
        }
    }

    @Test
    public void testBuilder_asset_vf() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getVFFontInAsset();
            FontVariationAxis[] axes = FontVariationAxis.fromFontVariationSettings(
                    FontTestUtil.getVarSettingsFromStyle(weight, italic));
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            Font font = new Font.Builder(am, path).setFontVariationSettings(axes).build();
            assertEquals(path, weight, font.getStyle().getWeight());
            assertEquals(path, slant, font.getStyle().getSlant());
            assertEquals(path, 0, font.getTtcIndex());
            assertEquals(path, axes, font.getAxes());
            assertNotNull(font.getBuffer());
            assertNull(font.getFile());
        }
    }

    @Test
    public void testBuilder_asset_override() throws IOException {
        int customWeight = 350;
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getFontPathFromStyle(weight, italic);
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            Font font = new Font.Builder(am, path).setWeight(customWeight).build();
            assertEquals(path, customWeight, font.getStyle().getWeight());
            assertEquals(path, slant, font.getStyle().getSlant());
            assertEquals(path, 0, font.getTtcIndex());
            assertNull(path, font.getAxes());
            assertNotNull(font.getBuffer());
            assertNull(font.getFile());
        }
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            String path = FontTestUtil.getFontPathFromStyle(weight, italic);

            Font font = new Font.Builder(am, path).setSlant(FontStyle.FONT_SLANT_ITALIC).build();
            assertEquals(path, weight, font.getStyle().getWeight());
            assertEquals(path, FontStyle.FONT_SLANT_ITALIC, font.getStyle().getSlant());
            assertEquals(path, 0, font.getTtcIndex());
            assertNull(path, font.getAxes());
            assertNotNull(font.getBuffer());
            assertNull(font.getFile());
        }
    }

    @Test(expected = NullPointerException.class)
    public void testBuilder_asset_invalid_null_asset() throws IOException {
        AssetManager am = null;
        new Font.Builder(am, "/some/path");
    }

    @Test(expected = IOException.class)
    public void testBuilder_asset_invalid_not_found() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        new Font.Builder(am, "/no/such/file").build();
    }

    @Test
    public void testBuilder_resource() throws IOException {
        Resources res = InstrumentationRegistry.getTargetContext().getResources();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            int resId = FontTestUtil.getFontResourceIdFromStyle(weight, italic);
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            Font font = new Font.Builder(res, resId).build();
            assertEquals("ResId=#" + resId, weight, font.getStyle().getWeight());
            assertEquals("ResId=#" + resId, slant, font.getStyle().getSlant());
            assertEquals("ResId=#" + resId, 0, font.getTtcIndex());
            assertNull("ResId=#" + resId, font.getAxes());
            assertNotNull("ResId=#" + resId, font.getBuffer());
            assertNull("ResId=#" + resId, font.getFile());
        }
    }

    @Test
    public void testBuilder_resource_ttc() throws IOException {
        Resources res = InstrumentationRegistry.getTargetContext().getResources();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            int resId = FontTestUtil.getTtcFontFileResourceId();
            int ttcIndex = FontTestUtil.getTtcIndexFromStyle(weight, italic);
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            Font font = new Font.Builder(res, resId).setTtcIndex(ttcIndex).build();
            assertEquals("ResId=#" + resId, weight, font.getStyle().getWeight());
            assertEquals("ResId=#" + resId, slant, font.getStyle().getSlant());
            assertEquals("ResId=#" + resId, ttcIndex, font.getTtcIndex());
            assertNull("ResId=#" + resId, font.getAxes());
            assertNotNull("ResId=#" + resId, font.getBuffer());
            assertNull("ResId=#" + resId, font.getFile());
        }
    }

    @Test
    public void testBuilder_resource_vf() throws IOException {
        Resources res = InstrumentationRegistry.getTargetContext().getResources();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            int resId = FontTestUtil.getVFFontResourceId();
            FontVariationAxis[] axes = FontVariationAxis.fromFontVariationSettings(
                    FontTestUtil.getVarSettingsFromStyle(weight, italic));
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            Font font = new Font.Builder(res, resId).setFontVariationSettings(axes).build();
            assertEquals("ResId=#" + resId, weight, font.getStyle().getWeight());
            assertEquals("ResId=#" + resId, slant, font.getStyle().getSlant());
            assertEquals("ResId=#" + resId, 0, font.getTtcIndex());
            assertEquals("ResId=#" + resId, axes, font.getAxes());
            assertNotNull("ResId=#" + font.getBuffer());
            assertNull("ResId=#" + resId, font.getFile());
        }
    }

    @Test
    public void testBuilder_resource_override() throws IOException {
        int customWeight = 350;
        Resources res = InstrumentationRegistry.getTargetContext().getResources();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            int resId = FontTestUtil.getFontResourceIdFromStyle(weight, italic);
            final int slant = italic ? FontStyle.FONT_SLANT_ITALIC : FontStyle.FONT_SLANT_UPRIGHT;

            Font font = new Font.Builder(res, resId).setWeight(customWeight).build();
            assertEquals("ResId=#" + resId, customWeight, font.getStyle().getWeight());
            assertEquals("ResId=#" + resId, slant, font.getStyle().getSlant());
            assertEquals("ResId=#" + resId, 0, font.getTtcIndex());
            assertNull("ResId=#" + resId, font.getAxes());
            assertNotNull("ResId=#" + resId, font.getBuffer());
            assertNull("ResId=#" + resId, font.getFile());
        }

        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            int resId = FontTestUtil.getFontResourceIdFromStyle(weight, italic);

            Font font = new Font.Builder(res, resId).setSlant(FontStyle.FONT_SLANT_ITALIC).build();
            assertEquals("ResId=#" + resId, weight, font.getStyle().getWeight());
            assertEquals("ResId=#" + resId, FontStyle.FONT_SLANT_ITALIC,
                    font.getStyle().getSlant());
            assertEquals("ResId=#" + resId, 0, font.getTtcIndex());
            assertNull("ResId=#" + resId, font.getAxes());
            assertNotNull("ResId=#" + resId, font.getBuffer());
            assertNull("ResId=#" + resId, font.getFile());
        }
    }

    @Test(expected = NullPointerException.class)
    public void testBuilder_resource_invalid_null_resource() throws IOException {
        Resources res = null;
        new Font.Builder(res, R.font.ascii);
    }

    @Test(expected = NotFoundException.class)
    public void testBuilder_resource_invalid_res_id() throws IOException {
        Resources res = InstrumentationRegistry.getTargetContext().getResources();
        new Font.Builder(res, -1).build();
    }

    @Test(expected = IOException.class)
    public void testBuilder_asset_invalid_xml_font() throws IOException {
        Resources res = InstrumentationRegistry.getTargetContext().getResources();
        new Font.Builder(res, R.font.multiweight_family /* XML font */).build();
    }

    @Test
    public void testEquals() throws IOException {
        Resources res = InstrumentationRegistry.getTargetContext().getResources();
        ArrayList<Font> fonts1 = new ArrayList<>();
        ArrayList<Font> fonts2 = new ArrayList<>();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            int resId = FontTestUtil.getFontResourceIdFromStyle(weight, italic);

            fonts1.add(new Font.Builder(res, resId).build());
            fonts2.add(new Font.Builder(res, resId).build());
        }

        for (int i = 0; i < fonts1.size(); ++i) {
            assertTrue(fonts1.get(i).equals(fonts1.get(i)));
            assertTrue(fonts1.get(i).equals(fonts2.get(i)));
            assertTrue(fonts2.get(i).equals(fonts1.get(i)));
        }

        for (int i = 0; i < fonts1.size(); ++i) {
            for (int j = i + 1; j < fonts1.size(); ++j) {
                assertFalse(fonts1.get(i).equals(fonts1.get(j)));
                assertFalse(fonts1.get(j).equals(fonts1.get(i)));
            }
        }
    }

    @Test
    public void testHashCode() throws IOException {
        Resources res = InstrumentationRegistry.getTargetContext().getResources();
        ArrayList<Font> fonts1 = new ArrayList<>();
        ArrayList<Font> fonts2 = new ArrayList<>();
        for (Pair<Integer, Boolean> style : FontTestUtil.getAllStyles()) {
            int weight = style.first.intValue();
            boolean italic = style.second.booleanValue();
            int resId = FontTestUtil.getFontResourceIdFromStyle(weight, italic);

            fonts1.add(new Font.Builder(res, resId).build());
            fonts2.add(new Font.Builder(res, resId).build());
        }

        for (int i = 0; i < fonts1.size(); ++i) {
            assertEquals(fonts1.get(i).hashCode(), fonts1.get(i).hashCode());
            assertEquals(fonts1.get(i).hashCode(), fonts2.get(i).hashCode());
        }
    }

    @Test(expected = IllegalArgumentException.class)
    public void testMaxFontWeight() throws IOException {
        final Resources res = InstrumentationRegistry.getTargetContext().getResources();
        new Font.Builder(res, R.font.ascii).setWeight(FontStyle.FONT_WEIGHT_MAX + 1).build();
    }

    @Test(expected = IllegalArgumentException.class)
    public void testMinFontWeight() throws IOException {
        final Resources res = InstrumentationRegistry.getTargetContext().getResources();
        new Font.Builder(res, R.font.ascii).setWeight(FontStyle.FONT_WEIGHT_MIN - 1).build();
    }
}
