/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.inputmethod.leanback;

import android.animation.Animator;
import android.animation.ValueAnimator;
import android.speech.RecognitionListener;
import android.os.Bundle;

import com.android.inputmethod.leanback.LeanbackKeyboardController.InputListener;
import com.android.inputmethod.leanback.voice.RecognizerView;
import com.android.inputmethod.leanback.voice.SpeechLevelSource;
import com.android.inputmethod.leanback.service.LeanbackImeService;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnFocusChangeListener;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.animation.AccelerateInterpolator;
import android.animation.ValueAnimator.AnimatorUpdateListener;
import android.animation.Animator.AnimatorListener;
import android.view.animation.Animation;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.Interpolator;
import android.view.animation.Transformation;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.InputMethodSubtype;
import android.graphics.PointF;
import android.graphics.Rect;
import android.speech.RecognizerIntent;
import android.speech.SpeechRecognizer;
import android.text.TextUtils;
import android.text.method.QwertyKeyListener;
import android.text.style.LocaleSpan;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.util.Log;
import android.inputmethodservice.Keyboard;
import android.inputmethodservice.Keyboard.Key;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * This is the keyboard container for GridIme that contains the following views:
 * <ul>
 * <li>voice button</li>
 * <li>main keyboard</li>
 * <li>action button</li>
 * <li>focus bubble</li>
 * <li>touch indicator</li>
 * <li>candidate view</li>
 * </ul>
 * Keyboard grid layout:
 *
 * <pre>
 * | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 0 |OTH|   |
 * |<- | - | - | - | - | - | - | - | - | ->|ER |ACT|
 * |<- | - | - | M | A | I | N | - | - | ->|   |   |
 * |<- | K | E | Y | B | O | A | R | D | ->|KEY|ION|
 * |<- | - | - | - | - | - | - | - | - | ->|S  |   |
 * </pre>
 */
public class LeanbackKeyboardContainer {

    private static final String TAG = "LbKbContainer";
    private static final boolean DEBUG = false;
    private static final boolean VOICE_SUPPORTED = true;
    private static final String IME_PRIVATE_OPTIONS_ESCAPE_NORTH_LEGACY = "EscapeNorth=1";
    private static final String IME_PRIVATE_OPTIONS_ESCAPE_NORTH = "escapeNorth";
    private static final String IME_PRIVATE_OPTIONS_VOICE_DISMISS_LEGACY = "VoiceDismiss=1";
    private static final String IME_PRIVATE_OPTIONS_VOICE_DISMISS = "voiceDismiss";

    /**
     * This is the length of animations that move an indicator across the keys. Snaps and flicks
     * will use this duration for the movement.
     */
    private static final long MOVEMENT_ANIMATION_DURATION = 150;

    /**
     * This interpolator is used for movement animations.
     */
    public static final Interpolator sMovementInterpolator = new DecelerateInterpolator(1.5f);

    /**
     * These are the states that the view can be in and affect the icon appearance. NO_TOUCH is when
     * there are no fingers down on the input device.
     */
    public static final int TOUCH_STATE_NO_TOUCH = 0;

    /**
     * TOUCH_SNAP indicates that a finger is down but the indicator is still considered snapped to a
     * letter. Once the user moves a given distance from the snapped position it will change to
     * TOUCH_MOVE.
     */
    public static final int TOUCH_STATE_TOUCH_SNAP = 1;

    /**
     * TOUCH_MOVE indicates the user is moving freely around the space and is not snapped to any
     * letter.
     */
    public static final int TOUCH_STATE_TOUCH_MOVE = 2;

    /**
     * CLICK indicates the selection button is currently pressed. When the button is released we
     * will transition back to snap or no touch depending on whether there is still a finger down on
     * the input device or not.
     */
    public static final int TOUCH_STATE_CLICK = 3;

    // The minimum distance the user must move their finger to transition from
    // the SNAP to the MOVE state
    public static final double TOUCH_MOVE_MIN_DISTANCE = .1;

    /**
     * When processing a flick or dpad event it is easier to move a key width + a fudge factor than
     * to directly compute what the next key position should be. This is the fudge factor.
     */
    public static final double DIRECTION_STEP_MULTIPLIER = 1.25;

    /**
     * Directions sent to event listeners.
     */
    public static final int DIRECTION_LEFT = 1 << 0;
    public static final int DIRECTION_DOWN = 1 << 1;
    public static final int DIRECTION_RIGHT = 1 << 2;
    public static final int DIRECTION_UP = 1 << 3;
    public static final int DIRECTION_DOWN_LEFT = DIRECTION_DOWN | DIRECTION_LEFT;
    public static final int DIRECTION_DOWN_RIGHT = DIRECTION_DOWN | DIRECTION_RIGHT;
    public static final int DIRECTION_UP_RIGHT = DIRECTION_UP | DIRECTION_RIGHT;
    public static final int DIRECTION_UP_LEFT = DIRECTION_UP | DIRECTION_LEFT;

    /**
     * handler messages
     */
    // align selector in onStartInputView
    private static final int MSG_START_INPUT_VIEW = 0;

    // If this were a physical keyboard the width in cm. This will be mapped
    // to the width in pixels but is representative of the mapping from the
    // remote input to the screen. Higher values will require larger moves to
    // get across the keyboard
    protected static final float PHYSICAL_WIDTH_CM = 12;
    // If this were a physical keyboard the height in cm. This will be mapped
    // to the height in pixels but is representative of the mapping from the
    // remote input to the screen. Higher values will require larger moves to
    // get across the keyboard
    protected static final float PHYSICAL_HEIGHT_CM = 5;

    /**
     * Listener for publishing voice input result to {@link LeanbackKeyboardController}
     */
    public static interface VoiceListener {
        public void onVoiceResult(String result);
    }

    public static interface DismissListener {
        public void onDismiss(boolean fromVoice);
    }

    /**
     * Class for holding information about the currently focused key.
     */
    public static class KeyFocus {
        public static final int TYPE_INVALID = -1;
        public static final int TYPE_MAIN = 0;
        public static final int TYPE_VOICE = 1;
        public static final int TYPE_ACTION = 2;
        public static final int TYPE_SUGGESTION = 3;

        /**
         * The bounding box for the current focused key/view
         */
        final Rect rect;

        /**
         * The index of the focused key or suggestion. This is invalid for views that don't have
         * indexed items.
         */
        int index;

        /**
         * The type of key which indicates which view/keyboard the focus is in.
         */
        int type;

        /**
         * The key code for the focused key. This is invalid for views that don't use key codes.
         */
        int code;

        /**
         * The text label for the focused key. This is invalid for views that don't use labels.
         */
        CharSequence label;

        public KeyFocus() {
            type = TYPE_INVALID;
            rect = new Rect();
        }

        @Override
        public String toString() {
            StringBuilder bob = new StringBuilder();
            bob.append("[type: ").append(type)
                    .append(", index: ").append(index)
                    .append(", code: ").append(code)
                    .append(", label: ").append(label)
                    .append(", rect: ").append(rect)
                    .append("]");
            return bob.toString();
        }

