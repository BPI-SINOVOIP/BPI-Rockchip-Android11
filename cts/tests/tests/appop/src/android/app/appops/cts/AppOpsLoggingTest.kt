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
 * limitations under the License.
 */

package android.app.appops.cts

import android.app.AppOpsManager
import android.app.AppOpsManager.OPSTR_ACCESS_ACCESSIBILITY
import android.app.AppOpsManager.OPSTR_CAMERA
import android.app.AppOpsManager.OPSTR_COARSE_LOCATION
import android.app.AppOpsManager.OPSTR_FINE_LOCATION
import android.app.AppOpsManager.OPSTR_GET_ACCOUNTS
import android.app.AppOpsManager.OPSTR_READ_CONTACTS
import android.app.AppOpsManager.OPSTR_READ_EXTERNAL_STORAGE
import android.app.AppOpsManager.OPSTR_WRITE_CONTACTS
import android.app.AppOpsManager.OnOpNotedCallback
import android.app.AppOpsManager.strOpToOp
import android.app.AsyncNotedAppOp
import android.app.PendingIntent
import android.app.SyncNotedAppOp
import android.app.WallpaperManager
import android.app.WallpaperManager.FLAG_SYSTEM
import android.bluetooth.BluetoothManager
import android.bluetooth.cts.BTAdapterUtils.enableAdapter as enableBTAdapter
import android.bluetooth.cts.BTAdapterUtils.disableAdapter as disableBTAdapter
import android.bluetooth.le.ScanCallback
import android.content.BroadcastReceiver
import android.content.ComponentName
import android.content.ContentValues
import android.content.Context
import android.content.Context.BIND_AUTO_CREATE
import android.content.Intent
import android.content.Intent.FLAG_ACTIVITY_NEW_TASK
import android.content.IntentFilter
import android.content.ServiceConnection
import android.content.pm.PackageManager.FEATURE_BLUETOOTH
import android.content.pm.PackageManager.FEATURE_BLUETOOTH_LE
import android.content.pm.PackageManager.FEATURE_TELEPHONY
import android.hardware.camera2.CameraDevice
import android.hardware.camera2.CameraManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.net.wifi.WifiManager
import android.os.Bundle
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.platform.test.annotations.AppModeFull
import android.provider.ContactsContract
import android.telephony.TelephonyManager
import androidx.test.platform.app.InstrumentationRegistry
import com.google.common.truth.Truth.assertThat
import org.junit.After
import org.junit.Assert.fail
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import java.util.concurrent.CompletableFuture
import java.util.concurrent.Executor
import java.util.concurrent.TimeUnit.MILLISECONDS
import java.util.concurrent.TimeoutException

private const val TEST_SERVICE_PKG = "android.app.appops.cts.appthatusesappops"
private const val TIMEOUT_MILLIS = 10000L

private external fun nativeNoteOp(
    op: Int,
    uid: Int,
    packageName: String,
    attributionTag: String? = null,
    message: String? = null
)

@AppModeFull(reason = "Test relies on other app to connect to. Instant apps can't see other apps")
class AppOpsLoggingTest {
    private val context = InstrumentationRegistry.getInstrumentation().targetContext
    private val appOpsManager = context.getSystemService(AppOpsManager::class.java)

    private val myUid = android.os.Process.myUid()
    private val myPackage = context.packageName

    private lateinit var testService: IAppOpsUserService
    private lateinit var serviceConnection: ServiceConnection

    // Collected note-op calls inside of this process
    private val noted = mutableListOf<Pair<SyncNotedAppOp, Array<StackTraceElement>>>()
    private val selfNoted = mutableListOf<Pair<SyncNotedAppOp, Array<StackTraceElement>>>()
    private val asyncNoted = mutableListOf<AsyncNotedAppOp>()

    @Before
    fun loadNativeCode() {
        System.loadLibrary("CtsAppOpsTestCases_jni")
    }

