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
 * limitations under the License
 */

package com.android.server.telecom;

import android.media.AudioManager;
import android.os.Looper;
import android.os.Message;
import android.telecom.Log;
import android.telecom.Logging.Runnable;
import android.telecom.Logging.Session;
import android.util.SparseArray;

import com.android.internal.util.IState;
import com.android.internal.util.IndentingPrintWriter;
import com.android.internal.util.State;
import com.android.internal.util.StateMachine;

public class CallAudioModeStateMachine extends StateMachine {
    public static class Factory {
        public CallAudioModeStateMachine create(SystemStateHelper systemStateHelper,
                AudioManager am) {
            return new CallAudioModeStateMachine(systemStateHelper, am);
        }
    }

    public static class MessageArgs {
        public boolean hasActiveOrDialingCalls;
        public boolean hasRingingCalls;
        public boolean hasHoldingCalls;
        public boolean hasAudioProcessingCalls;
        public boolean isTonePlaying;
        public boolean foregroundCallIsVoip;
        public Session session;

        private MessageArgs(boolean hasActiveOrDialingCalls, boolean hasRingingCalls,
                boolean hasHoldingCalls, boolean hasAudioProcessingCalls, boolean isTonePlaying,
                boolean foregroundCallIsVoip, Session session) {
            this.hasActiveOrDialingCalls = hasActiveOrDialingCalls;
            this.hasRingingCalls = hasRingingCalls;
            this.hasHoldingCalls = hasHoldingCalls;
            this.hasAudioProcessingCalls = hasAudioProcessingCalls;
            this.isTonePlaying = isTonePlaying;
            this.foregroundCallIsVoip = foregroundCallIsVoip;
            this.session = session;
        }

        @Override
        public String toString() {
            return "MessageArgs{" +
                    "hasActiveCalls=" + hasActiveOrDialingCalls +
                    ", hasRingingCalls=" + hasRingingCalls +
                    ", hasHoldingCalls=" + hasHoldingCalls +
                    ", hasAudioProcessingCalls=" + hasAudioProcessingCalls +
                    ", isTonePlaying=" + isTonePlaying +
                    ", foregroundCallIsVoip=" + foregroundCallIsVoip +
                    ", session=" + session +
                    '}';
        }

        public static class Builder {
            private boolean mHasActiveOrDialingCalls;
            private boolean mHasRingingCalls;
            private boolean mHasHoldingCalls;
            private boolean mHasAudioProcessingCalls;
            private boolean mIsTonePlaying;
            private boolean mForegroundCallIsVoip;
            private Session mSession;

            public Builder setHasActiveOrDialingCalls(boolean hasActiveOrDialingCalls) {
                mHasActiveOrDialingCalls = hasActiveOrDialingCalls;
                return this;
            }

            public Builder setHasRingingCalls(boolean hasRingingCalls) {
                mHasRingingCalls = hasRingingCalls;
                return this;
            }

            public Builder setHasHoldingCalls(boolean hasHoldingCalls) {
                mHasHoldingCalls = hasHoldingCalls;
                return this;
            }

            public Builder setHasAudioProcessingCalls(boolean hasAudioProcessingCalls) {
                mHasAudioProcessingCalls = hasAudioProcessingCalls;
                return this;
            }

            public Builder setIsTonePlaying(boolean isTonePlaying) {
                mIsTonePlaying = isTonePlaying;
                return this;
            }

            public Builder setForegroundCallIsVoip(boolean foregroundCallIsVoip) {
                mForegroundCallIsVoip = foregroundCallIsVoip;
                return this;
            }

            public Builder setSession(Session session) {
                mSession = session;
                return this;
            }

            public MessageArgs build() {
                return new MessageArgs(mHasActiveOrDialingCalls, mHasRingingCalls, mHasHoldingCalls,
                        mHasAudioProcessingCalls, mIsTonePlaying, mForegroundCallIsVoip, mSession);
            }
        }
    }

    // TODO: remove this and replace when the new audio mode gets pushed to AOSP.
    public static final int NEW_AUDIO_MODE_FOR_AUDIO_PROCESSING = 4;

