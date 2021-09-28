package com.android.tv.common.support.tis;

import static com.google.common.truth.Truth.assertThat;

import android.media.tv.TvContentRating;
import android.net.Uri;
import android.os.Build;
import android.support.annotation.FloatRange;
import android.support.annotation.Nullable;
import android.view.Surface;

import com.android.tv.common.support.tis.TifSession.TifSessionCallbacks;
import com.android.tv.common.support.tis.TifSession.TifSessionFactory;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

/** Tests for {@link SimpleSessionManager}. */
@RunWith(RobolectricTestRunner.class)
@Config(minSdk = Build.VERSION_CODES.LOLLIPOP, maxSdk = Build.VERSION_CODES.P)
public class SimpleSessionManagerTest {

    private SimpleSessionManager sessionManager;

    @Before
    public void setup() {
        sessionManager = new SimpleSessionManager(1);
    }

    @Test
    public void canCreateSession_none() {
        assertThat(sessionManager.canCreateNewSession()).isTrue();
    }

    @Test
    public void canCreateSession_one() {
        sessionManager.addSession(createTestSession());
        assertThat(sessionManager.canCreateNewSession()).isFalse();
    }

    @Test
    public void addSession() {
        assertThat(sessionManager.getSessionCount()).isEqualTo(0);
        sessionManager.addSession(createTestSession());
        assertThat(sessionManager.getSessionCount()).isEqualTo(1);
    }

    @Test
    public void onRelease() {
        WrappedSession testSession = createTestSession();
        sessionManager.addSession(testSession);
        assertThat(sessionManager.getSessionCount()).isEqualTo(1);
        testSession.onRelease();
        assertThat(sessionManager.getSessionCount()).isEqualTo(0);
    }

    @Test
    public void onRelease_withUnRegisteredSession() {
        WrappedSession testSession = createTestSession();
        sessionManager.addSession(createTestSession());
        assertThat(sessionManager.getSessionCount()).isEqualTo(1);
        testSession.onRelease();
        assertThat(sessionManager.getSessionCount()).isEqualTo(1);
    }

    @Test
    public void getSessions() {
        WrappedSession testSession = createTestSession();
        sessionManager.addSession(testSession);
        assertThat(sessionManager.getSessions()).containsExactly(testSession);
    }

    private WrappedSession createTestSession() {
        return new WrappedSession(
                RuntimeEnvironment.application,
                sessionManager,
                new TestTifSessionFactory(),
                "testInputId");
    }

    private static final class TestTifSessionFactory implements TifSessionFactory {

        @Override
        public TifSession create(TifSessionCallbacks callbacks, String inputId) {
            return new TestTifSession(callbacks);
        }
    }

    private static final class TestTifSession extends TifSession {

        private TestTifSession(TifSessionCallbacks callbacks) {
            super(callbacks);
        }

        @Override
        public void onRelease() {}

        @Override
        public boolean onSetSurface(@Nullable Surface surface) {
            return false;
        }

        @Override
        public void onSurfaceChanged(int format, int width, int height) {}

        @Override
        public void onSetStreamVolume(@FloatRange(from = 0.0, to = 1.0) float volume) {}

        @Override
        public boolean onTune(Uri channelUri) {
            return false;
        }

        @Override
        public void onSetCaptionEnabled(boolean enabled) {}

        @Override
        public void onUnblockContent(TvContentRating unblockedRating) {}
    }
}
