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

package com.android.networkstack

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.NotificationManager.IMPORTANCE_DEFAULT
import android.app.NotificationManager.IMPORTANCE_NONE
import android.app.PendingIntent
import android.app.PendingIntent.FLAG_IMMUTABLE
import android.content.Context
import android.content.Intent
import android.content.res.Resources
import android.net.CaptivePortalData
import android.net.ConnectivityManager
import android.net.ConnectivityManager.EXTRA_NETWORK
import android.net.ConnectivityManager.NetworkCallback
import android.net.LinkProperties
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkCapabilities.NET_CAPABILITY_VALIDATED
import android.net.NetworkCapabilities.TRANSPORT_WIFI
import android.net.Uri
import android.os.Handler
import android.os.UserHandle
import android.provider.Settings
import android.testing.AndroidTestingRunner
import android.testing.TestableLooper
import android.testing.TestableLooper.RunWithLooper
import androidx.test.filters.SmallTest
import androidx.test.platform.app.InstrumentationRegistry
import com.android.dx.mockito.inline.extended.ExtendedMockito.verify
import com.android.networkstack.NetworkStackNotifier.CHANNEL_CONNECTED
import com.android.networkstack.NetworkStackNotifier.CHANNEL_VENUE_INFO
import com.android.networkstack.NetworkStackNotifier.CONNECTED_NOTIFICATION_TIMEOUT_MS
import com.android.networkstack.NetworkStackNotifier.Dependencies
import com.android.networkstack.apishim.NetworkInformationShimImpl
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentCaptor
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.eq
import org.mockito.ArgumentMatchers.intThat
import org.mockito.Captor
import org.mockito.Mock
import org.mockito.Mockito.any
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.MockitoAnnotations
import kotlin.reflect.KClass
import kotlin.test.assertEquals

@RunWith(AndroidTestingRunner::class)
@SmallTest
@RunWithLooper
class NetworkStackNotifierTest {
    @Mock
    private lateinit var mContext: Context
    @Mock
    private lateinit var mCurrentUserContext: Context
    @Mock
    private lateinit var mAllUserContext: Context
    @Mock
    private lateinit var mDependencies: Dependencies
    @Mock
    private lateinit var mNm: NotificationManager
    @Mock
    private lateinit var mNotificationChannelsNm: NotificationManager
    @Mock
    private lateinit var mCm: ConnectivityManager
    @Mock
    private lateinit var mResources: Resources
    @Mock
    private lateinit var mPendingIntent: PendingIntent
    @Captor
    private lateinit var mNoteCaptor: ArgumentCaptor<Notification>
    @Captor
    private lateinit var mNoteIdCaptor: ArgumentCaptor<Int>
    @Captor
    private lateinit var mIntentCaptor: ArgumentCaptor<Intent>
    private lateinit var mLooper: TestableLooper
    private lateinit var mHandler: Handler
    private lateinit var mNotifier: NetworkStackNotifier

    private lateinit var mAllNetworksCb: NetworkCallback
    private lateinit var mDefaultNetworkCb: NetworkCallback

    // Lazy-init as CaptivePortalData does not exist on Q.
    private val mTestCapportLp by lazy {
        LinkProperties().apply {
            captivePortalData = CaptivePortalData.Builder()
                    .setCaptive(false)
                    .setVenueInfoUrl(Uri.parse(TEST_VENUE_INFO_URL))
                    .build()
        }
    }

    private val TEST_NETWORK = Network(42)
    private val TEST_NETWORK_TAG = TEST_NETWORK.networkHandle.toString()
    private val TEST_SSID = "TestSsid"
    private val EMPTY_CAPABILITIES = NetworkCapabilities()
    private val VALIDATED_CAPABILITIES = NetworkCapabilities()
            .addTransportType(TRANSPORT_WIFI)
            .addCapability(NET_CAPABILITY_VALIDATED)

    private val TEST_CONNECTED_DESCRIPTION = "Connected"
    private val TEST_VENUE_DESCRIPTION = "Connected / Tap to view website"

