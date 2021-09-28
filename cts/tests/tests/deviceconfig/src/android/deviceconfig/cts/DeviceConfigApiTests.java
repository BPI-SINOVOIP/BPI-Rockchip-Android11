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
 * limitations under the License
 */

package android.deviceconfig.cts;

import static android.provider.Settings.RESET_MODE_PACKAGE_DEFAULTS;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;

import android.os.SystemClock;
import android.provider.DeviceConfig;
import android.provider.DeviceConfig.OnPropertiesChangedListener;
import android.provider.DeviceConfig.Properties;

import androidx.test.InstrumentationRegistry;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

public final class DeviceConfigApiTests extends AbstractDeviceConfigTestCase {
    private static final String NAMESPACE1 = "namespace1";
    private static final String NAMESPACE2 = "namespace2";
    private static final String EMPTY_NAMESPACE = "empty_namespace";
    private static final String KEY1 = "key1";
    private static final String KEY2 = "key2";
    private static final String VALUE1 = "value1";
    private static final String VALUE2 = "value2";
    private static final String DEFAULT_VALUE = "default_value";

    private static final boolean DEFAULT_BOOLEAN_TRUE = true;
    private static final boolean DEFAULT_BOOLEAN_FALSE = false;
    private static final boolean BOOLEAN_TRUE = true;
    private static final boolean BOOLEAN_FALSE = false;
    private static final String INVALID_BOOLEAN = "TR_UE";

    private static final int DEFAULT_INT = 999;
    private static final int VALID_INT = 123;
    private static final String INVALID_INT = "12E";

    private static final long DEFAULT_LONG = 123456;
    private static final long VALID_LONG = 278724287;
    private static final String INVALID_LONG = "23232R42";

    private static final float DEFAULT_FLOAT = 123.456f;
    private static final float VALID_FLOAT = 456.789f;
    private static final String INVALID_FLOAT = "34343et";

    private static final long WAIT_FOR_PROPERTY_CHANGE_TIMEOUT_MILLIS = 2000; // 2 sec
    private final Object mLock = new Object();

    /**
     * Get necessary permissions to access and modify properties through DeviceConfig API.
     */
    @BeforeClass
    public static void setUp() throws Exception {
        if (!isSupported()) return;

        InstrumentationRegistry.getInstrumentation().getUiAutomation().adoptShellPermissionIdentity(
                WRITE_DEVICE_CONFIG_PERMISSION, READ_DEVICE_CONFIG_PERMISSION);
    }

    /**
     * Nullify properties in DeviceConfig API after completion of every test.
     */
    @After
    public void cleanUp() throws Exception {
        if (!isSupported()) return;

        // first wait to make sure callbacks for SetProperties/SetProperty
        // invoked in the test methods got emitted. So that the callbacks
        // won't interfere with setPropertiesAndAssertSuccessfulChange invoked
        // in nullifyProperty.
        TimeUnit.MILLISECONDS.sleep(WAIT_FOR_PROPERTY_CHANGE_TIMEOUT_MILLIS);
        nullifyProperty(NAMESPACE1, KEY1);
        nullifyProperty(NAMESPACE2, KEY1);
        nullifyProperty(NAMESPACE1, KEY2);
        nullifyProperty(NAMESPACE2, KEY2);
    }

    /**
     * Delete properties in DeviceConfig API after completion of all tests and drop shell
     * permissions.
     */
    @AfterClass
    public static void cleanUpAfterAllTests() {
        if (!isSupported()) return;

        deletePropertyThrowShell(NAMESPACE1, KEY1);
        deletePropertyThrowShell(NAMESPACE2, KEY1);
        deletePropertyThrowShell(NAMESPACE1, KEY2);
        deletePropertyThrowShell(NAMESPACE2, KEY2);
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .dropShellPermissionIdentity();
    }

    /**
     * Checks that getting property which does not exist returns null.
     */
    @Test
    public void testGetProperty_empty() {
        String result = DeviceConfig.getProperty(EMPTY_NAMESPACE, KEY1);
        assertNull("Request for non existant flag name in DeviceConfig API should return null "
                + "while " + result + " was returned", result);
    }

    /**
     * Checks that setting and getting property from the same namespace return correct value.
     */
    @Test
    public void testSetAndGetProperty_sameNamespace() {
        DeviceConfig.setProperty(NAMESPACE1, KEY1, VALUE1, /*makeDefault=*/false);
        String result = DeviceConfig.getProperty(NAMESPACE1, KEY1);
        assertEquals("Value read from DeviceConfig API does not match written value.", VALUE1,
                result);
    }

    /**
     * Checks that setting and getting properties from the same namespace return correct values.
     */
    @Test
    public void testSetAndGetProperties_sameNamespace() throws Exception {
        Properties properties = new Properties.Builder(NAMESPACE1)
                .setString(KEY1, VALUE1).setInt(KEY2, VALID_INT).build();
        assertEquals(DEFAULT_VALUE, DeviceConfig.getString(NAMESPACE1, KEY1, DEFAULT_VALUE));
        assertEquals(DEFAULT_INT, DeviceConfig.getInt(NAMESPACE1, KEY2, DEFAULT_INT));
        DeviceConfig.setProperties(properties);

        assertEquals(VALUE1, DeviceConfig.getString(NAMESPACE1, KEY1, DEFAULT_VALUE));
        assertEquals(VALID_INT, DeviceConfig.getInt(NAMESPACE1, KEY2, DEFAULT_INT));
    }

