/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.telephony.cts;

import static android.net.NetworkCapabilities.NET_CAPABILITY_INTERNET;
import static android.net.NetworkCapabilities.NET_CAPABILITY_NOT_CONGESTED;
import static android.net.NetworkCapabilities.NET_CAPABILITY_NOT_METERED;
import static android.net.NetworkCapabilities.NET_CAPABILITY_NOT_RESTRICTED;
import static android.net.NetworkCapabilities.NET_CAPABILITY_TEMPORARILY_NOT_METERED;
import static android.net.NetworkCapabilities.TRANSPORT_CELLULAR;
import static android.telephony.TelephonyManager.SET_OPPORTUNISTIC_SUB_SUCCESS;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.annotation.Nullable;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.net.ConnectivityManager;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.os.Looper;
import android.os.ParcelUuid;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.SubscriptionPlan;
import android.telephony.TelephonyManager;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.ShellIdentityUtils;
import com.android.compatibility.common.util.SystemUtil;
import com.android.compatibility.common.util.TestThread;
import com.android.internal.util.ArrayUtils;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import java.time.Period;
import java.time.ZonedDateTime;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Consumer;
import java.util.function.Predicate;
import java.util.stream.Collectors;


public class SubscriptionManagerTest {
    private static final String TAG = "SubscriptionManagerTest";
    private SubscriptionManager mSm;

    private int mSubId;
    private String mPackageName;

    /**
     * Callback used in testRegisterNetworkCallback that allows caller to block on
     * {@code onAvailable}.
     */
    private static class TestNetworkCallback extends ConnectivityManager.NetworkCallback {
        private final CountDownLatch mAvailableLatch = new CountDownLatch(1);

        public void waitForAvailable() throws InterruptedException {
            assertTrue("Cellular network did not come up after 5 seconds",
                    mAvailableLatch.await(5, TimeUnit.SECONDS));
        }

        @Override
        public void onAvailable(Network network) {
            mAvailableLatch.countDown();
        }
    }

    @BeforeClass
    public static void setUpClass() throws Exception {
        if (!isSupported()) return;

        final TestNetworkCallback callback = new TestNetworkCallback();
        final ConnectivityManager cm = InstrumentationRegistry.getContext()
                .getSystemService(ConnectivityManager.class);
        cm.registerNetworkCallback(new NetworkRequest.Builder()
                .addTransportType(TRANSPORT_CELLULAR)
                .addCapability(NET_CAPABILITY_INTERNET)
                .build(), callback);
        try {
            // Wait to get callback for availability of internet
            callback.waitForAvailable();
        } catch (InterruptedException e) {
            fail("NetworkCallback wait was interrupted.");
        } finally {
            cm.unregisterNetworkCallback(callback);
        }
    }

    @AfterClass
    public static void tearDownClass() throws Exception {
        if (!isSupported()) return;
        TelephonyUtils.flushTelephonyMetrics(InstrumentationRegistry.getInstrumentation());
    }

    @Before
    public void setUp() throws Exception {
        if (!isSupported()) return;

        mSm = InstrumentationRegistry.getContext().getSystemService(SubscriptionManager.class);
        mSubId = SubscriptionManager.getDefaultDataSubscriptionId();
        mPackageName = InstrumentationRegistry.getContext().getPackageName();
    }

    /**
     * Sanity check that both {@link PackageManager#FEATURE_TELEPHONY} and
     * {@link NetworkCapabilities#TRANSPORT_CELLULAR} network must both be
     * either defined or undefined; you can't cross the streams.
     */
    @Test
    public void testSanity() throws Exception {
        final boolean hasCellular = findCellularNetwork() != null;
        if (isSupported() && !hasCellular) {
            fail("Device claims to support " + PackageManager.FEATURE_TELEPHONY
                    + " but has no active cellular network, which is required for validation");
        } else if (!isSupported() && hasCellular) {
            fail("Device has active cellular network, but claims to not support "
                    + PackageManager.FEATURE_TELEPHONY);
        }

        if (isSupported()) {
            if (mSubId == SubscriptionManager.INVALID_SUBSCRIPTION_ID) {
                fail("Device must have a valid default data subId for validation");
            }
        }
    }

    @Test
    public void testGetActiveSubscriptionInfoCount() throws Exception {
        if (!isSupported()) return;
        assertTrue(mSm.getActiveSubscriptionInfoCount() <=
                mSm.getActiveSubscriptionInfoCountMax());
    }