    private val TEST_VENUE_INFO_URL = "https://testvenue.example.com/info"
    private val EMPTY_CAPPORT_LP = LinkProperties()

    @Before
    fun setUp() {
        MockitoAnnotations.initMocks(this)
        mLooper = TestableLooper.get(this)
        doReturn(mResources).`when`(mContext).resources
        doReturn(TEST_CONNECTED_DESCRIPTION).`when`(mResources).getString(R.string.connected)
        doReturn(TEST_VENUE_DESCRIPTION).`when`(mResources).getString(R.string.tap_for_info)

        // applicationInfo is used by Notification.Builder
        val realContext = InstrumentationRegistry.getInstrumentation().context
        doReturn(realContext.applicationInfo).`when`(mContext).applicationInfo
        doReturn(realContext.packageName).`when`(mContext).packageName

        doReturn(mCurrentUserContext).`when`(mContext).createPackageContextAsUser(
                realContext.packageName, 0, UserHandle.CURRENT)
        doReturn(mAllUserContext).`when`(mContext).createPackageContextAsUser(
                realContext.packageName, 0, UserHandle.ALL)

        mAllUserContext.mockService(Context.NOTIFICATION_SERVICE, NotificationManager::class, mNm)
        mContext.mockService(Context.NOTIFICATION_SERVICE, NotificationManager::class,
                mNotificationChannelsNm)
        mContext.mockService(Context.CONNECTIVITY_SERVICE, ConnectivityManager::class, mCm)

        doReturn(NotificationChannel(CHANNEL_VENUE_INFO, "TestChannel", IMPORTANCE_DEFAULT))
                .`when`(mNotificationChannelsNm).getNotificationChannel(CHANNEL_VENUE_INFO)

        doReturn(mPendingIntent).`when`(mDependencies).getActivityPendingIntent(
                any(), any(), anyInt())
        mNotifier = NetworkStackNotifier(mContext, mLooper.looper, mDependencies)
        mHandler = mNotifier.handler

        val allNetworksCbCaptor = ArgumentCaptor.forClass(NetworkCallback::class.java)
        verify(mCm).registerNetworkCallback(any() /* request */, allNetworksCbCaptor.capture(),
                eq(mHandler))
        mAllNetworksCb = allNetworksCbCaptor.value

        val defaultNetworkCbCaptor = ArgumentCaptor.forClass(NetworkCallback::class.java)
        verify(mCm).registerDefaultNetworkCallback(defaultNetworkCbCaptor.capture(), eq(mHandler))
        mDefaultNetworkCb = defaultNetworkCbCaptor.value
    }

    private fun <T : Any> Context.mockService(name: String, clazz: KClass<T>, service: T) {
        doReturn(service).`when`(this).getSystemService(name)
        doReturn(name).`when`(this).getSystemServiceName(clazz.java)
        doReturn(service).`when`(this).getSystemService(clazz.java)
    }

    @Test
    fun testNoNotification() {
        onCapabilitiesChanged(EMPTY_CAPABILITIES)
        onCapabilitiesChanged(VALIDATED_CAPABILITIES)

        mLooper.processAllMessages()
        verify(mNm, never()).notify(any(), anyInt(), any())
    }

    private fun verifyConnectedNotification(timeout: Long = CONNECTED_NOTIFICATION_TIMEOUT_MS) {
        verify(mNm).notify(eq(TEST_NETWORK_TAG), mNoteIdCaptor.capture(), mNoteCaptor.capture())
        val note = mNoteCaptor.value
        assertEquals(mPendingIntent, note.contentIntent)
        assertEquals(CHANNEL_CONNECTED, note.channelId)
        assertEquals(timeout, note.timeoutAfter)
        verify(mDependencies).getActivityPendingIntent(
                eq(mCurrentUserContext), mIntentCaptor.capture(),
                intThat { it or FLAG_IMMUTABLE != 0 })
    }

    private fun verifyCanceledNotificationAfterNetworkLost() {
        onLost(TEST_NETWORK)
        mLooper.processAllMessages()
        verify(mNm).cancel(TEST_NETWORK_TAG, mNoteIdCaptor.value)
    }

