package android.cts.statsd.validation;

import android.cts.statsd.atom.DeviceAtomTestCase;

/**
 * Tests Suite for directories used by Statsd.
 */
public class DirectoryValidationTest extends DeviceAtomTestCase {

    public void testStatsActiveMetricDirectoryExists() throws Exception {
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE,
                ".DirectoryTests", "testStatsActiveMetricDirectoryExists");
    }

    public void testStatsDataDirectoryExists() throws Exception {
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE,
                ".DirectoryTests", "testStatsDataDirectoryExists");
    }

    public void testStatsMetadataDirectoryExists() throws Exception {
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE,
                ".DirectoryTests", "testStatsMetadataDirectoryExists");
    }

    public void testStatsServiceDirectoryExists() throws Exception {
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE,
                ".DirectoryTests", "testStatsServiceDirectoryExists");
    }

    public void testTrainInfoDirectoryExists() throws Exception {
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE,
                ".DirectoryTests", "testTrainInfoDirectoryExists");
    }
}