    @Before
    fun setNotedAppOpsCollectorAndClearCollectedNoteOps() {
        setNotedAppOpsCollector()
        clearCollectedNotedOps()
    }

    @Before
    fun connectToService() {
        val serviceIntent = Intent()
        serviceIntent.component = ComponentName(TEST_SERVICE_PKG,
                TEST_SERVICE_PKG + ".AppOpsUserService")

        val newService = CompletableFuture<IAppOpsUserService>()
        serviceConnection = object : ServiceConnection {
            override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
                newService.complete(IAppOpsUserService.Stub.asInterface(service))
            }

            override fun onServiceDisconnected(name: ComponentName?) {
                fail("test service disconnected")
            }
        }

        context.bindService(serviceIntent, serviceConnection, BIND_AUTO_CREATE)
        testService = newService.get(TIMEOUT_MILLIS, MILLISECONDS)
    }

    private fun clearCollectedNotedOps() {
        noted.clear()
        selfNoted.clear()
        asyncNoted.clear()
    }

    private fun setNotedAppOpsCollector() {
        appOpsManager.setOnOpNotedCallback(Executor { it.run() },
                object : OnOpNotedCallback() {
                    override fun onNoted(op: SyncNotedAppOp) {
                        noted.add(op to Throwable().stackTrace)
                    }

                    override fun onSelfNoted(op: SyncNotedAppOp) {
                        selfNoted.add(op to Throwable().stackTrace)
                    }

                    override fun onAsyncNoted(asyncOp: AsyncNotedAppOp) {
                        asyncNoted.add(asyncOp)
                    }
                })
    }

    private inline fun rethrowThrowableFrom(r: () -> Unit) {
        try {
            r()
        } catch (e: Throwable) {
            throw e.cause ?: e
        }
    }

    @Test
    fun selfNoteAndCheckLog() {
        appOpsManager.noteOpNoThrow(OPSTR_COARSE_LOCATION, myUid, myPackage, null, null)

        assertThat(noted).isEmpty()
        assertThat(asyncNoted).isEmpty()

        assertThat(selfNoted.map { it.first.attributionTag to it.first.op })
            .containsExactly(null to OPSTR_COARSE_LOCATION)
    }

    @Test
    fun selfNoteAndCheckAttribution() {
        appOpsManager.noteOpNoThrow(OPSTR_COARSE_LOCATION, myUid, myPackage, TEST_ATTRIBUTION_TAG,
                null)

        assertThat(selfNoted.map { it.first.attributionTag }).containsExactly(TEST_ATTRIBUTION_TAG)
    }

    @Test
    fun nativeSelfNoteAndCheckLog() {
        nativeNoteOp(strOpToOp(OPSTR_COARSE_LOCATION), myUid, myPackage)

        assertThat(noted).isEmpty()
        assertThat(selfNoted).isEmpty()

        // All native notes will be reported as async notes
        eventually {
            assertThat(asyncNoted[0].attributionTag).isEqualTo(null)
            // There is always a message.
            assertThat(asyncNoted[0].message).isNotEqualTo(null)
            assertThat(asyncNoted[0].op).isEqualTo(OPSTR_COARSE_LOCATION)
            assertThat(asyncNoted[0].notingUid).isEqualTo(myUid)
        }
    }

    @Test
    fun nativeSelfNoteWithAttributionAndMsgAndCheckLog() {
        nativeNoteOp(strOpToOp(OPSTR_COARSE_LOCATION), myUid, myPackage,
            attributionTag = TEST_ATTRIBUTION_TAG, message = "testMsg")

        // All native notes will be reported as async notes
        eventually {
            assertThat(asyncNoted[0].attributionTag).isEqualTo(TEST_ATTRIBUTION_TAG)
            assertThat(asyncNoted[0].message).isEqualTo("testMsg")
        }
    }

    @Test
    fun disableCollectedAndNoteSyncOpAndCheckLog() {
        rethrowThrowableFrom {
            testService.disableCollectorAndCallSyncOpsWhichWillNotBeCollected(
                    AppOpsUserClient(context))
        }
    }

    @Test
    fun disableCollectedAndNoteASyncOpAndCheckLog() {
        rethrowThrowableFrom {
            testService.disableCollectorAndCallASyncOpsWhichWillBeCollected(
                    AppOpsUserClient(context))
        }
    }

    @Test
    fun noteSyncOpAndCheckLog() {
        rethrowThrowableFrom {
            testService.callApiThatNotesSyncOpAndCheckLog(AppOpsUserClient(context))
        }
    }

    @Test
    fun noteSyncWithAttributionOpAndCheckLog() {
        rethrowThrowableFrom {
            testService.callApiThatNotesSyncOpWithAttributionAndCheckLog(AppOpsUserClient(context))
        }
    }

    @Test
    fun callsBackIntoServiceAndCheckLog() {
        rethrowThrowableFrom {
            testService.callApiThatCallsBackIntoServiceAndCheckLog(
                AppOpsUserClient(context, testService))
        }
    }

    @Test
    fun noteSyncOpFromNativeCodeAndCheckLog() {
        rethrowThrowableFrom {
            testService.callApiThatNotesSyncOpFromNativeCodeAndCheckLog(
                    AppOpsUserClient(context))
        }
    }

    @Test
    fun noteSyncOpFromNativeCodeAndCheckMessage() {
        rethrowThrowableFrom {
            testService.callApiThatNotesSyncOpFromNativeCodeAndCheckMessage(
                    AppOpsUserClient(context))
        }
    }

    @Test
    fun noteSyncOpAndCheckStackTrace() {
        rethrowThrowableFrom {
            testService.callApiThatNotesSyncOpAndCheckStackTrace(AppOpsUserClient(context))
        }
    }

    @Test
    fun noteNonPermissionSyncOpAndCheckLog() {
        rethrowThrowableFrom {
            testService.callApiThatNotesNonPermissionSyncOpAndCheckLog(AppOpsUserClient(context))
        }
    }

    @Test
    fun noteSyncOpTwiceAndCheckLog() {
        rethrowThrowableFrom {
            testService.callApiThatNotesTwiceSyncOpAndCheckLog(AppOpsUserClient(context))
        }
    }

    @Test
    fun noteTwoSyncOpAndCheckLog() {
        rethrowThrowableFrom {
            testService.callApiThatNotesTwoSyncOpAndCheckLog(AppOpsUserClient(context))
        }
    }

    @Test
    fun noteSyncOpNativeAndCheckLog() {
        rethrowThrowableFrom {
            testService.callApiThatNotesSyncOpNativelyAndCheckLog(AppOpsUserClient(context))
        }
    }

    @Test
    fun noteNonPermissionSyncOpNativeAndCheckLog() {
        rethrowThrowableFrom {
            testService.callApiThatNotesNonPermissionSyncOpNativelyAndCheckLog(
                    AppOpsUserClient(context))
        }
    }

    @Test
    fun noteSyncOpOneway() {
        rethrowThrowableFrom {
            testService.callOnewayApiThatNotesSyncOpAndCheckLog(AppOpsUserClient(context))
        }
    }

    @Test
    fun noteSyncOpOnewayNative() {
        rethrowThrowableFrom {
            testService.callOnewayApiThatNotesSyncOpNativelyAndCheckLog(AppOpsUserClient(context))
        }
    }

    @Test
    fun noteSyncOpOtherUidAndCheckLog() {
        rethrowThrowableFrom {
            testService.callApiThatNotesSyncOpOtherUidAndCheckLog(AppOpsUserClient(context))
        }
    }

    @Test
    fun noteSyncOpOtherUidNativeAndCheckLog() {
        rethrowThrowableFrom {
            testService.callApiThatNotesSyncOpOtherUidNativelyAndCheckLog(AppOpsUserClient(context))
        }
    }

    @Test
    fun noteAsyncOpAndCheckLog() {
        rethrowThrowableFrom {
            testService.callApiThatNotesAsyncOpAndCheckLog(AppOpsUserClient(context))
        }
    }

    @Test
    fun noteAsyncOpWithAttributionAndCheckLog() {
        rethrowThrowableFrom {
            testService.callApiThatNotesAsyncOpWithAttributionAndCheckLog(AppOpsUserClient(context))
        }
    }

    @Test
    fun noteAsyncOpAndCheckDefaultMessage() {
        rethrowThrowableFrom {
            testService.callApiThatNotesAsyncOpAndCheckDefaultMessage(AppOpsUserClient(context))
        }
    }

    @Test
    fun noteAsyncOpAndCheckCustomMessage() {
        rethrowThrowableFrom {
            testService.callApiThatNotesAsyncOpAndCheckCustomMessage(AppOpsUserClient(context))
        }
    }

    @Test
    fun noteAsyncOpNativelyAndCheckCustomMessage() {
        rethrowThrowableFrom {
            testService.callApiThatNotesAsyncOpNativelyAndCheckCustomMessage(
                    AppOpsUserClient(context))
        }
    }

    @Test
    fun noteAsyncOpNativeAndCheckLog() {
        rethrowThrowableFrom {
            testService.callApiThatNotesAsyncOpNativelyAndCheckLog(AppOpsUserClient(context))
        }
    }

    /**
     * Realistic end-to-end test for scanning wifi
     */
    @Test
    fun getWifiScanResults() {
        val wifiManager = context.createAttributionContext(TEST_ATTRIBUTION_TAG)
            .getSystemService(WifiManager::class.java)

        val results = wifiManager.scanResults

        assertThat(noted[0].first.op).isEqualTo(OPSTR_FINE_LOCATION)
        assertThat(noted[0].first.attributionTag).isEqualTo(TEST_ATTRIBUTION_TAG)
        assertThat(noted[0].second.map { it.methodName }).contains("getWifiScanResults")
    }

    /**
     * Realistic end-to-end test for getting bluetooth scan results
     */
    @Test
    fun getBTScanResults() {
        assumeTrue("Device does not support bluetooth",
                context.packageManager.hasSystemFeature(FEATURE_BLUETOOTH))

        val testContext = context.createAttributionContext(TEST_ATTRIBUTION_TAG)
        val btAdapter = testContext.getSystemService(BluetoothManager::class.java).adapter

        val wasEnabled = enableBTAdapter(btAdapter, testContext)
        assumeTrue("Need to be able enable BT", wasEnabled)
        try {
            btAdapter.startDiscovery()
            try {
                assertThat(noted[0].first.op).isEqualTo(OPSTR_FINE_LOCATION)
                assertThat(noted[0].first.attributionTag).isEqualTo(TEST_ATTRIBUTION_TAG)
                assertThat(noted[0].second.map { it.methodName }).contains("getBTScanResults")
            } finally {
                btAdapter.cancelDiscovery()
            }
        } finally {
            disableBTAdapter(btAdapter, testContext)
        }
    }

    /**
     * Realistic end-to-end test for scanning LE bluetooth
     */
    @Test
    fun scanLEBluetooth() {
        assumeTrue("Device does not support LE bluetooth",
                context.packageManager.hasSystemFeature(FEATURE_BLUETOOTH_LE))

        val testContext = context.createAttributionContext(TEST_ATTRIBUTION_TAG)
        val btAdapter = testContext.getSystemService(BluetoothManager::class.java).adapter

        val wasEnabled = enableBTAdapter(btAdapter, testContext)
        assumeTrue("Need to be able enable BT", wasEnabled)
        try {
            val btScanner = btAdapter.bluetoothLeScanner
            val scanCallback = object : ScanCallback() {}

            btScanner.startScan(scanCallback)
            try {
                eventually {
                    assertThat(noted[0].first.op).isEqualTo(OPSTR_FINE_LOCATION)
                    assertThat(noted[0].first.attributionTag).isEqualTo(TEST_ATTRIBUTION_TAG)
                    // startScan calls into the system server which then calls back into the app to
                    // start the scan. I.e. the backtrace points back to a callback from the system
                    // server
                }
            } finally {
                btScanner.stopScan(scanCallback)
            }
        } finally {
            disableBTAdapter(btAdapter, testContext)
        }
    }

    /**
     * Realistic end-to-end test for getting last location
     */
    @Test
    fun getLastKnownLocation() {
        val locationManager = context.createAttributionContext(TEST_ATTRIBUTION_TAG)
            .getSystemService(LocationManager::class.java)

        assumeTrue("Device does not have a network provider",
            locationManager.getProviders(true).contains(LocationManager.NETWORK_PROVIDER))

        val location = locationManager.getLastKnownLocation(LocationManager.NETWORK_PROVIDER)
        assumeTrue("Could not get last known location", location != null)

        assertThat(noted.map { it.first.op }).containsAnyOf(OPSTR_COARSE_LOCATION,
            OPSTR_FINE_LOCATION)
        assertThat(noted[0].first.attributionTag).isEqualTo(TEST_ATTRIBUTION_TAG)
        assertThat(noted[0].second.map { it.methodName }).contains("getLastKnownLocation")
    }

    /**
     * Realistic end-to-end test for getting an async location
     */
    @Test
    fun getAsyncLocation() {
        val locationManager = context.createAttributionContext(TEST_ATTRIBUTION_TAG)
            .getSystemService(LocationManager::class.java)

        assumeTrue("Device does not have a network provider",
            locationManager.getProviders(true).contains(LocationManager.NETWORK_PROVIDER))

        val gotLocationChangeCallback = CompletableFuture<Unit>()

        val locationListener = object : LocationListener {
            override fun onStatusChanged(provider: String?, status: Int, extras: Bundle?) {}
            override fun onProviderEnabled(provider: String?) {}
            override fun onProviderDisabled(provider: String?) {}

            override fun onLocationChanged(location: Location?) {
                gotLocationChangeCallback.complete(Unit)
            }
        }

        locationManager.requestSingleUpdate(LocationManager.NETWORK_PROVIDER, locationListener,
                Looper.getMainLooper())

        try {
            gotLocationChangeCallback.get(TIMEOUT_MILLIS, MILLISECONDS)
        } catch (e: TimeoutException) {
            assumeTrue("Could not get location", false)
        }

        eventually {
            assertThat(asyncNoted.map { it.op }).containsAnyOf(OPSTR_COARSE_LOCATION,
                OPSTR_FINE_LOCATION)
            assertThat(asyncNoted[0].attributionTag).isEqualTo(TEST_ATTRIBUTION_TAG)

            assertThat(asyncNoted[0].message).contains(locationListener::class.java.name)
            assertThat(asyncNoted[0].message).contains(
                Integer.toString(System.identityHashCode(locationListener)))
        }
    }

    /**
     * Realistic end-to-end test for getting called back for a proximity alert
     */
    @Test
    fun triggerProximityAlert() {
        val PROXIMITY_ALERT_ACTION = "proxAlert"

        val gotProximityAlert = CompletableFuture<Unit>()

        val locationManager = context.createAttributionContext(TEST_ATTRIBUTION_TAG)
            .getSystemService(LocationManager::class.java)!!

        val proximityAlertReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent) {
                gotProximityAlert.complete(Unit)
            }
        }

        context.registerReceiver(proximityAlertReceiver, IntentFilter(PROXIMITY_ALERT_ACTION))
        try {
            val proximityAlertReceiverPendingIntent = PendingIntent.getBroadcast(context, 0,
                Intent(PROXIMITY_ALERT_ACTION).setPackage(myPackage)
                    .setFlags(Intent.FLAG_RECEIVER_FOREGROUND),
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_ONE_SHOT)

            locationManager.addProximityAlert(0.0, 0.0, Float.MAX_VALUE, TIMEOUT_MILLIS,
                proximityAlertReceiverPendingIntent)
            try {
                try {
                    gotProximityAlert.get(TIMEOUT_MILLIS, MILLISECONDS)
                } catch (e: TimeoutException) {
                    assumeTrue("Could not get proximity alert", false)
                }

                eventually {
                    assertThat(asyncNoted.map { it.op }).contains(OPSTR_FINE_LOCATION)
                    assertThat(asyncNoted[0].attributionTag).isEqualTo(TEST_ATTRIBUTION_TAG)
                    assertThat(asyncNoted[0].message).contains(PROXIMITY_ALERT_ACTION)
                }
            } finally {
                locationManager.removeProximityAlert(proximityAlertReceiverPendingIntent)
            }
        } finally {
            context.unregisterReceiver(proximityAlertReceiver)
        }
    }

    /**
     * Realistic end-to-end test for reading all contacts
     */
    @Test
    fun readFromContactsProvider() {
        context.createAttributionContext(TEST_ATTRIBUTION_TAG).contentResolver
            .query(ContactsContract.Contacts.CONTENT_URI, null, null, null, null)

        assertThat(noted.map { it.first.op }).containsExactly(OPSTR_READ_CONTACTS)
        assertThat(noted[0].first.attributionTag).isEqualTo(TEST_ATTRIBUTION_TAG)
        assertThat(noted[0].second.map { it.methodName }).contains("readFromContactsProvider")
    }

    /**
     * Realistic end-to-end test for adding a new contact
     */
    @Test
    fun writeToContactsProvider() {
        context.createAttributionContext(TEST_ATTRIBUTION_TAG).contentResolver
            .insert(ContactsContract.RawContacts.CONTENT_URI, ContentValues())

        assertThat(noted.map { it.first.op }).containsExactly(OPSTR_WRITE_CONTACTS)
        assertThat(noted[0].first.attributionTag).isEqualTo(TEST_ATTRIBUTION_TAG)
        assertThat(noted[0].second.map { it.methodName }).contains("writeToContactsProvider")
    }

    /**
     * Realistic end-to-end test for getting cell info
     */
    @Test
    fun getCellInfo() {
        assumeTrue(context.packageManager.hasSystemFeature(FEATURE_TELEPHONY))

        val telephonyManager = context.createAttributionContext(TEST_ATTRIBUTION_TAG)
            .getSystemService(TelephonyManager::class.java)

        telephonyManager.allCellInfo

        assertThat(noted[0].first.op).isEqualTo(OPSTR_FINE_LOCATION)
        assertThat(noted[0].first.attributionTag).isEqualTo(TEST_ATTRIBUTION_TAG)
        assertThat(noted[0].second.map { it.methodName }).contains("getCellInfo")
    }

    private fun openCamera(context: Context) {
        val cameraManager = context.getSystemService(CameraManager::class.java)

        val openedCamera = CompletableFuture<CameraDevice>()

        assumeTrue(cameraManager.cameraIdList.isNotEmpty())

        cameraManager.openCamera(cameraManager.cameraIdList[0], { it.run() },
            object : CameraDevice.StateCallback() {
                override fun onOpened(camera: CameraDevice) {
                    openedCamera.complete(camera)
                }

                override fun onDisconnected(camera: CameraDevice) {}
                override fun onError(camera: CameraDevice, error: Int) {}
            })

        openedCamera.get(TIMEOUT_MILLIS, MILLISECONDS).close()

        eventually {
            assertThat(asyncNoted[0].op).isEqualTo(OPSTR_CAMERA)
            assertThat(asyncNoted[0].attributionTag).isEqualTo(context.attributionTag)
            assertThat(asyncNoted[0].message).contains(cameraManager.cameraIdList[0])
        }
    }

    /**
     * Realistic end-to-end test for opening camera
     */
    @Test
    fun openCameraWithAttribution() {
        openCamera(context.createAttributionContext(TEST_ATTRIBUTION_TAG))
    }

    /**
     * Realistic end-to-end test for opening camera. This uses the default (==null) attribution.
     * This is interesting as null attribution handling is more complex in native code.
     */
    @Test
    fun openCameraWithDefaultAttribution() {
        openCamera(context.createAttributionContext(null))
    }

    /**
     * Realistic end-to-end test for getting wallpaper
     */
    @Test
    fun getWallpaper() {
        val wallpaperManager = context.createAttributionContext(TEST_ATTRIBUTION_TAG)
                .getSystemService(WallpaperManager::class.java)
        assumeTrue("Device does not support wallpaper",
                wallpaperManager.isWallpaperSupported())

        wallpaperManager.getWallpaperFile(FLAG_SYSTEM)

        assertThat(noted[0].first.op).isEqualTo(OPSTR_READ_EXTERNAL_STORAGE)
        assertThat(noted[0].first.attributionTag).isEqualTo(TEST_ATTRIBUTION_TAG)
        assertThat(noted[0].second.map { it.methodName }).contains("getWallpaper")
    }

    /**
     * Realistic end-to-end test for starting a permission protected activity
     */
    @Test
    fun startActivity() {
        context.createAttributionContext(TEST_ATTRIBUTION_TAG).startActivity(
                Intent().setComponent(ComponentName(TEST_SERVICE_PKG,
                        TEST_SERVICE_PKG + ".AutoClosingActivity"))
                        .setFlags(FLAG_ACTIVITY_NEW_TASK))

        assertThat(noted[0].first.op).isEqualTo(OPSTR_FINE_LOCATION)
        assertThat(noted[0].first.attributionTag).isEqualTo(TEST_ATTRIBUTION_TAG)
        assertThat(noted[0].second.map { it.methodName }).contains("startActivity")
    }

    @After
    fun removeNotedAppOpsCollector() {
        appOpsManager.setOnOpNotedCallback(null, null)
    }

    @After
    fun disconnectFromService() {
        context.unbindService(serviceConnection)
    }

    /**
     * Calls various noteOp-like methods in binder calls called by
     * {@link android.app.appops.cts.appthatusesappops.AppOpsUserService}
     */
    private class AppOpsUserClient(
        context: Context,
        val testService: IAppOpsUserService? = null
    ) : IAppOpsUserClient.Stub() {
        private val handler = Handler(Looper.getMainLooper())
        private val appOpsManager = context.getSystemService(AppOpsManager::class.java)

        private val myUid = android.os.Process.myUid()
        private val myPackage = context.packageName

        override fun noteSyncOp() {
            runWithShellPermissionIdentity {
                appOpsManager.noteOpNoThrow(OPSTR_COARSE_LOCATION, getCallingUid(),
                        TEST_SERVICE_PKG, null, null)
            }
        }

        override fun noteSyncOpWithAttribution(attributionTag: String) {
            runWithShellPermissionIdentity {
                appOpsManager.noteOpNoThrow(OPSTR_COARSE_LOCATION, getCallingUid(),
                    TEST_SERVICE_PKG, attributionTag, null)
            }
        }

        override fun callBackIntoService() {
            runWithShellPermissionIdentity {
                appOpsManager.noteOpNoThrow(OPSTR_FINE_LOCATION, getCallingUid(),
                    TEST_SERVICE_PKG, null, null)
            }

            testService?.callApiThatNotesSyncOpAndClearLog(this)
        }

        override fun noteNonPermissionSyncOp() {
            runWithShellPermissionIdentity {
                appOpsManager.noteOpNoThrow(OPSTR_ACCESS_ACCESSIBILITY, getCallingUid(),
                        TEST_SERVICE_PKG, null, null)
            }
        }

        override fun noteSyncOpTwice() {
            noteSyncOp()
            noteSyncOp()
        }

        override fun noteTwoSyncOp() {
            runWithShellPermissionIdentity {
                appOpsManager.noteOpNoThrow(OPSTR_COARSE_LOCATION, getCallingUid(),
                        TEST_SERVICE_PKG, null, null)

                appOpsManager.noteOpNoThrow(OPSTR_GET_ACCOUNTS, getCallingUid(), TEST_SERVICE_PKG,
                    null, null)
            }
        }

        override fun noteSyncOpNative() {
            runWithShellPermissionIdentity {
                nativeNoteOp(strOpToOp(OPSTR_COARSE_LOCATION), getCallingUid(), TEST_SERVICE_PKG)
            }
        }

        override fun noteNonPermissionSyncOpNative() {
            runWithShellPermissionIdentity {
                nativeNoteOp(strOpToOp(OPSTR_ACCESS_ACCESSIBILITY), getCallingUid(),
                        TEST_SERVICE_PKG)
            }
        }

        override fun noteSyncOpOneway() {
            runWithShellPermissionIdentity {
                appOpsManager.noteOpNoThrow(OPSTR_COARSE_LOCATION, getCallingUid(),
                        TEST_SERVICE_PKG, null, null)
            }
        }

        override fun noteSyncOpOnewayNative() {
            runWithShellPermissionIdentity {
                nativeNoteOp(strOpToOp(OPSTR_COARSE_LOCATION), getCallingUid(), TEST_SERVICE_PKG)
            }
        }

        override fun noteSyncOpOtherUid() {
            appOpsManager.noteOpNoThrow(OPSTR_COARSE_LOCATION, myUid, myPackage, null, null)
        }

        override fun noteSyncOpOtherUidNative() {
            nativeNoteOp(strOpToOp(OPSTR_COARSE_LOCATION), myUid, myPackage)
        }

        override fun noteAsyncOp() {
            val callingUid = getCallingUid()

            handler.post {
                runWithShellPermissionIdentity {
                    appOpsManager.noteOpNoThrow(OPSTR_COARSE_LOCATION, callingUid, TEST_SERVICE_PKG,
                        null, null)
                }
            }
        }

        override fun noteAsyncOpWithAttribution(attributionTag: String) {
            val callingUid = getCallingUid()

            handler.post {
                runWithShellPermissionIdentity {
                    appOpsManager.noteOpNoThrow(OPSTR_COARSE_LOCATION, callingUid, TEST_SERVICE_PKG,
                        attributionTag, null)
                }
            }
        }

        override fun noteAsyncOpWithCustomMessage() {
            val callingUid = getCallingUid()

            handler.post {
                runWithShellPermissionIdentity {
                    appOpsManager.noteOpNoThrow(OPSTR_COARSE_LOCATION, callingUid, TEST_SERVICE_PKG,
                            null, "custom msg")
                }
            }
        }

        override fun noteAsyncOpNative() {
            val callingUid = getCallingUid()

            handler.post {
                runWithShellPermissionIdentity {
                    nativeNoteOp(strOpToOp(OPSTR_COARSE_LOCATION), callingUid, TEST_SERVICE_PKG)
                }
            }
        }

        override fun noteAsyncOpNativeWithCustomMessage() {
            val callingUid = getCallingUid()

            handler.post {
                runWithShellPermissionIdentity {
                    nativeNoteOp(strOpToOp(OPSTR_COARSE_LOCATION), callingUid, TEST_SERVICE_PKG,
                        message = "native custom msg")
                }
            }
        }
    }
}
