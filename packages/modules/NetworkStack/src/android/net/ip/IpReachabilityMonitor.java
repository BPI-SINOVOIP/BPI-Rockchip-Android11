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

package android.net.ip;

import static android.net.metrics.IpReachabilityEvent.NUD_FAILED;
import static android.net.metrics.IpReachabilityEvent.NUD_FAILED_ORGANIC;
import static android.net.metrics.IpReachabilityEvent.PROVISIONING_LOST;
import static android.net.metrics.IpReachabilityEvent.PROVISIONING_LOST_ORGANIC;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.INetd;
import android.net.LinkProperties;
import android.net.RouteInfo;
import android.net.ip.IpNeighborMonitor.NeighborEvent;
import android.net.ip.IpNeighborMonitor.NeighborEventConsumer;
import android.net.metrics.IpConnectivityLog;
import android.net.metrics.IpReachabilityEvent;
import android.net.netlink.StructNdMsg;
import android.net.util.InterfaceParams;
import android.net.util.SharedLog;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.os.RemoteException;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Log;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.util.Preconditions;
import com.android.networkstack.R;

import java.io.PrintWriter;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;


/**
 * IpReachabilityMonitor.
 *
 * Monitors on-link IP reachability and notifies callers whenever any on-link
 * addresses of interest appear to have become unresponsive.
 *
 * This code does not concern itself with "why" a neighbour might have become
 * unreachable. Instead, it primarily reacts to the kernel's notion of IP
 * reachability for each of the neighbours we know to be critically important
 * to normal network connectivity. As such, it is often "just the messenger":
 * the neighbours about which it warns are already deemed by the kernel to have
 * become unreachable.
 *
 *
 * How it works:
 *
 *   1. The "on-link neighbours of interest" found in a given LinkProperties
 *      instance are added to a "watch list" via #updateLinkProperties().
 *      This usually means all default gateways and any on-link DNS servers.
 *
 *   2. We listen continuously for netlink neighbour messages (RTM_NEWNEIGH,
 *      RTM_DELNEIGH), watching only for neighbours in the watch list.
 *
 *        - A neighbour going into NUD_REACHABLE, NUD_STALE, NUD_DELAY, and
 *          even NUD_PROBE is perfectly normal; we merely record the new state.
 *
 *        - A neighbour's entry may be deleted (RTM_DELNEIGH), for example due
 *          to garbage collection.  This is not necessarily of immediate
 *          concern; we record the neighbour as moving to NUD_NONE.
 *
 *        - A neighbour transitioning to NUD_FAILED (for any reason) is
 *          critically important and is handled as described below in #4.
 *
 *   3. All on-link neighbours in the watch list can be forcibly "probed" by
 *      calling #probeAll(). This should be called whenever it is important to
 *      verify that critical neighbours on the link are still reachable, e.g.
 *      when roaming between BSSIDs.
 *
 *        - The kernel will send unicast ARP requests for IPv4 neighbours and
 *          unicast NS packets for IPv6 neighbours.  The expected replies will
 *          likely be unicast.
 *
 *        - The forced probing is done holding a wakelock. The kernel may,
 *          however, initiate probing of a neighbor on its own, i.e. whenever
 *          a neighbour has expired from NUD_DELAY.
 *
 *        - The kernel sends:
 *
 *              /proc/sys/net/ipv{4,6}/neigh/<ifname>/ucast_solicit
 *
 *          number of probes (usually 3) every:
 *
 *              /proc/sys/net/ipv{4,6}/neigh/<ifname>/retrans_time_ms
 *
 *          number of milliseconds (usually 1000ms). This normally results in
 *          3 unicast packets, 1 per second.
 *
 *        - If no response is received to any of the probe packets, the kernel
 *          marks the neighbour as being in state NUD_FAILED, and the listening
 *          process in #2 will learn of it.
 *
 *   4. We call the supplied Callback#notifyLost() function if the loss of a
 *      neighbour in NUD_FAILED would cause IPv4 or IPv6 configuration to
 *      become incomplete (a loss of provisioning).
 *
 *        - For example, losing all our IPv4 on-link DNS servers (or losing
 *          our only IPv6 default gateway) constitutes a loss of IPv4 (IPv6)
 *          provisioning; Callback#notifyLost() would be called.
 *
 *        - Since it can be non-trivial to reacquire certain IP provisioning
 *          state it may be best for the link to disconnect completely and
 *          reconnect afresh.
 *
 * Accessing an instance of this class from multiple threads is NOT safe.
 *
 * @hide
 */
