package com.android.cts.devicepolicy;

import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.assertMetricsLogged;

import static org.junit.Assert.fail;

import android.stats.devicepolicy.EventId;

import com.android.cts.devicepolicy.metrics.DevicePolicyEventWrapper;

import org.junit.Test;

/** Host-side tests to run the CtsPasswordComplexity device-side tests. */
public class PasswordComplexityTest extends BaseDevicePolicyTest {

    private static final String APP = "CtsPasswordComplexity.apk";
    private static final String PKG = "com.android.cts.passwordcomplexity";
    private static final String CLS = ".GetPasswordComplexityTest";

    private int mCurrentUserId;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        if (!mHasSecureLockScreen) {
          return;
        }

        if (!getDevice().executeShellCommand("cmd lock_settings verify")
                .startsWith("Lock credential verified successfully")) {
            fail("Please remove the device screen lock before running this test");
        }

        mCurrentUserId = getDevice().getCurrentUser();
        installAppAsUser(APP, mCurrentUserId);
    }

    @Override
    public void tearDown() throws Exception {
        if (mHasSecureLockScreen) {
            getDevice().uninstallPackage(PKG);
        }

        super.tearDown();
    }

    @Test
    public void testGetPasswordComplexity() throws Exception {
        if (!mHasSecureLockScreen) {
            return;
        }

        assertMetricsLogged(
                getDevice(),
                () -> runDeviceTestsAsUser(PKG, CLS, mCurrentUserId),
                new DevicePolicyEventWrapper
                        .Builder(EventId.GET_USER_PASSWORD_COMPLEXITY_LEVEL_VALUE)
                        .setStrings("notCalledFromParent", PKG).build());
    }
}
