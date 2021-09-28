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

package android.content.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.ContentValues;
import android.os.Parcel;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Map;
import java.util.Set;

@RunWith(AndroidJUnit4.class)
public class ContentValuesTest {
    ContentValues mContentValues;

    @Before
    public void setUp() throws Exception {
        mContentValues = new ContentValues();
    }

    @Test
    public void testConstructor() {
        new ContentValues();
        new ContentValues(5);
        new ContentValues(mContentValues);

        try {
            new ContentValues(-1);
            fail("There should be a IllegalArgumentException thrown out.");
        } catch (IllegalArgumentException e) {
            // expected, test success.
        }

        try {
            new ContentValues(null);
            fail("There should be a NullPointerException thrown out.");
        } catch (NullPointerException e) {
            // expected, test success.
        }
    }

    @Test
    public void testValueSet() {
        Set<Map.Entry<String, Object>> map;
        assertNotNull(map = mContentValues.valueSet());
        assertTrue(map.isEmpty());

        mContentValues.put("Long", 10L);
        mContentValues.put("Integer", 201);

        assertNotNull(map = mContentValues.valueSet());
        assertFalse(map.isEmpty());
        assertEquals(2, map.size());
    }

    @Test
    public void testPutNull() {
        mContentValues.putNull("key");
        assertNull(mContentValues.get("key"));

        mContentValues.putNull("value");
        assertNull(mContentValues.get("value"));

        mContentValues.putNull("");
        assertNull(mContentValues.get(""));

        // input null as param
        mContentValues.putNull(null);
    }

    @Test
    public void testGetAsLong() {
        Long expected = 10L;
        mContentValues.put("Long", expected);
        assertEquals(expected, mContentValues.getAsLong("Long"));

        expected = -1000L;
        mContentValues.put("Long", expected);
        assertEquals(expected, mContentValues.getAsLong("Long"));

        // input null as param
        assertNull(mContentValues.getAsLong(null));
    }

    @Test
    public void testGetAsByte() {
        Byte expected = 'a';
        mContentValues.put("Byte", expected);
        assertEquals(expected, mContentValues.getAsByte("Byte"));

        expected = 'z';
        mContentValues.put("Byte", expected);
        assertEquals(expected, mContentValues.getAsByte("Byte"));

        // input null as param
        assertNull(mContentValues.getAsByte(null));
    }

    @Test
    public void testGetAsInteger() {
        Integer expected = 20;
        mContentValues.put("Integer", expected);
        assertEquals(expected, mContentValues.getAsInteger("Integer"));

        expected = -20000;
        mContentValues.put("Integer", expected);
        assertEquals(expected, mContentValues.getAsInteger("Integer"));

        // input null as param
        assertNull(mContentValues.getAsInteger(null));
    }

    @Test
    public void testSize() {
        assertEquals(0, mContentValues.size());

        mContentValues.put("Integer", 10);
        mContentValues.put("Long", 10L);
        assertEquals(2, mContentValues.size());

        mContentValues.put("String", "b");
        mContentValues.put("Boolean", false);
        assertEquals(4, mContentValues.size());

        mContentValues.clear();
        assertEquals(0, mContentValues.size());
    }

    @Test
    public void testGetAsShort() {
        Short expected = 20;
        mContentValues.put("Short", expected);
        assertEquals(expected, mContentValues.getAsShort("Short"));

        expected = -200;
        mContentValues.put("Short", expected);
        assertEquals(expected, mContentValues.getAsShort("Short"));

        // input null as param
        assertNull(mContentValues.getAsShort(null));
    }

    @Test
    public void testHashCode() {
        assertEquals(0, mContentValues.hashCode());

        mContentValues.put("Float", 2.2F);
        mContentValues.put("Short", 12);
        assertTrue(0 != mContentValues.hashCode());

        int hashcode = mContentValues.hashCode();
        mContentValues.remove("Short");
        assertTrue(hashcode != mContentValues.hashCode());

        mContentValues.put("Short", 12);
        assertTrue(hashcode == mContentValues.hashCode());

        mContentValues.clear();
        assertEquals(0, mContentValues.hashCode());
    }

    @Test
    public void testGetAsFloat() {
        Float expected = 1.0F;
        mContentValues.put("Float", expected);
        assertEquals(expected, mContentValues.getAsFloat("Float"));

        expected = -5.5F;
        mContentValues.put("Float", expected);
        assertEquals(expected, mContentValues.getAsFloat("Float"));

        // input null as param
        assertNull(mContentValues.getAsFloat(null));
    }

    @Test
    public void testGetAsBoolean() {
        mContentValues.put("Boolean", true);
        assertTrue(mContentValues.getAsBoolean("Boolean"));

        mContentValues.put("Boolean", false);
        assertFalse(mContentValues.getAsBoolean("Boolean"));

        // input null as param
        assertNull(mContentValues.getAsBoolean(null));
    }

