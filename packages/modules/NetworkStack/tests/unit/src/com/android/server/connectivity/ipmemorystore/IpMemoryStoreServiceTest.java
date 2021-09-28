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

package com.android.server.connectivity.ipmemorystore;

import static com.android.server.connectivity.ipmemorystore.RegularMaintenanceJobService.InterruptMaintenance;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;

import android.app.job.JobScheduler;
import android.content.Context;
import android.net.ipmemorystore.Blob;
import android.net.ipmemorystore.IOnBlobRetrievedListener;
import android.net.ipmemorystore.IOnL2KeyResponseListener;
import android.net.ipmemorystore.IOnNetworkAttributesRetrievedListener;
import android.net.ipmemorystore.IOnSameL3NetworkResponseListener;
import android.net.ipmemorystore.IOnStatusAndCountListener;
import android.net.ipmemorystore.IOnStatusListener;
import android.net.ipmemorystore.NetworkAttributes;
import android.net.ipmemorystore.NetworkAttributesParcelable;
import android.net.ipmemorystore.SameL3NetworkResponse;
import android.net.ipmemorystore.SameL3NetworkResponseParcelable;
import android.net.ipmemorystore.Status;
import android.net.ipmemorystore.StatusParcelable;
import android.os.ConditionVariable;
import android.os.IBinder;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.server.networkstack.tests.R;

import libcore.io.Streams;

import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Modifier;
import java.net.Inet4Address;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.function.BiConsumer;
import java.util.function.Consumer;

