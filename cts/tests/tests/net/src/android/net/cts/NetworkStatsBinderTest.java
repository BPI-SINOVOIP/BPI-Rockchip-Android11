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

package android.net.cts;

import static android.os.Process.INVALID_UID;

import static org.junit.Assert.assertEquals;

import android.annotation.NonNull;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.INetworkStatsService;
import android.net.TrafficStats;
import android.os.Build;
import android.os.IBinder;
import android.os.Process;
import android.os.RemoteException;
import android.test.AndroidTestCase;
import android.util.SparseArray;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.internal.util.CollectionUtils;
import com.android.testutils.DevSdkIgnoreRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Function;
import java.util.function.Predicate;

@RunWith(AndroidJUnit4.class)
public class NetworkStatsBinderTest {
    // NOTE: These are shamelessly copied from TrafficStats.
    private static final int TYPE_RX_BYTES = 0;
    private static final int TYPE_RX_PACKETS = 1;
    private static final int TYPE_TX_BYTES = 2;
    private static final int TYPE_TX_PACKETS = 3;

    @Rule
    public DevSdkIgnoreRule mIgnoreRule = new DevSdkIgnoreRule(
            Build.VERSION_CODES.Q /* ignoreClassUpTo */);

    private final SparseArray<Function<Integer, Long>> mUidStatsQueryOpArray = new SparseArray<>();

    @Before
    public void setUp() throws Exception {
        mUidStatsQueryOpArray.put(TYPE_RX_BYTES, uid -> TrafficStats.getUidRxBytes(uid));
        mUidStatsQueryOpArray.put(TYPE_RX_PACKETS, uid -> TrafficStats.getUidRxPackets(uid));
        mUidStatsQueryOpArray.put(TYPE_TX_BYTES, uid -> TrafficStats.getUidTxBytes(uid));
        mUidStatsQueryOpArray.put(TYPE_TX_PACKETS, uid -> TrafficStats.getUidTxPackets(uid));
    }

    private long getUidStatsFromBinder(int uid, int type) throws Exception {
        Method getServiceMethod = Class.forName("android.os.ServiceManager")
                .getDeclaredMethod("getService", new Class[]{String.class});
        IBinder binder = (IBinder) getServiceMethod.invoke(null, Context.NETWORK_STATS_SERVICE);
        INetworkStatsService nss = INetworkStatsService.Stub.asInterface(binder);
        return nss.getUidStats(uid, type);
    }

    private int getFirstAppUidThat(@NonNull Predicate<Integer> predicate) {
        PackageManager pm = InstrumentationRegistry.getContext().getPackageManager();
        List<PackageInfo> apps = pm.getInstalledPackages(0 /* flags */);
        final PackageInfo match = CollectionUtils.find(apps,
                it -> it.applicationInfo != null && predicate.test(it.applicationInfo.uid));
        if (match != null) return match.applicationInfo.uid;
        return INVALID_UID;
    }

    @Test
    public void testAccessUidStatsFromBinder() throws Exception {
        final int myUid = Process.myUid();
        final List<Integer> testUidList = new ArrayList<>();

        // Prepare uid list for testing.
        testUidList.add(INVALID_UID);
        testUidList.add(Process.ROOT_UID);
        testUidList.add(Process.SYSTEM_UID);
        testUidList.add(myUid);
        testUidList.add(Process.LAST_APPLICATION_UID);
        testUidList.add(Process.LAST_APPLICATION_UID + 1);
        // If available, pick another existing uid for testing that is not already contained
        // in the list above.
        final int notMyUid = getFirstAppUidThat(uid -> uid >= 0 && !testUidList.contains(uid));
        if (notMyUid != INVALID_UID) testUidList.add(notMyUid);

        for (final int uid : testUidList) {
            for (int i = 0; i < mUidStatsQueryOpArray.size(); i++) {
                final int type = mUidStatsQueryOpArray.keyAt(i);
                try {
                    final long uidStatsFromBinder = getUidStatsFromBinder(uid, type);
                    final long uidTrafficStats = mUidStatsQueryOpArray.get(type).apply(uid);

                    // Verify that UNSUPPORTED is returned if the uid is not current app uid.
                    if (uid != myUid) {
                        assertEquals(uidStatsFromBinder, TrafficStats.UNSUPPORTED);
                    }
                    // Verify that returned result is the same with the result get from
                    // TrafficStats.
                    // TODO: If the test is flaky then it should instead assert that the values
                    //  are approximately similar.
                    assertEquals("uidStats is not matched for query type " + type
                                    + ", uid=" + uid + ", myUid=" + myUid, uidTrafficStats,
                            uidStatsFromBinder);
                } catch (IllegalAccessException e) {
                    /* Java language access prevents exploitation. */
                    return;
                } catch (InvocationTargetException e) {
                    /* Underlying method has been changed. */
                    return;
                } catch (ClassNotFoundException e) {
                    /* not vulnerable if hidden API no longer available */
                    return;
                } catch (NoSuchMethodException e) {
                    /* not vulnerable if hidden API no longer available */
                    return;
                } catch (RemoteException e) {
                    return;
                }
            }
        }
    }
}