    @Test
    public void testGetActiveSubscriptionInfoForIcc() throws Exception {
        if (!isSupported()) return;
        SubscriptionInfo info = ShellIdentityUtils.invokeMethodWithShellPermissions(mSm,
                (sm) -> sm.getActiveSubscriptionInfo(mSubId));
        assertNotNull(ShellIdentityUtils.invokeMethodWithShellPermissions(mSm,
                (sm) -> sm.getActiveSubscriptionInfoForIcc(info.getIccId())));
    }

    @Test
    public void testIsActiveSubscriptionId() throws Exception {
        if (!isSupported()) return;
        assertTrue(mSm.isActiveSubscriptionId(mSubId));
    }

    @Test
    public void testGetSubscriptionIds() throws Exception {
        if (!isSupported()) return;
        int slotId = SubscriptionManager.getSlotIndex(mSubId);
        int[] subIds = mSm.getSubscriptionIds(slotId);
        assertNotNull(subIds);
        assertTrue(ArrayUtils.contains(subIds, mSubId));
    }

    @Test
    public void testGetResourcesForSubId() {
        if (!isSupported()) return;
        Resources r = ShellIdentityUtils.invokeMethodWithShellPermissions(mSm,
                (sm) -> sm.getResourcesForSubId(InstrumentationRegistry.getContext(), mSubId));
        // this is an old method which returns mcc/mnc as ints, so use the old SM.getMcc/Mnc methods
        // because they also use ints
        assertEquals(mSm.getActiveSubscriptionInfo(mSubId).getMcc(), r.getConfiguration().mcc);
        assertEquals(mSm.getActiveSubscriptionInfo(mSubId).getMnc(), r.getConfiguration().mnc);
    }

    @Test
    public void testIsUsableSubscriptionId() throws Exception {
        if (!isSupported()) return;
        assertTrue(SubscriptionManager.isUsableSubscriptionId(mSubId));
    }

    @Test
    public void testActiveSubscriptions() throws Exception {
        if (!isSupported()) return;

        List<SubscriptionInfo> subList = mSm.getActiveSubscriptionInfoList();
        int[] idList = mSm.getActiveSubscriptionIdList();
        // Assert when there is no sim card present or detected
        assertNotNull("Active subscriber required", subList);
        assertNotNull("Active subscriber required", idList);
        assertFalse("Active subscriber required", subList.isEmpty());
        assertNotEquals("Active subscriber required", 0, idList.length);
        for (int i = 0; i < subList.size(); i++) {
            assertTrue(subList.get(i).getSubscriptionId() >= 0);
            assertTrue(subList.get(i).getSimSlotIndex() >= 0);
            assertTrue(ArrayUtils.contains(idList, subList.get(i).getSubscriptionId()));
            if (i >= 1) {
                assertTrue(subList.get(i - 1).getSimSlotIndex()
                        <= subList.get(i).getSimSlotIndex());
                assertTrue(subList.get(i - 1).getSimSlotIndex() < subList.get(i).getSimSlotIndex()
                        || subList.get(i - 1).getSubscriptionId()
                        < subList.get(i).getSubscriptionId());
            }
        }
    }

    @Test
    public void testSubscriptionPlans() throws Exception {
        if (!isSupported()) return;

        // Make ourselves the owner
        setSubPlanOwner(mSubId, mPackageName);

        // Push empty list and we get empty back
        mSm.setSubscriptionPlans(mSubId, Arrays.asList());
        assertEquals(Arrays.asList(), mSm.getSubscriptionPlans(mSubId));

        // Push simple plan and get it back
        final SubscriptionPlan plan = buildValidSubscriptionPlan();
        mSm.setSubscriptionPlans(mSubId, Arrays.asList(plan));
        assertEquals(Arrays.asList(plan), mSm.getSubscriptionPlans(mSubId));

        // Now revoke our access
        setSubPlanOwner(mSubId, null);
        try {
            mSm.setSubscriptionPlans(mSubId, Arrays.asList());
            fail();
        } catch (SecurityException expected) {
        }
        try {
            mSm.getSubscriptionPlans(mSubId);
            fail();
        } catch (SecurityException expected) {
        }
    }

