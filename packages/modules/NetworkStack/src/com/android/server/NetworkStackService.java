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

package com.android.server;

import static android.net.dhcp.IDhcpServer.STATUS_INVALID_ARGUMENT;
import static android.net.dhcp.IDhcpServer.STATUS_SUCCESS;
import static android.net.dhcp.IDhcpServer.STATUS_UNKNOWN_ERROR;

import static com.android.server.util.PermissionUtil.checkDumpPermission;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.net.IIpMemoryStore;
import android.net.IIpMemoryStoreCallbacks;
import android.net.INetd;
import android.net.INetworkMonitor;
import android.net.INetworkMonitorCallbacks;
import android.net.INetworkStackConnector;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.PrivateDnsConfigParcel;
import android.net.dhcp.DhcpServer;
import android.net.dhcp.DhcpServingParams;
import android.net.dhcp.DhcpServingParamsParcel;
import android.net.dhcp.IDhcpServerCallbacks;
import android.net.ip.IIpClientCallbacks;
import android.net.ip.IpClient;
import android.net.shared.PrivateDnsConfig;
import android.net.util.SharedLog;
import android.os.Build;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;
import android.text.TextUtils;
import android.util.ArraySet;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.android.internal.annotations.GuardedBy;
import com.android.internal.util.IndentingPrintWriter;
import com.android.networkstack.NetworkStackNotifier;
import com.android.networkstack.apishim.common.ShimUtils;
import com.android.server.connectivity.NetworkMonitor;
import com.android.server.connectivity.ipmemorystore.IpMemoryStoreService;
import com.android.server.util.PermissionUtil;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.lang.ref.WeakReference;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Objects;
import java.util.SortedSet;
import java.util.TreeSet;

/**
 * Android service used to start the network stack when bound to via an intent.
 *
 * <p>The service returns a binder for the system server to communicate with the network stack.
 */
public class NetworkStackService extends Service {
    private static final String TAG = NetworkStackService.class.getSimpleName();
    private static NetworkStackConnector sConnector;

    /**
     * Create a binder connector for the system server to communicate with the network stack.
     *
     * <p>On platforms where the network stack runs in the system server process, this method may
     * be called directly instead of obtaining the connector by binding to the service.
     */
    public static synchronized IBinder makeConnector(Context context) {
        if (sConnector == null) {
            sConnector = new NetworkStackConnector(context);
        }
        return sConnector;
    }

    @NonNull
    @Override
    public IBinder onBind(Intent intent) {
        return makeConnector(this);
    }

    /**
     * An interface for internal clients of the network stack service that can return
     * or create inline instances of the service it manages.
     */
    public interface NetworkStackServiceManager {
        /**
         * Get an instance of the IpMemoryStoreService.
         */
        IIpMemoryStore getIpMemoryStoreService();

        /**
         * Get an instance of the NetworkNotifier.
         */
        NetworkStackNotifier getNotifier();
    }

    /**
     * Permission checking dependency of the connector, useful for testing.
     */
    public static class PermissionChecker {
        /**
         * @see PermissionUtil#enforceNetworkStackCallingPermission()
         */
        public void enforceNetworkStackCallingPermission() {
            PermissionUtil.enforceNetworkStackCallingPermission();
        }
    }

    /**
     * Dependencies of {@link NetworkStackConnector}, useful for testing.
     */
    public static class Dependencies {
        /** @see IpMemoryStoreService */
        @NonNull
        public IpMemoryStoreService makeIpMemoryStoreService(@NonNull Context context) {
            return new IpMemoryStoreService(context);
        }

        /** @see NetworkStackNotifier */
        @NonNull
        public NetworkStackNotifier makeNotifier(@NonNull Context context, @NonNull Looper looper) {
            return new NetworkStackNotifier(context, looper);
        }

        /** @see DhcpServer */
        @NonNull
        public DhcpServer makeDhcpServer(@NonNull Context context, @NonNull String ifName,
                @NonNull DhcpServingParams params, @NonNull SharedLog log) {
            return new DhcpServer(context, ifName, params, log);
        }