    public static final int INITIALIZE = 1;
    // These ENTER_*_FOCUS commands are for testing.
    public static final int ENTER_CALL_FOCUS_FOR_TESTING = 2;
    public static final int ENTER_COMMS_FOCUS_FOR_TESTING = 3;
    public static final int ENTER_RING_FOCUS_FOR_TESTING = 4;
    public static final int ENTER_TONE_OR_HOLD_FOCUS_FOR_TESTING = 5;
    public static final int ENTER_AUDIO_PROCESSING_FOCUS_FOR_TESTING = 6;
    public static final int ABANDON_FOCUS_FOR_TESTING = 7;

    public static final int NO_MORE_ACTIVE_OR_DIALING_CALLS = 1001;
    public static final int NO_MORE_RINGING_CALLS = 1002;
    public static final int NO_MORE_HOLDING_CALLS = 1003;
    public static final int NO_MORE_AUDIO_PROCESSING_CALLS = 1004;

    public static final int NEW_ACTIVE_OR_DIALING_CALL = 2001;
    public static final int NEW_RINGING_CALL = 2002;
    public static final int NEW_HOLDING_CALL = 2003;
    public static final int NEW_AUDIO_PROCESSING_CALL = 2004;

    public static final int TONE_STARTED_PLAYING = 3001;
    public static final int TONE_STOPPED_PLAYING = 3002;

    public static final int FOREGROUND_VOIP_MODE_CHANGE = 4001;

    public static final int RINGER_MODE_CHANGE = 5001;

    // Used to indicate that Telecom is done doing things to the AudioManager and that it's safe
    // to release focus for other apps to take over.
    public static final int AUDIO_OPERATIONS_COMPLETE = 6001;

    public static final int RUN_RUNNABLE = 9001;

    private static final SparseArray<String> MESSAGE_CODE_TO_NAME = new SparseArray<String>() {{
        put(ENTER_CALL_FOCUS_FOR_TESTING, "ENTER_CALL_FOCUS_FOR_TESTING");
        put(ENTER_COMMS_FOCUS_FOR_TESTING, "ENTER_COMMS_FOCUS_FOR_TESTING");
        put(ENTER_RING_FOCUS_FOR_TESTING, "ENTER_RING_FOCUS_FOR_TESTING");
        put(ENTER_AUDIO_PROCESSING_FOCUS_FOR_TESTING, "ENTER_AUDIO_PROCESSING_FOCUS_FOR_TESTING");
        put(ENTER_TONE_OR_HOLD_FOCUS_FOR_TESTING, "ENTER_TONE_OR_HOLD_FOCUS_FOR_TESTING");
        put(ABANDON_FOCUS_FOR_TESTING, "ABANDON_FOCUS_FOR_TESTING");
        put(NO_MORE_ACTIVE_OR_DIALING_CALLS, "NO_MORE_ACTIVE_OR_DIALING_CALLS");
        put(NO_MORE_RINGING_CALLS, "NO_MORE_RINGING_CALLS");
        put(NO_MORE_HOLDING_CALLS, "NO_MORE_HOLDING_CALLS");
        put(NO_MORE_AUDIO_PROCESSING_CALLS, "NO_MORE_AUDIO_PROCESSING_CALLS");
        put(NEW_ACTIVE_OR_DIALING_CALL, "NEW_ACTIVE_OR_DIALING_CALL");
        put(NEW_RINGING_CALL, "NEW_RINGING_CALL");
        put(NEW_HOLDING_CALL, "NEW_HOLDING_CALL");
        put(NEW_AUDIO_PROCESSING_CALL, "NEW_AUDIO_PROCESSING_CALL");
        put(TONE_STARTED_PLAYING, "TONE_STARTED_PLAYING");
        put(TONE_STOPPED_PLAYING, "TONE_STOPPED_PLAYING");
        put(FOREGROUND_VOIP_MODE_CHANGE, "FOREGROUND_VOIP_MODE_CHANGE");
        put(RINGER_MODE_CHANGE, "RINGER_MODE_CHANGE");
        put(AUDIO_OPERATIONS_COMPLETE, "AUDIO_OPERATIONS_COMPLETE");

        put(RUN_RUNNABLE, "RUN_RUNNABLE");
    }};

