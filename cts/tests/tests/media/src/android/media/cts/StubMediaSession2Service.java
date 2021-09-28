/*
 * Copyright 2018 The Android Open Source Project
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

import android.media.MediaSession2;
import android.media.MediaSession2.ControllerInfo;
import android.media.MediaSession2Service;
import android.media.Session2CommandGroup;
import android.os.Process;

import com.android.internal.annotations.GuardedBy;

import java.util.List;

/**
 * Stub implementation of {@link MediaSession2Service} for testing.
 */
public class StubMediaSession2Service extends MediaSession2Service {
    /**
     * ID of the session that this service will create.
     */
    private static final String ID = "StubMediaSession2Service";

    @GuardedBy("StubMediaSession2Service.class")
    private static HandlerExecutor sHandlerExecutor;
    @GuardedBy("StubMediaSession2Service.class")
    private static StubMediaSession2Service sInstance;
    @GuardedBy("StubMediaSession2Service.class")
    private static TestInjector sTestInjector;

    public static void setHandlerExecutor(HandlerExecutor handlerExecutor) {
        synchronized (StubMediaSession2Service.class) {
            sHandlerExecutor = handlerExecutor;
        }
    }

    public static StubMediaSession2Service getInstance() {
        synchronized (StubMediaSession2Service.class) {
            return sInstance;
        }
    }

    public static void setTestInjector(TestInjector injector) {
        synchronized (StubMediaSession2Service.class) {
            sTestInjector = injector;
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        synchronized (StubMediaSession2Service.class) {
            sInstance = this;
        }
    }

    @Override
    public void onDestroy() {
        List<MediaSession2> sessions = getSessions();
        for (MediaSession2 session : sessions) {
            session.close();
        }
        synchronized (StubMediaSession2Service.class) {
            if (sTestInjector != null) {
                sTestInjector.onServiceDestroyed();
            }
            sInstance = null;
        }
        super.onDestroy();
    }

    @Override
    public MediaNotification onUpdateNotification(MediaSession2 session) {
        synchronized (StubMediaSession2Service.class) {
            if (sTestInjector != null) {
                sTestInjector.onUpdateNotification(session);
            }
            sInstance = null;
        }
        return null;
    }

    @Override
    public MediaSession2 onGetSession(ControllerInfo controllerInfo) {
        synchronized (StubMediaSession2Service.class) {
            if (sTestInjector != null) {
                return sTestInjector.onGetSession(controllerInfo);
            }
        }
        if (getSessions().size() > 0) {
            return getSessions().get(0);
        }
        return new MediaSession2.Builder(this)
                .setId(ID)
                .setSessionCallback(sHandlerExecutor, new MediaSession2.SessionCallback() {
                    @Override
                    public Session2CommandGroup onConnect(MediaSession2 session,
                            ControllerInfo controller) {
                        if (controller.getUid() == Process.myUid()) {
                            return new Session2CommandGroup.Builder().build();
                        }
                        return null;
                    }
                }).build();
    }

    public static abstract class TestInjector {
        MediaSession2 onGetSession(ControllerInfo controllerInfo) {
            return null;
        }

        MediaNotification onUpdateNotification(MediaSession2 session) {
            return null;
        }

        void onServiceDestroyed() {
            // no-op
        }
    }
}