    @Test
    public void testSubscriptionPlansOverrideCongested() throws Exception {
        if (!isSupported()) return;

        final ConnectivityManager cm = InstrumentationRegistry.getContext()
                .getSystemService(ConnectivityManager.class);
        final Network net = findCellularNetwork();
        assertNotNull("Active cellular network required", net);

        // Make ourselves the owner
        setSubPlanOwner(mSubId, mPackageName);

        // Missing plans means no overrides
        mSm.setSubscriptionPlans(mSubId, Arrays.asList());
        try {
            mSm.setSubscriptionOverrideCongested(mSubId, true, 0);
            fail();
        } catch (SecurityException | IllegalStateException expected) {
        }

        // Defining plans means we get to override
        mSm.setSubscriptionPlans(mSubId, Arrays.asList(buildValidSubscriptionPlan()));

        // Cellular is uncongested by default
        assertTrue(cm.getNetworkCapabilities(net).hasCapability(NET_CAPABILITY_NOT_CONGESTED));

        // Override should make it go congested
        {
            final CountDownLatch latch = waitForNetworkCapabilities(net, caps -> {
                return !caps.hasCapability(NET_CAPABILITY_NOT_CONGESTED);
            });
            mSm.setSubscriptionOverrideCongested(mSubId, true, 0);
            assertTrue(latch.await(10, TimeUnit.SECONDS));
        }

        // Clearing override should make it go uncongested
        {
            final CountDownLatch latch = waitForNetworkCapabilities(net, caps -> {
                return caps.hasCapability(NET_CAPABILITY_NOT_CONGESTED);
            });
            mSm.setSubscriptionOverrideCongested(mSubId, false, 0);
            assertTrue(latch.await(10, TimeUnit.SECONDS));
        }

        // Now revoke our access
        setSubPlanOwner(mSubId, null);
        try {
            mSm.setSubscriptionOverrideCongested(mSubId, true, 0);
            fail();
        } catch (SecurityException | IllegalStateException expected) {
        }
    }

    @Test
    public void testSetDefaultVoiceSubId() {
        if (!isSupported()) return;

        int oldSubId = SubscriptionManager.getDefaultVoiceSubscriptionId();
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity();
        try {
            mSm.setDefaultVoiceSubscriptionId(SubscriptionManager.INVALID_SUBSCRIPTION_ID);
            assertEquals(SubscriptionManager.INVALID_SUBSCRIPTION_ID,
                    SubscriptionManager.getDefaultVoiceSubscriptionId());
            mSm.setDefaultVoiceSubscriptionId(oldSubId);
            assertEquals(oldSubId, SubscriptionManager.getDefaultVoiceSubscriptionId());
        } finally {
            InstrumentationRegistry.getInstrumentation().getUiAutomation()
                    .dropShellPermissionIdentity();
        }
    }

    @Test
    public void testSubscriptionPlansOverrideUnmetered() throws Exception {
        if (!isSupported()) return;

        final ConnectivityManager cm = InstrumentationRegistry.getContext()
                .getSystemService(ConnectivityManager.class);
        final Network net = findCellularNetwork();
        assertNotNull("Active cellular network required", net);

        // Make ourselves the owner and define some plans
        setSubPlanOwner(mSubId, mPackageName);
        mSm.setSubscriptionPlans(mSubId, Arrays.asList(buildValidSubscriptionPlan()));

        // Cellular is metered by default
        assertFalse(cm.getNetworkCapabilities(net).hasCapability(NET_CAPABILITY_NOT_METERED));

        // Override should make it go temporarily unmetered
        {
            final CountDownLatch latch = waitForNetworkCapabilities(net, caps -> {
                return caps.hasCapability(NET_CAPABILITY_NOT_METERED);
            });
            mSm.setSubscriptionOverrideUnmetered(mSubId, true, 0);
            assertTrue(latch.await(10, TimeUnit.SECONDS));
        }

        // Clearing override should make it go metered
        {
            final CountDownLatch latch = waitForNetworkCapabilities(net, caps -> {
                return !caps.hasCapability(NET_CAPABILITY_NOT_METERED);
            });
            mSm.setSubscriptionOverrideUnmetered(mSubId, false, 0);
            assertTrue(latch.await(10, TimeUnit.SECONDS));
        }
    }