    /**
     * Checks that setting a property in one namespace does not set the same property in a different
     * namespace.
     */
    @Test
    public void testSetAndGetProperty_differentNamespace() {
        DeviceConfig.setProperty(NAMESPACE1, KEY1, VALUE1, /*makeDefault=*/false);
        String result = DeviceConfig.getProperty(NAMESPACE2, KEY1);
        assertNull("Value for same keys written to different namespaces must not clash", result);
    }

    /**
     * Checks that setting properties in one namespace does not set the same properties in a
     * different namespace.
     */
    @Test
    public void testSetAndGetProperties_differentNamespace() throws Exception {
        Properties properties = new Properties.Builder(NAMESPACE1)
                .setString(KEY1, VALUE1).setInt(KEY2, VALID_INT).build();
        DeviceConfig.setProperties(properties);

        assertEquals(DEFAULT_VALUE, DeviceConfig.getString(NAMESPACE2, KEY1, DEFAULT_VALUE));
        assertEquals(DEFAULT_INT, DeviceConfig.getInt(NAMESPACE2, KEY2, DEFAULT_INT));
    }

    /**
     * Checks that different namespaces can keep different values for the same key.
     */
    @Test
    public void testSetAndGetProperty_multipleNamespaces() {
        DeviceConfig.setProperty(NAMESPACE1, KEY1, VALUE1, /*makeDefault=*/false);
        DeviceConfig.setProperty(NAMESPACE2, KEY1, VALUE2, /*makeDefault=*/false);
        String result = DeviceConfig.getProperty(NAMESPACE1, KEY1);
        assertEquals("Value read from DeviceConfig API does not match written value.", VALUE1,
                result);
        result = DeviceConfig.getProperty(NAMESPACE2, KEY1);
        assertEquals("Value read from DeviceConfig API does not match written value.", VALUE2,
                result);
    }

    /**
     * Checks that different namespaces can keep different values for the same keys.
     */
    @Test
    public void testSetAndGetProperties_multipleNamespaces() throws Exception {
        int VALID_INT2 = VALID_INT + 2;
        Properties properties1 = new Properties.Builder(NAMESPACE1)
                .setString(KEY1, VALUE1).setInt(KEY2, VALID_INT).build();
        Properties properties2 = new Properties.Builder(NAMESPACE2)
                .setString(KEY1, VALUE2).setInt(KEY2, VALID_INT2).build();
        DeviceConfig.setProperties(properties1);
        DeviceConfig.setProperties(properties2);

        assertEquals(VALUE1, DeviceConfig.getString(NAMESPACE1, KEY1, DEFAULT_VALUE));
        assertEquals(VALID_INT, DeviceConfig.getInt(NAMESPACE1, KEY2, DEFAULT_INT));
        assertEquals(VALUE2, DeviceConfig.getString(NAMESPACE2, KEY1, DEFAULT_VALUE));
        assertEquals(VALID_INT2, DeviceConfig.getInt(NAMESPACE2, KEY2, DEFAULT_INT));
    }

    /**
     * Checks that saving value twice keeps the last value.
     */
    @Test
    public void testSetAndGetProperty_overrideValue() {
        DeviceConfig.setProperty(NAMESPACE1, KEY1, VALUE1, /*makeDefault=*/false);
        DeviceConfig.setProperty(NAMESPACE1, KEY1, VALUE2, /*makeDefault=*/false);
        String result = DeviceConfig.getProperty(NAMESPACE1, KEY1);
        assertEquals("New value written to the same namespace/key did not override previous"
                + " value.", VALUE2, result);
    }

    /**
     * Checks that saving values twice keeps the last values.
     */
    @Test
    public void testSetAndGetProperties_overrideValue() throws Exception {
        int VALID_INT2 = VALID_INT + 2;
        Properties properties1 = new Properties.Builder(NAMESPACE1)
                .setString(KEY1, VALUE1).setInt(KEY2, VALID_INT).build();
        Properties properties2 = new Properties.Builder(NAMESPACE1)
                .setString(KEY1, VALUE2).setInt(KEY2, VALID_INT2).build();
        DeviceConfig.setProperties(properties1);
        DeviceConfig.setProperties(properties2);

        assertEquals(VALUE2, DeviceConfig.getString(NAMESPACE1, KEY1, DEFAULT_VALUE));
        assertEquals(VALID_INT2, DeviceConfig.getInt(NAMESPACE1, KEY2, DEFAULT_INT));
    }

    /**
     * Checks that getString() for null property returns default value.
     */
    @Test
    public void testGetString_empty() {
        final String result = DeviceConfig.getString(NAMESPACE1, KEY1, DEFAULT_VALUE);
        assertEquals("DeviceConfig.getString() must return default value if property is null",
                DEFAULT_VALUE, result);
    }

    /**
     * Checks that getString() for null property returns default value even if it is null.
     */
    @Test
    public void testGetString_nullDefault() {
        final String result = DeviceConfig.getString(NAMESPACE1, KEY1, null);
        assertEquals("DeviceConfig.getString() must return default value if property is null",
                null, result);
    }

    /**
     * Checks that getString() returns string saved in property.
     */
    @Test
    public void testGetString_nonEmpty() {
        DeviceConfig.setProperty(NAMESPACE1, KEY1, VALUE1, /*makeDefault=*/false);

        final String result = DeviceConfig.getString(NAMESPACE1, KEY1, DEFAULT_VALUE);
        assertEquals("DeviceConfig.getString() must return same value as getProperty() when " +
                "property is not null", VALUE1, result);
    }

