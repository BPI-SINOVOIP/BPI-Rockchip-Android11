/*
 * Copyright 2019 The Android Open Source Project
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

package android.media.cts;

import static android.media.MediaRoute2Info.FEATURE_LIVE_AUDIO;
import static android.media.MediaRoute2Info.PLAYBACK_VOLUME_VARIABLE;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.content.Intent;
import android.media.MediaRoute2Info;
import android.media.MediaRoute2ProviderService;
import android.media.RouteDiscoveryPreference;
import android.media.RoutingSessionInfo;
import android.os.Bundle;
import android.os.IBinder;
import android.text.TextUtils;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.annotation.concurrent.GuardedBy;

public class StubMediaRoute2ProviderService extends MediaRoute2ProviderService {
    private static final String TAG = "SampleMR2ProviderSvc";
    private static final Object sLock = new Object();

    public static final String ROUTE_ID1 = "route_id1";
    public static final String ROUTE_NAME1 = "Sample Route 1";
    public static final String ROUTE_ID2 = "route_id2";
    public static final String ROUTE_NAME2 = "Sample Route 2";
    public static final String ROUTE_ID3_SESSION_CREATION_FAILED =
            "route_id3_session_creation_failed";
    public static final String ROUTE_NAME3 = "Sample Route 3 - Session creation failed";
    public static final String ROUTE_ID4_TO_SELECT_AND_DESELECT =
            "route_id4_to_select_and_deselect";
    public static final String ROUTE_NAME4 = "Sample Route 4 - Route to select and deselect";
    public static final String ROUTE_ID5_TO_TRANSFER_TO = "route_id5_to_transfer_to";
    public static final String ROUTE_NAME5 = "Sample Route 5 - Route to transfer to";

    public static final String ROUTE_ID_SPECIAL_FEATURE = "route_special_feature";
    public static final String ROUTE_NAME_SPECIAL_FEATURE = "Special Feature Route";

    public static final int VOLUME_MAX = 100;
    public static final int SESSION_VOLUME_MAX = 50;
    public static final int SESSION_VOLUME_INITIAL = 20;

    public static final String ROUTE_ID_FIXED_VOLUME = "route_fixed_volume";
    public static final String ROUTE_NAME_FIXED_VOLUME = "Fixed Volume Route";
    public static final String ROUTE_ID_VARIABLE_VOLUME = "route_variable_volume";
    public static final String ROUTE_NAME_VARIABLE_VOLUME = "Variable Volume Route";

    public static final String FEATURE_SAMPLE = "android.media.cts.FEATURE_SAMPLE";
    public static final String FEATURE_SPECIAL = "android.media.cts.FEATURE_SPECIAL";

    public static final List<String> FEATURES_ALL = new ArrayList();
    public static final List<String> FEATURES_SPECIAL = new ArrayList();

    static {
        FEATURES_ALL.add(FEATURE_SAMPLE);
        FEATURES_ALL.add(FEATURE_SPECIAL);
        FEATURES_ALL.add(FEATURE_LIVE_AUDIO);

        FEATURES_SPECIAL.add(FEATURE_SPECIAL);
    }

    Map<String, MediaRoute2Info> mRoutes = new HashMap<>();
    Map<String, String> mRouteIdToSessionId = new HashMap<>();
    private int mNextSessionId = 1000;

    @GuardedBy("sLock")
    private static StubMediaRoute2ProviderService sInstance;
    private Proxy mProxy;

    public void initializeRoutes() {
        MediaRoute2Info route1 = new MediaRoute2Info.Builder(ROUTE_ID1, ROUTE_NAME1)
                .addFeature(FEATURE_SAMPLE)
                .build();
        MediaRoute2Info route2 = new MediaRoute2Info.Builder(ROUTE_ID2, ROUTE_NAME2)
                .addFeature(FEATURE_SAMPLE)
                .build();
        MediaRoute2Info route3 = new MediaRoute2Info.Builder(
                ROUTE_ID3_SESSION_CREATION_FAILED, ROUTE_NAME3)
                .addFeature(FEATURE_SAMPLE)
                .build();
        MediaRoute2Info route4 = new MediaRoute2Info.Builder(
                ROUTE_ID4_TO_SELECT_AND_DESELECT, ROUTE_NAME4)
                .addFeature(FEATURE_SAMPLE)
                .build();
        MediaRoute2Info route5 = new MediaRoute2Info.Builder(
                ROUTE_ID5_TO_TRANSFER_TO, ROUTE_NAME5)
                .addFeature(FEATURE_SAMPLE)
                .build();
        MediaRoute2Info routeSpecial =
                new MediaRoute2Info.Builder(ROUTE_ID_SPECIAL_FEATURE, ROUTE_NAME_SPECIAL_FEATURE)
                        .addFeature(FEATURE_SAMPLE)
                        .addFeature(FEATURE_SPECIAL)
                        .build();
        MediaRoute2Info fixedVolumeRoute =
                new MediaRoute2Info.Builder(ROUTE_ID_FIXED_VOLUME, ROUTE_NAME_FIXED_VOLUME)
                        .addFeature(FEATURE_SAMPLE)
                        .setVolumeHandling(MediaRoute2Info.PLAYBACK_VOLUME_FIXED)
                        .build();
        MediaRoute2Info variableVolumeRoute =
                new MediaRoute2Info.Builder(ROUTE_ID_VARIABLE_VOLUME, ROUTE_NAME_VARIABLE_VOLUME)
                        .addFeature(FEATURE_SAMPLE)
                        .setVolumeHandling(PLAYBACK_VOLUME_VARIABLE)
                        .setVolumeMax(VOLUME_MAX)
                        .build();

        mRoutes.put(route1.getId(), route1);
        mRoutes.put(route2.getId(), route2);
        mRoutes.put(route3.getId(), route3);
        mRoutes.put(route4.getId(), route4);
        mRoutes.put(route5.getId(), route5);
        mRoutes.put(routeSpecial.getId(), routeSpecial);
        mRoutes.put(fixedVolumeRoute.getId(), fixedVolumeRoute);
        mRoutes.put(variableVolumeRoute.getId(), variableVolumeRoute);
    }

    public static StubMediaRoute2ProviderService getInstance() {
        synchronized (sLock) {
            return sInstance;
        }
    }

    public void clear() {
        mProxy = null;
        mRoutes.clear();
        mRouteIdToSessionId.clear();
        for (RoutingSessionInfo sessionInfo : getAllSessionInfo()) {
            notifySessionReleased(sessionInfo.getId());
        }
    }

    public void setProxy(@Nullable Proxy proxy) {
        mProxy = proxy;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        synchronized (sLock) {
            sInstance = this;
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        synchronized (sLock) {
            if (sInstance == this) {
                sInstance = null;
            }
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return super.onBind(intent);
    }

    @Override
    public void onSetRouteVolume(long requestId, String routeId, int volume) {
        MediaRoute2Info route = mRoutes.get(routeId);
        if (route == null) {
            return;
        }
        volume = Math.max(0, Math.min(volume, route.getVolumeMax()));
        mRoutes.put(routeId, new MediaRoute2Info.Builder(route)
                .setVolume(volume)
                .build());
        publishRoutes();
    }

    @Override
    public void onSetSessionVolume(long requestId, String sessionId, int volume) {
        RoutingSessionInfo sessionInfo = getSessionInfo(sessionId);
        if (sessionInfo == null) {
            return;
        }
        volume = Math.max(0, Math.min(volume, sessionInfo.getVolumeMax()));
        RoutingSessionInfo newSessionInfo = new RoutingSessionInfo.Builder(sessionInfo)
                .setVolume(volume)
                .build();
        notifySessionUpdated(newSessionInfo);
    }

    @Override
    public void onCreateSession(long requestId, String packageName, String routeId,
            @Nullable Bundle sessionHints) {
        Proxy proxy = mProxy;
        if (doesProxyOverridesMethod(proxy, "onCreateSession")) {
            proxy.onCreateSession(requestId, packageName, routeId, sessionHints);
            return;
        }

        MediaRoute2Info route = mRoutes.get(routeId);
        if (route == null || TextUtils.equals(ROUTE_ID3_SESSION_CREATION_FAILED, routeId)) {
            notifyRequestFailed(requestId, REASON_UNKNOWN_ERROR);
            return;
        }
        maybeDeselectRoute(routeId, requestId);

        final String sessionId = String.valueOf(mNextSessionId);
        mNextSessionId++;

        mRoutes.put(routeId, new MediaRoute2Info.Builder(route)
                .setClientPackageName(packageName)
                .build());
        mRouteIdToSessionId.put(routeId, sessionId);

        RoutingSessionInfo sessionInfo = new RoutingSessionInfo.Builder(sessionId, packageName)
                .addSelectedRoute(routeId)
                .addSelectableRoute(ROUTE_ID4_TO_SELECT_AND_DESELECT)
                .addTransferableRoute(ROUTE_ID5_TO_TRANSFER_TO)
                .setVolumeHandling(PLAYBACK_VOLUME_VARIABLE)
                .setVolumeMax(SESSION_VOLUME_MAX)
                .setVolume(SESSION_VOLUME_INITIAL)
                // Set control hints with given sessionHints
                .setControlHints(sessionHints)
                .build();
        notifySessionCreated(requestId, sessionInfo);
        publishRoutes();
    }

    @Override
    public void onReleaseSession(long requestId, String sessionId) {
        Proxy proxy = mProxy;
        if (doesProxyOverridesMethod(proxy, "onReleaseSession")) {
            proxy.onReleaseSession(requestId, sessionId);
            return;
        }

        RoutingSessionInfo sessionInfo = getSessionInfo(sessionId);
        if (sessionInfo == null) {
            return;
        }

        for (String routeId : sessionInfo.getSelectedRoutes()) {
            mRouteIdToSessionId.remove(routeId);
            MediaRoute2Info route = mRoutes.get(routeId);
            if (route != null) {
                mRoutes.put(routeId, new MediaRoute2Info.Builder(route)
                        .setClientPackageName(null)
                        .build());
            }
        }
        notifySessionReleased(sessionId);
        publishRoutes();
    }

    @Override
    public void onDiscoveryPreferenceChanged(RouteDiscoveryPreference preference) {
        Proxy proxy = mProxy;
        if (doesProxyOverridesMethod(proxy, "onDiscoveryPreferenceChanged")) {
            proxy.onDiscoveryPreferenceChanged(preference);
            return;
        }

        // Just call the empty super method in order to mark the callback as tested.
        super.onDiscoveryPreferenceChanged(preference);
    }

    @Override
    public void onSelectRoute(long requestId, String sessionId, String routeId) {
        Proxy proxy = mProxy;
        if (doesProxyOverridesMethod(proxy, "onSelectRoute")) {
            proxy.onSelectRoute(requestId, sessionId, routeId);
            return;
        }

        RoutingSessionInfo sessionInfo = getSessionInfo(sessionId);
        MediaRoute2Info route = mRoutes.get(routeId);
        if (route == null || sessionInfo == null) {
            return;
        }
        maybeDeselectRoute(routeId, requestId);

        mRoutes.put(routeId, new MediaRoute2Info.Builder(route)
                .setClientPackageName(sessionInfo.getClientPackageName())
                .build());
        mRouteIdToSessionId.put(routeId, sessionId);
        publishRoutes();

        RoutingSessionInfo newSessionInfo = new RoutingSessionInfo.Builder(sessionInfo)
                .addSelectedRoute(routeId)
                .removeSelectableRoute(routeId)
                .addDeselectableRoute(routeId)
                .build();
        notifySessionUpdated(newSessionInfo);
    }

    @Override
    public void onDeselectRoute(long requestId, String sessionId, String routeId) {
        Proxy proxy = mProxy;
        if (doesProxyOverridesMethod(proxy, "onDeselectRoute")) {
            proxy.onDeselectRoute(requestId, sessionId, routeId);
            return;
        }

        RoutingSessionInfo sessionInfo = getSessionInfo(sessionId);
        MediaRoute2Info route = mRoutes.get(routeId);

        if (sessionInfo == null || route == null
                || !sessionInfo.getSelectedRoutes().contains(routeId)) {
            return;
        }

        mRouteIdToSessionId.remove(routeId);
        mRoutes.put(routeId, new MediaRoute2Info.Builder(route)
                .setClientPackageName(null)
                .build());
        publishRoutes();

        if (sessionInfo.getSelectedRoutes().size() == 1) {
            notifySessionReleased(sessionId);
            return;
        }

        RoutingSessionInfo newSessionInfo = new RoutingSessionInfo.Builder(sessionInfo)
                .removeSelectedRoute(routeId)
                .addSelectableRoute(routeId)
                .removeDeselectableRoute(routeId)
                .build();
        notifySessionUpdated(newSessionInfo);
    }

    @Override
    public void onTransferToRoute(long requestId, String sessionId, String routeId) {
        Proxy proxy = mProxy;
        if (doesProxyOverridesMethod(proxy, "onTransferToRoute")) {
            proxy.onTransferToRoute(requestId, sessionId, routeId);
            return;
        }

        RoutingSessionInfo sessionInfo = getSessionInfo(sessionId);
        MediaRoute2Info route = mRoutes.get(routeId);

        if (sessionInfo == null || route == null) {
            return;
        }

        for (String selectedRouteId : sessionInfo.getSelectedRoutes()) {
            mRouteIdToSessionId.remove(selectedRouteId);
            MediaRoute2Info selectedRoute = mRoutes.get(selectedRouteId);
            if (selectedRoute != null) {
                mRoutes.put(selectedRouteId, new MediaRoute2Info.Builder(selectedRoute)
                        .setClientPackageName(null)
                        .build());
            }
        }

        mRoutes.put(routeId, new MediaRoute2Info.Builder(route)
                .setClientPackageName(sessionInfo.getClientPackageName())
                .build());
        mRouteIdToSessionId.put(routeId, sessionId);

        RoutingSessionInfo newSessionInfo = new RoutingSessionInfo.Builder(sessionInfo)
                .clearSelectedRoutes()
                .addSelectedRoute(routeId)
                .removeDeselectableRoute(routeId)
                .removeTransferableRoute(routeId)
                .build();
        notifySessionUpdated(newSessionInfo);
        publishRoutes();
    }

    void maybeDeselectRoute(String routeId, long requestId) {
        if (!mRouteIdToSessionId.containsKey(routeId)) {
            return;
        }

        String sessionId = mRouteIdToSessionId.get(routeId);
        onDeselectRoute(requestId, sessionId, routeId);
    }

    void publishRoutes() {
        notifyRoutes(new ArrayList<>(mRoutes.values()));
    }

    public static class Proxy {
        public void onCreateSession(long requestId, @NonNull String packageName,
                @NonNull String routeId, @Nullable Bundle sessionHints) {}
        public void onReleaseSession(long requestId, @NonNull String sessionId) {}
        public void onSelectRoute(long requestId, @NonNull String sessionId,
                @NonNull String routeId) {}
        public void onDeselectRoute(long requestId, @NonNull String sessionId,
                @NonNull String routeId) {}
        public void onTransferToRoute(long requestId, @NonNull String sessionId,
                @NonNull String routeId) {}
        public void onDiscoveryPreferenceChanged(RouteDiscoveryPreference preference) {}
        // TODO: Handle onSetRouteVolume() && onSetSessionVolume()
    }

    private static boolean doesProxyOverridesMethod(Proxy proxy, String methodName) {
        if (proxy == null) {
            return false;
        }
        Method[] methods = proxy.getClass().getMethods();
        if (methods == null) {
            return false;
        }
        for (int i = 0; i < methods.length; i++) {
            if (methods[i].getName().equals(methodName)) {
                // Found method. Check if it overrides
                return methods[i].getDeclaringClass() != Proxy.class;
            }
        }
        return false;
    }
}