    @Test
    public void testToString() {
        assertNotNull(mContentValues.toString());

        mContentValues.put("Float", 1.1F);
        assertNotNull(mContentValues.toString());
        assertTrue(mContentValues.toString().length() > 0);
    }

    @Test
    public void testGet() {
        Object expected = "android";
        mContentValues.put("Object", "android");
        assertSame(expected, mContentValues.get("Object"));

        expected = 20;
        mContentValues.put("Object", 20);
        assertSame(expected, mContentValues.get("Object"));

        // input null as params
        assertNull(mContentValues.get(null));
    }

    @Test
    public void testEquals() {
        mContentValues.put("Boolean", false);
        mContentValues.put("String", "string");

        ContentValues cv = new ContentValues();
        cv.put("Boolean", false);
        cv.put("String", "string");

        assertTrue(mContentValues.equals(cv));
    }

    @Test
    public void testEqualsFailure() {
        // the target object is not an instance of ContentValues.
        assertFalse(mContentValues.equals(new String()));

        // the two object is not equals
        mContentValues.put("Boolean", false);
        mContentValues.put("String", "string");

        ContentValues cv = new ContentValues();
        cv.put("Boolean", true);
        cv.put("String", "111");

        assertFalse(mContentValues.equals(cv));
    }

    @Test
    public void testGetAsDouble() {
        Double expected = 10.2;
        mContentValues.put("Double", expected);
        assertEquals(expected, mContentValues.getAsDouble("Double"));

        expected = -15.4;
        mContentValues.put("Double", expected);
        assertEquals(expected, mContentValues.getAsDouble("Double"));

        // input null as params
        assertNull(mContentValues.getAsDouble(null));
    }

    @Test
    public void testPutString() {
        String expected = "cts";
        mContentValues.put("String", expected);
        assertSame(expected, mContentValues.getAsString("String"));

        expected = "android";
        mContentValues.put("String", expected);
        assertSame(expected, mContentValues.getAsString("String"));

        // input null as params
        mContentValues.put(null, (String)null);
    }

    @Test
    public void testPutByte() {
        Byte expected = 'a';
        mContentValues.put("Byte", expected);
        assertSame(expected, mContentValues.getAsByte("Byte"));

        expected = 'z';
        mContentValues.put("Byte", expected);
        assertSame(expected, mContentValues.getAsByte("Byte"));

        // input null as params
        mContentValues.put(null, (Byte)null);
    }

    @Test
    public void testPutShort() {
        Short expected = 20;
        mContentValues.put("Short", expected);
        assertEquals(expected, mContentValues.getAsShort("Short"));

        expected = -200;
        mContentValues.put("Short", expected);
        assertEquals(expected, mContentValues.getAsShort("Short"));

        // input null as params
        mContentValues.put(null, (Short)null);
    }

    @Test
    public void testPutInteger() {
        Integer expected = 20;
        mContentValues.put("Integer", expected);
        assertEquals(expected, mContentValues.getAsInteger("Integer"));

        expected = -20000;
        mContentValues.put("Integer", expected);
        assertEquals(expected, mContentValues.getAsInteger("Integer"));

        // input null as params
        mContentValues.put(null, (Integer)null);
    }

    @Test
    public void testPutLong() {
        Long expected = 10L;
        mContentValues.put("Long", expected);
        assertEquals(expected, mContentValues.getAsLong("Long"));

        expected = -1000L;
        mContentValues.put("Long", expected);
        assertEquals(expected, mContentValues.getAsLong("Long"));

        // input null as params
        mContentValues.put(null, (Long)null);
    }

    @Test
    public void testPutFloat() {
        Float expected = 1.0F;
        mContentValues.put("Float", expected);
        assertEquals(expected, mContentValues.getAsFloat("Float"));

        expected = -5.5F;
        mContentValues.put("Float", expected);
        assertEquals(expected, mContentValues.getAsFloat("Float"));

        // input null as params
        mContentValues.put(null, (Float)null);
    }

    @Test
    public void testPutDouble() {
        Double expected = 10.2;
        mContentValues.put("Double", expected);
        assertEquals(expected, mContentValues.getAsDouble("Double"));

        expected = -15.4;
        mContentValues.put("Double", expected);
        assertEquals(expected, mContentValues.getAsDouble("Double"));

        // input null as params
        mContentValues.put(null, (Double)null);
    }

    @Test
    public void testPutBoolean() {
        // set the expected value
        mContentValues.put("Boolean", true);
        assertTrue(mContentValues.getAsBoolean("Boolean"));

        mContentValues.put("Boolean", false);
        assertFalse(mContentValues.getAsBoolean("Boolean"));

        // input null as params
        mContentValues.put(null, (Boolean)null);
    }