    public static final String TONE_HOLD_STATE_NAME = OtherFocusState.class.getSimpleName();
    public static final String UNFOCUSED_STATE_NAME = UnfocusedState.class.getSimpleName();
    public static final String AUDIO_PROCESSING_STATE_NAME =
            AudioProcessingFocusState.class.getSimpleName();
    public static final String CALL_STATE_NAME = SimCallFocusState.class.getSimpleName();
    public static final String RING_STATE_NAME = RingingFocusState.class.getSimpleName();
    public static final String COMMS_STATE_NAME = VoipCallFocusState.class.getSimpleName();

    private class BaseState extends State {
        @Override
        public boolean processMessage(Message msg) {
            switch (msg.what) {
                case ENTER_CALL_FOCUS_FOR_TESTING:
                    transitionTo(mSimCallFocusState);
                    return HANDLED;
                case ENTER_COMMS_FOCUS_FOR_TESTING:
                    transitionTo(mVoipCallFocusState);
                    return HANDLED;
                case ENTER_RING_FOCUS_FOR_TESTING:
                    transitionTo(mRingingFocusState);
                    return HANDLED;
                case ENTER_TONE_OR_HOLD_FOCUS_FOR_TESTING:
                    transitionTo(mOtherFocusState);
                    return HANDLED;
                case ENTER_AUDIO_PROCESSING_FOCUS_FOR_TESTING:
                    transitionTo(mAudioProcessingFocusState);
                    return HANDLED;
                case ABANDON_FOCUS_FOR_TESTING:
                    transitionTo(mUnfocusedState);
                    return HANDLED;
                case INITIALIZE:
                    mIsInitialized = true;
                    return HANDLED;
                case RUN_RUNNABLE:
                    java.lang.Runnable r = (java.lang.Runnable) msg.obj;
                    r.run();
                    return HANDLED;
                default:
                    return NOT_HANDLED;
            }
        }
    }

    private class UnfocusedState extends BaseState {
        @Override
        public void enter() {
            if (mIsInitialized) {
                mCallAudioManager.setCallAudioRouteFocusState(CallAudioRouteStateMachine.NO_FOCUS);
                mAudioManager.setMode(AudioManager.MODE_NORMAL);
                mMostRecentMode = AudioManager.MODE_NORMAL;
                // Don't release focus here -- wait until we get a signal that any other audio
                // operations triggered by this are done before releasing focus.
            }
        }

        @Override
        public boolean processMessage(Message msg) {
            if (super.processMessage(msg) == HANDLED) {
                return HANDLED;
            }
            MessageArgs args = (MessageArgs) msg.obj;
            switch (msg.what) {
                case NO_MORE_ACTIVE_OR_DIALING_CALLS:
                    // Do nothing.
                    return HANDLED;
                case NO_MORE_RINGING_CALLS:
                    // Do nothing.
                    return HANDLED;
                case NO_MORE_HOLDING_CALLS:
                    // Do nothing.
                    return HANDLED;
                case NO_MORE_AUDIO_PROCESSING_CALLS:
                    // Do nothing.
                    return HANDLED;
                case NEW_ACTIVE_OR_DIALING_CALL:
                    transitionTo(args.foregroundCallIsVoip
                            ? mVoipCallFocusState : mSimCallFocusState);
                    return HANDLED;
                case NEW_RINGING_CALL:
                    transitionTo(mRingingFocusState);
                    return HANDLED;
                case NEW_AUDIO_PROCESSING_CALL:
                    transitionTo(mAudioProcessingFocusState);
                    return HANDLED;
                case NEW_HOLDING_CALL:
                    // This really shouldn't happen, but transition to the focused state anyway.
                    Log.w(LOG_TAG, "Call was surprisingly put into hold from an unknown state." +
                            " Args are: \n" + args.toString());
                    transitionTo(mOtherFocusState);
                    return HANDLED;
                case TONE_STARTED_PLAYING:
                    // This shouldn't happen either, but perform the action anyway.
                    Log.w(LOG_TAG, "Tone started playing unexpectedly. Args are: \n"
                            + args.toString());
                    return HANDLED;
                case AUDIO_OPERATIONS_COMPLETE:
                    Log.i(LOG_TAG, "Abandoning audio focus: now UNFOCUSED");
                    mAudioManager.abandonAudioFocusForCall();
                    return HANDLED;
                default:
                    // The forced focus switch commands are handled by BaseState.
                    return NOT_HANDLED;
            }
        }
    }

