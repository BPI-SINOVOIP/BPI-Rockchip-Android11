package com.android.tv.common.support.tis;

import static com.google.common.truth.Truth.assertThat;

import android.media.tv.TvContentRating;
import android.media.tv.TvInputManager;
import android.media.tv.TvTrackInfo;
import android.net.Uri;
import android.os.Build;
import android.support.annotation.FloatRange;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.Pair;
import android.view.Surface;
import android.view.View;

import com.android.tv.common.support.tis.TifSession.TifSessionCallbacks;

import com.google.common.collect.ImmutableList;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import java.util.List;

/** Tests for {@link TifSession}. */
@RunWith(RobolectricTestRunner.class)
@Config(minSdk = Build.VERSION_CODES.LOLLIPOP, maxSdk = Build.VERSION_CODES.P)
public class TisSessionTest {

    private TestSession testSession;
    private TestCallback testCallback;

    @Before
    public void setup() {
        testCallback = new TestCallback();
        testSession = new TestSession(testCallback);
    }

    @Test
    public void notifyChannelReturned() {
        Uri uri = Uri.parse("http://example.com");
        testSession.notifyChannelRetuned(uri);
        assertThat(testCallback.channelUri).isEqualTo(uri);
    }

    @Test
    public void notifyTracksChanged() {
        List<TvTrackInfo> tracks =
                ImmutableList.of(new TvTrackInfo.Builder(TvTrackInfo.TYPE_VIDEO, "test").build());
        testSession.notifyTracksChanged(tracks);
        assertThat(testCallback.tracks).isEqualTo(tracks);
    }

    @Test
    public void notifyTrackSelected() {
        testSession.notifyTrackSelected(TvTrackInfo.TYPE_AUDIO, "audio_test");
        assertThat(testCallback.trackSelected)
                .isEqualTo(Pair.create(TvTrackInfo.TYPE_AUDIO, "audio_test"));
    }

    @Test
    public void notifyVideoAvailable() {
        testSession.notifyVideoAvailable();
        assertThat(testCallback.videoAvailable).isTrue();
    }

    @Test
    public void notifyVideoUnavailable() {
        testSession.notifyVideoUnavailable(TvInputManager.VIDEO_UNAVAILABLE_REASON_UNKNOWN);
        assertThat(testCallback.notifyVideoUnavailableReason)
                .isEqualTo(TvInputManager.VIDEO_UNAVAILABLE_REASON_UNKNOWN);
    }

    @Test
    public void notifyContentAllowed() {
        testSession.notifyContentAllowed();
        assertThat(testCallback.contentAllowed).isTrue();
    }

    @Test
    public void notifyContentBlocked() {
        TvContentRating rating = TvContentRating.createRating("1", "2", "3");
        testSession.notifyContentBlocked(rating);
        assertThat(testCallback.blockedContentRating).isEqualTo(rating);
    }

    @Test
    public void notifyTimeShiftStatusChanged() {
        testSession.notifyTimeShiftStatusChanged(TvInputManager.TIME_SHIFT_STATUS_AVAILABLE);
        assertThat(testCallback.timeShiftStatus)
                .isEqualTo(TvInputManager.TIME_SHIFT_STATUS_AVAILABLE);
    }

    @Test
    public void testSetOverlayViewEnabled() {
        testSession.helpTestSetOverlayViewEnabled(true);
        assertThat(testCallback.overlayViewEnabled).isTrue();

        testSession.helpTestSetOverlayViewEnabled(false);
        assertThat(testCallback.overlayViewEnabled).isFalse();
    }

    @Test
    public void testOnCreateOverlayView() {
        View actualView = testSession.onCreateOverlayView();
        assertThat(actualView).isNull(); // Default implementation returns a null.
    }

    @Test
    public void testOnOverlayViewSizeChanged() {
        testSession.onOverlayViewSizeChanged(5 /* width */, 7 /* height */);
        // Just verifing that the call completes.
    }

    private static final class TestCallback implements TifSessionCallbacks {

        private Uri channelUri;
        private List<TvTrackInfo> tracks;
        private Pair<Integer, String> trackSelected;
        private boolean videoAvailable;
        private int notifyVideoUnavailableReason;
        private boolean contentAllowed;
        private TvContentRating blockedContentRating;
        private int timeShiftStatus;
        private boolean overlayViewEnabled;

        @Override
        public void notifyChannelRetuned(Uri channelUri) {
            this.channelUri = channelUri;
        }

        @Override
        public void notifyTracksChanged(List<TvTrackInfo> tracks) {
            this.tracks = tracks;
        }

        @Override
        public void notifyTrackSelected(int type, String trackId) {
            this.trackSelected = Pair.create(type, trackId);
        }

        @Override
        public void notifyVideoAvailable() {
            this.videoAvailable = true;
        }

        @Override
        public void notifyVideoUnavailable(int reason) {
            this.notifyVideoUnavailableReason = reason;
        }

        @Override
        public void notifyContentAllowed() {
            this.contentAllowed = true;
        }

        @Override
        public void notifyContentBlocked(@NonNull TvContentRating rating) {
            this.blockedContentRating = rating;
        }

        @Override
        public void notifyTimeShiftStatusChanged(int status) {
            this.timeShiftStatus = status;
        }

        @Override
        public void setOverlayViewEnabled(boolean enabled) {
            this.overlayViewEnabled = enabled;
        }
    }

    private static final class TestSession extends TifSession {

        private TestSession(TifSessionCallbacks callbacks) {
            super(callbacks);
        }

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

        public void helpTestSetOverlayViewEnabled(boolean enabled) {
            super.setOverlayViewEnabled(enabled);
        }
    }
}
