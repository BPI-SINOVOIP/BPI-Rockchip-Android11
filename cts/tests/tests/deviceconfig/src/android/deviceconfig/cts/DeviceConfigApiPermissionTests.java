/*
 * Copyright (C) 2019 Google Inc.
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

package android.deviceconfig.cts;

import androidx.test.InstrumentationRegistry;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import android.provider.DeviceConfig;
import android.provider.DeviceConfig.OnPropertiesChangedListener;
import android.provider.DeviceConfig.Properties;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.Executor;

public final class DeviceConfigApiPermissionTests extends AbstractDeviceConfigTestCase {
    private static final String NAMESPACE = "namespace";
    private static final String NAMESPACE2 = "namespace2";
    private static final String PUBLIC_NAMESPACE = "textclassifier";
    private static final String KEY = "key";
    private static final String KEY2 = "key2";
    private static final String VALUE = "value";

    @After
    public void dropShellPermissionIdentityAfterTest() {
        if (!isSupported()) return;

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .dropShellPermissionIdentity();
    }
    /**
     * Checks that when application does not have READ_DEVICE_CONFIG or WRITE_DEVICE_CONFIG
     * permissions it cannot access any of DeviceConfig API methods
     * @throws Exception
     */
    @Test
    public void testDeviceConfigWithoutPermissions() {
        StringBuilder violations = new StringBuilder();

        // setters without write permission
        trySetPropertyWithoutWritePermission(violations);
        trySetPropertiesWithoutWritePermission(violations);

        // getters without read permission
        tryGetPropertyWithoutReadPermission(violations);
        tryGetPropertiesWithoutReadPermission(violations);
        tryAddOnPropertiesChangedListenerWithoutReadPermission(violations);

        // Bail if we found any violations
        if (violations.length() > 0) {
            fail(violations.toString());
        }
    }

    /**
     * Checks that when application has only WRITE_DEVICE_CONFIG permission it can access only
     * setProperty() methods
     * @throws Exception
     */
    @Test
    public void testDeviceConfigWithWritePermission() {

        StringBuilder violations = new StringBuilder();
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity(WRITE_DEVICE_CONFIG_PERMISSION);

        // setters with write permission
        trySetPropertyWithWritePermission(violations);
        trySetPropertiesWithWritePermission(violations);

        // getters without read permission
        tryGetPropertyWithoutReadPermission(violations);
        tryGetPropertiesWithoutReadPermission(violations);
        tryAddOnPropertiesChangedListenerWithoutReadPermission(violations);

        // Bail if we found any violations
        if (violations.length() > 0) {
            fail(violations.toString());
        }
    }

    /**
     * Checks that when application has only READ_DEVICE_CONFIG permission it can access only
     * getProperty() and addOnPropertiesChangeListener() methods
     * @throws Exception
     */
    @Test
    public void testDeviceConfigWithReadPermission() {
        StringBuilder violations = new StringBuilder();

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity(READ_DEVICE_CONFIG_PERMISSION);

        // setters without write permission
        trySetPropertyWithoutWritePermission(violations);
        trySetPropertiesWithoutWritePermission(violations);

        // getters with read permission
        tryGetPropertyWithReadPermission(violations);
        tryGetPropertiesWithReadPermission(violations);
        tryAddOnPropertiesChangedListenerWithReadPermission(violations);

        // Bail if we found any violations
        if (violations.length() > 0) {
            fail(violations.toString());
        }
    }

    /**
     * Checks that when application has both READ_DEVICE_CONFIG and WRITE_DEVICE_CONFIG permissions
     * it can access all methods
     * @throws Exception
     */
    @Test
    public void testDeviceConfigWithAllPermissions() {
        StringBuilder violations = new StringBuilder();

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity(
                        WRITE_DEVICE_CONFIG_PERMISSION,
                        READ_DEVICE_CONFIG_PERMISSION);

        // setters with write permission
        trySetPropertyWithWritePermission(violations);
        trySetPropertiesWithWritePermission(violations);

        // getters with read permission
        String property = tryGetPropertyWithReadPermission(violations);
        assertEquals("Value read from getProperty does not match written value.",
                VALUE, property);
        Properties properties = tryGetPropertiesWithReadPermission(violations);
        assertEquals("Value read from getProperties does not match written value.",
                VALUE, properties.getString(KEY2, "default_value"));
        tryAddOnPropertiesChangedListenerWithReadPermission(violations);

        // Bail if we found any violations
        if (violations.length() > 0) {
            fail(violations.toString());
        }
    }

    /**
     * Checks that when application that does not have read permission can still read from
     * public namespaces in DeviceConfig
     * @throws Exception
     */
    @Test
    public void testDeviceConfigPublicNamespacesWithoutReadPermission() {
        // if permission is not yet available in the system, skip the test
        StringBuilder violations = new StringBuilder();

        try {
            DeviceConfig.setProperty(PUBLIC_NAMESPACE, KEY, VALUE, /*makeDefault=*/ false);
            violations.append("DeviceConfig.setProperty() for public namespaces must not be "
                    + "accessible without WRITE_DEVICE_CONFIG permission\n");
        } catch (SecurityException e) {
        }

        try {
            Properties properties =
                    new Properties.Builder(PUBLIC_NAMESPACE).setString(KEY2, VALUE).build();
            DeviceConfig.setProperties(properties);
            violations.append("DeviceConfig.setProperties() for public namespaces must not be "
                    + " accessible without WRITE_DEVICE_CONFIG permission\n");
        } catch (DeviceConfig.BadConfigException e) {
            addExceptionToViolations(violations, "DeviceConfig.setProperties() should not throw "
                    + "BadConfigException without a known bad configuration", e);
        } catch (SecurityException e) {
        }

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity(WRITE_DEVICE_CONFIG_PERMISSION);

        try {
            DeviceConfig.setProperty(PUBLIC_NAMESPACE, KEY, VALUE, /*makeDefault=*/ false);
        } catch (SecurityException e) {
            addExceptionToViolations(violations, "DeviceConfig.setProperty() must be accessible "
                    + "with WRITE_DEVICE_CONFIG permission", e);
        }

        try {
            Properties properties =
                    new Properties.Builder(PUBLIC_NAMESPACE).setString(KEY, VALUE).build();
            DeviceConfig.setProperties(properties);
        } catch (DeviceConfig.BadConfigException e) {
            addExceptionToViolations(violations, "DeviceConfig.setProperties() should not throw "
                    + "BadConfigException without a known bad configuration", e);
        } catch (SecurityException e) {
            addExceptionToViolations(violations, "DeviceConfig.setProperties() must be accessible "
                    + "with WRITE_DEVICE_CONFIG permission", e);
        }

        try {
            String property = DeviceConfig.getProperty(PUBLIC_NAMESPACE, KEY);
            assertEquals("Value read from DeviceConfig API public namespace does not match written"
                    + " value.", VALUE, property);
        } catch (SecurityException e) {
            addExceptionToViolations(violations, "DeviceConfig.getProperty() for public namespaces "
                    + "must be accessible without READ_DEVICE_CONFIG permission", e);
        }

        try {
            Properties properties = DeviceConfig.getProperties(PUBLIC_NAMESPACE);
            assertEquals("Value read from DeviceConfig API public namespace does not match written"
                    + " value.", VALUE, properties.getString(KEY, "default_value"));
        } catch (SecurityException e) {
            addExceptionToViolations(violations, "DeviceConfig.getProperties() for public "
                    + "namespaces must be accessible without READ_DEVICE_CONFIG permission", e);
        }

        try {
            DeviceConfig.addOnPropertiesChangedListener(
                    PUBLIC_NAMESPACE, EXECUTOR, new TestOnPropertiesListener());
        } catch (SecurityException e) {
            addExceptionToViolations(violations, "DeviceConfig.addOnPropertiesChangeListener() for "
                    + "public namespaces must be accessible without READ_DEVICE_CONFIG permission",
                    e);
        }

        // Bail if we found any violations
        if (violations.length() > 0) {
            fail(violations.toString());
        }
    }

    class TestOnPropertiesListener implements OnPropertiesChangedListener {
        public void onPropertiesChanged(Properties properties) {

        }
    }

    private void trySetPropertyWithoutWritePermission(StringBuilder violations) {
        try {
            DeviceConfig.setProperty(NAMESPACE, KEY, VALUE, /*makeDefault=*/ false);
            violations.append("DeviceConfig.setProperty() must not be accessible without "
                    + "WRITE_DEVICE_CONFIG permission.\n");
        } catch (SecurityException e) {
        }
    }

    private void trySetPropertiesWithoutWritePermission(StringBuilder violations) {
        try {
            Properties properties =
                    new Properties.Builder(NAMESPACE2).setString(KEY2, VALUE).build();
            DeviceConfig.setProperties(properties);
            violations.append("DeviceConfig.setProperties() must not be accessible without "
                    + "WRITE_DEVICE_CONFIG permission.\n");
        } catch (DeviceConfig.BadConfigException e) {
            addExceptionToViolations(violations, "DeviceConfig.setProperties() should not throw "
                    + "BadConfigException without a known bad configuration.", e);
        } catch (SecurityException e) {
        }
    }

    private void tryGetPropertyWithoutReadPermission(StringBuilder violations) {
        try {
            DeviceConfig.getProperty(NAMESPACE, KEY);
            violations.append("DeviceConfig.getProperty() must not be accessible without "
                    + "READ_DEVICE_CONFIG permission.\n");
        } catch (SecurityException e) {
        }
    }

    private void tryGetPropertiesWithoutReadPermission(StringBuilder violations) {
        try {
            DeviceConfig.getProperties(NAMESPACE2);
            violations.append("DeviceConfig.getProperties() must not be accessible without "
                    + "READ_DEVICE_CONFIG permission.\n");
        } catch (SecurityException e) {
        }
    }

    private void tryAddOnPropertiesChangedListenerWithoutReadPermission(StringBuilder violations) {
        try {
            DeviceConfig.addOnPropertiesChangedListener(
                    NAMESPACE, EXECUTOR, new TestOnPropertiesListener());
            violations.append("DeviceConfig.addOnPropertiesChangedListener() must not be accessible"
                    + " without READ_DEVICE_CONFIG permission.\n");
        } catch (SecurityException e) {
        }
    }

    private void trySetPropertyWithWritePermission(StringBuilder violations) {
        try {
            DeviceConfig.setProperty(NAMESPACE, KEY, VALUE, /*makeDefault=*/ false);
        } catch (SecurityException e) {
            addExceptionToViolations(violations, "DeviceConfig.setProperty() must be accessible "
                    + "with WRITE_DEVICE_CONFIG permission", e);
        }
    }

    private void trySetPropertiesWithWritePermission(StringBuilder violations) {
        try {
            Properties properties =
                    new Properties.Builder(NAMESPACE2).setString(KEY2, VALUE).build();
            DeviceConfig.setProperties(properties);
        } catch (DeviceConfig.BadConfigException e) {
            violations.append("DeviceConfig.setProperties() should not throw BadConfigException"
                    + " without a known bad configuration.");
        } catch (SecurityException e) {
            addExceptionToViolations(violations, "DeviceConfig.setProperties() must be accessible "
                    + "with WRITE DEVICE_CONFIG permission", e);
        }
    }

    private String tryGetPropertyWithReadPermission(StringBuilder violations) {
        String property = null;
        try {
            property = DeviceConfig.getProperty(NAMESPACE, KEY);
        } catch (SecurityException e) {
            addExceptionToViolations(violations, "DeviceConfig.getProperty() must be accessible "
                    + "with READ_DEVICE_CONFIG permission", e);
        }
        return property;
    }

    private Properties tryGetPropertiesWithReadPermission(StringBuilder violations) {
        Properties properties = null;
        try {
            properties = DeviceConfig.getProperties(NAMESPACE2);
        } catch (SecurityException e) {
            addExceptionToViolations(violations, "DeviceConfig.getProperties() must be accessible "
                    + "with READ_DEVICE_CONFIG permission", e);
        }
        return properties;
    }

    private void tryAddOnPropertiesChangedListenerWithReadPermission(StringBuilder violations) {
        try {
            DeviceConfig.addOnPropertiesChangedListener(
                    NAMESPACE, EXECUTOR, new TestOnPropertiesListener());
        } catch (SecurityException e) {
            addExceptionToViolations(violations, "DeviceConfig.addOnPropertiesChangeListener() must"
                    + " be accessible with READ_DEVICE_CONFIG permission", e);
        }
    }

    private static void addExceptionToViolations(StringBuilder violations, String message,
            Exception e) {
        violations.append(message).append(": ").append(e).append("\n");
    }
}