    private class AudioProcessingFocusState extends BaseState {
        @Override
        public void enter() {
            if (mIsInitialized) {
                mCallAudioManager.setCallAudioRouteFocusState(CallAudioRouteStateMachine.NO_FOCUS);
                mAudioManager.setMode(NEW_AUDIO_MODE_FOR_AUDIO_PROCESSING);
                mMostRecentMode = NEW_AUDIO_MODE_FOR_AUDIO_PROCESSING;
            }
        }

        @Override
        public boolean processMessage(Message msg) {
            if (super.processMessage(msg) == HANDLED) {
                return HANDLED;
            }
            MessageArgs args = (MessageArgs) msg.obj;
            switch (msg.what) {
                case NO_MORE_ACTIVE_OR_DIALING_CALLS:
                    // Do nothing.
                    return HANDLED;
                case NO_MORE_RINGING_CALLS:
                    // Do nothing.
                    return HANDLED;
                case NO_MORE_HOLDING_CALLS:
                    // Do nothing.
                    return HANDLED;
                case NO_MORE_AUDIO_PROCESSING_CALLS:
                    BaseState destState = calculateProperStateFromArgs(args);
                    if (destState == this) {
                        Log.w(LOG_TAG, "Got spurious NO_MORE_AUDIO_PROCESSING_CALLS");
                    }
                    transitionTo(destState);
                    return HANDLED;
                case NEW_ACTIVE_OR_DIALING_CALL:
                    transitionTo(args.foregroundCallIsVoip
                            ? mVoipCallFocusState : mSimCallFocusState);
                    return HANDLED;
                case NEW_RINGING_CALL:
                    transitionTo(mRingingFocusState);
                    return HANDLED;
                case NEW_HOLDING_CALL:
                    // This really shouldn't happen, but recalculate from args and do the transition
                    Log.w(LOG_TAG, "Call was surprisingly put into hold from an unknown state." +
                            " Args are: \n" + args.toString());
                    transitionTo(mOtherFocusState);
                    return HANDLED;
                case NEW_AUDIO_PROCESSING_CALL:
                    // Can happen as a duplicate message
                    return HANDLED;
                case TONE_STARTED_PLAYING:
                    // This shouldn't happen either, but perform the action anyway.
                    Log.w(LOG_TAG, "Tone started playing unexpectedly. Args are: \n"
                            + args.toString());
                    return HANDLED;
                case AUDIO_OPERATIONS_COMPLETE:
                    Log.i(LOG_TAG, "Abandoning audio focus: now AUDIO_PROCESSING");
                    mAudioManager.abandonAudioFocusForCall();
                    return HANDLED;
                default:
                    // The forced focus switch commands are handled by BaseState.
                    return NOT_HANDLED;
            }
        }
    }

    private class RingingFocusState extends BaseState {
        // Keeps track of whether we're ringing with audio focus or if we've just entered the state
        // without acquiring focus because of a silent ringtone or something.
        private boolean mHasFocus = false;

        private void tryStartRinging() {
            if (mHasFocus && mCallAudioManager.isRingtonePlaying()) {
                Log.i(LOG_TAG, "RingingFocusState#tryStartRinging -- audio focus previously"
                        + " acquired and ringtone already playing -- skipping.");
                return;
            }

            if (mCallAudioManager.startRinging()) {
                mAudioManager.requestAudioFocusForCall(AudioManager.STREAM_RING,
                        AudioManager.AUDIOFOCUS_GAIN_TRANSIENT);
                // Do not set MODE_RINGTONE if we were previously in the CALL_SCREENING mode -- this
                // trips up the audio system.
                if (mAudioManager.getMode() != AudioManager.MODE_CALL_SCREENING) {
                    mAudioManager.setMode(AudioManager.MODE_RINGTONE);
                }
                mCallAudioManager.setCallAudioRouteFocusState(
                        CallAudioRouteStateMachine.RINGING_FOCUS);
                mHasFocus = true;
            } else {
                Log.i(LOG_TAG, "RINGING state, try start ringing but not acquiring audio focus");
            }
        }