    /**
     * Checks that getString() fails with NullPointerException when called with null namespace.
     */
    @Test
    public void testGetString_nullNamespace() {
        try {
            DeviceConfig.getString(null, KEY1, DEFAULT_VALUE);
            fail("DeviceConfig.getString() with null namespace must result in "
                    + "NullPointerException");
        } catch (NullPointerException e) {
            // expected
        }
    }

    /**
     * Checks that getString() fails with NullPointerException when called with null key.
     */
    @Test
    public void testGetString_nullName() {
        try {
            DeviceConfig.getString(NAMESPACE1, null, DEFAULT_VALUE);
            fail("DeviceConfig.getString() with null name must result in NullPointerException");
        } catch (NullPointerException e) {
            // expected
        }
    }

    /**
     * Checks that getBoolean() for null property returns default value.
     */
    @Test
    public void testGetBoolean_empty() {
        final boolean result = DeviceConfig.getBoolean(NAMESPACE1, KEY1, DEFAULT_BOOLEAN_TRUE);
        assertEquals("DeviceConfig.getBoolean() must return default value if property is null",
                DEFAULT_BOOLEAN_TRUE, result);
    }

    /**
     * Checks that getBoolean() returns boolean representation of string saved in property.
     */
    @Test
    public void testGetBoolean_valid() {
        DeviceConfig.setProperty(NAMESPACE1, KEY1, String.valueOf(BOOLEAN_TRUE),
                /*makeDefault=*/false);

        final boolean result = DeviceConfig.getBoolean(NAMESPACE1, KEY1, DEFAULT_BOOLEAN_FALSE);
        assertEquals("DeviceConfig.getString() must return boolean equivalent value of"
                + " getProperty() when property is not null", BOOLEAN_TRUE, result);
    }

    /**
     * Checks that getBoolean() returns false for any invalid property value.
     */
    @Test
    public void testGetBoolean_invalid() {
        DeviceConfig.setProperty(NAMESPACE1, KEY1, INVALID_BOOLEAN, /*makeDefault=*/false);

        final boolean result = DeviceConfig.getBoolean(NAMESPACE1, KEY1, DEFAULT_BOOLEAN_TRUE);
        // Anything non-null other than case insensitive "true" parses to false.
        assertEquals("DeviceConfig.getBoolean() must return boolean equivalent value of"
                + " getProperty() when property is not null", BOOLEAN_FALSE, result);
    }

    /**
     * Checks that getBoolean() fails with NullPointerException when called with null namespace.
     */
    @Test
    public void testGetBoolean_nullNamespace() {
        try {
            DeviceConfig.getBoolean(null, KEY1, DEFAULT_BOOLEAN_TRUE);
            fail("DeviceConfig.getBoolean() with null namespace must result in "
                    + "NullPointerException");
        } catch (NullPointerException e) {
            // expected
        }
    }

    /**
     * Checks that getBoolean() fails with NullPointerException when called with null name.
     */
    @Test
    public void testGetBoolean_nullName() {
        try {
            DeviceConfig.getBoolean(NAMESPACE1, null, DEFAULT_BOOLEAN_TRUE);
            fail("DeviceConfig.getBoolean() with null name must result in NullPointerException");
        } catch (NullPointerException e) {
            // expected
        }
    }

    /**
     * Checks that getInt() for null property returns default value.
     */
    @Test
    public void testGetInt_empty() {
        final int result = DeviceConfig.getInt(NAMESPACE1, KEY1, DEFAULT_INT);
        assertEquals("DeviceConfig.getInt() must return default value if property is null",
                DEFAULT_INT, result);
    }

    /**
     * Checks that getInt() returns integer representation of string saved in property.
     */
    @Test
    public void testGetInt_valid() {
        DeviceConfig.setProperty(NAMESPACE1, KEY1, String.valueOf(VALID_INT),
                /*makeDefault=*/false);

        final int result = DeviceConfig.getInt(NAMESPACE1, KEY1, DEFAULT_INT);
        assertEquals("DeviceConfig.getInt() must return integer equivalent value of"
                + " getProperty() when property is not null", VALID_INT, result);
    }

    /**
     * Checks that getInt() returns default value if property is not well-formed integer value.
     */
    @Test
    public void testGetInt_invalid() {
        DeviceConfig.setProperty(NAMESPACE1, KEY1, INVALID_INT, /*makeDefault=*/false);

        final int result = DeviceConfig.getInt(NAMESPACE1, KEY1, DEFAULT_INT);
        // Failure to parse results in using the default value
        assertEquals("DeviceConfig.getInt() must return integer equivalent value of"
                + " getProperty() when property is not null", DEFAULT_INT, result);
    }

    /**
     * Checks that getInt() fails with NullPointerException when called with null namespace.
     */
    @Test
    public void testGetInt_nullNamespace() {
        try {
            DeviceConfig.getInt(null, KEY1, VALID_INT);
            fail("DeviceConfig.getInt() with null namespace must result in NullPointerException");
        } catch (NullPointerException e) {
            // expected
        }
    }

    /**
     * Checks that getInt() fails with NullPointerException when called with null name.
     */
    @Test
    public void testGetInt_nullName() {
        try {
            DeviceConfig.getInt(NAMESPACE1, null, VALID_INT);
            fail("DeviceConfig.getInt() with null name must result in NullPointerException");
        } catch (NullPointerException e) {
            // expected
        }
    }

    /**
     * Checks that getLong() for null property returns default value.
     */
    @Test
    public void testGetLong_empty() {
        final long result = DeviceConfig.getLong(NAMESPACE1, KEY1, DEFAULT_LONG);
        assertEquals("DeviceConfig.getLong() must return default value if property is null",
                DEFAULT_LONG, result);
    }