        /** @see NetworkMonitor */
        @NonNull
        public NetworkMonitor makeNetworkMonitor(@NonNull Context context,
                @NonNull INetworkMonitorCallbacks cb, @NonNull Network network,
                @NonNull SharedLog log, @NonNull NetworkStackServiceManager nsServiceManager) {
            return new NetworkMonitor(context, cb, network, log, nsServiceManager);
        }

        /** @see IpClient */
        @NonNull
        public IpClient makeIpClient(@NonNull Context context, @NonNull String ifName,
                @NonNull IIpClientCallbacks cb, @NonNull NetworkObserverRegistry observerRegistry,
                @NonNull NetworkStackServiceManager nsServiceManager) {
            return new IpClient(context, ifName, cb, observerRegistry, nsServiceManager);
        }
    }

    /**
     * Connector implementing INetworkStackConnector for clients.
     */
    @VisibleForTesting
    public static class NetworkStackConnector extends INetworkStackConnector.Stub
            implements NetworkStackServiceManager {
        private static final int NUM_VALIDATION_LOG_LINES = 20;
        private final Context mContext;
        private final PermissionChecker mPermChecker;
        private final Dependencies mDeps;
        private final INetd mNetd;
        private final NetworkObserverRegistry mObserverRegistry;
        @GuardedBy("mIpClients")
        private final ArrayList<WeakReference<IpClient>> mIpClients = new ArrayList<>();
        private final IpMemoryStoreService mIpMemoryStoreService;
        @Nullable
        private final NetworkStackNotifier mNotifier;

        private static final int MAX_VALIDATION_LOGS = 10;
        @GuardedBy("mValidationLogs")
        private final ArrayDeque<SharedLog> mValidationLogs = new ArrayDeque<>(MAX_VALIDATION_LOGS);

        private static final String DUMPSYS_ARG_VERSION = "version";

        private static final String AIDL_KEY_NETWORKSTACK = "networkstack";
        private static final String AIDL_KEY_IPMEMORYSTORE = "ipmemorystore";
        private static final String AIDL_KEY_NETD = "netd";

        private static final int VERSION_UNKNOWN = -1;
        private static final String HASH_UNKNOWN = "unknown";

        /**
         * Versions of the AIDL interfaces observed by the network stack, in other words versions
         * that the framework and other modules communicating with the network stack are using.
         * The map may hold multiple values as the interface is used by modules with different
         * versions.
         */
        @GuardedBy("mFrameworkAidlVersions")
        private final ArraySet<AidlVersion> mAidlVersions = new ArraySet<>();

        private static final class AidlVersion implements Comparable<AidlVersion> {
            @NonNull
            final String mKey;
            final int mVersion;
            @NonNull
            final String mHash;

            private static final Comparator<AidlVersion> COMPARATOR =
                    Comparator.comparing((AidlVersion v) -> v.mKey)
                            .thenComparingInt(v -> v.mVersion)
                            .thenComparing(v -> v.mHash, String::compareTo);

            AidlVersion(@NonNull String key, int version, @NonNull String hash) {
                mKey = key;
                mVersion = version;
                mHash = hash;
            }

            @Override
            public int hashCode() {
                return Objects.hash(mVersion, mHash);
            }

            @Override
            public boolean equals(@Nullable Object obj) {
                if (!(obj instanceof AidlVersion)) return false;
                final AidlVersion other = (AidlVersion) obj;
                return Objects.equals(mKey, other.mKey)
                        && Objects.equals(mVersion, other.mVersion)
                        && Objects.equals(mHash, other.mHash);
            }

            @NonNull
            @Override
            public String toString() {
                // Use a format that can be easily parsed by tests for the version
                return String.format("%s:%s:%s", mKey, mVersion, mHash);
            }

            @Override
            public int compareTo(AidlVersion o) {
                return COMPARATOR.compare(this, o);
            }
        }

        private SharedLog addValidationLogs(Network network, String name) {
            final SharedLog log = new SharedLog(NUM_VALIDATION_LOG_LINES, network + " - " + name);
            synchronized (mValidationLogs) {
                while (mValidationLogs.size() >= MAX_VALIDATION_LOGS) {
                    mValidationLogs.removeLast();
                }
                mValidationLogs.addFirst(log);
            }
            return log;
        }

        NetworkStackConnector(@NonNull Context context) {
            this(context, new PermissionChecker(), new Dependencies());
        }

        @VisibleForTesting
        public NetworkStackConnector(
                @NonNull Context context, @NonNull PermissionChecker permChecker,
                @NonNull Dependencies deps) {
            mContext = context;
            mPermChecker = permChecker;
            mDeps = deps;
            mNetd = INetd.Stub.asInterface(
                    (IBinder) context.getSystemService(Context.NETD_SERVICE));
            mObserverRegistry = new NetworkObserverRegistry();
            mIpMemoryStoreService = mDeps.makeIpMemoryStoreService(context);
            // NetworkStackNotifier only shows notifications relevant for API level > Q
            if (ShimUtils.isReleaseOrDevelopmentApiAbove(Build.VERSION_CODES.Q)) {
                final HandlerThread notifierThread = new HandlerThread(
                        NetworkStackNotifier.class.getSimpleName());
                notifierThread.start();
                mNotifier = mDeps.makeNotifier(context, notifierThread.getLooper());
            } else {
                mNotifier = null;
            }

            int netdVersion;
            String netdHash;
            try {
                netdVersion = mNetd.getInterfaceVersion();
                netdHash = mNetd.getInterfaceHash();
            } catch (RemoteException e) {
                mLog.e("Error obtaining INetd version", e);
                netdVersion = VERSION_UNKNOWN;
                netdHash = HASH_UNKNOWN;
            }
            updateNetdAidlVersion(netdVersion, netdHash);

            try {
                mObserverRegistry.register(mNetd);
            } catch (RemoteException e) {
                mLog.e("Error registering observer on Netd", e);
            }
        }

        private void updateNetdAidlVersion(final int version, final String hash) {
            synchronized (mAidlVersions) {
                mAidlVersions.add(new AidlVersion(AIDL_KEY_NETD, version, hash));
            }
        }

        private void updateNetworkStackAidlVersion(final int version, final String hash) {
            synchronized (mAidlVersions) {
                mAidlVersions.add(new AidlVersion(AIDL_KEY_NETWORKSTACK, version, hash));
            }
        }

        private void updateIpMemoryStoreAidlVersion(final int version, final String hash) {
            synchronized (mAidlVersions) {
                mAidlVersions.add(new AidlVersion(AIDL_KEY_IPMEMORYSTORE, version, hash));
            }
        }

        @NonNull
        private final SharedLog mLog = new SharedLog(TAG);

        @Override
        public void makeDhcpServer(@NonNull String ifName, @NonNull DhcpServingParamsParcel params,
                @NonNull IDhcpServerCallbacks cb) throws RemoteException {
            mPermChecker.enforceNetworkStackCallingPermission();
            updateNetworkStackAidlVersion(cb.getInterfaceVersion(), cb.getInterfaceHash());
            final DhcpServer server;
            try {
                server = mDeps.makeDhcpServer(
                        mContext,
                        ifName,
                        DhcpServingParams.fromParcelableObject(params),
                        mLog.forSubComponent(ifName + ".DHCP"));
            } catch (DhcpServingParams.InvalidParameterException e) {
                mLog.e("Invalid DhcpServingParams", e);
                cb.onDhcpServerCreated(STATUS_INVALID_ARGUMENT, null);
                return;
            } catch (Exception e) {
                mLog.e("Unknown error starting DhcpServer", e);
                cb.onDhcpServerCreated(STATUS_UNKNOWN_ERROR, null);
                return;
            }
            cb.onDhcpServerCreated(STATUS_SUCCESS, server.makeConnector());
        }

        @Override
        public void makeNetworkMonitor(Network network, String name, INetworkMonitorCallbacks cb)
                throws RemoteException {
            mPermChecker.enforceNetworkStackCallingPermission();
            updateNetworkStackAidlVersion(cb.getInterfaceVersion(), cb.getInterfaceHash());
            final SharedLog log = addValidationLogs(network, name);
            final NetworkMonitor nm = mDeps.makeNetworkMonitor(mContext, cb, network, log, this);
            cb.onNetworkMonitorCreated(new NetworkMonitorConnector(nm, mPermChecker));
        }

        @Override
        public void makeIpClient(String ifName, IIpClientCallbacks cb) throws RemoteException {
            mPermChecker.enforceNetworkStackCallingPermission();
            updateNetworkStackAidlVersion(cb.getInterfaceVersion(), cb.getInterfaceHash());
            final IpClient ipClient = mDeps.makeIpClient(
                    mContext, ifName, cb, mObserverRegistry, this);

            synchronized (mIpClients) {
                final Iterator<WeakReference<IpClient>> it = mIpClients.iterator();
                while (it.hasNext()) {
                    final IpClient ipc = it.next().get();
                    if (ipc == null) {
                        it.remove();
                    }
                }
                mIpClients.add(new WeakReference<>(ipClient));
            }

            cb.onIpClientCreated(ipClient.makeConnector());
        }

        @Override
        public IIpMemoryStore getIpMemoryStoreService() {
            return mIpMemoryStoreService;
        }

        @Override
        public NetworkStackNotifier getNotifier() {
            return mNotifier;
        }

        @Override
        public void fetchIpMemoryStore(@NonNull final IIpMemoryStoreCallbacks cb)
                throws RemoteException {
            mPermChecker.enforceNetworkStackCallingPermission();
            updateIpMemoryStoreAidlVersion(cb.getInterfaceVersion(), cb.getInterfaceHash());
            cb.onIpMemoryStoreFetched(mIpMemoryStoreService);
        }

        @Override
        protected void dump(@NonNull FileDescriptor fd, @NonNull PrintWriter fout,
                @Nullable String[] args) {
            checkDumpPermission();

            final IndentingPrintWriter pw = new IndentingPrintWriter(fout, "  ");
            pw.println("NetworkStack version:");
            dumpVersion(pw);
            pw.println();

            if (args != null && args.length >= 1 && DUMPSYS_ARG_VERSION.equals(args[0])) {
                return;
            }

            pw.println("NetworkStack logs:");
            mLog.dump(fd, pw, args);

            // Dump full IpClient logs for non-GCed clients
            pw.println();
            pw.println("Recently active IpClient logs:");
            final ArrayList<IpClient> ipClients = new ArrayList<>();
            final HashSet<String> dumpedIpClientIfaces = new HashSet<>();
            synchronized (mIpClients) {
                for (WeakReference<IpClient> ipcRef : mIpClients) {
                    final IpClient ipc = ipcRef.get();
                    if (ipc != null) {
                        ipClients.add(ipc);
                    }
                }
            }

            for (IpClient ipc : ipClients) {
                pw.println(ipc.getName());
                pw.increaseIndent();
                ipc.dump(fd, pw, args);
                pw.decreaseIndent();
                dumpedIpClientIfaces.add(ipc.getInterfaceName());
            }

            // State machine and connectivity metrics logs are kept for GCed IpClients
            pw.println();
            pw.println("Other IpClient logs:");
            IpClient.dumpAllLogs(fout, dumpedIpClientIfaces);

            pw.println();
            pw.println("Validation logs (most recent first):");
            synchronized (mValidationLogs) {
                for (SharedLog p : mValidationLogs) {
                    pw.println(p.getTag());
                    pw.increaseIndent();
                    p.dump(fd, pw, args);
                    pw.decreaseIndent();
                }
            }
        }

        /**
         * Dump version information of the module and detected system version.
         */
        private void dumpVersion(@NonNull PrintWriter fout) {
            if (!ShimUtils.isReleaseOrDevelopmentApiAbove(Build.VERSION_CODES.Q)) {
                dumpVersionNumberOnly(fout);
                return;
            }

            fout.println("LocalInterface:" + this.VERSION + ":" + this.HASH);
            synchronized (mAidlVersions) {
                // Sort versions for deterministic order in output
                for (AidlVersion version : sortVersions(mAidlVersions)) {
                    fout.println(version);
                }
            }
        }

        private List<AidlVersion> sortVersions(Collection<AidlVersion> versions) {
            final List<AidlVersion> sorted = new ArrayList<>(versions);
            Collections.sort(sorted);
            return sorted;
        }

        /**
         * Legacy version of dumpVersion, only used for Q, as only the interface version number
         * was used in Q.
         *
         * <p>Q behavior needs to be preserved as conformance tests for Q still expect this format.
         * Once all conformance test suites are updated to expect the new format even on Q devices,
         * this can be removed.
         */
        private void dumpVersionNumberOnly(@NonNull PrintWriter fout) {
            fout.println("NetworkStackConnector: " + this.VERSION);
            final SortedSet<Integer> systemServerVersions = new TreeSet<>();
            int netdVersion = VERSION_UNKNOWN;
            synchronized (mAidlVersions) {
                for (AidlVersion version : mAidlVersions) {
                    switch (version.mKey) {
                        case AIDL_KEY_IPMEMORYSTORE:
                        case AIDL_KEY_NETWORKSTACK:
                            systemServerVersions.add(version.mVersion);
                            break;
                        case AIDL_KEY_NETD:
                            netdVersion = version.mVersion;
                            break;
                        default:
                            break;
                    }
                }
            }
            // TreeSet.toString is formatted as [a, b], but Q used ArraySet.toString formatted as
            // {a, b}. ArraySet does not have guaranteed ordering, which was not a problem in Q
            // when only one interface number was expected (and there was no unit test relying on
            // the ordering).
            fout.println("SystemServer: {" + TextUtils.join(", ", systemServerVersions) + "}");
            fout.println("Netd: " + netdVersion);
        }

        /**
         * Get the version of the AIDL interface.
         */
        @Override
        public int getInterfaceVersion() {
            return this.VERSION;
        }

        @Override
        public String getInterfaceHash() {
            return this.HASH;
        }
    }