    private fun verifyCanceledNotificationAfterDefaultNetworkLost() {
        onDefaultNetworkLost(TEST_NETWORK)
        mLooper.processAllMessages()
        verify(mNm).cancel(TEST_NETWORK_TAG, mNoteIdCaptor.value)
    }

    @Test
    fun testConnectedNotification_NoSsid() {
        onCapabilitiesChanged(EMPTY_CAPABILITIES)
        mNotifier.notifyCaptivePortalValidationPending(TEST_NETWORK)
        onCapabilitiesChanged(VALIDATED_CAPABILITIES)
        mLooper.processAllMessages()
        // There is no notification when SSID is not set.
        verify(mNm, never()).notify(any(), anyInt(), any())
    }

    @Test
    fun testConnectedNotification_WithSsid() {
        // NetworkCapabilities#getSSID is not available for API <= Q
        assumeTrue(NetworkInformationShimImpl.useApiAboveQ())
        val capabilities = NetworkCapabilities(VALIDATED_CAPABILITIES).setSSID(TEST_SSID)

        onCapabilitiesChanged(EMPTY_CAPABILITIES)
        mNotifier.notifyCaptivePortalValidationPending(TEST_NETWORK)
        onCapabilitiesChanged(capabilities)
        mLooper.processAllMessages()

        verifyConnectedNotification()
        verify(mResources).getString(R.string.connected)
        verifyWifiSettingsIntent(mIntentCaptor.value)
        verifyCanceledNotificationAfterNetworkLost()
    }

    @Test
    fun testConnectedVenueInfoNotification() {
        // Venue info (CaptivePortalData) is not available for API <= Q
        assumeTrue(NetworkInformationShimImpl.useApiAboveQ())
        mNotifier.notifyCaptivePortalValidationPending(TEST_NETWORK)
        onLinkPropertiesChanged(mTestCapportLp)
        onDefaultNetworkAvailable(TEST_NETWORK)
        val capabilities = NetworkCapabilities(VALIDATED_CAPABILITIES).setSSID(TEST_SSID)
        onCapabilitiesChanged(capabilities)

        mLooper.processAllMessages()

        verifyConnectedNotification(timeout = 0)
        verifyVenueInfoIntent(mIntentCaptor.value)
        verify(mResources).getString(R.string.tap_for_info)
        verifyCanceledNotificationAfterDefaultNetworkLost()
    }

    @Test
    fun testConnectedVenueInfoNotification_VenueInfoDisabled() {
        // Venue info (CaptivePortalData) is not available for API <= Q
        assumeTrue(NetworkInformationShimImpl.useApiAboveQ())
        val channel = NotificationChannel(CHANNEL_VENUE_INFO, "test channel", IMPORTANCE_NONE)
        doReturn(channel).`when`(mNotificationChannelsNm).getNotificationChannel(CHANNEL_VENUE_INFO)
        mNotifier.notifyCaptivePortalValidationPending(TEST_NETWORK)
        onLinkPropertiesChanged(mTestCapportLp)
        onDefaultNetworkAvailable(TEST_NETWORK)
        val capabilities = NetworkCapabilities(VALIDATED_CAPABILITIES).setSSID(TEST_SSID)
        onCapabilitiesChanged(capabilities)
        mLooper.processAllMessages()

        verifyConnectedNotification()
        verifyWifiSettingsIntent(mIntentCaptor.value)
        verify(mResources, never()).getString(R.string.tap_for_info)
        verifyCanceledNotificationAfterNetworkLost()
    }