    @Test
    public void testSubscriptionPlansUnmetered() throws Exception {
        if (!isSupported()) return;

        final ConnectivityManager cm = InstrumentationRegistry.getContext()
                .getSystemService(ConnectivityManager.class);
        final Network net = findCellularNetwork();
        assertNotNull("Active cellular network required", net);

        // Make ourselves the owner and define some plans
        setSubPlanOwner(mSubId, mPackageName);
        mSm.setSubscriptionPlans(mSubId, Arrays.asList(buildValidSubscriptionPlan()));

        // Cellular is metered by default
        assertFalse(cm.getNetworkCapabilities(net).hasCapability(
                NET_CAPABILITY_TEMPORARILY_NOT_METERED));

        SubscriptionPlan unmeteredPlan = SubscriptionPlan.Builder
                .createRecurring(ZonedDateTime.parse("2007-03-14T00:00:00.000Z"),
                        Period.ofMonths(1))
                .setTitle("CTS")
                .setDataLimit(SubscriptionPlan.BYTES_UNLIMITED,
                        SubscriptionPlan.LIMIT_BEHAVIOR_THROTTLED)
                .build();

        // Unmetered plan should make it go unmetered
        {
            final CountDownLatch latch = waitForNetworkCapabilities(net, caps -> {
                return caps.hasCapability(NET_CAPABILITY_TEMPORARILY_NOT_METERED);
            });
            mSm.setSubscriptionPlans(mSubId, Arrays.asList(unmeteredPlan));
            assertTrue(latch.await(10, TimeUnit.SECONDS));
        }

        // Metered plan should make it go metered
        {
            final CountDownLatch latch = waitForNetworkCapabilities(net, caps -> {
                return !caps.hasCapability(NET_CAPABILITY_TEMPORARILY_NOT_METERED);
            });
            mSm.setSubscriptionPlans(mSubId, Arrays.asList(buildValidSubscriptionPlan()));
            assertTrue(latch.await(10, TimeUnit.SECONDS));
        }
    }

    @Test
    public void testSubscriptionPlansInvalid() throws Exception {
        if (!isSupported()) return;

        // Make ourselves the owner
        setSubPlanOwner(mSubId, mPackageName);

        // Empty plans can't override
        assertOverrideFails();

        // Nonrecurring plan in the past can't override
        assertOverrideFails(SubscriptionPlan.Builder
                .createNonrecurring(ZonedDateTime.now().minusDays(14),
                        ZonedDateTime.now().minusDays(7))
                .setTitle("CTS")
                .setDataLimit(1_000_000_000, SubscriptionPlan.LIMIT_BEHAVIOR_DISABLED)
                .build());

        // Plan with undefined limit can't override
        assertOverrideFails(SubscriptionPlan.Builder
                .createRecurring(ZonedDateTime.parse("2007-03-14T00:00:00.000Z"),
                        Period.ofMonths(1))
                .setTitle("CTS")
                .build());

        // We can override when there is an active plan somewhere
        final SubscriptionPlan older = SubscriptionPlan.Builder
                .createNonrecurring(ZonedDateTime.now().minusDays(14),
                        ZonedDateTime.now().minusDays(7))
                .setTitle("CTS")
                .setDataLimit(1_000_000_000, SubscriptionPlan.LIMIT_BEHAVIOR_DISABLED)
                .build();
        final SubscriptionPlan newer = SubscriptionPlan.Builder
                .createNonrecurring(ZonedDateTime.now().minusDays(7),
                        ZonedDateTime.now().plusDays(7))
                .setTitle("CTS")
                .setDataLimit(1_000_000_000, SubscriptionPlan.LIMIT_BEHAVIOR_DISABLED)
                .build();
        assertOverrideSuccess(older, newer);
    }

