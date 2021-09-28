package com.android.tv.common.support.tis;

import static com.google.common.truth.Truth.assertThat;

import android.media.PlaybackParams;
import android.media.tv.TvContentRating;
import android.net.Uri;
import android.os.Build;
import android.view.View;

import com.android.tv.common.support.tis.TifSession.TifSessionCallbacks;
import com.android.tv.common.support.tis.TifSession.TifSessionFactory;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

/** Tests for {@link WrappedSession}. */
@RunWith(RobolectricTestRunner.class)
@Config(minSdk = Build.VERSION_CODES.M, maxSdk = Build.VERSION_CODES.P)
public class WrappedSessionTest {

    @Mock TifSession mockDelegate;
    private TifSessionFactory sessionFactory =
            new TifSessionFactory() {
                @Override
                public TifSession create(TifSessionCallbacks callbacks, String inputId) {
                    return mockDelegate;
                }
            };
    private WrappedSession wrappedSession;
    private SimpleSessionManager sessionManager = new SimpleSessionManager(1);

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        wrappedSession =
                new WrappedSession(
                        RuntimeEnvironment.application,
                        sessionManager,
                        sessionFactory,
                        "testInputId");
    }

    @Test
    public void onRelease() {
        sessionManager.addSession(wrappedSession);
        assertThat(sessionManager.getSessionCount()).isEqualTo(1);
        wrappedSession.onRelease();
        assertThat(sessionManager.getSessionCount()).isEqualTo(0);
        Mockito.verify(mockDelegate).onRelease();
        Mockito.verifyNoMoreInteractions(mockDelegate);
    }

    @Test
    public void onSetSurface() {
        wrappedSession.onSetSurface(null);
        Mockito.verify(mockDelegate).onSetSurface(null);
        Mockito.verifyNoMoreInteractions(mockDelegate);
    }

    @Test
    public void onSurfaceChanged() {
        wrappedSession.onSurfaceChanged(1, 2, 3);
        Mockito.verify(mockDelegate).onSurfaceChanged(1, 2, 3);
        Mockito.verifyNoMoreInteractions(mockDelegate);
    }

    @Test
    public void onSetStreamVolume() {
        wrappedSession.onSetStreamVolume(.8f);
        Mockito.verify(mockDelegate).onSetStreamVolume(.8f);
        Mockito.verifyNoMoreInteractions(mockDelegate);
    }

    @Test
    public void onTune() {
        Uri uri = Uri.EMPTY;
        wrappedSession.onTune(uri);
        Mockito.verify(mockDelegate).onTune(uri);
        Mockito.verifyNoMoreInteractions(mockDelegate);
    }

    @Test
    public void onSetCaptionEnabled() {
        wrappedSession.onSetCaptionEnabled(true);
        Mockito.verify(mockDelegate).onSetCaptionEnabled(true);
        Mockito.verifyNoMoreInteractions(mockDelegate);
    }

    @Test
    @Config(minSdk = Build.VERSION_CODES.M)
    public void onTimeShiftGetCurrentPosition() {
        Mockito.when(mockDelegate.onTimeShiftGetCurrentPosition()).thenReturn(7L);
        assertThat(wrappedSession.onTimeShiftGetCurrentPosition()).isEqualTo(7L);
        Mockito.verify(mockDelegate).onTimeShiftGetCurrentPosition();
        Mockito.verifyNoMoreInteractions(mockDelegate);
    }

    @Test
    @Config(minSdk = Build.VERSION_CODES.M)
    public void onTimeShiftGetStartPosition() {
        Mockito.when(mockDelegate.onTimeShiftGetStartPosition()).thenReturn(8L);
        assertThat(wrappedSession.onTimeShiftGetStartPosition()).isEqualTo(8L);
        Mockito.verify(mockDelegate).onTimeShiftGetStartPosition();
        Mockito.verifyNoMoreInteractions(mockDelegate);
    }

    @Test
    @Config(minSdk = Build.VERSION_CODES.M)
    public void onTimeShiftPause() {
        wrappedSession.onTimeShiftPause();
        Mockito.verify(mockDelegate).onTimeShiftPause();
        Mockito.verifyNoMoreInteractions(mockDelegate);
    }

    @Test
    @Config(minSdk = Build.VERSION_CODES.M)
    public void onTimeShiftResume() {
        wrappedSession.onTimeShiftResume();
        Mockito.verify(mockDelegate).onTimeShiftResume();
        Mockito.verifyNoMoreInteractions(mockDelegate);
    }

    @Test
    @Config(minSdk = Build.VERSION_CODES.M)
    public void onTimeShiftSeekTo() {
        wrappedSession.onTimeShiftSeekTo(9L);
        Mockito.verify(mockDelegate).onTimeShiftSeekTo(9L);
        Mockito.verifyNoMoreInteractions(mockDelegate);
    }

    @Test
    @Config(minSdk = Build.VERSION_CODES.M)
    public void onTimeShiftSetPlaybackParams() {
        PlaybackParams paras = new PlaybackParams();
        wrappedSession.onTimeShiftSetPlaybackParams(paras);
        Mockito.verify(mockDelegate).onTimeShiftSetPlaybackParams(paras);
        Mockito.verifyNoMoreInteractions(mockDelegate);
    }

    @Test
    @Config(minSdk = Build.VERSION_CODES.M)
    public void onUnblockContent() {
        TvContentRating rating =
                TvContentRating.createRating(
                        "domain", "rating_system", "rating", "subrating1", "subrating2");
        wrappedSession.onUnblockContent(rating);
        Mockito.verify(mockDelegate).onUnblockContent(rating);
        Mockito.verifyNoMoreInteractions(mockDelegate);
    }

    @Test
    public void onParentalControlsChanged() {
        wrappedSession.onParentalControlsChanged();
        Mockito.verify(mockDelegate).onParentalControlsChanged();
        Mockito.verifyNoMoreInteractions(mockDelegate);
    }

    @Test
    public void testSetOverlayViewEnabled() {
        wrappedSession.setOverlayViewEnabled(true);
        // Just verifying that the call completes.
    }

    @Test
    public void testOnCreateOverlayView() {
        View v = new View(RuntimeEnvironment.application);
        Mockito.when(mockDelegate.onCreateOverlayView()).thenReturn(v);

        View actualView = wrappedSession.onCreateOverlayView();

        assertThat(actualView).isEqualTo(v);
        Mockito.verify(mockDelegate).onCreateOverlayView();
        Mockito.verifyNoMoreInteractions(mockDelegate);
    }

    @Test
    public void testOnOverlayViewSizeChanged() {
        wrappedSession.onOverlayViewSizeChanged(5 /* width */, 13 /* height */);
        Mockito.verify(mockDelegate).onOverlayViewSizeChanged(5, 13);
        Mockito.verifyNoMoreInteractions(mockDelegate);
    }
}