    /**
     * Proxy for {@link NetworkMonitor} that implements {@link INetworkMonitor}.
     */
    @VisibleForTesting
    public static class NetworkMonitorConnector extends INetworkMonitor.Stub {
        @NonNull
        private final NetworkMonitor mNm;
        @NonNull
        private final PermissionChecker mPermChecker;

        public NetworkMonitorConnector(@NonNull NetworkMonitor nm,
                @NonNull PermissionChecker permChecker) {
            mNm = nm;
            mPermChecker = permChecker;
        }

        @Override
        public void start() {
            mPermChecker.enforceNetworkStackCallingPermission();
            mNm.start();
        }

        @Override
        public void launchCaptivePortalApp() {
            mPermChecker.enforceNetworkStackCallingPermission();
            mNm.launchCaptivePortalApp();
        }

        @Override
        public void notifyCaptivePortalAppFinished(int response) {
            mPermChecker.enforceNetworkStackCallingPermission();
            mNm.notifyCaptivePortalAppFinished(response);
        }

        @Override
        public void setAcceptPartialConnectivity() {
            mPermChecker.enforceNetworkStackCallingPermission();
            mNm.setAcceptPartialConnectivity();
        }

        @Override
        public void forceReevaluation(int uid) {
            mPermChecker.enforceNetworkStackCallingPermission();
            mNm.forceReevaluation(uid);
        }

