/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.net.cts

import android.Manifest.permission.CONNECTIVITY_INTERNAL
import android.Manifest.permission.NETWORK_SETTINGS
import android.Manifest.permission.READ_DEVICE_CONFIG
import android.Manifest.permission.WRITE_DEVICE_CONFIG
import android.content.pm.PackageManager.FEATURE_TELEPHONY
import android.content.pm.PackageManager.FEATURE_WIFI
import android.net.ConnectivityManager
import android.net.ConnectivityManager.NetworkCallback
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkCapabilities.NET_CAPABILITY_CAPTIVE_PORTAL
import android.net.NetworkCapabilities.TRANSPORT_WIFI
import android.net.NetworkRequest
import android.net.Uri
import android.net.cts.util.CtsNetUtils
import android.net.wifi.WifiManager
import android.os.Build
import android.os.ConditionVariable
import android.platform.test.annotations.AppModeFull
import android.provider.DeviceConfig
import android.provider.DeviceConfig.NAMESPACE_CONNECTIVITY
import android.text.TextUtils
import androidx.test.platform.app.InstrumentationRegistry.getInstrumentation
import androidx.test.runner.AndroidJUnit4
import com.android.compatibility.common.util.SystemUtil
import com.android.testutils.isDevSdkInRange
import fi.iki.elonen.NanoHTTPD
import fi.iki.elonen.NanoHTTPD.Response.IStatus
import fi.iki.elonen.NanoHTTPD.Response.Status
import junit.framework.AssertionFailedError
import org.junit.After
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.runner.RunWith
import java.util.concurrent.CompletableFuture
import java.util.concurrent.TimeUnit
import java.util.concurrent.TimeoutException
import kotlin.test.Test
import kotlin.test.assertNotEquals
import kotlin.test.assertTrue

private const val TEST_CAPTIVE_PORTAL_HTTPS_URL_SETTING = "test_captive_portal_https_url"
private const val TEST_CAPTIVE_PORTAL_HTTP_URL_SETTING = "test_captive_portal_http_url"
private const val TEST_URL_EXPIRATION_TIME = "test_url_expiration_time"

private const val TEST_HTTPS_URL_PATH = "https_path"
private const val TEST_HTTP_URL_PATH = "http_path"
private const val TEST_PORTAL_URL_PATH = "portal_path"

private const val LOCALHOST_HOSTNAME = "localhost"

// Re-connecting to the AP, obtaining an IP address, revalidating can take a long time
private const val WIFI_CONNECT_TIMEOUT_MS = 120_000L
private const val TEST_TIMEOUT_MS = 10_000L

private fun <T> CompletableFuture<T>.assertGet(timeoutMs: Long, message: String): T {
    try {
        return get(timeoutMs, TimeUnit.MILLISECONDS)
    } catch (e: TimeoutException) {
        throw AssertionFailedError(message)
    }
}

@AppModeFull(reason = "WRITE_DEVICE_CONFIG permission can't be granted to instant apps")
@RunWith(AndroidJUnit4::class)
class CaptivePortalTest {
    private val context: android.content.Context by lazy { getInstrumentation().context }
    private val wm by lazy { context.getSystemService(WifiManager::class.java) }
    private val cm by lazy { context.getSystemService(ConnectivityManager::class.java) }
    private val pm by lazy { context.packageManager }
    private val utils by lazy { CtsNetUtils(context) }

    private val server = HttpServer()

    @Before
    fun setUp() {
        doAsShell(READ_DEVICE_CONFIG) {
            // Verify that the test URLs are not normally set on the device, but do not fail if the
            // test URLs are set to what this test uses (URLs on localhost), in case the test was
            // interrupted manually and rerun.
            assertEmptyOrLocalhostUrl(TEST_CAPTIVE_PORTAL_HTTPS_URL_SETTING)
            assertEmptyOrLocalhostUrl(TEST_CAPTIVE_PORTAL_HTTP_URL_SETTING)
        }
        clearTestUrls()
        server.start()
    }

    @After
    fun tearDown() {
        clearTestUrls()
        if (pm.hasSystemFeature(FEATURE_WIFI)) {
            reconnectWifi()
        }
        server.stop()
    }

    private fun assertEmptyOrLocalhostUrl(urlKey: String) {
        val url = DeviceConfig.getProperty(NAMESPACE_CONNECTIVITY, urlKey)
        assertTrue(TextUtils.isEmpty(url) || LOCALHOST_HOSTNAME == Uri.parse(url).host,
                "$urlKey must not be set in production scenarios (current value: $url)")
    }

    private fun clearTestUrls() {
        setHttpsUrl(null)
        setHttpUrl(null)
        setUrlExpiration(null)
    }