        @Override
        public void enter() {
            Log.i(LOG_TAG, "Audio focus entering RINGING state");
            tryStartRinging();
            mCallAudioManager.stopCallWaiting();
        }

        @Override
        public void exit() {
            // Audio mode and audio stream will be set by the next state.
            mCallAudioManager.stopRinging();
            mHasFocus = false;
        }

        @Override
        public boolean processMessage(Message msg) {
            if (super.processMessage(msg) == HANDLED) {
                return HANDLED;
            }
            MessageArgs args = (MessageArgs) msg.obj;
            switch (msg.what) {
                case NO_MORE_ACTIVE_OR_DIALING_CALLS:
                    // Do nothing. Loss of an active call should not impact ringer.
                    return HANDLED;
                case NO_MORE_HOLDING_CALLS:
                    // Do nothing and keep ringing.
                    return HANDLED;
                case NO_MORE_RINGING_CALLS:
                    BaseState destState = calculateProperStateFromArgs(args);
                    if (destState == this) {
                        Log.w(LOG_TAG, "Got spurious NO_MORE_RINGING_CALLS");
                    }
                    transitionTo(destState);
                    return HANDLED;
                case NEW_ACTIVE_OR_DIALING_CALL:
                    // If a call becomes active suddenly, give it priority over ringing.
                    transitionTo(args.foregroundCallIsVoip
                            ? mVoipCallFocusState : mSimCallFocusState);
                    return HANDLED;
                case NEW_AUDIO_PROCESSING_CALL:
                    // If we don't have any more ringing calls, transition to audio processing.
                    if (!args.hasRingingCalls) {
                        transitionTo(mAudioProcessingFocusState);
                    } else {
                        Log.w(LOG_TAG, "Got a audio processing call while there's still a call "
                                + "ringing");
                    }
                case NEW_RINGING_CALL:
                    // Can happen as a duplicate message
                    return HANDLED;
                case NEW_HOLDING_CALL:
                    // This really shouldn't happen, but transition to the focused state anyway.
                    Log.w(LOG_TAG, "Call was surprisingly put into hold while ringing." +
                            " Args are: " + args.toString());
                    transitionTo(mOtherFocusState);
                    return HANDLED;
                case RINGER_MODE_CHANGE: {
                    Log.i(LOG_TAG, "RINGING state, received RINGER_MODE_CHANGE");
                    tryStartRinging();
                    return HANDLED;
                }
                case AUDIO_OPERATIONS_COMPLETE:
                    Log.w(LOG_TAG, "Should not be seeing AUDIO_OPERATIONS_COMPLETE in a focused"
                            + " state");
                    return HANDLED;
                default:
                    // The forced focus switch commands are handled by BaseState.
                    return NOT_HANDLED;
            }
        }
    }

    private class SimCallFocusState extends BaseState {
        @Override
        public void enter() {
            Log.i(LOG_TAG, "Audio focus entering SIM CALL state");
            mAudioManager.requestAudioFocusForCall(AudioManager.STREAM_VOICE_CALL,
                    AudioManager.AUDIOFOCUS_GAIN_TRANSIENT);
            mAudioManager.setMode(AudioManager.MODE_IN_CALL);
            mMostRecentMode = AudioManager.MODE_IN_CALL;
            mCallAudioManager.setCallAudioRouteFocusState(CallAudioRouteStateMachine.ACTIVE_FOCUS);
        }