        @Override
        public void notifyPrivateDnsChanged(PrivateDnsConfigParcel config) {
            mPermChecker.enforceNetworkStackCallingPermission();
            mNm.notifyPrivateDnsSettingsChanged(PrivateDnsConfig.fromParcel(config));
        }

        @Override
        public void notifyDnsResponse(int returnCode) {
            mPermChecker.enforceNetworkStackCallingPermission();
            mNm.notifyDnsResponse(returnCode);
        }

        @Override
        public void notifyNetworkConnected(LinkProperties lp, NetworkCapabilities nc) {
            mPermChecker.enforceNetworkStackCallingPermission();
            mNm.notifyNetworkConnected(lp, nc);
        }

        @Override
        public void notifyNetworkDisconnected() {
            mPermChecker.enforceNetworkStackCallingPermission();
            mNm.notifyNetworkDisconnected();
        }

        @Override
        public void notifyLinkPropertiesChanged(LinkProperties lp) {
            mPermChecker.enforceNetworkStackCallingPermission();
            mNm.notifyLinkPropertiesChanged(lp);
        }

        @Override
        public void notifyNetworkCapabilitiesChanged(NetworkCapabilities nc) {
            mPermChecker.enforceNetworkStackCallingPermission();
            mNm.notifyNetworkCapabilitiesChanged(nc);
        }

        @Override
        public int getInterfaceVersion() {
            return this.VERSION;
        }

        @Override
        public String getInterfaceHash() {
            return this.HASH;
        }
    }
}