    @Test
    fun testVenueInfoNotification() {
        // Venue info (CaptivePortalData) is not available for API <= Q
        assumeTrue(NetworkInformationShimImpl.useApiAboveQ())
        onLinkPropertiesChanged(mTestCapportLp)
        onDefaultNetworkAvailable(TEST_NETWORK)
        val capabilities = NetworkCapabilities(VALIDATED_CAPABILITIES).setSSID(TEST_SSID)
        onCapabilitiesChanged(capabilities)
        mLooper.processAllMessages()

        verify(mNm).notify(eq(TEST_NETWORK_TAG), mNoteIdCaptor.capture(), mNoteCaptor.capture())
        verify(mDependencies).getActivityPendingIntent(
                eq(mCurrentUserContext), mIntentCaptor.capture(),
                intThat { it or FLAG_IMMUTABLE != 0 })
        verifyVenueInfoIntent(mIntentCaptor.value)
        verifyCanceledNotificationAfterDefaultNetworkLost()
    }

    @Test
    fun testVenueInfoNotification_VenueInfoDisabled() {
        // Venue info (CaptivePortalData) is not available for API <= Q
        assumeTrue(NetworkInformationShimImpl.useApiAboveQ())
        doReturn(null).`when`(mNm).getNotificationChannel(CHANNEL_VENUE_INFO)
        onLinkPropertiesChanged(mTestCapportLp)
        onDefaultNetworkAvailable(TEST_NETWORK)
        onCapabilitiesChanged(VALIDATED_CAPABILITIES)
        mLooper.processAllMessages()

        verify(mNm, never()).notify(any(), anyInt(), any())
    }

    @Test
    fun testNonDefaultVenueInfoNotification() {
        // Venue info (CaptivePortalData) is not available for API <= Q
        assumeTrue(NetworkInformationShimImpl.useApiAboveQ())
        onLinkPropertiesChanged(mTestCapportLp)
        onCapabilitiesChanged(VALIDATED_CAPABILITIES)
        mLooper.processAllMessages()

        verify(mNm, never()).notify(eq(TEST_NETWORK_TAG), anyInt(), any())
    }

    @Test
    fun testEmptyCaptivePortalDataVenueInfoNotification() {
        // Venue info (CaptivePortalData) is not available for API <= Q
        assumeTrue(NetworkInformationShimImpl.useApiAboveQ())
        onLinkPropertiesChanged(EMPTY_CAPPORT_LP)
        onCapabilitiesChanged(VALIDATED_CAPABILITIES)
        mLooper.processAllMessages()

        verify(mNm, never()).notify(eq(TEST_NETWORK_TAG), anyInt(), any())
    }

    @Test
    fun testUnvalidatedNetworkVenueInfoNotification() {
        // Venue info (CaptivePortalData) is not available for API <= Q
        assumeTrue(NetworkInformationShimImpl.useApiAboveQ())
        onLinkPropertiesChanged(mTestCapportLp)
        onCapabilitiesChanged(EMPTY_CAPABILITIES)
        mLooper.processAllMessages()

        verify(mNm, never()).notify(eq(TEST_NETWORK_TAG), anyInt(), any())
    }

    private fun verifyVenueInfoIntent(intent: Intent) {
        assertEquals(Intent.ACTION_VIEW, intent.action)
        assertEquals(Uri.parse(TEST_VENUE_INFO_URL), intent.data)
        assertEquals<Network?>(TEST_NETWORK, intent.getParcelableExtra(EXTRA_NETWORK))
    }

    private fun verifyWifiSettingsIntent(intent: Intent) {
        assertEquals(Settings.ACTION_WIFI_SETTINGS, intent.action)
    }

    private fun onDefaultNetworkAvailable(network: Network) {
        mHandler.post {
            mDefaultNetworkCb.onAvailable(network)
        }
    }

    private fun onDefaultNetworkLost(network: Network) {
        mHandler.post {
            mDefaultNetworkCb.onLost(network)
        }
    }

    private fun onCapabilitiesChanged(capabilities: NetworkCapabilities) {
        mHandler.post {
            mAllNetworksCb.onCapabilitiesChanged(TEST_NETWORK, capabilities)
        }
    }

    private fun onLinkPropertiesChanged(lp: LinkProperties) {
        mHandler.post {
            mAllNetworksCb.onLinkPropertiesChanged(TEST_NETWORK, lp)
        }
    }

    private fun onLost(network: Network) {
        mHandler.post {
            mAllNetworksCb.onLost(network)
        }
    }
}