public class IpReachabilityMonitor {
    private static final String TAG = "IpReachabilityMonitor";
    private static final boolean DBG = Log.isLoggable(TAG, Log.DEBUG);
    private static final boolean VDBG = Log.isLoggable(TAG, Log.VERBOSE);

    // Upper and lower bound for NUD probe parameters.
    protected static final int MAX_NUD_SOLICIT_NUM = 15;
    protected static final int MIN_NUD_SOLICIT_NUM = 5;
    protected static final int MAX_NUD_SOLICIT_INTERVAL_MS = 1000;
    protected static final int MIN_NUD_SOLICIT_INTERVAL_MS = 750;

    public interface Callback {
        /**
         * This callback function must execute as quickly as possible as it is
         * run on the same thread that listens to kernel neighbor updates.
         *
         * TODO: refactor to something like notifyProvisioningLost(String msg).
         */
        void notifyLost(InetAddress ip, String logMsg);
    }

    /**
     * Encapsulates IpReachabilityMonitor dependencies on systems that hinder unit testing.
     * TODO: consider also wrapping MultinetworkPolicyTracker in this interface.
     */
    interface Dependencies {
        void acquireWakeLock(long durationMs);
        IpNeighborMonitor makeIpNeighborMonitor(Handler h, SharedLog log, NeighborEventConsumer cb);

        static Dependencies makeDefault(Context context, String iface) {
            final String lockName = TAG + "." + iface;
            final PowerManager pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
            final WakeLock lock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, lockName);