    /**
     * Checks that getLong() returns long representation of string saved in property.
     */
    @Test
    public void testGetLong_valid() {
        DeviceConfig.setProperty(NAMESPACE1, KEY1, String.valueOf(VALID_LONG),
                /*makeDefault=*/false);

        final long result = DeviceConfig.getLong(NAMESPACE1, KEY1, DEFAULT_LONG);
        assertEquals("DeviceConfig.getLong() must return long equivalent value of"
                + " getProperty() when property is not null", VALID_LONG, result);
    }

    /**
     * Checks that getLong() returns default value if property is not well-formed long value.
     */
    @Test
    public void testGetLong_invalid() {
        DeviceConfig.setProperty(NAMESPACE1, KEY1, INVALID_LONG, /*makeDefault=*/false);

        final long result = DeviceConfig.getLong(NAMESPACE1, KEY1, DEFAULT_LONG);
        // Failure to parse results in using the default value
        assertEquals("DeviceConfig.getLong() must return long equivalent value of"
                + " getProperty() when property is not null", DEFAULT_LONG, result);
    }

    /**
     * Checks that getLong() fails with NullPointerException when called with null namespace.
     */
    @Test
    public void testGetLong_nullNamespace() {
        try {
            DeviceConfig.getLong(null, KEY1, DEFAULT_LONG);
            fail("DeviceConfig.getLong() with null namespace must result in "
                    + "NullPointerException");
        } catch (NullPointerException e) {
            // expected
        }
    }

    /**
     * Checks that getLong() fails with NullPointerException when called with null name.
     */
    @Test
    public void testGetLong_nullName() {
        try {
            DeviceConfig.getLong(NAMESPACE1, null, 0);
            fail("DeviceConfig.getLong() with null name must result in NullPointerException");
        } catch (NullPointerException e) {
            // expected
        }
    }

    /**
     * Checks that getFloat() for null property returns default value.
     */
    @Test
    public void testGetFloat_empty() {
        final float result = DeviceConfig.getFloat(NAMESPACE1, KEY1, DEFAULT_FLOAT);
        assertEquals("DeviceConfig.getFloat() must return default value if property is null",
                DEFAULT_FLOAT, result, 0.0);
    }

    /**
     * Checks that getFloat() returns float representation of string saved in property.
     */
    @Test
    public void testGetFloat_valid() {
        DeviceConfig.setProperty(NAMESPACE1, KEY1, String.valueOf(VALID_FLOAT),
                /*makeDefault=*/false);

        final float result = DeviceConfig.getFloat(NAMESPACE1, KEY1, DEFAULT_FLOAT);
        assertEquals("DeviceConfig.getFloat() must return float equivalent value of"
                + " getProperty() when property is not null", VALID_FLOAT, result, 0.0);
    }

    /**
     * Checks that getFloat() returns default value if property is not well-formed float value.
     */
    @Test
    public void testGetFloat_invalid() {
        DeviceConfig.setProperty(NAMESPACE1, KEY1, INVALID_FLOAT, /*makeDefault=*/false);

        final float result = DeviceConfig.getFloat(NAMESPACE1, KEY1, DEFAULT_FLOAT);
        // Failure to parse results in using the default value
        assertEquals("DeviceConfig.getFloat() must return float equivalent value of"
                + " getProperty() when property is not null", DEFAULT_FLOAT, result, 0.0f);
    }

    /**
     * Checks that getFloat() fails with NullPointerException when called with null namespace.
     */
    @Test
    public void testGetFloat_nullNamespace() {
        try {
            DeviceConfig.getFloat(null, KEY1, DEFAULT_FLOAT);
            fail("DeviceConfig.getFloat() with null namespace must result in "
                    + "NullPointerException");
        } catch (NullPointerException e) {
            // expected
        }
    }

    /**
     * Checks that getFloat() fails with NullPointerException when called with null name.
     */
    @Test
    public void testGetFloat_nullName() {
        try {
            DeviceConfig.getFloat(NAMESPACE1, null, DEFAULT_FLOAT);
            fail("DeviceConfig.getFloat() with null name must result in NullPointerException");
        } catch (NullPointerException e) {
            // expected
        }
    }

    /**
     * Checks that setProperty() fails with NullPointerException when called with null namespace.
     */
    @Test
    public void testSetProperty_nullNamespace() {
        try {
            DeviceConfig.setProperty(null, KEY1, DEFAULT_VALUE, /*makeDefault=*/false);
            fail("DeviceConfig.setProperty() with null namespace must result in "
                    + "NullPointerException");
        } catch (NullPointerException e) {
            // expected
        }
    }

    /**
     * Checks that setProperty() fails with NullPointerException when called with null name.
     */
    @Test
    public void testSetProperty_nullName() {
        try {
            DeviceConfig.setProperty(NAMESPACE1, null, DEFAULT_VALUE, /*makeDefault=*/false);
            fail("DeviceConfig.setProperty() with null name must result in NullPointerException");
        } catch (NullPointerException e) {
            // expected
        }
    }