        @Override
        public boolean processMessage(Message msg) {
            if (super.processMessage(msg) == HANDLED) {
                return HANDLED;
            }
            MessageArgs args = (MessageArgs) msg.obj;
            switch (msg.what) {
                case NO_MORE_ACTIVE_OR_DIALING_CALLS:
                    // Switch to either ringing, holding, or inactive
                    transitionTo(calculateProperStateFromArgs(args));
                    return HANDLED;
                case NO_MORE_RINGING_CALLS:
                    // Don't transition state, but stop any call-waiting tones that may have been
                    // playing.
                    if (args.isTonePlaying) {
                        mCallAudioManager.stopCallWaiting();
                    }
                    // If a MT-audio-speedup call gets disconnected by the connection service
                    // concurrently with the user answering it, we may get this message
                    // indicating that a ringing call has disconnected while this state machine
                    // is in the SimCallFocusState.
                    if (!args.hasActiveOrDialingCalls) {
                        transitionTo(calculateProperStateFromArgs(args));
                    }
                    return HANDLED;
                case NO_MORE_HOLDING_CALLS:
                    if (args.foregroundCallIsVoip) {
                        transitionTo(mVoipCallFocusState);
                    }
                    return HANDLED;
                case NEW_ACTIVE_OR_DIALING_CALL:
                    if (args.foregroundCallIsVoip) {
                        transitionTo(mVoipCallFocusState);
                    }
                    return HANDLED;
                case NEW_RINGING_CALL:
                    // Don't make a call ring over an active call, but do play a call waiting tone.
                    mCallAudioManager.startCallWaiting("call already active");
                    return HANDLED;
                case NEW_HOLDING_CALL:
                    // Just check the voip mode. Putting an active call on hold will be handled when
                    // NO_MORE_ACTIVE_CALLS is processed.
                    if (args.foregroundCallIsVoip) {
                        transitionTo(mVoipCallFocusState);
                    }
                    return HANDLED;
                case NEW_AUDIO_PROCESSING_CALL:
                    // If we don't have any more active calls, transition to audio processing.
                    if (!args.hasActiveOrDialingCalls) {
                        transitionTo(mAudioProcessingFocusState);
                    } else {
                        Log.w(LOG_TAG, "Got a audio processing call while there's still a call "
                                + "active");
                    }
                case FOREGROUND_VOIP_MODE_CHANGE:
                    if (args.foregroundCallIsVoip) {
                        transitionTo(mVoipCallFocusState);
                    }
                    return HANDLED;
                case AUDIO_OPERATIONS_COMPLETE:
                    Log.w(LOG_TAG, "Should not be seeing AUDIO_OPERATIONS_COMPLETE in a focused"
                            + " state");
                    return HANDLED;
                default:
                    // The forced focus switch commands are handled by BaseState.
                    return NOT_HANDLED;
            }
        }
    }

    private class VoipCallFocusState extends BaseState {
        @Override
        public void enter() {
            Log.i(LOG_TAG, "Audio focus entering VOIP CALL state");
            mAudioManager.requestAudioFocusForCall(AudioManager.STREAM_VOICE_CALL,
                    AudioManager.AUDIOFOCUS_GAIN_TRANSIENT);
            mAudioManager.setMode(AudioManager.MODE_IN_COMMUNICATION);
            mMostRecentMode = AudioManager.MODE_IN_COMMUNICATION;
            mCallAudioManager.setCallAudioRouteFocusState(CallAudioRouteStateMachine.ACTIVE_FOCUS);
        }

        @Override
        public boolean processMessage(Message msg) {
            if (super.processMessage(msg) == HANDLED) {
                return HANDLED;
            }
            MessageArgs args = (MessageArgs) msg.obj;
            switch (msg.what) {
                case NO_MORE_ACTIVE_OR_DIALING_CALLS:
                    // Switch to either ringing, holding, or inactive
                    transitionTo(calculateProperStateFromArgs(args));
                    return HANDLED;
                case NO_MORE_RINGING_CALLS:
                    // Don't transition state, but stop any call-waiting tones that may have been
                    // playing.
                    if (args.isTonePlaying) {
                        mCallAudioManager.stopCallWaiting();
                    }
                    return HANDLED;
                case NO_MORE_HOLDING_CALLS:
                    if (!args.foregroundCallIsVoip) {
                        transitionTo(mSimCallFocusState);
                    }
                    return HANDLED;
                case NEW_ACTIVE_OR_DIALING_CALL:
                    if (!args.foregroundCallIsVoip) {
                        transitionTo(mSimCallFocusState);
                    }
                    return HANDLED;
                case NEW_RINGING_CALL:
                    // Don't make a call ring over an active call, but do play a call waiting tone.
                    mCallAudioManager.startCallWaiting("call already active");
                    return HANDLED;
                case NEW_HOLDING_CALL:
                    // Just check the voip mode. Putting an active call on hold will be handled when
                    // NO_MORE_ACTIVE_CALLS is processed.
                    if (!args.foregroundCallIsVoip) {
                        transitionTo(mSimCallFocusState);
                    }
                    return HANDLED;
                case NEW_AUDIO_PROCESSING_CALL:
                    // If we don't have any more active calls, transition to audio processing.
                    if (!args.hasActiveOrDialingCalls) {
                        transitionTo(mAudioProcessingFocusState);
                    } else {
                        Log.w(LOG_TAG, "Got a audio processing call while there's still a call "
                                + "active");
                    }
                case FOREGROUND_VOIP_MODE_CHANGE:
                    if (!args.foregroundCallIsVoip) {
                        transitionTo(mSimCallFocusState);
                    }
                    return HANDLED;
                case AUDIO_OPERATIONS_COMPLETE:
                    Log.w(LOG_TAG, "Should not be seeing AUDIO_OPERATIONS_COMPLETE in a focused"
                            + " state");
                    return HANDLED;
                default:
                    // The forced focus switch commands are handled by BaseState.
                    return NOT_HANDLED;
            }
        }
    }