    @Test
    public void testPutByteArray() {
        byte[] expected = new byte[] {'1', '2', '3', '4'};
        mContentValues.put("byte[]", expected);
        assertSame(expected, mContentValues.getAsByteArray("byte[]"));

        // input null as params
        mContentValues.put(null, (byte[])null);
    }

    @Test
    public void testContainsKey() {
        mContentValues.put("Double", 10.2);
        mContentValues.put("Float", 1.0F);

        assertTrue(mContentValues.containsKey("Double"));
        assertTrue(mContentValues.containsKey("Float"));

        assertFalse(mContentValues.containsKey("abc"));
        assertFalse(mContentValues.containsKey("cts"));

        // input null as param
        assertFalse(mContentValues.containsKey(null));
    }

    @Test
    public void testClear() {
        assertEquals(0, mContentValues.size());

        mContentValues.put("Double", 10.2);
        mContentValues.put("Float", 1.0F);
        assertEquals(2, mContentValues.size());

        mContentValues.clear();
        assertEquals(0, mContentValues.size());
    }

    @Test
    @SuppressWarnings("deprecation")
    public void testAccessStringArrayList() {
        // set the expected value
        ArrayList<String> expected = new ArrayList<String>();
        expected.add(0, "cts");
        expected.add(1, "android");

        mContentValues.putStringArrayList("StringArrayList", expected);
        assertSame(expected, mContentValues.getStringArrayList("StringArrayList"));

        // input null as params
        mContentValues.putStringArrayList(null, null);
        assertNull(mContentValues.getStringArrayList(null));
    }

    @Test
    public void testRemove() {
        assertEquals(0, mContentValues.size());

        mContentValues.put("Double", 10.2);
        mContentValues.put("Float", 1.0F);
        mContentValues.put("Integer", -11);
        mContentValues.put("Boolean", false);
        assertEquals(4, mContentValues.size());

        mContentValues.remove("Integer");
        assertEquals(3, mContentValues.size());

        mContentValues.remove("Double");
        assertEquals(2, mContentValues.size());

        mContentValues.remove("Boolean");
        assertEquals(1, mContentValues.size());

        mContentValues.remove("Float");
        assertEquals(0, mContentValues.size());

        // remove null
        mContentValues.remove(null);
    }

    @Test
    public void testGetAsString() {
        String expected = "cts";
        mContentValues.put("String", expected);
        assertSame(expected, mContentValues.getAsString("String"));

        expected = "android";
        mContentValues.put("String", expected);
        assertSame(expected, mContentValues.getAsString("String"));

        // input null as param
        assertNull(mContentValues.getAsString(null));
    }

    @Test
    public void testGetAsByteArray() {
        byte[] expected = new byte[] {'1', '2', '3', '4'};
        mContentValues.put("byte[]", expected);
        assertSame(expected, mContentValues.getAsByteArray("byte[]"));

        // input null as param
        assertNull(mContentValues.getAsByteArray(null));
    }

    @Test
    @SuppressWarnings({ "unchecked" })
    public void testWriteToParcel() {
        final ContentValues before = new ContentValues();
        before.put("Integer", -110);
        before.put("String", "cts");
        before.put("Boolean", false);

        Parcel p = Parcel.obtain();
        before.writeToParcel(p, 0);
        p.setDataPosition(0);

        final ContentValues after = ContentValues.CREATOR.createFromParcel(p);
        assertEquals(3, after.size());
        assertEquals(-110, after.get("Integer"));
        assertEquals("cts", after.get("String"));
        assertEquals(false, after.get("Boolean"));
    }

    @Test
    public void testWriteToParcelFailure() {
        try {
            mContentValues.writeToParcel(null, -1);
            fail("There should be a NullPointerException thrown out.");
        } catch (NullPointerException e) {
            // expected, test success.
        }
    }

    @Test
    public void testDescribeContents() {
        assertEquals(0, mContentValues.describeContents());
    }

    @Test
    public void testPutAll() {
        assertEquals(0, mContentValues.size());

        mContentValues.put("Integer", -11);
        assertEquals(1, mContentValues.size());

        ContentValues cv = new ContentValues();
        cv.put("String", "cts");
        cv.put("Boolean", true);
        assertEquals(2, cv.size());

        mContentValues.putAll(cv);
        assertEquals(3, mContentValues.size());
    }

    @Test
    public void testPutAllFailure() {
        try {
            mContentValues.putAll(null);
            fail("There should be a NullPointerException thrown out.");
        } catch (NullPointerException e) {
            // expected, test success.
        }
    }

    @Test
    public void testIsEmpty() {
        final ContentValues values = new ContentValues();
        assertTrue(values.isEmpty());
        values.put("k", "v");
        assertFalse(values.isEmpty());
        values.clear();
        assertTrue(values.isEmpty());
    }
}