    /**
     * Checks that Properties.getString() for null property returns default value.
     */
    @Test
    public void testGetPropertiesString_empty() {
        setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, VALUE1);
        final Properties properties =
                setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, null);
        final String result = properties.getString(KEY1, DEFAULT_VALUE);
        assertEquals("DeviceConfig.Properties.getString() must return default value if property "
                        + "is null", DEFAULT_VALUE, result);
    }

    /**
     * Checks that Properties.getString() for null property returns default value even if it is
     * null.
     */
    @Test
    public void testGetPropertiesString_nullDefault() {
        setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, DEFAULT_VALUE);
        final Properties properties =
                setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, null);
        final String result = properties.getString(KEY1, null);
        assertEquals("DeviceConfig.Properties.getString() must return default value if property is "
                        + "null", null, result);
    }

    /**
     * Checks that Properties.getString() returns string saved in property.
     */
    @Test
    public void testGetPropertiesString_nonEmpty() {
        final Properties properties =
                setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, VALUE1);

        final String result = properties.getString(KEY1, DEFAULT_VALUE);
        assertEquals("DeviceConfig.Properties.getString() must return same value as getProperty() "
                + "when property is not null", VALUE1, result);
    }

    /**
     * Checks that Properties.getBoolean() for null property returns default value.
     */
    @Test
    public void testGetPropertiesBoolean_empty() {
        setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, String.valueOf(BOOLEAN_TRUE));
        final Properties properties =
                setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, null);
        final boolean result = properties.getBoolean(KEY1, DEFAULT_BOOLEAN_TRUE);
        assertEquals("DeviceConfig.Properties.getBoolean() must return default value if property "
                        + "is null", DEFAULT_BOOLEAN_TRUE, result);
    }

    /**
     * Checks that Properties.getBoolean() returns boolean representation of string saved in
     * property.
     */
    @Test
    public void testGetPropertiesBoolean_valid() {
        final Properties properties = setPropertiesAndAssertSuccessfulChange(
                NAMESPACE1, KEY1, String.valueOf(BOOLEAN_TRUE));
        final boolean result = properties.getBoolean(KEY1, DEFAULT_BOOLEAN_FALSE);
        assertEquals("DeviceConfig.Properties.getString() must return boolean equivalent value of"
                + " getProperty() when property is not null", BOOLEAN_TRUE, result);
    }

    /**
     * Checks that Properties.getBoolean() returns false for any invalid (non parselable) property
     * value.
     */
    @Test
    public void testGetPropertiesBoolean_invalid() {
        final Properties properties =
                setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, INVALID_BOOLEAN);

        final boolean result = properties.getBoolean(KEY1, DEFAULT_BOOLEAN_TRUE);
        // Anything non-null other than case insensitive "true" parses to false.
        assertEquals("DeviceConfig.Properties.getBoolean() must return boolean equivalent value of"
                + " getProperty() when property is not null", BOOLEAN_FALSE, result);
    }

    /**
     * Checks that Properties.getInt() for null property returns default value.
     */
    @Test
    public void testGetPropertiesInt_empty() {
        setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, String.valueOf(VALID_INT));
        final Properties properties =
                setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, null);

        final int result = properties.getInt(KEY1, DEFAULT_INT);
        assertEquals("DeviceConfig.Properties.getInt() must return default value if property is "
                        + "null", DEFAULT_INT, result);
    }

    /**
     * Checks that Properties.getInt() returns integer representation of string saved in property.
     */
    @Test
    public void testGetPropertiesInt_valid() {
        final Properties properties =
                setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, String.valueOf(VALID_INT));

        final int result = properties.getInt(KEY1, DEFAULT_INT);
        assertEquals("DeviceConfig.Properties.getInt() must return integer equivalent value of"
                + " getProperty() when property is not null", VALID_INT, result);
    }

    /**
     * Checks that Properties.getInt() returns default value if property is not well-formed integer
     * value.
     */
    @Test
    public void testGetPropertiesInt_invalid() {
        final Properties properties =
                setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, INVALID_INT);

        final int result = properties.getInt(KEY1, DEFAULT_INT);
        // Failure to parse results in using the default value
        assertEquals("DeviceConfig.Properties.getInt() must return integer equivalent value of"
                + " getProperty() when property is not null", DEFAULT_INT, result);
    }

    /**
     * Checks that Properties.getLong() for null property returns default value.
     */
    @Test
    public void testGetPropertiesLong_empty() {
        setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, String.valueOf(VALID_LONG));
        final Properties properties =
                setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, null);

        final long result = properties.getLong(KEY1, DEFAULT_LONG);
        assertEquals("DeviceConfig.Properties.getLong() must return default value if property is "
                        + "null", DEFAULT_LONG, result);
    }

    /**
     * Checks that Properties.getLong() returns long representation of string saved in property.
     */
    @Test
    public void testGetPropertiesLong_valid() {
        final Properties properties =
                setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, String.valueOf(VALID_LONG));

        final long result = properties.getLong(KEY1, DEFAULT_LONG);
        assertEquals("DeviceConfig.Properties.getLong() must return long equivalent value of"
                + " getProperty() when property is not null", VALID_LONG, result);
    }

    /**
     * Checks that Properties.getLong() returns default value if property is not well-formed long
     * value.
     */
    @Test
    public void testGetPropertiesLong_invalid() {
        final Properties properties =
                setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, INVALID_LONG);

        final long result = properties.getLong(KEY1, DEFAULT_LONG);
        // Failure to parse results in using the default value
        assertEquals("DeviceConfig.Properties.getLong() must return long equivalent value of"
                + " getProperty() when property is not null", DEFAULT_LONG, result);
    }

    /**
     * Checks that Properties.getFloat() for null property returns default value.
     */
    @Test
    public void testGetPropertiesFloat_empty() {
        setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, String.valueOf(VALID_FLOAT));
        final Properties properties =
                setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, null);
        final float result = properties.getFloat(KEY1, DEFAULT_FLOAT);
        assertEquals("DeviceConfig.Properties.getFloat() must return default value if property is "
                        + "null", DEFAULT_FLOAT, result, 0.0f);
    }

    /**
     * Checks that Properties.getFloat() returns float representation of string saved in property.
     */
    @Test
    public void testGetPropertiesFloat_valid() {
        final Properties properties = setPropertiesAndAssertSuccessfulChange(
                NAMESPACE1, KEY1, String.valueOf(VALID_FLOAT));

        final float result = properties.getFloat(KEY1, DEFAULT_FLOAT);
        assertEquals("DeviceConfig.Properties.getFloat() must return float equivalent value of"
                + " getProperty() when property is not null", VALID_FLOAT, result, 0.0f);
    }

    /**
     * Checks that Properties.getFloat() returns default value if property is not well-formed float
     * value.
     */
    @Test
    public void testGetPropertiesFloat_invalid() {
        final Properties properties = setPropertiesAndAssertSuccessfulChange(
                NAMESPACE1, KEY1, INVALID_FLOAT);

        final float result = properties.getFloat(KEY1, DEFAULT_FLOAT);
        // Failure to parse results in using the default value
        assertEquals("DeviceConfig.Properties.getFloat() must return float equivalent value of"
                + " getProperty() when property is not null", DEFAULT_FLOAT, result, 0.0f);
    }

    /**
     * Test that properties listener is successfully registered and provides callbacks on value
     * change when DeviceConfig.setProperty is called.
     */
    @Test
    public void testPropertiesListener_setProperty() {
        setPropertiesAndAssertSuccessfulChange(NAMESPACE1, KEY1, VALUE1);
    }

    /**
     * Test that properties listener is successfully registered and provides callbacks on value
     * change when DeviceConfig.setProperties is called.
     */
    @Test
    public void testPropertiesListener_setProperties() throws Exception {
        Properties properties = new Properties.Builder(NAMESPACE1)
                .setString(KEY1, VALUE1).setInt(KEY2, VALID_INT).build();
        setPropertiesAndAssertSuccessfulChange(properties);
    }

    /**
     * Test that two properties listeners subscribed to the same namespace are successfully
     * registered and unregistered while receiving correct updates in all states.
     */
    @Test
    public void testTwoPropertiesListenersSameNamespace() {
        final List<PropertyUpdate> receivedUpdates1 = new ArrayList<>();
        final List<PropertyUpdate> receivedUpdates2 = new ArrayList<>();

        OnPropertiesChangedListener listener1 = createOnPropertiesChangedListener(receivedUpdates1);
        OnPropertiesChangedListener listener2 = createOnPropertiesChangedListener(receivedUpdates2);

        try {
            DeviceConfig.addOnPropertiesChangedListener(NAMESPACE1, EXECUTOR, listener1);
            DeviceConfig.setProperty(NAMESPACE1, KEY1, VALUE1, /*makeDefault=*/false);

            waitForListenerUpdateOrTimeout(receivedUpdates1, /*expectedTotalUpdatesCount=*/1);
            waitForListenerUpdateOrTimeout(receivedUpdates2, /*expectedTotalUpdatesCount=*/0);

            assertEquals("OnPropertiesListener did not receive expected update",
                    receivedUpdates1.size(), /*expectedTotalUpdatesCount=*/1);
            assertEquals("OnPropertiesListener received unexpected update",
                    receivedUpdates2.size(), /*expectedTotalUpdatesCount=*/0);
            receivedUpdates1.get(0).assertEqual(NAMESPACE1, KEY1, VALUE1);

            DeviceConfig.addOnPropertiesChangedListener(NAMESPACE1, EXECUTOR, listener2);
            DeviceConfig.setProperty(NAMESPACE1, KEY1, VALUE2, /*makeDefault=*/false);

            waitForListenerUpdateOrTimeout(receivedUpdates1, /*expectedTotalUpdatesCount=*/2);
            waitForListenerUpdateOrTimeout(receivedUpdates2, /*expectedTotalUpdatesCount=*/1);

            assertEquals("OnPropertiesListener did not receive expected update",
                    receivedUpdates1.size(), 2);
            assertEquals("OnPropertiesListener did not receive expected update",
                    receivedUpdates2.size(), 1);
            receivedUpdates1.get(1).assertEqual(NAMESPACE1, KEY1, VALUE2);
            receivedUpdates2.get(0).assertEqual(NAMESPACE1, KEY1, VALUE2);

            DeviceConfig.removeOnPropertiesChangedListener(listener1);
            DeviceConfig.setProperty(NAMESPACE1, KEY1, VALUE1, /*makeDefault=*/false);

            waitForListenerUpdateOrTimeout(receivedUpdates2, /*expectedTotalUpdatesCount=*/2);
            waitForListenerUpdateOrTimeout(receivedUpdates1, /*expectedTotalUpdatesCount=*/2);

            assertEquals("OnPropertiesListener received unexpected update",
                    receivedUpdates1.size(), 2);
            assertEquals("OnPropertiesListener did not receive expected update",
                    receivedUpdates2.size(), 2);

            receivedUpdates2.get(1).assertEqual(NAMESPACE1, KEY1, VALUE1);

            DeviceConfig.removeOnPropertiesChangedListener(listener2);
            DeviceConfig.setProperty(NAMESPACE1, KEY1, VALUE2, /*makeDefault=*/false);

            waitForListenerUpdateOrTimeout(receivedUpdates2, /*expectedTotalUpdatesCount=*/2);
            waitForListenerUpdateOrTimeout(receivedUpdates1, /*expectedTotalUpdatesCount=*/2);

            assertEquals("OnPropertiesListener received unexpected update",
                    receivedUpdates1.size(), 2);
            assertEquals("OnPropertiesListener received unexpected update",
                    receivedUpdates2.size(), 2);
        } finally {
            DeviceConfig.removeOnPropertiesChangedListener(listener1);
            DeviceConfig.removeOnPropertiesChangedListener(listener2);
        }
    }

    /**
     * Test that two properties listeners subscribed to different namespaces are successfully
     * registered and unregistered while receiving correct updates in all states.
     */
    @Test
    public void testTwoPropertiesListenersDifferentNamespace() {
        final List<PropertyUpdate> receivedUpdates1 = new ArrayList<>();
        final List<PropertyUpdate> receivedUpdates2 = new ArrayList<>();

        OnPropertiesChangedListener listener1 = createOnPropertiesChangedListener(receivedUpdates1);
        OnPropertiesChangedListener listener2 = createOnPropertiesChangedListener(receivedUpdates2);

        try {
            DeviceConfig.addOnPropertiesChangedListener(NAMESPACE1, EXECUTOR, listener1);
            DeviceConfig.setProperty(NAMESPACE1, KEY1, VALUE1, /*makeDefault=*/false);

            waitForListenerUpdateOrTimeout(receivedUpdates1, /*expectedTotalUpdatesCount=*/1);
            waitForListenerUpdateOrTimeout(receivedUpdates2, /*expectedTotalUpdatesCount=*/0);

            assertEquals("OnPropertiesListener did not receive expected update",
                    receivedUpdates1.size(), 1);
            assertEquals("OnPropertiesListener received unexpected update",
                    receivedUpdates2.size(), 0);
            receivedUpdates1.get(0).assertEqual(NAMESPACE1, KEY1, VALUE1);

            DeviceConfig.addOnPropertiesChangedListener(NAMESPACE2, EXECUTOR, listener2);
            DeviceConfig.setProperty(NAMESPACE1, KEY1, VALUE2, /*makeDefault=*/false);

            waitForListenerUpdateOrTimeout(receivedUpdates1, /*expectedTotalUpdatesCount=*/2);
            waitForListenerUpdateOrTimeout(receivedUpdates2, /*expectedTotalUpdatesCount=*/0);

            assertEquals("OnPropertiesListener did not receive expected update",
                    receivedUpdates1.size(), 2);
            assertEquals("OnPropertiesListener received unexpected update",
                    receivedUpdates2.size(), 0);
            receivedUpdates1.get(1).assertEqual(NAMESPACE1, KEY1, VALUE2);

            DeviceConfig.setProperty(NAMESPACE2, KEY1, VALUE1, /*makeDefault=*/false);
            waitForListenerUpdateOrTimeout(receivedUpdates2, /*expectedTotalUpdatesCount=*/1);
            waitForListenerUpdateOrTimeout(receivedUpdates1, /*expectedTotalUpdatesCount=*/2);

            assertEquals("OnPropertiesListener received unexpected update",
                    receivedUpdates1.size(), 2);
            assertEquals("OnPropertiesListener did not receive expected update",
                    receivedUpdates2.size(), 1);

            receivedUpdates2.get(0).assertEqual(NAMESPACE2, KEY1, VALUE1);

            DeviceConfig.removeOnPropertiesChangedListener(listener1);
            DeviceConfig.setProperty(NAMESPACE1, KEY1, VALUE1, /*makeDefault=*/false);

            waitForListenerUpdateOrTimeout(receivedUpdates2, /*expectedTotalUpdatesCount=*/1);
            waitForListenerUpdateOrTimeout(receivedUpdates1, /*expectedTotalUpdatesCount=*/2);

            assertEquals("OnPropertiesListener received unexpected update",
                    receivedUpdates1.size(), 2);
            assertEquals("OnPropertiesListener received unexpected update",
                    receivedUpdates2.size(), 1);

            DeviceConfig.setProperty(NAMESPACE2, KEY1, VALUE2, /*makeDefault=*/false);
            waitForListenerUpdateOrTimeout(receivedUpdates2, /*expectedTotalUpdatesCount=*/2);
            waitForListenerUpdateOrTimeout(receivedUpdates1, /*expectedTotalUpdatesCount=*/2);

            assertEquals("OnPropertiesListener received unexpected update",
                    receivedUpdates1.size(), 2);
            assertEquals("OnPropertiesListener did not receive expected update",
                    receivedUpdates2.size(), 2);

            receivedUpdates2.get(1).assertEqual(NAMESPACE2, KEY1, VALUE2);
            DeviceConfig.removeOnPropertiesChangedListener(listener2);

            waitForListenerUpdateOrTimeout(receivedUpdates2, /*expectedTotalUpdatesCount=*/2);
            waitForListenerUpdateOrTimeout(receivedUpdates1, /*expectedTotalUpdatesCount=*/2);

            assertEquals("OnPropertiesListener received unexpected update",
                    receivedUpdates1.size(), 2);
            assertEquals("OnPropertiesListener received unexpected update",
                    receivedUpdates2.size(), 2);

        } catch(Exception e) {
            throw e;
        } finally {
            DeviceConfig.removeOnPropertiesChangedListener(listener1);
            DeviceConfig.removeOnPropertiesChangedListener(listener2);
        }
    }

    /**
     * Test that reset to package default successfully resets values.
     */
    @Test
    public void testResetToPackageDefaults() {
        DeviceConfig.setProperty(NAMESPACE1, KEY1, VALUE1, /*makeDefault=*/true);
        DeviceConfig.setProperty(NAMESPACE1, KEY1, VALUE2, /*makeDefault=*/false);

        assertEquals(DeviceConfig.getProperty(NAMESPACE1, KEY1), VALUE2);

        DeviceConfig.resetToDefaults(RESET_MODE_PACKAGE_DEFAULTS, NAMESPACE1);

        assertEquals(DeviceConfig.getProperty(NAMESPACE1, KEY1), VALUE1);
    }

    private OnPropertiesChangedListener createOnPropertiesChangedListener(
            List<PropertyUpdate> receivedUpdates) {
        OnPropertiesChangedListener changeListener = new OnPropertiesChangedListener() {
            @Override
            public void onPropertiesChanged(Properties properties) {
                synchronized (mLock) {
                    receivedUpdates.add(new PropertyUpdate(properties));
                    mLock.notifyAll();
                }
            }
        };
        return changeListener;
    }

    private void waitForListenerUpdateOrTimeout(
            List<PropertyUpdate> receivedUpdates, int expectedTotalUpdatesCount) {

        final long startTimeMillis = SystemClock.uptimeMillis();
        synchronized (mLock) {
            while (true) {
                if (receivedUpdates.size() >= expectedTotalUpdatesCount) {
                    return;
                }
                final long elapsedTimeMillis = SystemClock.uptimeMillis() - startTimeMillis;
                if (elapsedTimeMillis >= WAIT_FOR_PROPERTY_CHANGE_TIMEOUT_MILLIS) {
                    return;
                }
                final long remainingTimeMillis = WAIT_FOR_PROPERTY_CHANGE_TIMEOUT_MILLIS
                        - elapsedTimeMillis;
                try {
                    mLock.wait(remainingTimeMillis);
                } catch (InterruptedException ie) {
                    /* ignore */
                }
            }
        }
    }

    private Properties setPropertiesAndAssertSuccessfulChange(String setNamespace, String setName,
            String setValue) {
        final List<PropertyUpdate> receivedUpdates = new ArrayList<>();
        OnPropertiesChangedListener changeListener = createOnPropertiesChangedListener(receivedUpdates);

        DeviceConfig.addOnPropertiesChangedListener(setNamespace, EXECUTOR, changeListener);

        DeviceConfig.setProperty(setNamespace, setName, setValue, /*makeDefault=*/false);
        waitForListenerUpdateOrTimeout(receivedUpdates, 1);
        DeviceConfig.removeOnPropertiesChangedListener(changeListener);

        assertEquals("Failed to receive update to OnPropertiesChangedListener",
                receivedUpdates.size(), 1);
        PropertyUpdate propertiesUpdate = receivedUpdates.get(0);
        propertiesUpdate.assertEqual(setNamespace, setName, setValue);

        return propertiesUpdate.properties;
    }

    private Properties setPropertiesAndAssertSuccessfulChange(Properties properties)
            throws Exception {
        final List<PropertyUpdate> receivedUpdates = new ArrayList<>();
        OnPropertiesChangedListener changeListener
                = createOnPropertiesChangedListener(receivedUpdates);
        DeviceConfig.addOnPropertiesChangedListener(
                properties.getNamespace(), EXECUTOR, changeListener);

        DeviceConfig.setProperties(properties);
        waitForListenerUpdateOrTimeout(receivedUpdates, 1);
        DeviceConfig.removeOnPropertiesChangedListener(changeListener);

        assertEquals("Failed to receive update to OnPropertiesChangedListener",
                1, receivedUpdates.size());
        PropertyUpdate propertiesUpdate = receivedUpdates.get(0);
        propertiesUpdate.assertEqual(properties);

        return propertiesUpdate.properties;
    }

    private void nullifyProperty(String namespace, String key) {
        if (DeviceConfig.getString(namespace, key, null) != null) {
            setPropertiesAndAssertSuccessfulChange(namespace, key, null);
        }
    }

    private static void deletePropertyThrowShell(String namespace, String key) {
        InstrumentationRegistry.getInstrumentation().getUiAutomation().executeShellCommand(
                "device_config delete " + namespace + " " + key);
    }

    private static class PropertyUpdate {
        Properties properties;

        PropertyUpdate(Properties properties) {
            this.properties = properties;
        }

        void assertEqual(String namespace, String name, String value) {
            Properties properties =
                    new Properties.Builder(namespace).setString(name, value).build();
            assertEqual(properties);
        }

        void assertEqual(Properties expected) {
            assertEquals(expected.getNamespace(), properties.getNamespace());
            assertEquals(expected.getKeyset().size(), properties.getKeyset().size());
            for (String key : properties.getKeyset()) {
                assertEquals(expected.getString(key, DEFAULT_VALUE),
                        properties.getString(key, DEFAULT_VALUE));
                assertEquals(expected.getBoolean(key, DEFAULT_BOOLEAN_FALSE),
                        properties.getBoolean(key, DEFAULT_BOOLEAN_FALSE));
                assertEquals(expected.getInt(key, DEFAULT_INT),
                        properties.getInt(key, DEFAULT_INT));
                assertEquals(expected.getFloat(key, DEFAULT_FLOAT),
                        properties.getFloat(key, DEFAULT_FLOAT), 0);
                assertEquals(expected.getLong(key, DEFAULT_LONG),
                        properties.getLong(key, DEFAULT_LONG));
            }
        }
    }
}
