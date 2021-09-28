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
package android.media.cts;

import static android.media.cts.Utils.compareRemoteUserInfo;
import static android.media.session.PlaybackState.STATE_PLAYING;

import android.content.Intent;
import android.media.AudioAttributes;
import android.media.AudioManager;
import android.media.Rating;
import android.media.VolumeProvider;
import android.media.session.MediaController;
import android.media.session.MediaSession;
import android.media.session.MediaSessionManager.RemoteUserInfo;
import android.media.session.PlaybackState;
import android.media.session.PlaybackState.CustomAction;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.os.ResultReceiver;
import android.test.AndroidTestCase;
import android.view.KeyEvent;

/**
 * Test {@link android.media.session.MediaController}.
 */
@NonMediaMainlineTest
public class MediaControllerTest extends AndroidTestCase {
    // The maximum time to wait for an operation.
    private static final long TIME_OUT_MS = 3000L;
    private static final String SESSION_TAG = "test-session";
    private static final String EXTRAS_KEY = "test-key";
    private static final String EXTRAS_VALUE = "test-val";

    private final Object mWaitLock = new Object();
    private Handler mHandler = new Handler(Looper.getMainLooper());
    private MediaSession mSession;
    private Bundle mSessionInfo;
    private MediaSessionCallback mCallback = new MediaSessionCallback();
    private MediaController mController;
    private RemoteUserInfo mControllerInfo;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mSessionInfo = new Bundle();
        mSessionInfo.putString(EXTRAS_KEY, EXTRAS_VALUE);
        mSession = new MediaSession(getContext(), SESSION_TAG, mSessionInfo);
        mSession.setCallback(mCallback, mHandler);
        mController = mSession.getController();
        mControllerInfo = new RemoteUserInfo(
                getContext().getPackageName(), Process.myPid(), Process.myUid());
    }

    public void testGetPackageName() {
        assertEquals(getContext().getPackageName(), mController.getPackageName());
    }

    public void testGetPlaybackState() {
        final int testState = STATE_PLAYING;
        final long testPosition = 100000L;
        final float testSpeed = 1.0f;
        final long testActions = PlaybackState.ACTION_PLAY | PlaybackState.ACTION_STOP
                | PlaybackState.ACTION_SEEK_TO;
        final long testActiveQueueItemId = 3377;
        final long testBufferedPosition = 100246L;
        final String testErrorMsg = "ErrorMsg";

        final Bundle extras = new Bundle();
        extras.putString(EXTRAS_KEY, EXTRAS_VALUE);

        final double positionDelta = 500;

        PlaybackState state = new PlaybackState.Builder()
                .setState(testState, testPosition, testSpeed)
                .setActions(testActions)
                .setActiveQueueItemId(testActiveQueueItemId)
                .setBufferedPosition(testBufferedPosition)
                .setErrorMessage(testErrorMsg)
                .setExtras(extras)
                .build();

        mSession.setPlaybackState(state);

        // Note: No need to wait since the AIDL call is not oneway.
        PlaybackState stateOut = mController.getPlaybackState();
        assertNotNull(stateOut);
        assertEquals(testState, stateOut.getState());
        assertEquals(testPosition, stateOut.getPosition(), positionDelta);
        assertEquals(testSpeed, stateOut.getPlaybackSpeed(), 0.0f);
        assertEquals(testActions, stateOut.getActions());
        assertEquals(testActiveQueueItemId, stateOut.getActiveQueueItemId());
        assertEquals(testBufferedPosition, stateOut.getBufferedPosition());
        assertEquals(testErrorMsg, stateOut.getErrorMessage().toString());
        assertNotNull(stateOut.getExtras());
        assertEquals(EXTRAS_VALUE, stateOut.getExtras().get(EXTRAS_KEY));
    }

    public void testGetRatingType() {
        assertEquals("Default rating type of a session must be Rating.RATING_NONE",
                Rating.RATING_NONE, mController.getRatingType());

        mSession.setRatingType(Rating.RATING_5_STARS);
        // Note: No need to wait since the AIDL call is not oneway.
        assertEquals(Rating.RATING_5_STARS, mController.getRatingType());
    }

    public void testGetSessionToken() throws Exception {
        assertEquals(mSession.getSessionToken(), mController.getSessionToken());

        Bundle sessionInfo = mController.getSessionInfo();
        assertNotNull(sessionInfo);
        assertEquals(EXTRAS_VALUE, sessionInfo.getString(EXTRAS_KEY));
    }

    public void testGetTag() {
        assertEquals(SESSION_TAG, mController.getTag());
    }

    public void testSendCommand() throws Exception {
        synchronized (mWaitLock) {
            mCallback.reset();
            final String command = "test-command";
            final Bundle extras = new Bundle();
            extras.putString(EXTRAS_KEY, EXTRAS_VALUE);
            mController.sendCommand(command, extras, new ResultReceiver(null));
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnCommandCalled);
            assertNotNull(mCallback.mCommandCallback);
            assertEquals(command, mCallback.mCommand);
            assertEquals(EXTRAS_VALUE, mCallback.mExtras.getString(EXTRAS_KEY));
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));
        }
    }

    public void testSetPlaybackSpeed() throws Exception {
        synchronized (mWaitLock) {
            mCallback.reset();

            final float testSpeed = 2.0f;
            mController.getTransportControls().setPlaybackSpeed(testSpeed);
            mWaitLock.wait(TIME_OUT_MS);

            assertTrue(mCallback.mOnSetPlaybackSpeedCalled);
            assertEquals(testSpeed, mCallback.mSpeed, 0.0f);
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));
        }
    }

    public void testAdjustVolumeWithIllegalDirection() {
        // Call the method with illegal direction. System should not reboot.
        mController.adjustVolume(37, 0);
    }

    public void testVolumeControl() throws Exception {
        VolumeProvider vp = new VolumeProvider(VolumeProvider.VOLUME_CONTROL_ABSOLUTE, 11, 5) {
            @Override
            public void onSetVolumeTo(int volume) {
                synchronized (mWaitLock) {
                    setCurrentVolume(volume);
                    mWaitLock.notify();
                }
            }

            @Override
            public void onAdjustVolume(int direction) {
                synchronized (mWaitLock) {
                    switch (direction) {
                        case AudioManager.ADJUST_LOWER:
                            setCurrentVolume(getCurrentVolume() - 1);
                            break;
                        case AudioManager.ADJUST_RAISE:
                            setCurrentVolume(getCurrentVolume() + 1);
                            break;
                    }
                    mWaitLock.notify();
                }
            }
        };
        mSession.setPlaybackToRemote(vp);

        synchronized (mWaitLock) {
            // test setVolumeTo
            mController.setVolumeTo(7, 0);
            mWaitLock.wait(TIME_OUT_MS);
            assertEquals(7, vp.getCurrentVolume());

            // test adjustVolume
            mController.adjustVolume(AudioManager.ADJUST_LOWER, 0);
            mWaitLock.wait(TIME_OUT_MS);
            assertEquals(6, vp.getCurrentVolume());

            mController.adjustVolume(AudioManager.ADJUST_RAISE, 0);
            mWaitLock.wait(TIME_OUT_MS);
            assertEquals(7, vp.getCurrentVolume());
        }
    }

    public void testTransportControlsAndMediaSessionCallback() throws Exception {
        MediaController.TransportControls controls = mController.getTransportControls();
        final MediaSession.Callback callback = (MediaSession.Callback) mCallback;

        synchronized (mWaitLock) {
            mCallback.reset();
            controls.play();
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnPlayCalled);
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            controls.pause();
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnPauseCalled);
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            controls.stop();
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnStopCalled);
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            controls.fastForward();
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnFastForwardCalled);
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            controls.rewind();
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnRewindCalled);
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            controls.skipToPrevious();
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnSkipToPreviousCalled);
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            controls.skipToNext();
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnSkipToNextCalled);
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            final long seekPosition = 1000;
            controls.seekTo(seekPosition);
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnSeekToCalled);
            assertEquals(seekPosition, mCallback.mSeekPosition);
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            final Rating rating = Rating.newStarRating(Rating.RATING_5_STARS, 3f);
            controls.setRating(rating);
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnSetRatingCalled);
            assertEquals(rating.getRatingStyle(), mCallback.mRating.getRatingStyle());
            assertEquals(rating.getStarRating(), mCallback.mRating.getStarRating());
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            final String mediaId = "test-media-id";
            final Bundle extras = new Bundle();
            extras.putString(EXTRAS_KEY, EXTRAS_VALUE);
            controls.playFromMediaId(mediaId, extras);
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnPlayFromMediaIdCalled);
            assertEquals(mediaId, mCallback.mMediaId);
            assertEquals(EXTRAS_VALUE, mCallback.mExtras.getString(EXTRAS_KEY));
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            final String query = "test-query";
            controls.playFromSearch(query, extras);
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnPlayFromSearchCalled);
            assertEquals(query, mCallback.mQuery);
            assertEquals(EXTRAS_VALUE, mCallback.mExtras.getString(EXTRAS_KEY));
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            final Uri uri = Uri.parse("content://test/popcorn.mod");
            controls.playFromUri(uri, extras);
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnPlayFromUriCalled);
            assertEquals(uri, mCallback.mUri);
            assertEquals(EXTRAS_VALUE, mCallback.mExtras.getString(EXTRAS_KEY));
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            final String action = "test-action";
            controls.sendCustomAction(action, extras);
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnCustomActionCalled);
            assertEquals(action, mCallback.mAction);
            assertEquals(EXTRAS_VALUE, mCallback.mExtras.getString(EXTRAS_KEY));
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            mCallback.mOnCustomActionCalled = false;
            final CustomAction customAction =
                    new CustomAction.Builder(action, action, -1).setExtras(extras).build();
            controls.sendCustomAction(customAction, extras);
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnCustomActionCalled);
            assertEquals(action, mCallback.mAction);
            assertEquals(EXTRAS_VALUE, mCallback.mExtras.getString(EXTRAS_KEY));
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            final long queueItemId = 1000;
            controls.skipToQueueItem(queueItemId);
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnSkipToQueueItemCalled);
            assertEquals(queueItemId, mCallback.mQueueItemId);
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            controls.prepare();
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnPrepareCalled);
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            controls.prepareFromMediaId(mediaId, extras);
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnPrepareFromMediaIdCalled);
            assertEquals(mediaId, mCallback.mMediaId);
            assertEquals(EXTRAS_VALUE, mCallback.mExtras.getString(EXTRAS_KEY));
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            controls.prepareFromSearch(query, extras);
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnPrepareFromSearchCalled);
            assertEquals(query, mCallback.mQuery);
            assertEquals(EXTRAS_VALUE, mCallback.mExtras.getString(EXTRAS_KEY));
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            controls.prepareFromUri(uri, extras);
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnPrepareFromUriCalled);
            assertEquals(uri, mCallback.mUri);
            assertEquals(EXTRAS_VALUE, mCallback.mExtras.getString(EXTRAS_KEY));
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            mCallback.reset();
            KeyEvent event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_STOP);
            mController.dispatchMediaButtonEvent(event);
            mWaitLock.wait(TIME_OUT_MS);
            assertTrue(mCallback.mOnMediaButtonEventCalled);
            // KeyEvent doesn't override equals.
            assertEquals(KeyEvent.ACTION_DOWN, mCallback.mKeyEvent.getAction());
            assertEquals(KeyEvent.KEYCODE_MEDIA_STOP, mCallback.mKeyEvent.getKeyCode());
            assertTrue(compareRemoteUserInfo(mControllerInfo, mCallback.mCallerInfo));

            // just call the callback once directly so it's marked as tested
            try {
                callback.onPlay();
                callback.onPause();
                callback.onStop();
                callback.onFastForward();
                callback.onRewind();
                callback.onSkipToPrevious();
                callback.onSkipToNext();
                callback.onSeekTo(mCallback.mSeekPosition);
                callback.onSetRating(mCallback.mRating);
                callback.onPlayFromMediaId(mCallback.mMediaId, mCallback.mExtras);
                callback.onPlayFromSearch(mCallback.mQuery, mCallback.mExtras);
                callback.onPlayFromUri(mCallback.mUri, mCallback.mExtras);
                callback.onCustomAction(mCallback.mAction, mCallback.mExtras);
                callback.onCustomAction(mCallback.mAction, mCallback.mExtras);
                callback.onSkipToQueueItem(mCallback.mQueueItemId);
                callback.onPrepare();
                callback.onPrepareFromMediaId(mCallback.mMediaId, mCallback.mExtras);
                callback.onPrepareFromSearch(mCallback.mQuery, mCallback.mExtras);
                callback.onPrepareFromUri(Uri.parse("http://d.android.com"), mCallback.mExtras);
                callback.onCommand(mCallback.mCommand, mCallback.mExtras, null);
                callback.onSetPlaybackSpeed(mCallback.mSpeed);
                Intent mediaButtonIntent = new Intent(Intent.ACTION_MEDIA_BUTTON);
                mediaButtonIntent.putExtra(Intent.EXTRA_KEY_EVENT, event);
                callback.onMediaButtonEvent(mediaButtonIntent);
            } catch (IllegalStateException ex) {
                // Expected, since the MediaSession.getCurrentControllerInfo() is called in every
                // callback method, but no controller is sending any command.
            }
        }
    }

    private class MediaSessionCallback extends MediaSession.Callback {
        private long mSeekPosition;
        private long mQueueItemId;
        private Rating mRating;
        private String mMediaId;
        private String mQuery;
        private Uri mUri;
        private String mAction;
        private String mCommand;
        private Bundle mExtras;
        private ResultReceiver mCommandCallback;
        private KeyEvent mKeyEvent;
        private RemoteUserInfo mCallerInfo;
        private float mSpeed;

        private boolean mOnPlayCalled;
        private boolean mOnPauseCalled;
        private boolean mOnStopCalled;
        private boolean mOnFastForwardCalled;
        private boolean mOnRewindCalled;
        private boolean mOnSkipToPreviousCalled;
        private boolean mOnSkipToNextCalled;
        private boolean mOnSeekToCalled;
        private boolean mOnSkipToQueueItemCalled;
        private boolean mOnSetRatingCalled;
        private boolean mOnPlayFromMediaIdCalled;
        private boolean mOnPlayFromSearchCalled;
        private boolean mOnPlayFromUriCalled;
        private boolean mOnCustomActionCalled;
        private boolean mOnCommandCalled;
        private boolean mOnPrepareCalled;
        private boolean mOnPrepareFromMediaIdCalled;
        private boolean mOnPrepareFromSearchCalled;
        private boolean mOnPrepareFromUriCalled;
        private boolean mOnMediaButtonEventCalled;
        private boolean mOnSetPlaybackSpeedCalled;

        public void reset() {
            mSeekPosition = -1;
            mQueueItemId = -1;
            mRating = null;
            mMediaId = null;
            mQuery = null;
            mUri = null;
            mAction = null;
            mExtras = null;
            mCommand = null;
            mCommandCallback = null;
            mKeyEvent = null;
            mCallerInfo = null;
            mSpeed = -1.0f;

            mOnPlayCalled = false;
            mOnPauseCalled = false;
            mOnStopCalled = false;
            mOnFastForwardCalled = false;
            mOnRewindCalled = false;
            mOnSkipToPreviousCalled = false;
            mOnSkipToNextCalled = false;
            mOnSkipToQueueItemCalled = false;
            mOnSeekToCalled = false;
            mOnSetRatingCalled = false;
            mOnPlayFromMediaIdCalled = false;
            mOnPlayFromSearchCalled = false;
            mOnPlayFromUriCalled = false;
            mOnCustomActionCalled = false;
            mOnCommandCalled = false;
            mOnPrepareCalled = false;
            mOnPrepareFromMediaIdCalled = false;
            mOnPrepareFromSearchCalled = false;
            mOnPrepareFromUriCalled = false;
            mOnMediaButtonEventCalled = false;
            mOnSetPlaybackSpeedCalled = false;
        }

        @Override
        public void onPlay() {
            synchronized (mWaitLock) {
                mOnPlayCalled = true;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public void onPause() {
            synchronized (mWaitLock) {
                mOnPauseCalled = true;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public void onStop() {
            synchronized (mWaitLock) {
                mOnStopCalled = true;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public void onFastForward() {
            synchronized (mWaitLock) {
                mOnFastForwardCalled = true;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public void onRewind() {
            synchronized (mWaitLock) {
                mOnRewindCalled = true;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public void onSkipToPrevious() {
            synchronized (mWaitLock) {
                mOnSkipToPreviousCalled = true;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public void onSkipToNext() {
            synchronized (mWaitLock) {
                mOnSkipToNextCalled = true;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public void onSeekTo(long pos) {
            synchronized (mWaitLock) {
                mOnSeekToCalled = true;
                mSeekPosition = pos;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public void onSetRating(Rating rating) {
            synchronized (mWaitLock) {
                mOnSetRatingCalled = true;
                mRating = rating;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public void onPlayFromMediaId(String mediaId, Bundle extras) {
            synchronized (mWaitLock) {
                mOnPlayFromMediaIdCalled = true;
                mMediaId = mediaId;
                mExtras = extras;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public void onPlayFromSearch(String query, Bundle extras) {
            synchronized (mWaitLock) {
                mOnPlayFromSearchCalled = true;
                mQuery = query;
                mExtras = extras;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public void onPlayFromUri(Uri uri, Bundle extras) {
            synchronized (mWaitLock) {
                mOnPlayFromUriCalled = true;
                mUri = uri;
                mExtras = extras;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public void onCustomAction(String action, Bundle extras) {
            synchronized (mWaitLock) {
                mOnCustomActionCalled = true;
                mAction = action;
                mExtras = extras;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public void onSkipToQueueItem(long id) {
            synchronized (mWaitLock) {
                mOnSkipToQueueItemCalled = true;
                mQueueItemId = id;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public void onCommand(String command, Bundle extras, ResultReceiver cb) {
            synchronized (mWaitLock) {
                mOnCommandCalled = true;
                mCommand = command;
                mExtras = extras;
                mCommandCallback = cb;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public void onPrepare() {
            synchronized (mWaitLock) {
                mOnPrepareCalled = true;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public void onPrepareFromMediaId(String mediaId, Bundle extras) {
            synchronized (mWaitLock) {
                mOnPrepareFromMediaIdCalled = true;
                mMediaId = mediaId;
                mExtras = extras;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public void onPrepareFromSearch(String query, Bundle extras) {
            synchronized (mWaitLock) {
                mOnPrepareFromSearchCalled = true;
                mQuery = query;
                mExtras = extras;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public void onPrepareFromUri(Uri uri, Bundle extras) {
            synchronized (mWaitLock) {
                mOnPrepareFromUriCalled = true;
                mUri = uri;
                mExtras = extras;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mWaitLock.notify();
            }
        }

        @Override
        public boolean onMediaButtonEvent(Intent mediaButtonIntent) {
            synchronized (mWaitLock) {
                mOnMediaButtonEventCalled = true;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mKeyEvent = mediaButtonIntent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
                mWaitLock.notify();
            }
            return super.onMediaButtonEvent(mediaButtonIntent);
        }

        @Override
        public void onSetPlaybackSpeed(float speed) {
            synchronized (mWaitLock) {
                mOnSetPlaybackSpeedCalled = true;
                mCallerInfo = mSession.getCurrentControllerInfo();
                mSpeed = speed;
                mWaitLock.notify();
            }
        }
    }
}
