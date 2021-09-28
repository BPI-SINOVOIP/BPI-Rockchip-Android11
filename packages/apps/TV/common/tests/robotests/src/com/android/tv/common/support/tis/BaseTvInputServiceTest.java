package com.android.tv.common.support.tis;

import static com.google.common.truth.Truth.assertThat;

import android.content.Intent;
import android.media.tv.TvContentRating;
import android.media.tv.TvInputManager;
import android.net.Uri;
import android.os.Build;
import android.support.annotation.Nullable;
import android.view.Surface;

import com.android.tv.common.support.tis.TifSession.TifSessionCallbacks;
import com.android.tv.common.support.tis.TifSession.TifSessionFactory;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.android.controller.ServiceController;
import org.robolectric.annotation.Config;

/** Tests for {@link BaseTvInputService}. */
@RunWith(RobolectricTestRunner.class)
@Config(minSdk = Build.VERSION_CODES.LOLLIPOP, maxSdk = Build.VERSION_CODES.P)
public class BaseTvInputServiceTest {

    private static class TestTvInputService extends BaseTvInputService {

        private final SessionManager sessionManager = new SimpleSessionManager(1);

        private int parentalControlsChangedCount = 0;
        private final TifSessionFactory sessionFactory;

        private TestTvInputService() {
            super();
            this.sessionFactory =
                    new TifSessionFactory() {
                        @Override
                        public TifSession create(TifSessionCallbacks callbacks, String inputId) {
                            return new TifSession(callbacks) {
                                @Override
                                public boolean onSetSurface(@Nullable Surface surface) {
                                    return false;
                                }

                                @Override
                                public void onSurfaceChanged(int format, int width, int height) {}

                                @Override
                                public void onSetStreamVolume(float volume) {}

                                @Override
                                public boolean onTune(Uri channelUri) {
                                    return false;
                                }

                                @Override
                                public void onSetCaptionEnabled(boolean enabled) {}

                                @Override
                                public void onUnblockContent(TvContentRating unblockedRating) {}

                                @Override
                                public void onParentalControlsChanged() {
                                    parentalControlsChangedCount++;
                                }
                            };
                        }
                    };
        }

        @Override
        protected TifSessionFactory getTifSessionFactory() {
            return sessionFactory;
        }

        @Override
        protected SessionManager getSessionManager() {
            return sessionManager;
        }

        private int getParentalControlsChangedCount() {
            return parentalControlsChangedCount;
        }
    }

    TestTvInputService tvInputService;
    ServiceController<TestTvInputService> controller;

    @Before
    public void setUp() {
        controller = Robolectric.buildService(TestTvInputService.class);
        tvInputService = controller.create().get();
    }

    @Test
    public void createSession_once() {
        assertThat(tvInputService.onCreateSession("test")).isNotNull();
    }

    @Test
    public void createSession_twice() {
        WrappedSession first = tvInputService.onCreateSession("test");
        assertThat(first).isNotNull();
        WrappedSession second = tvInputService.onCreateSession("test");
        assertThat(second).isNull();
    }

    @Test
    public void createSession_release() {
        WrappedSession first = tvInputService.onCreateSession("test");
        assertThat(first).isNotNull();
        first.onRelease();
        WrappedSession second = tvInputService.onCreateSession("test");
        assertThat(second).isNotNull();
        assertThat(second).isNotSameInstanceAs(first);
    }

    @Test
    public void testReceiver_actionEnabledChanged() {
        tvInputService.getSessionManager().addSession(tvInputService.onCreateSession("test"));
        tvInputService.broadcastReceiver.onReceive(
                RuntimeEnvironment.application,
                new Intent(TvInputManager.ACTION_PARENTAL_CONTROLS_ENABLED_CHANGED));
        assertThat(tvInputService.getParentalControlsChangedCount()).isEqualTo(1);
    }

    @Test
    public void testReceiver_actionBlockedChanged() {
        tvInputService.getSessionManager().addSession(tvInputService.onCreateSession("test"));
        tvInputService.broadcastReceiver.onReceive(
                RuntimeEnvironment.application,
                new Intent(TvInputManager.ACTION_BLOCKED_RATINGS_CHANGED));
        assertThat(tvInputService.getParentalControlsChangedCount()).isEqualTo(1);
    }

    @Test
    public void testReceiver_invalidAction() {
        tvInputService.getSessionManager().addSession(tvInputService.onCreateSession("test"));
        tvInputService.broadcastReceiver.onReceive(
                RuntimeEnvironment.application, new Intent("test"));
        assertThat(tvInputService.getParentalControlsChangedCount()).isEqualTo(0);
    }
}
