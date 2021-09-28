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

package android.net.cts;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assume.assumeTrue;

import android.content.Context;
import android.net.ConnectivityManager;
import android.platform.test.annotations.AppModeFull;
import android.os.FileUtils;
import android.os.ParcelFileDescriptor;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.ApiLevelUtil;
import com.android.compatibility.common.util.SystemUtil;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Formatter;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class NetworkWatchlistTest {

    private static final String TEST_WATCHLIST_XML = "assets/network_watchlist_config_for_test.xml";
    private static final String TEST_EMPTY_WATCHLIST_XML =
            "assets/network_watchlist_config_empty_for_test.xml";
    private static final String TMP_CONFIG_PATH =
            "/data/local/tmp/network_watchlist_config_for_test.xml";
    // Generated from sha256sum network_watchlist_config_for_test.xml
    private static final String TEST_WATCHLIST_CONFIG_HASH =
            "B5FC4636994180D54E1E912F78178AB1D8BD2BE71D90CA9F5BBC3284E4D04ED4";

    private ConnectivityManager mConnectivityManager;
    private boolean mHasFeature;

    @Before
    public void setUp() throws Exception {
        mHasFeature = isAtLeastP();
        mConnectivityManager =
                (ConnectivityManager) InstrumentationRegistry.getContext().getSystemService(
                        Context.CONNECTIVITY_SERVICE);
        assumeTrue(mHasFeature);
        // Set empty watchlist test config before testing
        setWatchlistConfig(TEST_EMPTY_WATCHLIST_XML);
        // Verify test watchlist config is not set before testing
        byte[] result = mConnectivityManager.getNetworkWatchlistConfigHash();
        assertNotNull("Watchlist config does not exist", result);
        assertNotEquals(TEST_WATCHLIST_CONFIG_HASH, byteArrayToHexString(result));
    }

    @After
    public void tearDown() throws Exception {
        if (mHasFeature) {
            // Set empty watchlist test config after testing
            setWatchlistConfig(TEST_EMPTY_WATCHLIST_XML);
        }
    }

    private void cleanup() throws IOException {
        runCommand("rm " + TMP_CONFIG_PATH);
    }

    private boolean isAtLeastP() throws Exception {
        // TODO: replace with ApiLevelUtil.isAtLeast(Build.VERSION_CODES.P) when the P API level
        // constant is defined.
        return ApiLevelUtil.getCodename().compareToIgnoreCase("P") >= 0;
    }

    /**
     * Test if ConnectivityManager.getNetworkWatchlistConfigHash() correctly
     * returns the hash of config we set.
     */
    @Test
    @AppModeFull(reason = "Cannot access resource file in instant app mode")
    public void testGetWatchlistConfigHash() throws Exception {
        // Set watchlist config file for test
        setWatchlistConfig(TEST_WATCHLIST_XML);
        // Test if watchlist config hash value is correct
        byte[] result = mConnectivityManager.getNetworkWatchlistConfigHash();
        Assert.assertEquals(TEST_WATCHLIST_CONFIG_HASH, byteArrayToHexString(result));
    }

    private static String byteArrayToHexString(byte[] bytes) {
        Formatter formatter = new Formatter();
        for (byte b : bytes) {
            formatter.format("%02X", b);
        }
        return formatter.toString();
    }

    private void saveResourceToFile(String res, String filePath) throws IOException {
        // App can't access /data/local/tmp directly, so we pipe resource to file through stdin.
        ParcelFileDescriptor stdin = pipeFromStdin(filePath);
        pipeResourceToFileDescriptor(res, stdin);
    }

    /* Pipe stdin to a file in filePath. Returns PFD for stdin. */
    private ParcelFileDescriptor pipeFromStdin(String filePath) {
        // Not all devices have symlink for /dev/stdin, so use /proc/self/fd/0 directly.
        // /dev/stdin maps to /proc/self/fd/0.
        return runRwCommand("cp /proc/self/fd/0 " + filePath)[1];
    }

    private void pipeResourceToFileDescriptor(String res, ParcelFileDescriptor pfd)
            throws IOException {
        InputStream resStream = getClass().getClassLoader().getResourceAsStream(res);
        FileOutputStream fdStream = new ParcelFileDescriptor.AutoCloseOutputStream(pfd);

        FileUtils.copy(resStream, fdStream);

        try {
            fdStream.close();
        } catch (IOException e) {
        }
    }

    private static String runCommand(String command) throws IOException {
        return SystemUtil.runShellCommand(InstrumentationRegistry.getInstrumentation(), command);
    }

    private static ParcelFileDescriptor[] runRwCommand(String command) {
        return InstrumentationRegistry.getInstrumentation()
                .getUiAutomation().executeShellCommandRw(command);
    }

    private void setWatchlistConfig(String watchlistConfigFile) throws Exception {
        cleanup();
        saveResourceToFile(watchlistConfigFile, TMP_CONFIG_PATH);
        final String cmdResult = runCommand(
                "cmd network_watchlist set-test-config " + TMP_CONFIG_PATH).trim();
        assertThat(cmdResult).contains("Success");
        cleanup();
    }
}
