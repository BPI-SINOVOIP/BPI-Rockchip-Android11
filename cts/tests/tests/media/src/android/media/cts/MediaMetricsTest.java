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

package android.media.cts;

import static org.junit.Assert.assertEquals;

import android.media.MediaMetrics;
import android.os.Bundle;
import android.os.Process;
import androidx.test.runner.AndroidJUnit4;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests for MediaMetrics item handling.
 */

@NonMediaMainlineTest
@RunWith(AndroidJUnit4.class)
public class MediaMetricsTest {

    /**
     * This tests MediaMetrics item creation.
     */
    @Test
    public void testBasicItem() {
        final String key = "Key";
        final MediaMetrics.Item item = new MediaMetrics.Item(key);

        item.putInt("int", (int) 1)
            .putLong("long", (long) 2)
            .putString("string", "ABCD")
            .putDouble("double", (double) 3.1);

        // Verify what is in the item by converting to a bundle.
        // This uses special MediaMetrics.Item APIs for CTS test.
        // The BUNDLE_* string keys represent internal Item data to be verified.
        final Bundle bundle = item.toBundle();

        assertEquals(1, bundle.getInt("int"));
        assertEquals(2, bundle.getLong("long"));
        assertEquals("ABCD", bundle.getString("string"));
        assertEquals(3.1, bundle.getDouble("double"), 1e-6 /* delta */);

        assertEquals("Key", bundle.getString(MediaMetrics.Item.BUNDLE_KEY));
        assertEquals(-1, bundle.getInt(MediaMetrics.Item.BUNDLE_PID));  // default PID
        assertEquals(-1, bundle.getInt(MediaMetrics.Item.BUNDLE_UID));  // default UID
        assertEquals(0, bundle.getChar(MediaMetrics.Item.BUNDLE_VERSION));
        assertEquals(key.length() + 1, bundle.getChar(MediaMetrics.Item.BUNDLE_KEY_SIZE));
        assertEquals(0, bundle.getLong(MediaMetrics.Item.BUNDLE_TIMESTAMP)); // default Time
        assertEquals(4, bundle.getInt(MediaMetrics.Item.BUNDLE_PROPERTY_COUNT));
    }

    /**
     * This tests MediaMetrics item buffer expansion when the initial buffer capacity is set to 1.
     */
    @Test
    public void testBigItem() {
        final String key = "Key";
        final int intKeyCount = 10000;
        final MediaMetrics.Item item = new MediaMetrics.Item(
                key, 1 /* pid */, 2 /* uid */, 3 /* time */, 1 /* capacity */);

        item.putInt("int", (int) 1)
            .putLong("long", (long) 2)
            .putString("string", "ABCD")
            .putDouble("double", (double) 3.1);

        // Putting 10000 int properties forces the item to reallocate the buffer several times.
        for (Integer i = 0; i < intKeyCount; ++i) {
            item.putInt(i.toString(), i);
        }

        // Verify what is in the item by converting to a bundle.
        // This uses special MediaMetrics.Item APIs for CTS test.
        // The BUNDLE_* string keys represent internal Item data to be verified.
        final Bundle bundle = item.toBundle();

        assertEquals(1, bundle.getInt("int"));
        assertEquals(2, bundle.getLong("long"));
        assertEquals("ABCD", bundle.getString("string"));
        assertEquals(3.1, bundle.getDouble("double"), 1e-6 /* delta */);

        assertEquals(key, bundle.getString(MediaMetrics.Item.BUNDLE_KEY));
        assertEquals(1, bundle.getInt(MediaMetrics.Item.BUNDLE_PID));
        assertEquals(2, bundle.getInt(MediaMetrics.Item.BUNDLE_UID));
        assertEquals(0, bundle.getChar(MediaMetrics.Item.BUNDLE_VERSION));
        assertEquals(key.length() + 1, bundle.getChar(MediaMetrics.Item.BUNDLE_KEY_SIZE));
        assertEquals(3, bundle.getLong(MediaMetrics.Item.BUNDLE_TIMESTAMP));

        for (Integer i = 0; i < intKeyCount; ++i) {
            assertEquals((int) i, bundle.getInt(i.toString()));
        }

        assertEquals(intKeyCount + 4, bundle.getInt(MediaMetrics.Item.BUNDLE_PROPERTY_COUNT));

        item.clear(); // removes properties.
        item.putInt("value", (int) 100);

        final Bundle bundle2 = item.toBundle();

        assertEquals(key, bundle2.getString(MediaMetrics.Item.BUNDLE_KEY));
        assertEquals(1, bundle2.getInt(MediaMetrics.Item.BUNDLE_PID));
        assertEquals(2, bundle2.getInt(MediaMetrics.Item.BUNDLE_UID));
        assertEquals(0, bundle2.getChar(MediaMetrics.Item.BUNDLE_VERSION));
        assertEquals(key.length() + 1, bundle2.getChar(MediaMetrics.Item.BUNDLE_KEY_SIZE));
        assertEquals(0, bundle2.getLong(MediaMetrics.Item.BUNDLE_TIMESTAMP)); // time is reset.

        for (Integer i = 0; i < intKeyCount; ++i) {
            assertEquals(-1, bundle2.getInt(i.toString(), -1));
        }
        assertEquals(100, bundle2.getInt("value"));
        assertEquals(1, bundle2.getInt(MediaMetrics.Item.BUNDLE_PROPERTY_COUNT));

        // Now override pid, uid, and time.
        item.setPid(10)
            .setUid(11)
            .setTimestamp(12);
        final Bundle bundle3 = item.toBundle();
        assertEquals(10, bundle3.getInt(MediaMetrics.Item.BUNDLE_PID));
        assertEquals(11, bundle3.getInt(MediaMetrics.Item.BUNDLE_UID));
        assertEquals(12, bundle3.getLong(MediaMetrics.Item.BUNDLE_TIMESTAMP));
    }
}