    @Test
    public void testSubscriptionPlansNetworkTypeValidation() throws Exception {
        if (!isSupported()) return;

        // Make ourselves the owner
        setSubPlanOwner(mSubId, mPackageName);

        // Error when adding 2 plans with the same network type
        List<SubscriptionPlan> plans = new ArrayList<>();
        plans.add(buildValidSubscriptionPlan());
        plans.add(SubscriptionPlan.Builder
                .createRecurring(ZonedDateTime.parse("2007-03-14T00:00:00.000Z"),
                        Period.ofMonths(1))
                .setTitle("CTS")
                .setNetworkTypes(new int[] {TelephonyManager.NETWORK_TYPE_LTE})
                .build());
        plans.add(SubscriptionPlan.Builder
                .createRecurring(ZonedDateTime.parse("2007-03-14T00:00:00.000Z"),
                        Period.ofMonths(1))
                .setTitle("CTS")
                .setNetworkTypes(new int[] {TelephonyManager.NETWORK_TYPE_LTE})
                .build());
        try {
            mSm.setSubscriptionPlans(mSubId, plans);
            fail();
        } catch (IllegalArgumentException expected) {
        }

        // Error when there is no general plan
        plans.clear();
        plans.add(SubscriptionPlan.Builder
                .createRecurring(ZonedDateTime.parse("2007-03-14T00:00:00.000Z"),
                        Period.ofMonths(1))
                .setTitle("CTS")
                .setNetworkTypes(new int[] {TelephonyManager.NETWORK_TYPE_LTE})
                .build());
        try {
            mSm.setSubscriptionPlans(mSubId, plans);
            fail();
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testSubscriptionPlanResetNetworkTypes() {
        SubscriptionPlan plan = SubscriptionPlan.Builder
                .createRecurring(ZonedDateTime.parse("2007-03-14T00:00:00.000Z"),
                        Period.ofMonths(1))
                .setTitle("CTS")
                .setNetworkTypes(new int[] {TelephonyManager.NETWORK_TYPE_LTE})
                .setDataLimit(1_000_000_000, SubscriptionPlan.LIMIT_BEHAVIOR_DISABLED)
                .setDataUsage(500_000_000, System.currentTimeMillis())
                .resetNetworkTypes()
                .build();
        assertEquals(plan, buildValidSubscriptionPlan());
    }

    @Test
    public void testSubscriptionGrouping() throws Exception {
        if (!isSupported()) return;

        // Set subscription group with current sub Id. This should fail
        // because we don't have MODIFY_PHONE_STATE or carrier privilege permission.
        List<Integer> subGroup = new ArrayList();
        subGroup.add(mSubId);
        try {
            mSm.createSubscriptionGroup(subGroup);
            fail();
        } catch (SecurityException expected) {
        }

        // Getting subscriptions in group should return null as setSubscriptionGroup
        // should fail.
        SubscriptionInfo info = mSm.getActiveSubscriptionInfo(mSubId);
        assertNull(info.getGroupUuid());

        // Remove from subscription group with current sub Id. This should fail
        // because we don't have MODIFY_PHONE_STATE or carrier privilege permission.
        try {
            mSm.addSubscriptionsIntoGroup(subGroup, null);
            fail();
        } catch (NullPointerException expected) {
        }

        // Add into subscription group that doesn't exist. This should fail
        // because we don't have MODIFY_PHONE_STATE or carrier privilege permission.
        try {
            ParcelUuid groupUuid = new ParcelUuid(UUID.randomUUID());
            mSm.addSubscriptionsIntoGroup(subGroup, groupUuid);
            fail();
        } catch (SecurityException expected) {
        }

        // Remove from subscription group with current sub Id. This should fail
        // because we don't have MODIFY_PHONE_STATE or carrier privilege permission.
        try {
            mSm.removeSubscriptionsFromGroup(subGroup, null);
            fail();
        } catch (NullPointerException expected) {
        }
    }

    @Test
    public void testSubscriptionGroupingWithPermission() throws Exception {
        if (!isSupported()) return;

        // Set subscription group with current sub Id.
        List<Integer> subGroup = new ArrayList();
        subGroup.add(mSubId);
        ParcelUuid uuid = ShellIdentityUtils.invokeMethodWithShellPermissions(mSm,
                (sm) -> sm.createSubscriptionGroup(subGroup));

        // Getting subscriptions in group.
        List<SubscriptionInfo> infoList = mSm.getSubscriptionsInGroup(uuid);
        assertNotNull(infoList);
        assertEquals(1, infoList.size());
        assertNull(infoList.get(0).getGroupUuid());

        infoList = ShellIdentityUtils.invokeMethodWithShellPermissions(mSm,
                (sm) -> sm.getSubscriptionsInGroup(uuid));
        assertNotNull(infoList);
        assertEquals(1, infoList.size());
        assertEquals(uuid, infoList.get(0).getGroupUuid());

        List<SubscriptionInfo> availableInfoList;
        try {
            mSm.getAvailableSubscriptionInfoList();
            fail("SecurityException should be thrown without READ_PRIVILEGED_PHONE_STATE");
        } catch (SecurityException ex) {
            // Ignore
        }
        availableInfoList = ShellIdentityUtils.invokeMethodWithShellPermissions(mSm,
                (sm) -> sm.getAvailableSubscriptionInfoList());
        if (availableInfoList.size() > 1) {
            List<Integer> availableSubGroup = availableInfoList.stream()
                    .map(info -> info.getSubscriptionId())
                    .filter(subId -> subId != mSubId)
                    .collect(Collectors.toList());

            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mSm,
                    (sm) -> sm.addSubscriptionsIntoGroup(availableSubGroup, uuid));

            infoList = mSm.getSubscriptionsInGroup(uuid);
            assertNotNull(infoList);
            assertEquals(availableInfoList.size(), infoList.size());

            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mSm,
                    (sm) -> sm.removeSubscriptionsFromGroup(availableSubGroup, uuid));
        }

        // Remove from subscription group with current sub Id.
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mSm,
                (sm) -> sm.removeSubscriptionsFromGroup(subGroup, uuid));