            return new Dependencies() {
                public void acquireWakeLock(long durationMs) {
                    lock.acquire(durationMs);
                }

                public IpNeighborMonitor makeIpNeighborMonitor(Handler h, SharedLog log,
                        NeighborEventConsumer cb) {
                    return new IpNeighborMonitor(h, log, cb);
                }
            };
        }
    }

    private final InterfaceParams mInterfaceParams;
    private final IpNeighborMonitor mIpNeighborMonitor;
    private final SharedLog mLog;
    private final Callback mCallback;
    private final Dependencies mDependencies;
    private final boolean mUsingMultinetworkPolicyTracker;
    private final ConnectivityManager mCm;
    private final IpConnectivityLog mMetricsLog;
    private final Context mContext;
    private final INetd mNetd;
    private LinkProperties mLinkProperties = new LinkProperties();
    private Map<InetAddress, NeighborEvent> mNeighborWatchList = new HashMap<>();
    // Time in milliseconds of the last forced probe request.
    private volatile long mLastProbeTimeMs;
    private int mNumSolicits;
    private int mInterSolicitIntervalMs;

    public IpReachabilityMonitor(
            Context context, InterfaceParams ifParams, Handler h, SharedLog log, Callback callback,
            boolean usingMultinetworkPolicyTracker, final INetd netd) {
        this(context, ifParams, h, log, callback, usingMultinetworkPolicyTracker,
                Dependencies.makeDefault(context, ifParams.name), new IpConnectivityLog(), netd);
    }

    @VisibleForTesting
    IpReachabilityMonitor(Context context, InterfaceParams ifParams, Handler h, SharedLog log,
            Callback callback, boolean usingMultinetworkPolicyTracker, Dependencies dependencies,
            final IpConnectivityLog metricsLog, final INetd netd) {
        if (ifParams == null) throw new IllegalArgumentException("null InterfaceParams");

        mContext = context;
        mInterfaceParams = ifParams;
        mLog = log.forSubComponent(TAG);
        mCallback = callback;
        mUsingMultinetworkPolicyTracker = usingMultinetworkPolicyTracker;
        mCm = context.getSystemService(ConnectivityManager.class);
        mDependencies = dependencies;
        mMetricsLog = metricsLog;
        mNetd = netd;
        Preconditions.checkNotNull(mNetd);
        Preconditions.checkArgument(!TextUtils.isEmpty(mInterfaceParams.name));

        // In case the overylaid parameters specify an invalid configuration, set the parameters
        // to the hardcoded defaults first, then set them to the values used in the steady state.
        try {
            setNeighborParameters(MIN_NUD_SOLICIT_NUM, MIN_NUD_SOLICIT_INTERVAL_MS);
        } catch (Exception e) {
            Log.e(TAG, "Failed to adjust neighbor parameters with hardcoded defaults");
        }
        setNeighbourParametersForSteadyState();

        mIpNeighborMonitor = mDependencies.makeIpNeighborMonitor(h, mLog,
                (NeighborEvent event) -> {
                    if (mInterfaceParams.index != event.ifindex) return;
                    if (!mNeighborWatchList.containsKey(event.ip)) return;

                    final NeighborEvent prev = mNeighborWatchList.put(event.ip, event);

                    // TODO: Consider what to do with other states that are not within
                    // NeighborEvent#isValid() (i.e. NUD_NONE, NUD_INCOMPLETE).
                    if (event.nudState == StructNdMsg.NUD_FAILED) {
                        mLog.w("ALERT neighbor went from: " + prev + " to: " + event);
                        handleNeighborLost(event);
                    } else if (event.nudState == StructNdMsg.NUD_REACHABLE) {
                        maybeRestoreNeighborParameters();
                    }
                });
        mIpNeighborMonitor.start();
    }

    public void stop() {
        mIpNeighborMonitor.stop();
        clearLinkProperties();
    }

    public void dump(PrintWriter pw) {
        if (Looper.myLooper() == mIpNeighborMonitor.getHandler().getLooper()) {
            pw.println(describeWatchList("\n"));
            return;
        }

        final ConditionVariable cv = new ConditionVariable(false);
        mIpNeighborMonitor.getHandler().post(() -> {
            pw.println(describeWatchList("\n"));
            cv.open();
        });

        if (!cv.block(1000)) {
            pw.println("Timed out waiting for IpReachabilityMonitor dump");
        }
    }

    private String describeWatchList() { return describeWatchList(" "); }

    private String describeWatchList(String sep) {
        final StringBuilder sb = new StringBuilder();
        sb.append("iface{" + mInterfaceParams + "}," + sep);
        sb.append("ntable=[" + sep);
        String delimiter = "";
        for (Map.Entry<InetAddress, NeighborEvent> entry : mNeighborWatchList.entrySet()) {
            sb.append(delimiter).append(entry.getKey().getHostAddress() + "/" + entry.getValue());
            delimiter = "," + sep;
        }
        sb.append("]");
        return sb.toString();
    }

    private static boolean isOnLink(List<RouteInfo> routes, InetAddress ip) {
        for (RouteInfo route : routes) {
            if (!route.hasGateway() && route.matches(ip)) {
                return true;
            }
        }
        return false;
    }

    public void updateLinkProperties(LinkProperties lp) {
        if (!mInterfaceParams.name.equals(lp.getInterfaceName())) {
            // TODO: figure out whether / how to cope with interface changes.
            Log.wtf(TAG, "requested LinkProperties interface '" + lp.getInterfaceName() +
                    "' does not match: " + mInterfaceParams.name);
            return;
        }

        mLinkProperties = new LinkProperties(lp);
        Map<InetAddress, NeighborEvent> newNeighborWatchList = new HashMap<>();

        final List<RouteInfo> routes = mLinkProperties.getRoutes();
        for (RouteInfo route : routes) {
            if (route.hasGateway()) {
                InetAddress gw = route.getGateway();
                if (isOnLink(routes, gw)) {
                    newNeighborWatchList.put(gw, mNeighborWatchList.getOrDefault(gw, null));
                }
            }
        }

        for (InetAddress dns : lp.getDnsServers()) {
            if (isOnLink(routes, dns)) {
                newNeighborWatchList.put(dns, mNeighborWatchList.getOrDefault(dns, null));
            }
        }

        mNeighborWatchList = newNeighborWatchList;
        if (DBG) { Log.d(TAG, "watch: " + describeWatchList()); }
    }

    public void clearLinkProperties() {
        mLinkProperties.clear();
        mNeighborWatchList.clear();
        if (DBG) { Log.d(TAG, "clear: " + describeWatchList()); }
    }

    private void handleNeighborLost(NeighborEvent event) {
        final LinkProperties whatIfLp = new LinkProperties(mLinkProperties);

        InetAddress ip = null;
        for (Map.Entry<InetAddress, NeighborEvent> entry : mNeighborWatchList.entrySet()) {
            // TODO: Consider using NeighborEvent#isValid() here; it's more
            // strict but may interact badly if other entries are somehow in
            // NUD_INCOMPLETE (say, during network attach).
            final NeighborEvent val = entry.getValue();

            // Find all the neighbors that have gone into FAILED state.
            // Ignore entries for which we have never received an event. If there are neighbors
            // that never respond to ARP/ND, the kernel will send several FAILED events, then
            // an INCOMPLETE event, and then more FAILED events. The INCOMPLETE event will
            // populate the map and the subsequent FAILED event will be processed.
            if (val == null || val.nudState != StructNdMsg.NUD_FAILED) continue;

            ip = entry.getKey();
            for (RouteInfo route : mLinkProperties.getRoutes()) {
                if (ip.equals(route.getGateway())) {
                    whatIfLp.removeRoute(route);
                }
            }

            if (avoidingBadLinks() || !(ip instanceof Inet6Address)) {
                // We should do this unconditionally, but alas we cannot: b/31827713.
                whatIfLp.removeDnsServer(ip);
            }
        }

        final boolean lostProvisioning =
                (mLinkProperties.isIpv4Provisioned() && !whatIfLp.isIpv4Provisioned())
                || (mLinkProperties.isIpv6Provisioned() && !whatIfLp.isIpv6Provisioned());

        if (lostProvisioning) {
            final String logMsg = "FAILURE: LOST_PROVISIONING, " + event;
            Log.w(TAG, logMsg);
            if (mCallback != null) {
                // TODO: remove |ip| when the callback signature no longer has
                // an InetAddress argument.
                mCallback.notifyLost(ip, logMsg);
            }
        }
        logNudFailed(lostProvisioning);
    }

    private void maybeRestoreNeighborParameters() {
        for (Map.Entry<InetAddress, NeighborEvent> entry : mNeighborWatchList.entrySet()) {
            if (DBG) {
                Log.d(TAG, "neighbour IPv4(v6): " + entry.getKey() + " neighbour state: "
                        + StructNdMsg.stringForNudState(entry.getValue().nudState));
            }
            final NeighborEvent val = entry.getValue();
            // If an entry is null, consider that probing for that neighbour has completed.
            if (val == null || val.nudState != StructNdMsg.NUD_REACHABLE) return;
        }

        // Probing for all neighbours in the watchlist is complete and the connection is stable,
        // restore NUD probe parameters to steadystate value. In the case where neighbours
        // are responsive, this code will run before the wakelock expires.
        setNeighbourParametersForSteadyState();
    }

    private boolean avoidingBadLinks() {
        return !mUsingMultinetworkPolicyTracker || mCm.shouldAvoidBadWifi();
    }

    public void probeAll() {
        setNeighbourParametersPostRoaming();

        final List<InetAddress> ipProbeList = new ArrayList<>(mNeighborWatchList.keySet());
        if (!ipProbeList.isEmpty()) {
            // Keep the CPU awake long enough to allow all ARP/ND
            // probes a reasonable chance at success. See b/23197666.
            //
            // The wakelock we use is (by default) refcounted, and this version
            // of acquire(timeout) queues a release message to keep acquisitions
            // and releases balanced.
            mDependencies.acquireWakeLock(getProbeWakeLockDuration());
        }

        for (InetAddress ip : ipProbeList) {
            final int rval = IpNeighborMonitor.startKernelNeighborProbe(mInterfaceParams.index, ip);
            mLog.log(String.format("put neighbor %s into NUD_PROBE state (rval=%d)",
                     ip.getHostAddress(), rval));
            logEvent(IpReachabilityEvent.PROBE, rval);
        }
        mLastProbeTimeMs = SystemClock.elapsedRealtime();
    }

    private long getProbeWakeLockDuration() {
        final long gracePeriodMs = 500;
        return (long) (mNumSolicits * mInterSolicitIntervalMs) + gracePeriodMs;
    }

    private void setNeighbourParametersPostRoaming() {
        setNeighborParametersFromResources(R.integer.config_nud_postroaming_solicit_num,
                R.integer.config_nud_postroaming_solicit_interval);
    }

    private void setNeighbourParametersForSteadyState() {
        setNeighborParametersFromResources(R.integer.config_nud_steadystate_solicit_num,
                R.integer.config_nud_steadystate_solicit_interval);
    }

    private void setNeighborParametersFromResources(final int numResId, final int intervalResId) {
        try {
            final int numSolicits = mContext.getResources().getInteger(numResId);
            final int interSolicitIntervalMs = mContext.getResources().getInteger(intervalResId);
            setNeighborParameters(numSolicits, interSolicitIntervalMs);
        } catch (Exception e) {
            Log.e(TAG, "Failed to adjust neighbor parameters");
        }
    }

    private void setNeighborParameters(int numSolicits, int interSolicitIntervalMs)
            throws RemoteException, IllegalArgumentException {
        Preconditions.checkArgument(numSolicits >= MIN_NUD_SOLICIT_NUM,
                "numSolicits must be at least " + MIN_NUD_SOLICIT_NUM);
        Preconditions.checkArgument(numSolicits <= MAX_NUD_SOLICIT_NUM,
                "numSolicits must be at most " + MAX_NUD_SOLICIT_NUM);
        Preconditions.checkArgument(interSolicitIntervalMs >= MIN_NUD_SOLICIT_INTERVAL_MS,
                "interSolicitIntervalMs must be at least " + MIN_NUD_SOLICIT_INTERVAL_MS);
        Preconditions.checkArgument(interSolicitIntervalMs <= MAX_NUD_SOLICIT_INTERVAL_MS,
                "interSolicitIntervalMs must be at most " + MAX_NUD_SOLICIT_INTERVAL_MS);

        for (int family : new Integer[]{INetd.IPV4, INetd.IPV6}) {
            mNetd.setProcSysNet(family, INetd.NEIGH, mInterfaceParams.name, "retrans_time_ms",
                    Integer.toString(interSolicitIntervalMs));
            mNetd.setProcSysNet(family, INetd.NEIGH, mInterfaceParams.name, "ucast_solicit",
                    Integer.toString(numSolicits));
        }

        mNumSolicits = numSolicits;
        mInterSolicitIntervalMs = interSolicitIntervalMs;
    }

    private void logEvent(int probeType, int errorCode) {
        int eventType = probeType | (errorCode & 0xff);
        mMetricsLog.log(mInterfaceParams.name, new IpReachabilityEvent(eventType));
    }

    private void logNudFailed(boolean lostProvisioning) {
        long duration = SystemClock.elapsedRealtime() - mLastProbeTimeMs;
        boolean isFromProbe = (duration < getProbeWakeLockDuration());
        int eventType = nudFailureEventType(isFromProbe, lostProvisioning);
        mMetricsLog.log(mInterfaceParams.name, new IpReachabilityEvent(eventType));
    }

    /**
     * Returns the NUD failure event type code corresponding to the given conditions.
     */
    private static int nudFailureEventType(boolean isFromProbe, boolean isProvisioningLost) {
        if (isFromProbe) {
            return isProvisioningLost ? PROVISIONING_LOST : NUD_FAILED;
        } else {
            return isProvisioningLost ? PROVISIONING_LOST_ORGANIC : NUD_FAILED_ORGANIC;
        }
    }
}