    /**
     * This class is used for calls on hold and end-of-call tones.
     */
    private class OtherFocusState extends BaseState {
        @Override
        public void enter() {
            Log.i(LOG_TAG, "Audio focus entering TONE/HOLDING state");
            mAudioManager.requestAudioFocusForCall(AudioManager.STREAM_VOICE_CALL,
                    AudioManager.AUDIOFOCUS_GAIN_TRANSIENT);
            mAudioManager.setMode(mMostRecentMode);
            mCallAudioManager.setCallAudioRouteFocusState(CallAudioRouteStateMachine.ACTIVE_FOCUS);
        }

        @Override
        public boolean processMessage(Message msg) {
            if (super.processMessage(msg) == HANDLED) {
                return HANDLED;
            }
            MessageArgs args = (MessageArgs) msg.obj;
            switch (msg.what) {
                case NO_MORE_HOLDING_CALLS:
                    if (args.hasActiveOrDialingCalls) {
                        transitionTo(args.foregroundCallIsVoip
                                ? mVoipCallFocusState : mSimCallFocusState);
                    } else if (args.hasRingingCalls) {
                        transitionTo(mRingingFocusState);
                    } else if (!args.isTonePlaying) {
                        transitionTo(mUnfocusedState);
                    }
                    // Do nothing if a tone is playing.
                    return HANDLED;
                case NEW_ACTIVE_OR_DIALING_CALL:
                    transitionTo(args.foregroundCallIsVoip
                            ? mVoipCallFocusState : mSimCallFocusState);
                    return HANDLED;
                case NEW_RINGING_CALL:
                    // TODO: consider whether to move this into MessageArgs if more things start
                    // to use it.
                    if (args.hasHoldingCalls && mSystemStateHelper.isDeviceAtEar()) {
                        mCallAudioManager.startCallWaiting(
                                "Device is at ear with held call");
                    } else {
                        transitionTo(mRingingFocusState);
                    }
                    return HANDLED;
                case NEW_HOLDING_CALL:
                    // Do nothing.
                    return HANDLED;
                case NO_MORE_RINGING_CALLS:
                    // If there are no more ringing calls in this state, then stop any call-waiting
                    // tones that may be playing.
                    mCallAudioManager.stopCallWaiting();
                    return HANDLED;
                case TONE_STOPPED_PLAYING:
                    transitionTo(calculateProperStateFromArgs(args));
                    return HANDLED;
                case AUDIO_OPERATIONS_COMPLETE:
                    Log.w(LOG_TAG, "Should not be seeing AUDIO_OPERATIONS_COMPLETE in a focused"
                            + " state");
                    return HANDLED;
                default:
                    return NOT_HANDLED;
            }
        }
    }

    private static final String LOG_TAG = CallAudioModeStateMachine.class.getSimpleName();

    private final BaseState mUnfocusedState = new UnfocusedState();
    private final BaseState mRingingFocusState = new RingingFocusState();
    private final BaseState mSimCallFocusState = new SimCallFocusState();
    private final BaseState mVoipCallFocusState = new VoipCallFocusState();
    private final BaseState mAudioProcessingFocusState = new AudioProcessingFocusState();
    private final BaseState mOtherFocusState = new OtherFocusState();