    @Test
    fun testCaptivePortalIsNotDefaultNetwork() {
        assumeTrue(pm.hasSystemFeature(FEATURE_TELEPHONY))
        assumeTrue(pm.hasSystemFeature(FEATURE_WIFI))
        utils.connectToWifi()
        utils.connectToCell()

        // Have network validation use a local server that serves a HTTPS error / HTTP redirect
        server.addResponse(TEST_PORTAL_URL_PATH, Status.OK,
                content = "Test captive portal content")
        server.addResponse(TEST_HTTPS_URL_PATH, Status.INTERNAL_ERROR)
        server.addResponse(TEST_HTTP_URL_PATH, Status.REDIRECT,
                locationHeader = server.makeUrl(TEST_PORTAL_URL_PATH))
        setHttpsUrl(server.makeUrl(TEST_HTTPS_URL_PATH))
        setHttpUrl(server.makeUrl(TEST_HTTP_URL_PATH))
        // URL expiration needs to be in the next 10 minutes
        setUrlExpiration(System.currentTimeMillis() + TimeUnit.MINUTES.toMillis(9))

        // Expect the portal content to be fetched at some point after detecting the portal.
        // Some implementations may fetch the URL before startCaptivePortalApp is called.
        val portalContentRequestCv = server.addExpectRequestCv(TEST_PORTAL_URL_PATH)

        // Wait for a captive portal to be detected on the network
        val wifiNetworkFuture = CompletableFuture<Network>()
        val wifiCb = object : NetworkCallback() {
            override fun onCapabilitiesChanged(
                network: Network,
                nc: NetworkCapabilities
            ) {
                if (nc.hasCapability(NET_CAPABILITY_CAPTIVE_PORTAL)) {
                    wifiNetworkFuture.complete(network)
                }
            }
        }
        cm.requestNetwork(NetworkRequest.Builder().addTransportType(TRANSPORT_WIFI).build(), wifiCb)

        try {
            reconnectWifi()
            val network = wifiNetworkFuture.assertGet(WIFI_CONNECT_TIMEOUT_MS,
                    "Captive portal not detected after ${WIFI_CONNECT_TIMEOUT_MS}ms")

            val wifiDefaultMessage = "Wifi should not be the default network when a captive " +
                    "portal was detected and another network (mobile data) can provide internet " +
                    "access."
            assertNotEquals(network, cm.activeNetwork, wifiDefaultMessage)

            val startPortalAppPermission =
                    if (isDevSdkInRange(0, Build.VERSION_CODES.Q)) CONNECTIVITY_INTERNAL
                    else NETWORK_SETTINGS
            doAsShell(startPortalAppPermission) { cm.startCaptivePortalApp(network) }
            assertTrue(portalContentRequestCv.block(TEST_TIMEOUT_MS), "The captive portal login " +
                    "page was still not fetched ${TEST_TIMEOUT_MS}ms after startCaptivePortalApp.")

            assertNotEquals(network, cm.activeNetwork, wifiDefaultMessage)
        } finally {
            cm.unregisterNetworkCallback(wifiCb)
            server.stop()
            // disconnectFromCell should be called after connectToCell
            utils.disconnectFromCell()
        }
    }

    private fun setHttpsUrl(url: String?) = setConfig(TEST_CAPTIVE_PORTAL_HTTPS_URL_SETTING, url)
    private fun setHttpUrl(url: String?) = setConfig(TEST_CAPTIVE_PORTAL_HTTP_URL_SETTING, url)
    private fun setUrlExpiration(timestamp: Long?) = setConfig(TEST_URL_EXPIRATION_TIME,
            timestamp?.toString())

    private fun setConfig(configKey: String, value: String?) {
        doAsShell(WRITE_DEVICE_CONFIG) {
            DeviceConfig.setProperty(
                    NAMESPACE_CONNECTIVITY, configKey, value, false /* makeDefault */)
        }
    }

    private fun doAsShell(vararg permissions: String, action: () -> Unit) {
        // Wrap the below call to allow for more kotlin-like syntax
        SystemUtil.runWithShellPermissionIdentity(action, permissions)
    }

    private fun reconnectWifi() {
        utils.ensureWifiDisconnected(null /* wifiNetworkToCheck */)
        utils.ensureWifiConnected()
    }

    /**
     * A minimal HTTP server running on localhost (loopback), on a random available port.
     */
    private class HttpServer : NanoHTTPD("localhost", 0 /* auto-select the port */) {
        // Map of URL path -> HTTP response code
        private val responses = HashMap<String, Response>()

        // Map of path -> CV to open as soon as a request to the path is received
        private val waitForRequestCv = HashMap<String, ConditionVariable>()

        /**
         * Create a URL string that, when fetched, will hit this server with the given URL [path].
         */
        fun makeUrl(path: String): String {
            return Uri.Builder()
                    .scheme("http")
                    .encodedAuthority("localhost:$listeningPort")
                    .query(path)
                    .build()
                    .toString()
        }

        fun addResponse(
            path: String,
            statusCode: IStatus,
            locationHeader: String? = null,
            content: String = ""
        ) {
            val response = newFixedLengthResponse(statusCode, "text/plain", content)
            locationHeader?.let { response.addHeader("Location", it) }
            responses[path] = response
        }

        /**
         * Create a [ConditionVariable] that will open when a request to [path] is received.
         */
        fun addExpectRequestCv(path: String): ConditionVariable {
            return ConditionVariable().apply { waitForRequestCv[path] = this }
        }

        override fun serve(session: IHTTPSession): Response {
            waitForRequestCv[session.queryParameterString]?.open()
            return responses[session.queryParameterString]
                    // Default response is a 404
                    ?: super.serve(session)
        }
    }
}