        public void set(KeyFocus focus) {
            index = focus.index;
            type = focus.type;
            code = focus.code;
            label = focus.label;
            rect.set(focus.rect);
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) {
                return true;
            }
            if (o == null || getClass() != o.getClass()) {
                return false;
            }

            KeyFocus keyFocus = (KeyFocus) o;

            if (code != keyFocus.code) {
                return false;
            }
            if (index != keyFocus.index) {
                return false;
            }
            if (type != keyFocus.type) {
                return false;
            }
            if (label != null ? !label.equals(keyFocus.label) : keyFocus.label != null) {
                return false;
            }
            if (!rect.equals(keyFocus.rect)) {
                return false;
            }

            return true;
        }

        @Override
        public int hashCode() {
            int result = rect.hashCode();
            result = 31 * result + index;
            result = 31 * result + type;
            result = 31 * result + code;
            result = 31 * result + (label != null ? label.hashCode() : 0);
            return result;
        }
    }

    private class VoiceIntroAnimator {
        private AnimatorListener mEnterListener;
        private AnimatorListener mExitListener;
        private ValueAnimator mValueAnimator;

        public VoiceIntroAnimator(AnimatorListener enterListener, AnimatorListener exitListener) {
            mEnterListener = enterListener;
            mExitListener = exitListener;

            mValueAnimator = ValueAnimator.ofFloat(mAlphaOut, mAlphaIn);
            mValueAnimator.setDuration(mVoiceAnimDur);
            mValueAnimator.setInterpolator(new AccelerateInterpolator());
        }

        void startEnterAnimation() {
            if (!isVoiceVisible() && !mValueAnimator.isRunning()) {
                start(true);
            }
        }

        void startExitAnimation() {
            if (isVoiceVisible() && !mValueAnimator.isRunning()) {
                start(false);
            }
        }

        private void start(final boolean enterVoice) {
            // TODO make animation continous
            mValueAnimator.cancel();

            mValueAnimator.removeAllListeners();
            mValueAnimator.addListener(enterVoice ? mEnterListener : mExitListener);
            mValueAnimator.removeAllUpdateListeners();
            mValueAnimator.addUpdateListener(new AnimatorUpdateListener() {

                @Override
                public void onAnimationUpdate(ValueAnimator animation) {
                    float progress = (Float) mValueAnimator.getAnimatedValue();
                    float antiProgress = mAlphaIn + mAlphaOut - progress;

                    float kbAlpha = enterVoice ? antiProgress : progress;
                    float voiceAlpha = enterVoice ? progress : antiProgress;

                    mMainKeyboardView.setAlpha(kbAlpha);
                    mActionButtonView.setAlpha(kbAlpha);
                    mVoiceButtonView.setAlpha(voiceAlpha);

                    if (progress == mAlphaOut) {
                        // first pass
                        if (enterVoice) {
                            mVoiceButtonView.setVisibility(View.VISIBLE);
                        } else {
                            mMainKeyboardView.setVisibility(View.VISIBLE);
                            mActionButtonView.setVisibility(View.VISIBLE);
                        }
                    } else if (progress == mAlphaIn) {
                        // done
                        if (enterVoice) {
                            mMainKeyboardView.setVisibility(View.INVISIBLE);
                            mActionButtonView.setVisibility(View.INVISIBLE);
                        } else {
                            mVoiceButtonView.setVisibility(View.INVISIBLE);
                        }
                    }
                }
            });

            mValueAnimator.start();
        }
    }

    /**
     * keyboard flags based on the edittext types
     */
    // if suggestions are enabled and suggestion view is visible
    private boolean mSuggestionsEnabled;
    // if auto entering space after period or suggestions is enabled
    private boolean mAutoEnterSpaceEnabled;
    // if voice button is enabled
    private boolean mVoiceEnabled;
    // initial main keyboard to show for the specific edittext
    private Keyboard mInitialMainKeyboard;
    // text resource id of the enter key. If set to 0, show enter key image
    private int mEnterKeyTextResId;
    private CharSequence mEnterKeyText;

    /**
     * This animator controls the way the touch indicator grows and shrinks when changing states.
     */
    private ValueAnimator mSelectorAnimator;

    /**
     * The current state of touch.
     */
    private int mTouchState = TOUCH_STATE_NO_TOUCH;

    private VoiceListener mVoiceListener;

    private DismissListener mDismissListener;

    private LeanbackImeService mContext;
    private RelativeLayout mRootView;

    private View mKeyboardsContainer;
    private View mSuggestionsBg;
    private HorizontalScrollView mSuggestionsContainer;
    private LinearLayout mSuggestions;
    private LeanbackKeyboardView mMainKeyboardView;
    private Button mActionButtonView;
    private ScaleAnimation mSelectorAnimation;
    private View mSelector;
    private float mOverestimate;

    // The modeled physical position of the current selection in cm
    private PointF mPhysicalSelectPos = new PointF(2, .5f);
    // The position of the touch indicator in cm
    private PointF mPhysicalTouchPos = new PointF(2, .5f);
    // A point for doing temporary calculations
    private PointF mTempPoint = new PointF();

    private KeyFocus mCurrKeyInfo = new KeyFocus();
    private KeyFocus mDownKeyInfo = new KeyFocus();
    private KeyFocus mTempKeyInfo = new KeyFocus();

    private LeanbackKeyboardView mPrevView;
    private Rect mRect = new Rect();
    private Float mX;
    private Float mY;
    private int mMiniKbKeyIndex;

    private final int mClickAnimDur;
    private final int mVoiceAnimDur;
    private final float mAlphaIn;
    private final float mAlphaOut;

    private Keyboard mAbcKeyboard;
    private Keyboard mSymKeyboard;
    private Keyboard mNumKeyboard;

    // if we should capitalize the first letter in each sentence
    private boolean mCapSentences;

    // if we should capitalize the first letter in each word
    private boolean mCapWords;

    // if we should capitalize every character
    private boolean mCapCharacters;

    // if voice is on
    private boolean mVoiceOn;

    // Whether to allow escaping north or not
    private boolean mEscapeNorthEnabled;

    // Whether to dismiss when voice button is pressed
    private boolean mVoiceKeyDismissesEnabled;

    /**
     * Voice
     */
    private Intent mRecognizerIntent;
    private SpeechRecognizer mSpeechRecognizer;
    private SpeechLevelSource mSpeechLevelSource;
    private RecognizerView mVoiceButtonView;

    private class ScaleAnimation extends Animation {
        private final ViewGroup.LayoutParams mParams;
        private final View mView;
        private float mStartX;
        private float mStartY;
        private float mStartWidth;
        private float mStartHeight;
        private float mEndX;
        private float mEndY;
        private float mEndWidth;
        private float mEndHeight;

        public ScaleAnimation(FrameLayout view) {
            mView = view;
            mParams = view.getLayoutParams();
            setDuration(MOVEMENT_ANIMATION_DURATION);
            setInterpolator(sMovementInterpolator);
        }

        public void setAnimationBounds(float x, float y, float width, float height) {
            mEndX = x;
            mEndY = y;
            mEndWidth = width;
            mEndHeight = height;
        }

        @Override
        protected void applyTransformation(float interpolatedTime, Transformation t) {
            if (interpolatedTime == 0) {
                mStartX = mView.getX();
                mStartY = mView.getY();
                mStartWidth = mParams.width;
                mStartHeight = mParams.height;
            } else {
                setValues(((mEndX - mStartX) * interpolatedTime + mStartX),
                    ((mEndY - mStartY) * interpolatedTime + mStartY),
                    ((int)((mEndWidth - mStartWidth) * interpolatedTime + mStartWidth)),
                    ((int)((mEndHeight - mStartHeight) * interpolatedTime + mStartHeight)));
            }
        }

        public void setValues(float x, float y, float width, float height) {
            mView.setX(x);
            mView.setY(y);
            mParams.width = (int)(width);
            mParams.height = (int)(height);
            mView.setLayoutParams(mParams);
            mView.requestLayout();
        }
    };

    private AnimatorListener mVoiceEnterListener = new AnimatorListener() {

        @Override
        public void onAnimationStart(Animator animation) {
            mSelector.setVisibility(View.INVISIBLE);
            startRecognition(mContext);
        }

        @Override
        public void onAnimationRepeat(Animator animation) {
        }

        @Override
        public void onAnimationEnd(Animator animation) {
        }

        @Override
        public void onAnimationCancel(Animator animation) {
        }
    };

    private AnimatorListener mVoiceExitListener = new AnimatorListener() {

        @Override
        public void onAnimationStart(Animator animation) {
            mVoiceButtonView.showNotListening();
            mSpeechRecognizer.cancel();
            mSpeechRecognizer.setRecognitionListener(null);
            mVoiceOn = false;
        }

        @Override
        public void onAnimationRepeat(Animator animation) {
        }

        @Override
        public void onAnimationEnd(Animator animation) {
            mSelector.setVisibility(View.VISIBLE);
        }

        @Override
        public void onAnimationCancel(Animator animation) {
        }
    };

    private final VoiceIntroAnimator mVoiceAnimator;

    // Tracks whether or not a touch event is in progress. This is true while
    // a finger is down on the pad.
    private boolean mTouchDown = false;

    public LeanbackKeyboardContainer(Context context) {
        mContext = (LeanbackImeService) context;

        final Resources res = mContext.getResources();
        mVoiceAnimDur = res.getInteger(R.integer.voice_anim_duration);
        mAlphaIn = res.getFraction(R.fraction.alpha_in, 1, 1);
        mAlphaOut = res.getFraction(R.fraction.alpha_out, 1, 1);

        mVoiceAnimator = new VoiceIntroAnimator(mVoiceEnterListener, mVoiceExitListener);

        initKeyboards();

        mRootView = (RelativeLayout) mContext.getLayoutInflater()
                .inflate(R.layout.root_leanback, null);
        mKeyboardsContainer = mRootView.findViewById(R.id.keyboard);
        mSuggestionsBg = mRootView.findViewById(R.id.candidate_background);
        mSuggestionsContainer =
                (HorizontalScrollView) mRootView.findViewById(R.id.suggestions_container);
        mSuggestions = (LinearLayout) mSuggestionsContainer.findViewById(R.id.suggestions);

        mMainKeyboardView = (LeanbackKeyboardView) mRootView.findViewById(R.id.main_keyboard);
        mVoiceButtonView = (RecognizerView) mRootView.findViewById(R.id.voice);

        mActionButtonView = (Button) mRootView.findViewById(R.id.enter);

        mSelector = mRootView.findViewById(R.id.selector);
        mSelectorAnimation = new ScaleAnimation((FrameLayout) mSelector);

        mOverestimate = mContext.getResources().getFraction(R.fraction.focused_scale, 1, 1);
        float scale = context.getResources().getFraction(R.fraction.clicked_scale, 1, 1);
        mClickAnimDur = context.getResources().getInteger(R.integer.clicked_anim_duration);
        mSelectorAnimator = ValueAnimator.ofFloat(1.0f, scale);
        mSelectorAnimator.setDuration(mClickAnimDur);
        mSelectorAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animation) {
                float scale = (Float) animation.getAnimatedValue();
                mSelector.setScaleX(scale);
                mSelector.setScaleY(scale);
            }
        });

        mSpeechLevelSource = new SpeechLevelSource();
        mVoiceButtonView.setSpeechLevelSource(mSpeechLevelSource);
        mSpeechRecognizer = SpeechRecognizer.createSpeechRecognizer(mContext);
        mVoiceButtonView.setCallback(new RecognizerView.Callback() {
            @Override
            public void onStartRecordingClicked() {
                startVoiceRecording();
            }

            @Override
            public void onStopRecordingClicked() {
                cancelVoiceRecording();
            }

            @Override
            public void onCancelRecordingClicked() {
                cancelVoiceRecording();
            }
        });

    }

    public void startVoiceRecording() {
        if (mVoiceEnabled) {
            if (mVoiceKeyDismissesEnabled) {
                if (DEBUG) Log.v(TAG, "Voice Dismiss");
                mDismissListener.onDismiss(true);
            } else {
                mVoiceAnimator.startEnterAnimation();
            }
        }
    }

    public void cancelVoiceRecording() {
        mVoiceAnimator.startExitAnimation();
    }

    public void resetVoice() {
        mMainKeyboardView.setAlpha(mAlphaIn);
        mActionButtonView.setAlpha(mAlphaIn);
        mVoiceButtonView.setAlpha(mAlphaOut);

        mMainKeyboardView.setVisibility(View.VISIBLE);
        mActionButtonView.setVisibility(View.VISIBLE);
        mVoiceButtonView.setVisibility(View.INVISIBLE);
    }

    public boolean isVoiceVisible() {
        return mVoiceButtonView.getVisibility() == View.VISIBLE;
    }

    private void initKeyboards() {
        Locale locale = Locale.getDefault();

        if (isMatch(locale, LeanbackLocales.QWERTY_GB)) {
            mAbcKeyboard = new Keyboard(mContext, R.xml.qwerty_en_gb);
            mSymKeyboard = new Keyboard(mContext, R.xml.sym_en_gb);
        } else if (isMatch(locale, LeanbackLocales.QWERTY_IN)) {
            mAbcKeyboard = new Keyboard(mContext, R.xml.qwerty_en_in);
            mSymKeyboard = new Keyboard(mContext, R.xml.sym_en_in);
        } else if (isMatch(locale, LeanbackLocales.QWERTY_ES_EU)) {
            mAbcKeyboard = new Keyboard(mContext, R.xml.qwerty_es_eu);
            mSymKeyboard = new Keyboard(mContext, R.xml.sym_eu);
        } else if (isMatch(locale, LeanbackLocales.QWERTY_ES_US)) {
            mAbcKeyboard = new Keyboard(mContext, R.xml.qwerty_es_us);
            mSymKeyboard = new Keyboard(mContext, R.xml.sym_us);
        } else if (isMatch(locale, LeanbackLocales.QWERTY_AZ)) {
            mAbcKeyboard = new Keyboard(mContext, R.xml.qwerty_az);
            mSymKeyboard = new Keyboard(mContext, R.xml.sym_eu);
        } else if (isMatch(locale, LeanbackLocales.QWERTY_CA)) {
            mAbcKeyboard = new Keyboard(mContext, R.xml.qwerty_ca);
            mSymKeyboard = new Keyboard(mContext, R.xml.sym_eu);
        } else if (isMatch(locale, LeanbackLocales.QWERTY_DA)) {
            mAbcKeyboard = new Keyboard(mContext, R.xml.qwerty_da);
            mSymKeyboard = new Keyboard(mContext, R.xml.sym_eu);
        } else if (isMatch(locale, LeanbackLocales.QWERTY_ET)) {
            mAbcKeyboard = new Keyboard(mContext, R.xml.qwerty_et);
            mSymKeyboard = new Keyboard(mContext, R.xml.sym_eu);
        } else if (isMatch(locale, LeanbackLocales.QWERTY_FI)) {
            mAbcKeyboard = new Keyboard(mContext, R.xml.qwerty_fi);
            mSymKeyboard = new Keyboard(mContext, R.xml.sym_eu);
        } else if (isMatch(locale, LeanbackLocales.QWERTY_NB)) {
            // in the LatinIME nb uses the US symbols (usd instead of euro)
            mAbcKeyboard = new Keyboard(mContext, R.xml.qwerty_nb);
            mSymKeyboard = new Keyboard(mContext, R.xml.sym_us);
        } else if (isMatch(locale, LeanbackLocales.QWERTY_SV)) {
            mAbcKeyboard = new Keyboard(mContext, R.xml.qwerty_sv);
            mSymKeyboard = new Keyboard(mContext, R.xml.sym_eu);
        } else if (isMatch(locale, LeanbackLocales.QWERTY_US)) {
            mAbcKeyboard = new Keyboard(mContext, R.xml.qwerty_us);
            mSymKeyboard = new Keyboard(mContext, R.xml.sym_us);
        } else if (isMatch(locale, LeanbackLocales.QWERTZ_CH)) {
            mAbcKeyboard = new Keyboard(mContext, R.xml.qwertz_ch);
            mSymKeyboard = new Keyboard(mContext, R.xml.sym_eu);
        } else if (isMatch(locale, LeanbackLocales.QWERTZ)) {
            mAbcKeyboard = new Keyboard(mContext, R.xml.qwertz);
            mSymKeyboard = new Keyboard(mContext, R.xml.sym_eu);
        } else if (isMatch(locale, LeanbackLocales.AZERTY)) {
            mAbcKeyboard = new Keyboard(mContext, R.xml.azerty);
            mSymKeyboard = new Keyboard(mContext, R.xml.sym_azerty);
        } else {
            mAbcKeyboard = new Keyboard(mContext, R.xml.qwerty_eu);
            mSymKeyboard = new Keyboard(mContext, R.xml.sym_eu);
        }

        mNumKeyboard = new Keyboard(mContext, R.xml.number);
    }

    private boolean isMatch(Locale locale, Locale[] list) {
        for (Locale compare : list) {
            // comparison language is either blank or they match
            if (TextUtils.isEmpty(compare.getLanguage()) ||
                    TextUtils.equals(locale.getLanguage(), compare.getLanguage())) {
                // comparison country is either blank or they match
                if (TextUtils.isEmpty(compare.getCountry()) ||
                            TextUtils.equals(locale.getCountry(), compare.getCountry())) {
                    return true;
                }
            }
        }

        return false;
    }

    /**
     * This method is called when we start the input at a NEW input field to set up the IME options,
     * such as suggestions, voice, and action
     */
    public void onStartInput(EditorInfo attribute) {
        setImeOptions(mContext.getResources(), attribute);
        mVoiceOn = false;
    }

    /**
     * This method is called whenever we bring up the IME at an input field.
     */
    public void onStartInputView() {
        // This must be done here because modifying the views before it is
        // shown can cause selection handles to be shown if using a USB
        // keyboard in a WebView.
        clearSuggestions();

        RelativeLayout.LayoutParams lp =
                (RelativeLayout.LayoutParams) mKeyboardsContainer.getLayoutParams();
        if (mSuggestionsEnabled) {
            lp.removeRule(RelativeLayout.ALIGN_PARENT_TOP);
            mSuggestionsContainer.setVisibility(View.VISIBLE);
            mSuggestionsBg.setVisibility(View.VISIBLE);
        } else {
            lp.addRule(RelativeLayout.ALIGN_PARENT_TOP);
            mSuggestionsContainer.setVisibility(View.GONE);
            mSuggestionsBg.setVisibility(View.GONE);
        }
        mKeyboardsContainer.setLayoutParams(lp);

        mMainKeyboardView.setKeyboard(mInitialMainKeyboard);
        // TODO fix this for number keyboard
        mVoiceButtonView.setMicEnabled(mVoiceEnabled);
        resetVoice();
        dismissMiniKeyboard();

        // setImeOptions will be called before this, setting the text resource value
        if (!TextUtils.isEmpty(mEnterKeyText)) {
            mActionButtonView.setText(mEnterKeyText);
            mActionButtonView.setContentDescription(mEnterKeyText);
        } else {
            mActionButtonView.setText(mEnterKeyTextResId);
            mActionButtonView.setContentDescription(mContext.getString(mEnterKeyTextResId));
        }

        if (mCapCharacters) {
            setShiftState(LeanbackKeyboardView.SHIFT_LOCKED);
        } else if (mCapSentences || mCapWords) {
            setShiftState(LeanbackKeyboardView.SHIFT_ON);
        } else {
            setShiftState(LeanbackKeyboardView.SHIFT_OFF);
        }
    }

    /**
     * This method is called when the keyboard layout is complete, to set up the initial focus and
     * visibility. This method gets called later than {@link onStartInput} and
     * {@link onStartInputView}.
     */
    public void onInitInputView() {
        resetFocusCursor();
        mSelector.setVisibility(View.VISIBLE);
    }

    public RelativeLayout getView() {
        return mRootView;
    }

    public void setVoiceListener(VoiceListener listener) {
        mVoiceListener = listener;
    }

    public void setDismissListener(DismissListener listener) {
        mDismissListener = listener;
    }

    private void setImeOptions(Resources resources, EditorInfo attribute) {
        mSuggestionsEnabled = true;
        mAutoEnterSpaceEnabled = true;
        mVoiceEnabled = true;
        mInitialMainKeyboard = mAbcKeyboard;
        mEscapeNorthEnabled = false;
        mVoiceKeyDismissesEnabled = false;

        // set keyboard properties
        switch (LeanbackUtils.getInputTypeClass(attribute)) {
            case EditorInfo.TYPE_CLASS_NUMBER:
            case EditorInfo.TYPE_CLASS_DATETIME:
            case EditorInfo.TYPE_CLASS_PHONE:
                mSuggestionsEnabled = false;
                mVoiceEnabled = false;
                // TODO use number keyboard for these input types
                mInitialMainKeyboard = mAbcKeyboard;
                break;
            case EditorInfo.TYPE_CLASS_TEXT:
                switch (LeanbackUtils.getInputTypeVariation(attribute)) {
                    case EditorInfo.TYPE_TEXT_VARIATION_PASSWORD:
                    case EditorInfo.TYPE_TEXT_VARIATION_WEB_PASSWORD:
                    case EditorInfo.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD:
                    case EditorInfo.TYPE_TEXT_VARIATION_PERSON_NAME:
                        mSuggestionsEnabled = false;
                        mVoiceEnabled = false;
                        mInitialMainKeyboard = mAbcKeyboard;
                        break;
                    case EditorInfo.TYPE_TEXT_VARIATION_EMAIL_ADDRESS:
                    case EditorInfo.TYPE_TEXT_VARIATION_URI:
                    case EditorInfo.TYPE_TEXT_VARIATION_WEB_EDIT_TEXT:
                    case EditorInfo.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS:
                        mSuggestionsEnabled = true;
                        mAutoEnterSpaceEnabled = false;
                        mVoiceEnabled = false;
                        mInitialMainKeyboard = mAbcKeyboard;
                        break;
                }
                break;
        }

        if (mSuggestionsEnabled) {
            mSuggestionsEnabled = (attribute.inputType
                    & EditorInfo.TYPE_TEXT_FLAG_NO_SUGGESTIONS) == 0;
        }
        if (mAutoEnterSpaceEnabled) {
            mAutoEnterSpaceEnabled = mSuggestionsEnabled && mAutoEnterSpaceEnabled;
        }
        mCapSentences = (attribute.inputType
                & EditorInfo.TYPE_TEXT_FLAG_CAP_SENTENCES) != 0;
        mCapWords = ((attribute.inputType & EditorInfo.TYPE_TEXT_FLAG_CAP_WORDS) != 0) ||
                (LeanbackUtils.getInputTypeVariation(attribute)
                    == EditorInfo.TYPE_TEXT_VARIATION_PERSON_NAME);
        mCapCharacters = (attribute.inputType
                & EditorInfo.TYPE_TEXT_FLAG_CAP_CHARACTERS) != 0;

        if (attribute.privateImeOptions != null) {
            if (attribute.privateImeOptions.contains(IME_PRIVATE_OPTIONS_ESCAPE_NORTH) ||
                    attribute.privateImeOptions.contains(
                            IME_PRIVATE_OPTIONS_ESCAPE_NORTH_LEGACY)) {
                mEscapeNorthEnabled = true;
            }
            if (attribute.privateImeOptions.contains(IME_PRIVATE_OPTIONS_VOICE_DISMISS) ||
                    attribute.privateImeOptions.contains(
                            IME_PRIVATE_OPTIONS_VOICE_DISMISS_LEGACY)) {
                mVoiceKeyDismissesEnabled = true;
            }
        }

        if (DEBUG) {
            Log.d(TAG, "sugg: " + mSuggestionsEnabled + " | capSentences: " + mCapSentences
                    + " | capWords: " + mCapWords + " | capChar: " + mCapCharacters
                    + " | escapeNorth: " + mEscapeNorthEnabled
                    + " | voiceDismiss : " + mVoiceKeyDismissesEnabled
            );
        }

        // set enter key
        mEnterKeyText = attribute.actionLabel;
        if (TextUtils.isEmpty(mEnterKeyText)) {
            switch (LeanbackUtils.getImeAction(attribute)) {
                case EditorInfo.IME_ACTION_GO:
                    mEnterKeyTextResId = R.string.label_go_key;
                    break;
                case EditorInfo.IME_ACTION_NEXT:
                    mEnterKeyTextResId = R.string.label_next_key;
                    break;
                case EditorInfo.IME_ACTION_SEARCH:
                    mEnterKeyTextResId = R.string.label_search_key;
                    break;
                case EditorInfo.IME_ACTION_SEND:
                    mEnterKeyTextResId = R.string.label_send_key;
                    break;
                default:
                    mEnterKeyTextResId = R.string.label_done_key;
                    break;
            }
        }

        if (!VOICE_SUPPORTED) {
            mVoiceEnabled = false;
        }
    }

    public boolean isVoiceEnabled() {
        return mVoiceEnabled;
    }

    public boolean areSuggestionsEnabled() {
        return mSuggestionsEnabled;
    }

    public boolean enableAutoEnterSpace() {
        return mAutoEnterSpaceEnabled;
    }

    private PointF getAlignmentPosition(float posXCm, float posYCm, PointF result) {
        float width = mRootView.getWidth() - mRootView.getPaddingRight()
                - mRootView.getPaddingLeft()
                - mContext.getResources().getDimension(R.dimen.selector_size);
        float height = mRootView.getHeight() - mRootView.getPaddingTop()
                - mRootView.getPaddingBottom()
                - mContext.getResources().getDimension(R.dimen.selector_size);
        result.x = posXCm / PHYSICAL_WIDTH_CM * width + mRootView.getPaddingLeft();
        result.y = posYCm / PHYSICAL_HEIGHT_CM * height + mRootView.getPaddingTop();
        return result;
    }

    private void getPhysicalPosition(float x, float y, PointF result) {
        x -= mSelector.getWidth() / 2;
        y -= mSelector.getHeight() / 2;
        float width = mRootView.getWidth() - mRootView.getPaddingRight()
                - mRootView.getPaddingLeft()
                - mContext.getResources().getDimension(R.dimen.selector_size);
        float height = mRootView.getHeight() - mRootView.getPaddingTop()
                - mRootView.getPaddingBottom()
                - mContext.getResources().getDimension(R.dimen.selector_size);
        float posXCm = (x - mRootView.getPaddingLeft()) * PHYSICAL_WIDTH_CM / width;
        float posYCm = (y - mRootView.getPaddingTop()) * PHYSICAL_HEIGHT_CM / height;
        result.x = posXCm;
        result.y = posYCm;
    }

    private void offsetRect(Rect rect, View view) {
        rect.left = 0;
        rect.top = 0;
        rect.right = view.getWidth();
        rect.bottom = view.getHeight();
        ((ViewGroup) mRootView).offsetDescendantRectToMyCoords(view, rect);
    }

    /**
     * Finds the {@link KeyFocus} on screen that best matches the given pixel positions
     *
     * @param x position in pixels, if null, use the last valid x value
     * @param y position in pixels, if null, use the last valid y value
     * @param focus the focus object to update with the result
     * @return true if focus was successfully found, false otherwise.
    */
    public boolean getBestFocus(Float x, Float y, KeyFocus focus) {
        boolean validFocus = true;

        offsetRect(mRect, mActionButtonView);
        int actionLeft = mRect.left;

        offsetRect(mRect, mMainKeyboardView);
        int keyboardTop = mRect.top;

        // use last if invalid
        x = (x == null) ? mX : x;
        y = (y == null) ? mY : y;

        final int count = mSuggestions.getChildCount();
        if (y < keyboardTop && count > 0 && mSuggestionsEnabled) {
            for (int i = 0; i < count; i++) {
                View suggestView = mSuggestions.getChildAt(i);
                offsetRect(mRect, suggestView);
                if (x < mRect.right || i+1 == count) {
                    suggestView.requestFocus();
                    LeanbackUtils.sendAccessibilityEvent(suggestView.findViewById(R.id.text), true);
                    configureFocus(focus, mRect, i, KeyFocus.TYPE_SUGGESTION);
                    break;
                }
            }
        } else if (y < keyboardTop && mEscapeNorthEnabled) {
            validFocus = false;
            escapeNorth();
        } else if (x > actionLeft) {
            // closest is the action button
            offsetRect(mRect, mActionButtonView);
            configureFocus(focus, mRect, 0, KeyFocus.TYPE_ACTION);
        } else {
            mX = x;
            mY = y;

            // In the main view
            offsetRect(mRect, mMainKeyboardView);
            x = (x - mRect.left);
            y = (y - mRect.top);

            int index = mMainKeyboardView.getNearestIndex(x, y);
            Key key = mMainKeyboardView.getKey(index);
            configureFocus(focus, mRect, index, key, KeyFocus.TYPE_MAIN);
        }

        return validFocus;
    }

    private void escapeNorth() {
        if (DEBUG) Log.v(TAG, "Escaping north");
        mDismissListener.onDismiss(false);
    }

    private void configureFocus(KeyFocus focus, Rect rect, int index, int type) {
        focus.type = type;
        focus.index = index;
        focus.rect.set(rect);
    }

    private void configureFocus(KeyFocus focus, Rect rect, int index, Key key, int type) {
        focus.type = type;
        if (key == null) {
            return;
        }
        if (key.codes != null) {
            focus.code = key.codes[0];
        } else {
            focus.code = KeyEvent.KEYCODE_UNKNOWN;
        }
        focus.index = index;
        focus.label = key.label;
        focus.rect.left = key.x + rect.left;
        focus.rect.top = key.y + rect.top;
        focus.rect.right = focus.rect.left + key.width;
        focus.rect.bottom = focus.rect.top + key.height;
    }

    private void setKbFocus(KeyFocus focus, boolean forceFocusChange, boolean animate) {
        if (focus.equals(mCurrKeyInfo) && !forceFocusChange) {
            // Nothing changed
            return;
        }
        LeanbackKeyboardView prevView = mPrevView;
        mPrevView = null;
        boolean overestimateWidth = false;
        boolean overestimateHeight = false;

        switch (focus.type) {
            case KeyFocus.TYPE_VOICE:
                mVoiceButtonView.setMicFocused(true);
                dismissMiniKeyboard();
                break;
            case KeyFocus.TYPE_ACTION:
                LeanbackUtils.sendAccessibilityEvent(mActionButtonView, true);
                dismissMiniKeyboard();
                break;
            case KeyFocus.TYPE_SUGGESTION:
                dismissMiniKeyboard();
                break;
            case KeyFocus.TYPE_MAIN:
                overestimateHeight = true;
                overestimateWidth = (focus.code != LeanbackKeyboardView.ASCII_SPACE);
                mMainKeyboardView.setFocus(focus.index, mTouchState == TOUCH_STATE_CLICK, overestimateWidth);
                mPrevView = mMainKeyboardView;
                break;
        }

        if (prevView != null && prevView != mPrevView) {
            prevView.setFocus(-1, mTouchState == TOUCH_STATE_CLICK);
        }

        setSelectorToFocus(focus.rect, overestimateWidth, overestimateHeight, animate);
        mCurrKeyInfo.set(focus);
    }

    public void setSelectorToFocus(Rect rect, boolean overestimateWidth, boolean overestimateHeight,
            boolean animate) {
        if (mSelector.getWidth() == 0 || mSelector.getHeight() == 0
                || rect.width() == 0 || rect.height() == 0) {
            return;
        }

        float width = rect.width();
        float height = rect.height();

        if (overestimateHeight) {
            height *= mOverestimate;
        }
        if (overestimateWidth) {
            width *= mOverestimate;
        }

        float major = Math.max(width, height);
        float minor = Math.min(width, height);
        // if the difference between the width and height is less than 10%,
        // keep the width and height the same.
        if (major / minor < 1.1) {
            width = height = Math.max(width, height);
        }

        float x = rect.exactCenterX() - width/2;
        float y = rect.exactCenterY() - height/2;
        mSelectorAnimation.cancel();
        if (animate) {
            mSelectorAnimation.reset();
            mSelectorAnimation.setAnimationBounds(x, y, width, height);
            mSelector.startAnimation(mSelectorAnimation);
        } else {
            mSelectorAnimation.setValues(x, y, width, height);
        }
    }

    public Keyboard.Key getKey(int type, int index) {
        return (type == KeyFocus.TYPE_MAIN) ? mMainKeyboardView.getKey(index) : null;
    }

    public int getCurrKeyCode() {
        Key key = getKey(mCurrKeyInfo.type, mCurrKeyInfo.index);
        if (key != null) {
            return key.codes[0];
        }
        return 0;
    }

    public int getTouchState() {
        return mTouchState;
    }

    /**
     * Set the view state which affects how the touch indicator is drawn. This code currently
     * assumes the state changes below for simplicity. If the state machine is updated this code
     * should probably be checked to ensure it still works. NO_TOUCH -> on touch start -> SNAP SNAP
     * -> on enough movement -> MOVE MOVE -> on hover long enough -> SNAP SNAP -> on a click down ->
     * CLICK CLICK -> on click released -> SNAP ANY STATE -> on touch end -> NO_TOUCH
     *
     * @param state The new state to transition to
     */
    public void setTouchState(int state) {
        switch (state) {
            case TOUCH_STATE_NO_TOUCH:
                if (mTouchState == TOUCH_STATE_TOUCH_MOVE || mTouchState == TOUCH_STATE_CLICK) {
                    // If the touch indicator was small make it big again
                    mSelectorAnimator.reverse();
                }
                break;
            case TOUCH_STATE_TOUCH_SNAP:
                if (mTouchState == TOUCH_STATE_CLICK) {
                    // And make the touch indicator big again
                    mSelectorAnimator.reverse();
                } else if (mTouchState == TOUCH_STATE_TOUCH_MOVE) {
                    // Just make the touch indicator big
                    mSelectorAnimator.reverse();
                }
                break;
            case TOUCH_STATE_TOUCH_MOVE:
                if (mTouchState == TOUCH_STATE_NO_TOUCH || mTouchState == TOUCH_STATE_TOUCH_SNAP) {
                    // Shrink the touch indicator
                    mSelectorAnimator.start();
                }
                break;
            case TOUCH_STATE_CLICK:
                if (mTouchState == TOUCH_STATE_NO_TOUCH || mTouchState == TOUCH_STATE_TOUCH_SNAP) {
                    // Shrink the touch indicator
                    mSelectorAnimator.start();
                }
                break;
        }
        setTouchStateInternal(state);
        setKbFocus(mCurrKeyInfo, true, true);
    }

    public KeyFocus getCurrFocus() {
        return mCurrKeyInfo;
    }

    public void onVoiceClick() {
        if (mVoiceButtonView != null) {
            mVoiceButtonView.onClick();
        }
    }

    public void onModeChangeClick() {
        dismissMiniKeyboard();
        if (mMainKeyboardView.getKeyboard().equals(mSymKeyboard)) {
            mMainKeyboardView.setKeyboard(mAbcKeyboard);
        } else {
            mMainKeyboardView.setKeyboard(mSymKeyboard);
        }
    }

    public void onShiftClick() {
        setShiftState(mMainKeyboardView.isShifted() ? LeanbackKeyboardView.SHIFT_OFF
                : LeanbackKeyboardView.SHIFT_ON);
    }

    public void onTextEntry() {
        // reset shift if caps is not on
        if (mMainKeyboardView.isShifted()) {
            if (!isCapsLockOn() && !mCapCharacters) {
                setShiftState(LeanbackKeyboardView.SHIFT_OFF);
            }
        } else {
            if (isCapsLockOn() || mCapCharacters) {
                setShiftState(LeanbackKeyboardView.SHIFT_LOCKED);
            }
        }

        if (dismissMiniKeyboard()) {
            moveFocusToIndex(mMiniKbKeyIndex, KeyFocus.TYPE_MAIN);
        }
    }

    public void onSpaceEntry() {
        if (mMainKeyboardView.isShifted()) {
            if (!isCapsLockOn() && !mCapCharacters && !mCapWords) {
                setShiftState(LeanbackKeyboardView.SHIFT_OFF);
            }
        } else {
            if (isCapsLockOn() || mCapCharacters || mCapWords) {
                setShiftState(LeanbackKeyboardView.SHIFT_ON);
            }
        }
    }

    public void onPeriodEntry() {
        if (mMainKeyboardView.isShifted()) {
            if (!isCapsLockOn() && !mCapCharacters && !mCapWords && !mCapSentences) {
                setShiftState(LeanbackKeyboardView.SHIFT_OFF);
            }
        } else {
            if (isCapsLockOn() || mCapCharacters || mCapWords || mCapSentences) {
                setShiftState(LeanbackKeyboardView.SHIFT_ON);
            }
        }
    }

    public boolean dismissMiniKeyboard() {
        return mMainKeyboardView.dismissMiniKeyboard();
    }

    public boolean isCurrKeyShifted() {
        return mMainKeyboardView.isShifted();
    }

    public CharSequence getSuggestionText(int index) {
        CharSequence text = null;

        if(index >= 0 && index < mSuggestions.getChildCount()){
            Button suggestion =
                    (Button) mSuggestions.getChildAt(index).findViewById(R.id.text);
            if (suggestion != null) {
                text = suggestion.getText();
            }
        }

        return text;
    }

    /**
     * This method sets the keyboard focus and update the layout of the new focus
     *
     * @param focus the new focus of the keyboard
     */
    public void setFocus(KeyFocus focus) {
        setKbFocus(focus, false, true);
    }

    public boolean getNextFocusInDirection(int direction, KeyFocus startFocus, KeyFocus nextFocus) {
        boolean validNextFocus = true;

        switch (startFocus.type) {
            case KeyFocus.TYPE_VOICE:
                // TODO move between voice button and kb button
                break;
            case KeyFocus.TYPE_ACTION:
                offsetRect(mRect, mMainKeyboardView);
                if ((direction & DIRECTION_LEFT) != 0) {
                    // y is null, so we use the last y.  This way a user can hold left and wrap
                    // around the keyboard while staying in the same row
                    validNextFocus = getBestFocus((float) mRect.right, null, nextFocus);
                } else if ((direction & DIRECTION_UP) != 0) {
                    offsetRect(mRect, mSuggestions);
                    validNextFocus = getBestFocus(
                            (float) startFocus.rect.centerX(), (float) mRect.centerY(), nextFocus);
                }
                break;
            case KeyFocus.TYPE_SUGGESTION:
                if ((direction & DIRECTION_DOWN) != 0) {
                    offsetRect(mRect, mMainKeyboardView);
                    validNextFocus = getBestFocus(
                            (float) startFocus.rect.centerX(), (float) mRect.top, nextFocus);
                } else if ((direction & DIRECTION_UP) != 0) {
                    if (mEscapeNorthEnabled) {
                        escapeNorth();
                    }
                } else {
                    boolean left = (direction & DIRECTION_LEFT) != 0;
                    boolean right = (direction & DIRECTION_RIGHT) != 0;

                    if (left || right) {
                        // Cannot offset on the suggestion container because as it scrolls those
                        // values change
                        offsetRect(mRect, mRootView);
                        MarginLayoutParams lp =
                                (MarginLayoutParams) mSuggestionsContainer.getLayoutParams();
                        int leftSide = mRect.left + lp.leftMargin;
                        int rightSide = mRect.right - lp.rightMargin;
                        int index = startFocus.index + (left ? -1 : 1);

                        View suggestView = mSuggestions.getChildAt(index);
                        if (suggestView != null) {
                            offsetRect(mRect, suggestView);

                            if (mRect.left < leftSide && mRect.right > rightSide) {
                                mRect.left = leftSide;
                                mRect.right = rightSide;
                            } else if (mRect.left < leftSide) {
                                mRect.right = leftSide + mRect.width();
                                mRect.left = leftSide;
                            } else if (mRect.right > rightSide) {
                                mRect.left = rightSide - mRect.width();
                                mRect.right = rightSide;
                            }

                            suggestView.requestFocus();
                            LeanbackUtils.sendAccessibilityEvent(
                                    suggestView.findViewById(R.id.text), true);
                            configureFocus(nextFocus, mRect, index, KeyFocus.TYPE_SUGGESTION);
                        }
                    }
                }
                break;
            case KeyFocus.TYPE_MAIN:
                Key key = getKey(startFocus.type, startFocus.index);
                // Step within the view.  Using height because all keys are the same height
                // and widths vary.  Half the height is to ensure the next key is reached
                float extraSlide = startFocus.rect.height()/2.0f;
                float x = startFocus.rect.centerX();
                float y = startFocus.rect.centerY();
                if (startFocus.code == LeanbackKeyboardView.ASCII_SPACE) {
                    // if we're moving off of space, use the old x position for memory
                    x = mX;
                }
                if ((direction & DIRECTION_LEFT) != 0) {
                    if ((key.edgeFlags & Keyboard.EDGE_LEFT) == 0) {
                        // not on the left edge of the kb
                        x = startFocus.rect.left - extraSlide;
                    }
                } else if ((direction & DIRECTION_RIGHT) != 0) {
                    if ((key.edgeFlags & Keyboard.EDGE_RIGHT) != 0) {
                        // jump to the action button
                        offsetRect(mRect, mActionButtonView);
                        x = mRect.centerX();
                    } else {
                        x = startFocus.rect.right + extraSlide;
                    }
                }
                // Don't need any special handling for up/down due to
                // layout positioning. If the layout changes this should be
                // reconsidered.
                if ((direction & DIRECTION_UP) != 0) {
                    y -= startFocus.rect.height() * DIRECTION_STEP_MULTIPLIER;
                } else if ((direction & DIRECTION_DOWN) != 0) {
                    y += startFocus.rect.height() * DIRECTION_STEP_MULTIPLIER;
                }
                getPhysicalPosition(x, y, mTempPoint);
                validNextFocus = getBestFocus(x, y, nextFocus);
                break;
        }

        return validNextFocus;
    }

    private PointF getTouchSnapPosition() {
        PointF snapPos = new PointF();
        getPhysicalPosition(mCurrKeyInfo.rect.centerX(), mCurrKeyInfo.rect.centerY(), snapPos);
        return snapPos;
    }

    public void clearSuggestions() {
        mSuggestions.removeAllViews();

        if (getCurrFocus().type == KeyFocus.TYPE_SUGGESTION) {
            resetFocusCursor();
        }
    }

    public void updateSuggestions(ArrayList<String> suggestions) {
        final int oldCount = mSuggestions.getChildCount();
        final int newCount = suggestions.size();

        if (newCount < oldCount) {
            // remove excess views
            mSuggestions.removeViews(newCount, oldCount-newCount);
        } else if (newCount > oldCount) {
            // add more
            for (int i = oldCount; i < newCount; i++) {
                View suggestion =  mContext.getLayoutInflater()
                        .inflate(R.layout.candidate, null);
                mSuggestions.addView(suggestion);
            }
        }

        for (int i = 0; i < newCount; i++) {
            Button suggestion =
                    (Button) mSuggestions.getChildAt(i).findViewById(R.id.text);
            suggestion.setText(suggestions.get(i));
            suggestion.setContentDescription(suggestions.get(i));
        }

        if (getCurrFocus().type == KeyFocus.TYPE_SUGGESTION) {
            resetFocusCursor();
        }
    }

    /**
     * Moves the selector back to the entry point key (T in general)
     */
    public void resetFocusCursor() {
        // T is the best starting letter, it's in the 5th column and 2nd row,
        // this approximates that location
        double x = 0.45;
        double y = 0.375;
        offsetRect(mRect, mMainKeyboardView);
        mX = (float)(mRect.left + x*mRect.width());
        mY = (float)(mRect.top + y*mRect.height());
        getBestFocus(mX, mY, mTempKeyInfo);
        setKbFocus(mTempKeyInfo, true, false);

        setTouchStateInternal(TOUCH_STATE_NO_TOUCH);
        mSelectorAnimator.reverse();
        mSelectorAnimator.end();
    }

    private void setTouchStateInternal(int state) {
        mTouchState = state;
    }

    private void setShiftState(int state) {
        mMainKeyboardView.setShiftState(state);
    }

    private void startRecognition(Context context) {
        mRecognizerIntent = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
        mRecognizerIntent.putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL,
                RecognizerIntent.LANGUAGE_MODEL_FREE_FORM);
        mRecognizerIntent.putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true);
        mSpeechRecognizer.setRecognitionListener(new RecognitionListener() {
            float peakRmsLevel = 0;
            int rmsCounter = 0;

            @Override
            public void onBeginningOfSpeech() {
                mVoiceButtonView.showRecording();
            }

            @Override
            public void onEndOfSpeech() {
                mVoiceButtonView.showRecognizing();
                mVoiceOn = false;
            }

            @Override
            public void onError(int error) {
                cancelVoiceRecording();
                switch (error) {
                    case SpeechRecognizer.ERROR_NO_MATCH:
                        Log.d(TAG, "recognizer error no match");
                        break;
                    case SpeechRecognizer.ERROR_SERVER:
                        Log.d(TAG, "recognizer error server error");
                        break;
                    case SpeechRecognizer.ERROR_SPEECH_TIMEOUT:
                        Log.d(TAG, "recognizer error speech timeout");
                        break;
                    case SpeechRecognizer.ERROR_CLIENT:
                        Log.d(TAG, "recognizer error client error");
                        break;
                    default:
                        Log.d(TAG, "recognizer other error " + error);
                        break;
                }
            }

            @Override
            public synchronized void onPartialResults(Bundle partialResults) {
            }

            @Override
            public void onReadyForSpeech(Bundle params) {
                mVoiceButtonView.showListening();
            }

            @Override
            public void onEvent(int eventType, Bundle params) {
            }

            @Override
            public void onBufferReceived(byte[] buffer) {
            }

            @Override
            public synchronized void onRmsChanged(float rmsdB) {

                mVoiceOn = true;
                mSpeechLevelSource.setSpeechLevel((rmsdB < 0) ? 0 : (int) (10 * rmsdB));
                peakRmsLevel = Math.max(rmsdB, peakRmsLevel);
                rmsCounter++;

                if (rmsCounter > 100 && peakRmsLevel == 0) {
                    mVoiceButtonView.showNotListening();
                }
            }

            @Override
            public void onResults(Bundle results) {
                final ArrayList<String> matches =
                        results.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION);
                if (matches != null) {
                    if (mVoiceListener != null) {
                        mVoiceListener.onVoiceResult(matches.get(0));
                    }
                }

                cancelVoiceRecording();
            }
        });
        mSpeechRecognizer.startListening(mRecognizerIntent);
    }

    public boolean isMiniKeyboardOnScreen() {
        return mMainKeyboardView.isMiniKeyboardOnScreen();
    }

    public boolean onKeyLongPress() {
        if (mCurrKeyInfo.code == Keyboard.KEYCODE_SHIFT) {
            onToggleCapsLock();
            setTouchState(TOUCH_STATE_NO_TOUCH);
            return true;
        } else if (mCurrKeyInfo.type == KeyFocus.TYPE_MAIN) {
            mMainKeyboardView.onKeyLongPress();
            if (mMainKeyboardView.isMiniKeyboardOnScreen()) {
                mMiniKbKeyIndex = mCurrKeyInfo.index;
                moveFocusToIndex(mMainKeyboardView.getBaseMiniKbIndex(), KeyFocus.TYPE_MAIN);
                return true;
            }
        }

        return false;
    }

    private void moveFocusToIndex(int index, int type) {
        Key key = mMainKeyboardView.getKey(index);
        configureFocus(mTempKeyInfo, mRect, index, key, type);
        setTouchState(TOUCH_STATE_NO_TOUCH);
        setKbFocus(mTempKeyInfo, true, true);
    }

    private void onToggleCapsLock() {
        onShiftDoubleClick(isCapsLockOn());
    }

    public void onShiftDoubleClick(boolean wasCapsLockOn) {
        setShiftState(
                wasCapsLockOn ? LeanbackKeyboardView.SHIFT_OFF : LeanbackKeyboardView.SHIFT_LOCKED);
    }

    public boolean isCapsLockOn() {
        return mMainKeyboardView.getShiftState() == LeanbackKeyboardView.SHIFT_LOCKED;
    }
}