    private final AudioManager mAudioManager;
    private final SystemStateHelper mSystemStateHelper;
    private CallAudioManager mCallAudioManager;

    private int mMostRecentMode;
    private boolean mIsInitialized = false;

    public CallAudioModeStateMachine(SystemStateHelper systemStateHelper,
            AudioManager audioManager) {
        super(CallAudioModeStateMachine.class.getSimpleName());
        mAudioManager = audioManager;
        mSystemStateHelper = systemStateHelper;
        mMostRecentMode = AudioManager.MODE_NORMAL;

        createStates();
    }

    /**
     * Used for testing
     */
    public CallAudioModeStateMachine(SystemStateHelper systemStateHelper,
            AudioManager audioManager, Looper looper) {
        super(CallAudioModeStateMachine.class.getSimpleName(), looper);
        mAudioManager = audioManager;
        mSystemStateHelper = systemStateHelper;
        mMostRecentMode = AudioManager.MODE_NORMAL;

        createStates();
    }

    private void createStates() {
        addState(mUnfocusedState);
        addState(mRingingFocusState);
        addState(mSimCallFocusState);
        addState(mVoipCallFocusState);
        addState(mAudioProcessingFocusState);
        addState(mOtherFocusState);
        setInitialState(mUnfocusedState);
        start();
        sendMessage(INITIALIZE, new MessageArgs.Builder()
                .setHasActiveOrDialingCalls(false)
                .setHasRingingCalls(false)
                .setHasHoldingCalls(false)
                .setIsTonePlaying(false)
                .setForegroundCallIsVoip(false)
                .setSession(Log.createSubsession())
                .build());
    }

    public void setCallAudioManager(CallAudioManager callAudioManager) {
        mCallAudioManager = callAudioManager;
    }

    public String getCurrentStateName() {
        IState currentState = getCurrentState();
        return currentState == null ? "no state" : currentState.getName();
    }

    public void sendMessageWithArgs(int messageCode, MessageArgs args) {
        sendMessage(messageCode, args);
    }

    @Override
    protected void onPreHandleMessage(Message msg) {
        if (msg.obj != null && msg.obj instanceof MessageArgs) {
            Log.continueSession(((MessageArgs) msg.obj).session, "CAMSM.pM_" + msg.what);
            Log.i(LOG_TAG, "Message received: %s.", MESSAGE_CODE_TO_NAME.get(msg.what));
        } else if (msg.what == RUN_RUNNABLE && msg.obj instanceof Runnable) {
            Log.i(LOG_TAG, "Running runnable for testing");
        } else {
                Log.w(LOG_TAG, "Message sent must be of type nonnull MessageArgs, but got " +
                        (msg.obj == null ? "null" : msg.obj.getClass().getSimpleName()));
                Log.w(LOG_TAG, "The message was of code %d = %s",
                        msg.what, MESSAGE_CODE_TO_NAME.get(msg.what));
        }
    }

    public void dumpPendingMessages(IndentingPrintWriter pw) {
        getHandler().getLooper().dump(pw::println, "");
    }

    @Override
    protected void onPostHandleMessage(Message msg) {
        Log.endSession();
    }

    private BaseState calculateProperStateFromArgs(MessageArgs args) {
        // If there are active, audio-processing, holding, or ringing calls,
        // switch to the appropriate focus.
        // Otherwise abandon focus.

        // The order matters here. If there are active calls, holding focus for them takes priority.
        // After that, we want to prioritize holding calls over ringing calls so that when a
        // call-waiting call gets answered, there's no transition in and out of the ringing focus
        // state. After that, we want tones since we actually hold focus during them, then the
        // audio processing state because that will release focus.
        if (args.hasActiveOrDialingCalls) {
            if (args.foregroundCallIsVoip) {
                return mVoipCallFocusState;
            } else {
                return mSimCallFocusState;
            }
        } else if (args.hasHoldingCalls) {
            return mOtherFocusState;
        } else if (args.hasRingingCalls) {
            return mRingingFocusState;
        } else if (args.isTonePlaying) {
            return mOtherFocusState;
        } else if (args.hasAudioProcessingCalls) {
            return mAudioProcessingFocusState;
        }
        return mUnfocusedState;
    }

}
