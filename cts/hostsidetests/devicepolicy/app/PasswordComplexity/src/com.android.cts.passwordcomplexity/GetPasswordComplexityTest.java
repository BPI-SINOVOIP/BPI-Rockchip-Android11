package com.android.cts.passwordcomplexity;

import static android.app.admin.DevicePolicyManager.PASSWORD_COMPLEXITY_HIGH;
import static android.app.admin.DevicePolicyManager.PASSWORD_COMPLEXITY_LOW;
import static android.app.admin.DevicePolicyManager.PASSWORD_COMPLEXITY_MEDIUM;
import static android.app.admin.DevicePolicyManager.PASSWORD_COMPLEXITY_NONE;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.fail;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.support.test.uiautomator.UiDevice;
import android.text.TextUtils;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

/** Tests for {@link DevicePolicyManager#getPasswordComplexity()}. */
@SmallTest
public class GetPasswordComplexityTest {

    private DevicePolicyManager mDpm;
    private UiDevice mDevice;
    private String mScreenLock;

    @Before
    public void setUp() throws Exception {
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        if (!mDevice.executeShellCommand("cmd lock_settings verify")
                .startsWith("Lock credential verified successfully")) {
            fail("Please remove the device screen lock before running this test");
        }

        mDpm = (DevicePolicyManager) InstrumentationRegistry
                .getContext().getSystemService(Context.DEVICE_POLICY_SERVICE);
    }

    @After
    public void tearDown() throws Exception {
        clearScreenLock();
    }

    @Test
    public void getPasswordComplexity_none() {
        assertPasswordComplexity(PASSWORD_COMPLEXITY_NONE);
    }

    @Test
    public void getPasswordComplexity_alphanumeric6_high() throws Exception {
        setPassword("abc!23");
        assertPasswordComplexity(PASSWORD_COMPLEXITY_HIGH);
    }

    @Test
    public void getPasswordComplexity_alphanumeric5_medium() throws Exception {
        setPassword("bc!23");
        assertPasswordComplexity(PASSWORD_COMPLEXITY_MEDIUM);
    }

    @Test
    public void getPasswordComplexity_alphanumeric4_medium() throws Exception {
        setPassword("c!23");
        assertPasswordComplexity(PASSWORD_COMPLEXITY_MEDIUM);
    }

    @Test
    public void getPasswordComplexity_alphabetic6_high() throws Exception {
        setPassword("abc!qw");
        assertPasswordComplexity(PASSWORD_COMPLEXITY_HIGH);
    }

    @Test
    public void getPasswordComplexity_alphabetic5_medium() throws Exception {
        setPassword("bc!qw");
        assertPasswordComplexity(PASSWORD_COMPLEXITY_MEDIUM);
    }

    @Test
    public void getPasswordComplexity_alphabetic4_medium() throws Exception {
        setPassword("c!qw");
        assertPasswordComplexity(PASSWORD_COMPLEXITY_MEDIUM);
    }

    @Test
    public void getPasswordComplexity_numericComplex8_high() throws Exception {
        setPin("12389647");
        assertPasswordComplexity(PASSWORD_COMPLEXITY_HIGH);
    }

    @Test
    public void getPasswordComplexity_numericComplex7_medium() throws Exception {
        setPin("1238964");
        assertPasswordComplexity(PASSWORD_COMPLEXITY_MEDIUM);
    }

    @Test
    public void getPasswordComplexity_numericComplex4_medium() throws Exception {
        setPin("1238");
        assertPasswordComplexity(PASSWORD_COMPLEXITY_MEDIUM);
    }

    @Test
    public void getPasswordComplexity_numeric16_low() throws Exception {
        setPin("1234567898765432");
        assertPasswordComplexity(PASSWORD_COMPLEXITY_LOW);
    }

    @Test
    public void getPasswordComplexity_numeric4_low() throws Exception {
        setPin("1234");
        assertPasswordComplexity(PASSWORD_COMPLEXITY_LOW);
    }

    @Test
    public void getPasswordComplexity_pattern9_low() throws Exception {
        setPattern("123456789");
        assertPasswordComplexity(PASSWORD_COMPLEXITY_LOW);
    }

    @Test
    public void getPasswordComplexity_pattern4_low() throws Exception {
        setPattern("1234");
        assertPasswordComplexity(PASSWORD_COMPLEXITY_LOW);
    }

    private void setPattern(String pattern) throws Exception {
        String out = mDevice.executeShellCommand("cmd lock_settings set-pattern " + pattern);
        if (!out.startsWith("Pattern set to")) {
            fail("Failed to set pattern: " + out);
        }
        mScreenLock = pattern;
    }

    private void setPin(String pin) throws Exception {
        String out = mDevice.executeShellCommand("cmd lock_settings set-pin " + pin);
        if (!out.startsWith("Pin set to")) {
            fail("Failed to set pin: " + out);
        }
        mScreenLock = pin;
    }

    private void setPassword(String password) throws Exception {
        String out =
                mDevice.executeShellCommand("cmd lock_settings set-password " + password);
        if (!out.startsWith("Password set to")) {
            fail("Failed to set password: " + out);
        }
        mScreenLock = password;
    }

    private void clearScreenLock() throws Exception {
        if (TextUtils.isEmpty(mScreenLock)) {
            return;
        }
        String out =
                mDevice.executeShellCommand("cmd lock_settings clear --old " + mScreenLock);
        if (!out.startsWith("Lock credential cleared")) {
            fail("Failed to clear user credential: " + out);
        }
        mScreenLock = null;
    }

    private void assertPasswordComplexity(int expectedComplexity) {
        // password metrics is updated asynchronously so let's be lenient here and retry a few times
        final int maxRetries = 15;
        int retry = 0;
        while (retry < maxRetries && mDpm.getPasswordComplexity() != expectedComplexity) {
            retry++;
            try {
                Thread.sleep(200);
            } catch (InterruptedException e) {
                break;
            }
        }
        assertEquals(expectedComplexity, mDpm.getPasswordComplexity());
    }
}