/** Unit tests for {@link IpMemoryStoreService}. */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class IpMemoryStoreServiceTest {
    private static final String TEST_CLIENT_ID = "testClientId";
    private static final String TEST_DATA_NAME = "testData";
    private static final String TEST_DATABASE_NAME = "test.db";

    private static final int TEST_DATABASE_SIZE_THRESHOLD = 100 * 1024; //100KB
    private static final int DEFAULT_TIMEOUT_MS = 5000;
    private static final int LONG_TIMEOUT_MS = 30000;
    private static final int FAKE_KEY_COUNT = 20;
    private static final long LEASE_EXPIRY_NULL = -1L;
    private static final long UNIX_TIME_MS_2000_01_01 = 946652400000L;
    private static final long UNIX_TIME_MS_2100_01_01 = 4102412400000L;
    private static final int MTU_NULL = -1;
    private static final String[] FAKE_KEYS;
    private static final byte[] TEST_BLOB_DATA = new byte[] { -3, 6, 8, -9, 12,
            -128, 0, 89, 112, 91, -34 };
    static {
        FAKE_KEYS = new String[FAKE_KEY_COUNT];
        for (int i = 0; i < FAKE_KEYS.length; ++i) {
            FAKE_KEYS[i] = "fakeKey" + i;
        }
    }

    @Mock
    private Context mMockContext;
    @Mock
    private JobScheduler mMockJobScheduler;
    private File mDbFile;

    private IpMemoryStoreService mService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        final Context context = InstrumentationRegistry.getContext();
        final File dir = context.getFilesDir();
        mDbFile = new File(dir, TEST_DATABASE_NAME);
        doReturn(mDbFile).when(mMockContext).getDatabasePath(anyString());
        doReturn(mMockJobScheduler).when(mMockContext)
                .getSystemService(Context.JOB_SCHEDULER_SERVICE);
        mService = new IpMemoryStoreService(mMockContext) {
            @Override
            protected int getDbSizeThreshold() {
                return TEST_DATABASE_SIZE_THRESHOLD;
            }

            @Override
            boolean isDbSizeOverThreshold() {
                // Add a 100ms delay here for pausing maintenance job a while. Interrupted flag can
                // be set at this time.
                waitForMs(100);
                return super.isDbSizeOverThreshold();
            }
        };
    }

    @After
    public void tearDown() {
        mService.shutdown();
        mDbFile.delete();
    }

    private void copyTestData(final File file) throws Exception {
        try (
                InputStream in = InstrumentationRegistry.getContext()
                        .getResources().openRawResource(R.raw.test);
                OutputStream out = new FileOutputStream(file)
        ) {
            Streams.copy(in, out);
        }
    }

    /** Helper method to build test network attributes */
    private static NetworkAttributes.Builder buildTestNetworkAttributes(
            final Inet4Address ipAddress, final long expiry, final String cluster,
            final List<InetAddress> dnsServers, final int mtu) {
        final NetworkAttributes.Builder na = new NetworkAttributes.Builder();
        if (null != ipAddress) {
            na.setAssignedV4Address(ipAddress);
        }
        if (LEASE_EXPIRY_NULL != expiry) {
            na.setAssignedV4AddressExpiry(expiry);
        }
        if (null != cluster) {
            na.setCluster(cluster);
        }
        if (null != dnsServers) {
            na.setDnsAddresses(dnsServers);
        }
        if (MTU_NULL != mtu) {
            na.setMtu(mtu);
        }
        return na;
    }

    /** Helper method to make a vanilla IOnStatusListener */
    private IOnStatusListener onStatus(Consumer<Status> functor) {
        return new IOnStatusListener() {
            @Override
            public void onComplete(final StatusParcelable statusParcelable) throws RemoteException {
                functor.accept(new Status(statusParcelable));
            }

            @Override
            public IBinder asBinder() {
                return null;
            }

            @Override
            public int getInterfaceVersion() {
                return this.VERSION;
            }

            @Override
            public String getInterfaceHash() {
                return this.HASH;
            }
        };
    }

    /** Helper method to make a vanilla IOnStatusAndCountListener */
    private IOnStatusAndCountListener onDeleteStatus(BiConsumer<Status, Integer> functor) {
        return new IOnStatusAndCountListener() {
            @Override
            public void onComplete(final StatusParcelable statusParcelable, final int deletedCount)
                    throws RemoteException {
                functor.accept(new Status(statusParcelable), deletedCount);
            }

            @Override
            public IBinder asBinder() {
                return null;
            }

            @Override
            public int getInterfaceVersion() {
                return this.VERSION;
            }

            @Override
            public String getInterfaceHash() {
                return this.HASH;
            }
        };
    }

    /** Helper method to make an IOnBlobRetrievedListener */
    private interface OnBlobRetrievedListener {
        void onBlobRetrieved(Status status, String l2Key, String name, byte[] data);
    }
    private IOnBlobRetrievedListener onBlobRetrieved(final OnBlobRetrievedListener functor) {
        return new IOnBlobRetrievedListener() {
            @Override
            public void onBlobRetrieved(final StatusParcelable statusParcelable,
                    final String l2Key, final String name, final Blob blob) throws RemoteException {
                functor.onBlobRetrieved(new Status(statusParcelable), l2Key, name,
                        null == blob ? null : blob.data);
            }

            @Override
            public IBinder asBinder() {
                return null;
            }

            @Override
            public int getInterfaceVersion() {
                return this.VERSION;
            }

            @Override
            public String getInterfaceHash() {
                return this.HASH;
            }
        };
    }

    /** Helper method to make an IOnNetworkAttributesRetrievedListener */
    private interface OnNetworkAttributesRetrievedListener  {
        void onNetworkAttributesRetrieved(Status status, String l2Key, NetworkAttributes attr);
    }
    private IOnNetworkAttributesRetrievedListener onNetworkAttributesRetrieved(
            final OnNetworkAttributesRetrievedListener functor) {
        return new IOnNetworkAttributesRetrievedListener() {
            @Override
            public void onNetworkAttributesRetrieved(final StatusParcelable status,
                    final String l2Key, final NetworkAttributesParcelable attributes)
                    throws RemoteException {
                functor.onNetworkAttributesRetrieved(new Status(status), l2Key,
                        null == attributes ? null : new NetworkAttributes(attributes));
            }

            @Override
            public IBinder asBinder() {
                return null;
            }

            @Override
            public int getInterfaceVersion() {
                return this.VERSION;
            }

            @Override
            public String getInterfaceHash() {
                return this.HASH;
            }
        };
    }

    /** Helper method to make an IOnSameL3NetworkResponseListener */
    private interface OnSameL3NetworkResponseListener {
        void onSameL3NetworkResponse(Status status, SameL3NetworkResponse answer);
    }
    private IOnSameL3NetworkResponseListener onSameResponse(
            final OnSameL3NetworkResponseListener functor) {
        return new IOnSameL3NetworkResponseListener() {
            @Override
            public void onSameL3NetworkResponse(final StatusParcelable status,
                    final SameL3NetworkResponseParcelable sameL3Network)
                    throws RemoteException {
                functor.onSameL3NetworkResponse(new Status(status),
                        null == sameL3Network ? null : new SameL3NetworkResponse(sameL3Network));
            }

            @Override
            public IBinder asBinder() {
                return null;
            }

            @Override
            public int getInterfaceVersion() {
                return this.VERSION;
            }

            @Override
            public String getInterfaceHash() {
                return this.HASH;
            }
        };
    }

    /** Helper method to make an IOnL2KeyResponseListener */
    private interface OnL2KeyResponseListener {
        void onL2KeyResponse(Status status, String key);
    }
    private IOnL2KeyResponseListener onL2KeyResponse(final OnL2KeyResponseListener functor) {
        return new IOnL2KeyResponseListener() {
            @Override
            public void onL2KeyResponse(final StatusParcelable status, final String key)
                    throws RemoteException {
                functor.onL2KeyResponse(new Status(status), key);
            }

            @Override
            public IBinder asBinder() {
                return null;
            }

            @Override
            public int getInterfaceVersion() {
                return this.VERSION;
            }

            @Override
            public String getInterfaceHash() {
                return this.HASH;
            }
        };
    }

    // Helper method to factorize some boilerplate
    private void doLatched(final String timeoutMessage, final Consumer<CountDownLatch> functor) {
        doLatched(timeoutMessage, functor, DEFAULT_TIMEOUT_MS);
    }

    private void doLatched(final String timeoutMessage, final Consumer<CountDownLatch> functor,
            final int timeout) {
        final CountDownLatch latch = new CountDownLatch(1);
        functor.accept(latch);
        try {
            if (!latch.await(timeout, TimeUnit.MILLISECONDS)) {
                fail(timeoutMessage);
            }
        } catch (InterruptedException e) {
            fail("Thread was interrupted");
        }
    }

    // Helper method to store network attributes to database. Returns the stored attributes.
    private NetworkAttributes storeAttributes(final String l2Key, final NetworkAttributes na) {
        return storeAttributes("Did not complete storing attributes", l2Key, na);
    }
    private NetworkAttributes storeAttributes(final String timeoutMessage, final String l2Key,
            final NetworkAttributes na) {
        doLatched(timeoutMessage, latch -> mService.storeNetworkAttributes(l2Key, na.toParcelable(),
                onStatus(status -> {
                    assertTrue("Store not successful : " + status.resultCode, status.isSuccess());
                    latch.countDown();
                })));
        return na;
    }

    // Helper method to store blob data to database
    private void storeBlobOrFail(final String l2Key, final Blob b, final byte[] data) {
        storeBlobOrFail("Did not complete storing private data", l2Key, b, data);
    }
    private void storeBlobOrFail(final String timeoutMessage, final String l2Key, final Blob b,
            final byte[] data) {
        b.data = data;
        doLatched(timeoutMessage, latch -> mService.storeBlob(l2Key, TEST_CLIENT_ID, TEST_DATA_NAME,
                b, onStatus(status -> {
                    assertTrue("Store status not successful : " + status.resultCode,
                            status.isSuccess());
                    latch.countDown();
                })));
    }

    /**
     * This method is used to generate test.db file.
     *
     * Here are the steps to update the test.db file if you need to change value in DB.
     * 1. Create a new test like "testGenerateDB" and have only one line code "generateFakeData()".
     * 2. Comment out "mDbFile.delete()" in tearDown() method.
     * 3. Run "atest IpMemoryStoreServiceTest#testGenerateDB".
     * 4. Run "adb root; adb pull /data/data/com.android.server.networkstack.tests/files/test.db
     *    $YOUR_CODE_BASE/package/module/NetworkStack/tests/unit/res/raw/test.db".
     *
     */
    private void generateFakeData() {
        final int fakeDataCount = 1000;
        final int expiredRecordsCount = 100;
        try {
            final NetworkAttributes.Builder na = buildTestNetworkAttributes(
                    (Inet4Address) Inet4Address.getByName("1.2.3.4"), LEASE_EXPIRY_NULL,
                    "cluster1", Arrays.asList(Inet6Address.getByName("0A1C:2E40:480A::1CA6")),
                    219);
            final long time = System.currentTimeMillis() - 1;
            for (int i = 0; i < fakeDataCount; i++) {
                int errorCode = IpMemoryStoreDatabase.storeNetworkAttributes(
                        mService.mDb,
                        "fakeKey" + i,
                        i < expiredRecordsCount
                                ? UNIX_TIME_MS_2000_01_01 : UNIX_TIME_MS_2100_01_01 + i,
                        na.build());
                assertEquals(errorCode, Status.SUCCESS);

                errorCode = IpMemoryStoreDatabase.storeBlob(
                        mService.mDb, "fakeKey" + i, TEST_CLIENT_ID, TEST_DATA_NAME,
                        TEST_BLOB_DATA);
                assertEquals(errorCode, Status.SUCCESS);
            }

            // After inserted fake data, db size should be larger than threshold.
            assertTrue(mService.isDbSizeOverThreshold());
        } catch (final UnknownHostException e) {
            fail("Insert fake data fail");
        }
    }

    /** Wait for assigned time. */
    private void waitForMs(long ms) {
        try {
            Thread.sleep(ms);
        } catch (final InterruptedException e) {
            fail("Thread was interrupted");
        }
    }

    @Test
    public void testNetworkAttributes() throws UnknownHostException {
        final String l2Key = FAKE_KEYS[0];
        final NetworkAttributes.Builder na = buildTestNetworkAttributes(
                (Inet4Address) Inet4Address.getByName("1.2.3.4"),
                System.currentTimeMillis() + 7_200_000, "cluster1", null, 219);
        NetworkAttributes attributes = na.build();
        storeAttributes(l2Key, attributes);

        doLatched("Did not complete retrieving attributes", latch ->
                mService.retrieveNetworkAttributes(l2Key, onNetworkAttributesRetrieved(
                        (status, key, attr) -> {
                            assertTrue("Retrieve network attributes not successful : "
                                    + status.resultCode, status.isSuccess());
                            assertEquals(l2Key, key);
                            assertEquals(attributes, attr);
                            latch.countDown();
                        })));

        final NetworkAttributes.Builder na2 = new NetworkAttributes.Builder();
        na.setDnsAddresses(Arrays.asList(
                new InetAddress[] {Inet6Address.getByName("0A1C:2E40:480A::1CA6")}));
        final NetworkAttributes attributes2 = na2.build();
        storeAttributes("Did not complete storing attributes 2", l2Key, attributes2);

        doLatched("Did not complete retrieving attributes 2", latch ->
                mService.retrieveNetworkAttributes(l2Key, onNetworkAttributesRetrieved(
                        (status, key, attr) -> {
                            assertTrue("Retrieve network attributes not successful : "
                                    + status.resultCode, status.isSuccess());
                            assertEquals(l2Key, key);
                            assertEquals(attributes.assignedV4Address, attr.assignedV4Address);
                            assertEquals(attributes.assignedV4AddressExpiry,
                                    attr.assignedV4AddressExpiry);
                            assertEquals(attributes.cluster, attr.cluster);
                            assertEquals(attributes.mtu, attr.mtu);
                            assertEquals(attributes2.dnsAddresses, attr.dnsAddresses);
                            latch.countDown();
                        })));

        doLatched("Did not complete retrieving attributes 3", latch ->
                mService.retrieveNetworkAttributes(l2Key + "nonexistent",
                        onNetworkAttributesRetrieved(
                                (status, key, attr) -> {
                                    assertTrue("Retrieve network attributes not successful : "
                                            + status.resultCode, status.isSuccess());
                                    assertEquals(l2Key + "nonexistent", key);
                                    assertNull("Retrieved data not stored", attr);
                                    latch.countDown();
                                }
                        )));

        // Verify that this test does not miss any new field added later.
        // If any field is added to NetworkAttributes it must be tested here for storing
        // and retrieving.
        assertEquals(5, Arrays.stream(NetworkAttributes.class.getDeclaredFields())
                .filter(f -> !Modifier.isStatic(f.getModifiers())).count());
    }

    @Test
    public void testInvalidAttributes() {
        doLatched("Did not complete storing bad attributes", latch ->
                mService.storeNetworkAttributes("key", null, onStatus(status -> {
                    assertFalse("Success storing on a null key",
                            status.isSuccess());
                    assertEquals(Status.ERROR_ILLEGAL_ARGUMENT, status.resultCode);
                    latch.countDown();
                })));

        final NetworkAttributes na = new NetworkAttributes.Builder().setMtu(2).build();
        doLatched("Did not complete storing bad attributes", latch ->
                mService.storeNetworkAttributes(null, na.toParcelable(), onStatus(status -> {
                    assertFalse("Success storing null attributes on a null key",
                            status.isSuccess());
                    assertEquals(Status.ERROR_ILLEGAL_ARGUMENT, status.resultCode);
                    latch.countDown();
                })));

        doLatched("Did not complete storing bad attributes", latch ->
                mService.storeNetworkAttributes(null, null, onStatus(status -> {
                    assertFalse("Success storing null attributes on a null key",
                            status.isSuccess());
                    assertEquals(Status.ERROR_ILLEGAL_ARGUMENT, status.resultCode);
                    latch.countDown();
                })));

        doLatched("Did not complete retrieving bad attributes", latch ->
                mService.retrieveNetworkAttributes(null, onNetworkAttributesRetrieved(
                        (status, key, attr) -> {
                            assertFalse("Success retrieving attributes for a null key",
                                    status.isSuccess());
                            assertEquals(Status.ERROR_ILLEGAL_ARGUMENT, status.resultCode);
                            assertNull(key);
                            assertNull(attr);
                            latch.countDown();
                        })));
    }

    @Test
    public void testPrivateData() {
        final String l2Key = FAKE_KEYS[0];
        final Blob b = new Blob();
        storeBlobOrFail(l2Key, b, TEST_BLOB_DATA);

        doLatched("Did not complete retrieving private data", latch ->
                mService.retrieveBlob(l2Key, TEST_CLIENT_ID, TEST_DATA_NAME, onBlobRetrieved(
                        (status, key, name, data) -> {
                            assertTrue("Retrieve blob status not successful : " + status.resultCode,
                                    status.isSuccess());
                            assertEquals(l2Key, key);
                            assertEquals(name, TEST_DATA_NAME);
                            assertTrue(Arrays.equals(b.data, data));
                            latch.countDown();
                        })));

        // Most puzzling error message ever
        doLatched("Did not complete retrieving nothing", latch ->
                mService.retrieveBlob(l2Key, TEST_CLIENT_ID, TEST_DATA_NAME + "2", onBlobRetrieved(
                        (status, key, name, data) -> {
                            assertTrue("Retrieve blob status not successful : " + status.resultCode,
                                    status.isSuccess());
                            assertEquals(l2Key, key);
                            assertEquals(name, TEST_DATA_NAME + "2");
                            assertNull(data);
                            latch.countDown();
                        })));
    }

    private List<NetworkAttributes> storeFixture() throws Exception {
        final ArrayList<NetworkAttributes> stored = new ArrayList<>();
        final NetworkAttributes.Builder na = new NetworkAttributes.Builder();
        na.setCluster("cluster0");
        stored.add(storeAttributes(FAKE_KEYS[0], na.build()));

        na.setDnsAddresses(Arrays.asList(
                new InetAddress[] {Inet6Address.getByName("8D56:9AF1::08EE:20F1")}));
        na.setMtu(208);
        stored.add(storeAttributes(FAKE_KEYS[1], na.build()));
        na.setMtu(null);
        na.setAssignedV4Address((Inet4Address) Inet4Address.getByName("1.2.3.4"));
        na.setDnsAddresses(Arrays.asList(
                new InetAddress[] {Inet6Address.getByName("0A1C:2E40:480A::1CA6")}));
        na.setCluster("cluster1");
        stored.add(storeAttributes(FAKE_KEYS[2], na.build()));
        na.setMtu(219);
        stored.add(storeAttributes(FAKE_KEYS[3], na.build()));
        na.setCluster(null);
        na.setMtu(240);
        stored.add(storeAttributes(FAKE_KEYS[4], na.build()));
        na.setAssignedV4Address((Inet4Address) Inet4Address.getByName("5.6.7.8"));
        stored.add(storeAttributes(FAKE_KEYS[5], na.build()));
        return stored;
    }

    @Test
    public void testFindL2Key() throws Exception {
        final List<NetworkAttributes> stored = storeFixture();
        final NetworkAttributes.Builder na = new NetworkAttributes.Builder(
                stored.get(stored.size() - 1));

        // Matches key 5 exactly
        doLatched("Did not finish finding L2Key", latch ->
                mService.findL2Key(na.build().toParcelable(), onL2KeyResponse((status, key) -> {
                    assertTrue("Retrieve network sameness not successful : " + status.resultCode,
                            status.isSuccess());
                    assertEquals(FAKE_KEYS[5], key);
                    latch.countDown();
                })));

        // MTU matches key 4 but v4 address matches key 5. The latter is stronger.
        na.setMtu(240);
        doLatched("Did not finish finding L2Key", latch ->
                mService.findL2Key(na.build().toParcelable(), onL2KeyResponse((status, key) -> {
                    assertTrue("Retrieve network sameness not successful : " + status.resultCode,
                            status.isSuccess());
                    assertEquals(FAKE_KEYS[5], key);
                    latch.countDown();
                })));

        // Closest to key 3 (indeed, identical)
        na.setCluster("cluster1");
        na.setAssignedV4Address((Inet4Address) Inet4Address.getByName("1.2.3.4"));
        na.setMtu(219);
        doLatched("Did not finish finding L2Key", latch ->
                mService.findL2Key(na.build().toParcelable(), onL2KeyResponse((status, key) -> {
                    assertTrue("Retrieve network sameness not successful : " + status.resultCode,
                            status.isSuccess());
                    assertEquals(FAKE_KEYS[3], key);
                    latch.countDown();
                })));

        // Cluster alone must not be strong enough to override the rest
        na.setCluster("cluster0");
        doLatched("Did not finish finding L2Key", latch ->
                mService.findL2Key(na.build().toParcelable(), onL2KeyResponse((status, key) -> {
                    assertTrue("Retrieve network sameness not successful : " + status.resultCode,
                            status.isSuccess());
                    assertEquals(FAKE_KEYS[3], key);
                    latch.countDown();
                })));

        // Still closest to key 3, though confidence is lower
        na.setCluster("cluster1");
        na.setDnsAddresses(null);
        doLatched("Did not finish finding L2Key", latch ->
                mService.findL2Key(na.build().toParcelable(), onL2KeyResponse((status, key) -> {
                    assertTrue("Retrieve network sameness not successful : " + status.resultCode,
                            status.isSuccess());
                    assertEquals(FAKE_KEYS[3], key);
                    latch.countDown();
                })));

        // But changing the MTU makes this closer to key 2
        na.setMtu(208);
        doLatched("Did not finish finding L2Key", latch ->
                mService.findL2Key(na.build().toParcelable(), onL2KeyResponse((status, key) -> {
                    assertTrue("Retrieve network sameness not successful : " + status.resultCode,
                            status.isSuccess());
                    assertEquals(FAKE_KEYS[2], key);
                    latch.countDown();
                })));

        // MTU alone not strong enough to make this group-close
        na.setCluster(null);
        na.setDnsAddresses(null);
        na.setAssignedV4Address(null);
        doLatched("Did not finish finding L2Key", latch ->
                mService.findL2Key(na.build().toParcelable(), onL2KeyResponse((status, key) -> {
                    assertTrue("Retrieve network sameness not successful : " + status.resultCode,
                            status.isSuccess());
                    assertNull(key);
                    latch.countDown();
                })));
    }

    private void assertNetworksSameness(final String key1, final String key2, final int sameness) {
        doLatched("Did not finish evaluating sameness", latch ->
                mService.isSameNetwork(key1, key2, onSameResponse((status, answer) -> {
                    assertTrue("Retrieve network sameness not successful : " + status.resultCode,
                            status.isSuccess());
                    assertEquals(sameness, answer.getNetworkSameness());
                    latch.countDown();
                })));
    }

    @Test
    public void testIsSameNetwork() throws UnknownHostException {
        final NetworkAttributes.Builder na = buildTestNetworkAttributes(
                (Inet4Address) Inet4Address.getByName("1.2.3.4"), LEASE_EXPIRY_NULL,
                "cluster1", Arrays.asList(Inet6Address.getByName("0A1C:2E40:480A::1CA6")),
                219);

        storeAttributes(FAKE_KEYS[0], na.build());
        // 0 and 1 have identical attributes
        storeAttributes(FAKE_KEYS[1], na.build());

        // Hopefully only the MTU being different still means it's the same network
        na.setMtu(200);
        storeAttributes(FAKE_KEYS[2], na.build());

        // Hopefully different MTU, assigned V4 address and cluster make a different network,
        // even with identical DNS addresses
        na.setAssignedV4Address(null);
        na.setCluster("cluster2");
        storeAttributes(FAKE_KEYS[3], na.build());

        assertNetworksSameness(FAKE_KEYS[0], FAKE_KEYS[1], SameL3NetworkResponse.NETWORK_SAME);
        assertNetworksSameness(FAKE_KEYS[0], FAKE_KEYS[2], SameL3NetworkResponse.NETWORK_SAME);
        assertNetworksSameness(FAKE_KEYS[1], FAKE_KEYS[2], SameL3NetworkResponse.NETWORK_SAME);
        assertNetworksSameness(FAKE_KEYS[0], FAKE_KEYS[3], SameL3NetworkResponse.NETWORK_DIFFERENT);
        assertNetworksSameness(FAKE_KEYS[0], "neverInsertedKey",
                SameL3NetworkResponse.NETWORK_NEVER_CONNECTED);

        doLatched("Did not finish evaluating sameness", latch ->
                mService.isSameNetwork(null, null, onSameResponse((status, answer) -> {
                    assertFalse("Retrieve network sameness suspiciously successful : "
                            + status.resultCode, status.isSuccess());
                    assertEquals(Status.ERROR_ILLEGAL_ARGUMENT, status.resultCode);
                    assertNull(answer);
                    latch.countDown();
                })));
    }

    private NetworkAttributes fetchAttributes(@NonNull final String l2Key) throws Exception {
        final CompletableFuture<NetworkAttributes> f = new CompletableFuture<>();
        mService.retrieveNetworkAttributes(l2Key, onNetworkAttributesRetrieved(
                (status, key, attr) -> {
                    assertTrue("Retrieve network attributes not successful : "
                            + status.resultCode, status.isSuccess());
                    f.complete(attr);
                }));
        return f.get(DEFAULT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    private void delete(@NonNull final String l2Key) {
        doLatched("Did not finish deleting", latch ->
                mService.delete(l2Key, false /* needWipe */, onDeleteStatus((status, deleted) -> {
                    assertTrue("Deleting failed :" + status.resultCode, status.isSuccess());
                    assertEquals("Deleting count != 1 :" + deleted, 1, deleted.intValue());
                    latch.countDown();
                })), LONG_TIMEOUT_MS);
    }

    @Test
    public void testDelete() throws Exception {
        storeFixture();

        delete(FAKE_KEYS[0]);
        delete(FAKE_KEYS[3]);

        assertNull(fetchAttributes(FAKE_KEYS[0]));
        assertNotNull(fetchAttributes(FAKE_KEYS[1]));
        assertNotNull(fetchAttributes(FAKE_KEYS[2]));
        assertNull(fetchAttributes(FAKE_KEYS[3]));
        assertNotNull(fetchAttributes(FAKE_KEYS[4]));
        assertNotNull(fetchAttributes(FAKE_KEYS[5]));
    }

    @Test
    public void testDeleteCluster() throws Exception {
        storeFixture();

        doLatched("Did not finish deleting", latch ->
                mService.deleteCluster("cluster1", false /* needWipe */,
                        onDeleteStatus((status, deletedCount) -> {
                            assertTrue("Delete failed : " + status.resultCode, status.isSuccess());
                            // The fixture stores 2 keys under "cluster1"
                            assertEquals("Unexpected deleted count : " + deletedCount,
                                    2, deletedCount.intValue());
                            latch.countDown();
                        })), LONG_TIMEOUT_MS);

        assertNotNull(fetchAttributes(FAKE_KEYS[0]));
        assertNotNull(fetchAttributes(FAKE_KEYS[1]));
        assertNull(fetchAttributes(FAKE_KEYS[2]));
        assertNull(fetchAttributes(FAKE_KEYS[3]));
        assertNotNull(fetchAttributes(FAKE_KEYS[4]));
        assertNotNull(fetchAttributes(FAKE_KEYS[5]));
    }

    @Test
    public void testFullMaintenance() throws Exception {
        copyTestData(mDbFile);
        // After inserted test data, db size should be larger than threshold.
        assertTrue(mService.isDbSizeOverThreshold());

        final InterruptMaintenance im = new InterruptMaintenance(0/* Fake JobId */);
        // Do full maintenance and then db size should go down and meet the threshold.
        doLatched("Maintenance unexpectedly completed successfully", latch ->
                mService.fullMaintenance(onStatus((status) -> {
                    assertTrue("Execute full maintenance failed: "
                            + status.resultCode, status.isSuccess());
                    latch.countDown();
                }), im), LONG_TIMEOUT_MS);

        // Assume that maintenance is successful, db size shall meet the threshold.
        assertFalse(mService.isDbSizeOverThreshold());
    }

    @Test
    public void testInterruptMaintenance() throws Exception {
        copyTestData(mDbFile);
        // After inserted test data, db size should be larger than threshold.
        assertTrue(mService.isDbSizeOverThreshold());

        final InterruptMaintenance im = new InterruptMaintenance(0/* Fake JobId */);

        // Test interruption immediately.
        im.setInterrupted(true);
        // Do full maintenance and the expectation is not completed by interruption.
        doLatched("Maintenance unexpectedly completed successfully", latch ->
                mService.fullMaintenance(onStatus((status) -> {
                    assertFalse(status.isSuccess());
                    latch.countDown();
                }), im), LONG_TIMEOUT_MS);

        // Assume that no data are removed, db size shall be over the threshold.
        assertTrue(mService.isDbSizeOverThreshold());

        // Reset the flag and test interruption during maintenance.
        im.setInterrupted(false);

        final ConditionVariable latch = new ConditionVariable();
        // Do full maintenance and the expectation is not completed by interruption.
        mService.fullMaintenance(onStatus((status) -> {
            assertFalse(status.isSuccess());
            latch.open();
        }), im);

        // Give a little bit of time for maintenance to start up for realism
        waitForMs(50);
        // Interrupt maintenance job.
        im.setInterrupted(true);

        if (!latch.block(LONG_TIMEOUT_MS)) {
            fail("Maintenance unexpectedly completed successfully");
        }

        // Assume that only do dropAllExpiredRecords method in previous maintenance, db size shall
        // still be over the threshold.
        assertTrue(mService.isDbSizeOverThreshold());
    }

    @Test
    public void testFactoryReset() throws UnknownHostException {
        final String l2Key = FAKE_KEYS[0];

        // store network attributes
        final NetworkAttributes.Builder na = buildTestNetworkAttributes(
                (Inet4Address) Inet4Address.getByName("1.2.3.4"),
                System.currentTimeMillis() + 7_200_000, "cluster1", null, 219);
        storeAttributes(l2Key, na.build());

        // store private data blob
        final Blob b = new Blob();
        storeBlobOrFail(l2Key, b, TEST_BLOB_DATA);

        // wipe all data in Database
        mService.factoryReset();

        // retrieved network attributes should be null
        doLatched("Did not complete retrieving attributes", latch ->
                mService.retrieveNetworkAttributes(l2Key, onNetworkAttributesRetrieved(
                        (status, key, attr) -> {
                            assertTrue("Retrieve network attributes not successful : "
                                    + status.resultCode, status.isSuccess());
                            assertEquals(l2Key, key);
                            assertNull(attr);
                            latch.countDown();
                        })));

        // retrieved private data blob should be null
        doLatched("Did not complete retrieving private data", latch ->
                mService.retrieveBlob(l2Key, TEST_CLIENT_ID, TEST_DATA_NAME, onBlobRetrieved(
                        (status, key, name, data) -> {
                            assertTrue("Retrieve blob status not successful : " + status.resultCode,
                                    status.isSuccess());
                            assertEquals(l2Key, key);
                            assertEquals(name, TEST_DATA_NAME);
                            assertNull(data);
                            latch.countDown();
                        })));
    }

    @Test
    @Ignore
    public void testTasksAreSerial() {
        final long sleepTimeMs = 1000;
        final long startTime = System.currentTimeMillis();
        mService.retrieveNetworkAttributes("somekey", onNetworkAttributesRetrieved(
                (status, key, attr) -> {
                    assertTrue("Unexpected status : " + status.resultCode, status.isSuccess());
                    try {
                        Thread.sleep(sleepTimeMs);
                    } catch (InterruptedException e) {
                        fail("InterruptedException");
                    }
                }));
        doLatched("Serial tasks timing out", latch ->
                mService.retrieveNetworkAttributes("somekey", onNetworkAttributesRetrieved(
                        (status, key, attr) -> {
                            assertTrue("Unexpected status : " + status.resultCode,
                                    status.isSuccess());
                            assertTrue(System.currentTimeMillis() >= startTime + sleepTimeMs);
                        })), DEFAULT_TIMEOUT_MS);
    }
}