        infoList = mSm.getSubscriptionsInGroup(uuid);
        assertNotNull(infoList);
        assertTrue(infoList.isEmpty());
    }

    @Test
    public void testAddSubscriptionIntoNewGroupWithPermission() throws Exception {
        if (!isSupported()) return;

        // Set subscription group with current sub Id.
        List<Integer> subGroup = new ArrayList();
        subGroup.add(mSubId);
        ParcelUuid uuid = new ParcelUuid(UUID.randomUUID());
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mSm,
                (sm) -> sm.addSubscriptionsIntoGroup(subGroup, uuid));

        // Getting subscriptions in group.
        List<SubscriptionInfo> infoList = mSm.getSubscriptionsInGroup(uuid);
        assertNotNull(infoList);
        assertEquals(1, infoList.size());
        assertNull(infoList.get(0).getGroupUuid());

        infoList = ShellIdentityUtils.invokeMethodWithShellPermissions(mSm,
                (sm) -> sm.getSubscriptionsInGroup(uuid));
        assertNotNull(infoList);
        assertEquals(1, infoList.size());
        assertEquals(uuid, infoList.get(0).getGroupUuid());

        // Remove from subscription group with current sub Id.
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mSm,
                (sm) -> sm.removeSubscriptionsFromGroup(subGroup, uuid));

        infoList = mSm.getSubscriptionsInGroup(uuid);
        assertNotNull(infoList);
        assertTrue(infoList.isEmpty());
    }

    @Test
    public void testSettingOpportunisticSubscription() throws Exception {
        if (!isSupported()) return;

        // Set subscription to be opportunistic. This should fail
        // because we don't have MODIFY_PHONE_STATE or carrier privilege permission.
        try {
            mSm.setOpportunistic(true, mSubId);
            fail();
        } catch (SecurityException expected) {
        }

        // Shouldn't crash.
        SubscriptionInfo info = mSm.getActiveSubscriptionInfo(mSubId);
        info.isOpportunistic();
    }

    @Test
    public void testMccMncString() {
        if (!isSupported()) return;

        SubscriptionInfo info = mSm.getActiveSubscriptionInfo(mSubId);
        String mcc = info.getMccString();
        String mnc = info.getMncString();
        assertTrue(mcc == null || mcc.length() <= 3);
        assertTrue(mnc == null || mnc.length() <= 3);
    }

    @Test
    public void testSubscriptionInfoCarrierId() {
        if (!isSupported()) return;

        SubscriptionInfo info = mSm.getActiveSubscriptionInfo(mSubId);
        int carrierId = info.getCarrierId();
        assertTrue(carrierId >= TelephonyManager.UNKNOWN_CARRIER_ID);
    }

    @Test
    public void testGetOpportunisticSubscriptions() throws Exception {
        if (!isSupported()) return;

        List<SubscriptionInfo> infoList = mSm.getOpportunisticSubscriptions();

        for (SubscriptionInfo info : infoList) {
            assertTrue(info.isOpportunistic());
        }
    }

    @Test
    public void testGetEnabledSubscriptionId() {
        if (!isSupported()) return;
        int slotId = SubscriptionManager.getSlotIndex(mSubId);
        if (!SubscriptionManager.isValidSlotIndex(slotId)) {
            fail("Invalid slot id " + slotId + " for subscription id " + mSubId);
        }
        int enabledSubId = executeWithShellPermissionAndDefault(-1, mSm,
                (sm) -> sm.getEnabledSubscriptionId(slotId));
        assertEquals(mSubId, enabledSubId);
    }

    @Test
    public void testSetAndCheckSubscriptionEnabled() {
        if (!isSupported()) return;
        boolean enabled = executeWithShellPermissionAndDefault(false, mSm,
                (sm) -> sm.isSubscriptionEnabled(mSubId));

        AtomicBoolean waitForIsEnabledValue = new AtomicBoolean(!enabled);
        // wait for the first call to take effect
        Object lock = new Object();
        AtomicBoolean setSubscriptionEnabledCallCompleted = new AtomicBoolean(false);
        TestThread t = new TestThread(new Runnable() {
            @Override
            public void run() {
                Looper.prepare();

                SubscriptionManager.OnSubscriptionsChangedListener listener =
                        new SubscriptionManager.OnSubscriptionsChangedListener() {
                            @Override
                            public void onSubscriptionsChanged() {
                                boolean waitForValue = waitForIsEnabledValue.get();
                                if (executeWithShellPermissionAndDefault(!waitForValue, mSm,
                                        (sm) -> sm.isSubscriptionEnabled(mSubId)) == waitForValue) {
                                    synchronized (lock) {
                                        setSubscriptionEnabledCallCompleted.set(true);
                                        lock.notifyAll();
                                    }
                                }
                            }
                        };
                mSm.addOnSubscriptionsChangedListener(listener);

                Looper.loop();
            }
        });

        try {
            t.start();
            // Enable or disable subscription may require users UX confirmation or may not be
            // supported. Call APIs to make sure there's no crash.
            executeWithShellPermissionAndDefault(false, mSm,
                    (sm) -> sm.setSubscriptionEnabled(mSubId, !enabled));

            synchronized (lock) {
                if (!setSubscriptionEnabledCallCompleted.get()) {
                    lock.wait(5000);
                }
            }
            if (!setSubscriptionEnabledCallCompleted.get()) {
                // not treating this as test failure as it may be due to UX confirmation or may not
                // be supported
                Log.e(TAG, "setSubscriptionEnabled() did not complete");
                return;
            }

            // switch back to the original value
            waitForIsEnabledValue.set(enabled);
            setSubscriptionEnabledCallCompleted.set(false);
            executeWithShellPermissionAndDefault(false, mSm,
                    (sm) -> sm.setSubscriptionEnabled(mSubId, enabled));

            // wait to make sure device is left in the same state after the test as it was before
            // the test
            synchronized (lock) {
                if (!setSubscriptionEnabledCallCompleted.get()) {
                    // longer wait time on purpose as re-enabling can take a longer time
                    lock.wait(50000);
                }
            }
            if (!setSubscriptionEnabledCallCompleted.get()) {
                // treat this as failure because it worked the first time
                fail("setSubscriptionEnabled() did not work second time");
            }

            // Reset default data subId as it may have been changed as part of the calls above
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mSm,
                    (sm) -> sm.setDefaultDataSubId(mSubId));

            // Other tests also expect that cellular data must be available if telephony is
            // supported. Wait for that before returning.
            final CountDownLatch latch = waitForCellularNetwork();
            latch.await(10, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            fail("InterruptedException");
        }
    }

    @Test
    public void testGetActiveDataSubscriptionId() {
        if (!isSupported()) return;

        int activeDataSubIdCurrent = executeWithShellPermissionAndDefault(
                SubscriptionManager.INVALID_SUBSCRIPTION_ID, mSm,
                (sm) -> sm.getActiveDataSubscriptionId());

        if (activeDataSubIdCurrent != SubscriptionManager.INVALID_SUBSCRIPTION_ID) {
            List<SubscriptionInfo> subscriptionInfos = mSm.getCompleteActiveSubscriptionInfoList();
            boolean foundSub = subscriptionInfos.stream()
                    .anyMatch(x -> x.getSubscriptionId() == activeDataSubIdCurrent);
            assertTrue(foundSub);
        }
    }

    @Test
    public void testSetPreferredDataSubscriptionId() {
        if (!isSupported()) return;
        int preferredSubId = executeWithShellPermissionAndDefault(-1, mSm,
                (sm) -> sm.getPreferredDataSubscriptionId());
        if (preferredSubId != SubscriptionManager.DEFAULT_SUBSCRIPTION_ID) {
            // Make sure to switch back to primary/default data sub first.
            setPreferredDataSubId(SubscriptionManager.DEFAULT_SUBSCRIPTION_ID);
        }

        List<SubscriptionInfo> subscriptionInfos = mSm.getCompleteActiveSubscriptionInfoList();

        for (SubscriptionInfo subInfo : subscriptionInfos) {
            // Only test on opportunistic subscriptions.
            if (!subInfo.isOpportunistic()) continue;
            setPreferredDataSubId(subInfo.getSubscriptionId());
        }

        // Switch data back to previous preferredSubId.
        setPreferredDataSubId(preferredSubId);
    }

    private void setPreferredDataSubId(int subId) {
        final LinkedBlockingQueue<Integer> resultQueue = new LinkedBlockingQueue<>(1);
        Executor executor = (command)-> command.run();
        Consumer<Integer> consumer = (res)-> {
            if (res == null) {
                resultQueue.offer(-1);
            } else {
                resultQueue.offer(res);
            }
        };

        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mSm,
                (sm) -> sm.setPreferredDataSubscriptionId(subId, false,
                        executor, consumer));
        int res = -1;
        try {
            res = resultQueue.poll(2, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            fail("Cannot get the modem result in time");
        }

        assertEquals(SET_OPPORTUNISTIC_SUB_SUCCESS, res);
        int getValue = executeWithShellPermissionAndDefault(-1, mSm,
                (sm) -> sm.getPreferredDataSubscriptionId());
        assertEquals(subId, getValue);
    }

    private <T, U> T executeWithShellPermissionAndDefault(T defaultValue, U targetObject,
            ShellIdentityUtils.ShellPermissionMethodHelper<T, U> helper) {
        try {
            return ShellIdentityUtils.invokeMethodWithShellPermissions(targetObject, helper);
        } catch (Exception e) {
            // do nothing, return default
        }
        return defaultValue;
    }

    private void assertOverrideSuccess(SubscriptionPlan... plans) {
        mSm.setSubscriptionPlans(mSubId, Arrays.asList(plans));
        mSm.setSubscriptionOverrideCongested(mSubId, false, 0);
    }

    private void assertOverrideFails(SubscriptionPlan... plans) {
        mSm.setSubscriptionPlans(mSubId, Arrays.asList(plans));
        try {
            mSm.setSubscriptionOverrideCongested(mSubId, false, 0);
            fail();
        } catch (SecurityException | IllegalStateException expected) {
        }
    }

    public static CountDownLatch waitForNetworkCapabilities(Network network,
            Predicate<NetworkCapabilities> predicate) {
        final CountDownLatch latch = new CountDownLatch(1);
        final ConnectivityManager cm = InstrumentationRegistry.getContext()
                .getSystemService(ConnectivityManager.class);
        cm.registerNetworkCallback(new NetworkRequest.Builder().build(),
                new NetworkCallback() {
                    @Override
                    public void onCapabilitiesChanged(Network net, NetworkCapabilities caps) {
                        if (net.equals(network) && predicate.test(caps)) {
                            latch.countDown();
                            cm.unregisterNetworkCallback(this);
                        }
                    }
                });
        return latch;
    }

    /**
     * Corresponding to findCellularNetwork()
     */
    private static CountDownLatch waitForCellularNetwork() {
        final CountDownLatch latch = new CountDownLatch(1);
        final ConnectivityManager cm = InstrumentationRegistry.getContext()
                .getSystemService(ConnectivityManager.class);
        cm.registerNetworkCallback(new NetworkRequest.Builder().build(),
                new NetworkCallback() {
                    @Override
                    public void onCapabilitiesChanged(Network net, NetworkCapabilities caps) {
                        if (caps.hasTransport(TRANSPORT_CELLULAR)
                                && caps.hasCapability(NET_CAPABILITY_INTERNET)
                                && caps.hasCapability(NET_CAPABILITY_NOT_RESTRICTED)) {
                            latch.countDown();
                            cm.unregisterNetworkCallback(this);
                        }
                    }
                });
        return latch;
    }

    private static SubscriptionPlan buildValidSubscriptionPlan() {
        return SubscriptionPlan.Builder
                .createRecurring(ZonedDateTime.parse("2007-03-14T00:00:00.000Z"),
                        Period.ofMonths(1))
                .setTitle("CTS")
                .setDataLimit(1_000_000_000, SubscriptionPlan.LIMIT_BEHAVIOR_DISABLED)
                .setDataUsage(500_000_000, System.currentTimeMillis())
                .build();
    }

    private static @Nullable Network findCellularNetwork() {
        final ConnectivityManager cm = InstrumentationRegistry.getContext()
                .getSystemService(ConnectivityManager.class);
        for (Network net : cm.getAllNetworks()) {
            final NetworkCapabilities caps = cm.getNetworkCapabilities(net);
            if (caps != null && caps.hasTransport(TRANSPORT_CELLULAR)
                    && caps.hasCapability(NET_CAPABILITY_INTERNET)
                    && caps.hasCapability(NET_CAPABILITY_NOT_RESTRICTED)) {
                return net;
            }
        }
        return null;
    }

    private static boolean isSupported() {
        return InstrumentationRegistry.getContext().getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_TELEPHONY);
    }

    private static boolean isDSDS() {
        TelephonyManager tm = InstrumentationRegistry.getContext()
                .getSystemService(TelephonyManager.class);
        return tm != null && tm.getPhoneCount() > 1;
    }

    private static void setSubPlanOwner(int subId, String packageName) throws Exception {
        SystemUtil.runShellCommand(InstrumentationRegistry.getInstrumentation(),
                "cmd netpolicy set sub-plan-owner " + subId + " " + packageName);
    }
}
