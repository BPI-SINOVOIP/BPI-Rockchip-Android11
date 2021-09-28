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

import android.content.Context;

import java.util.ArrayList;

import android.content.res.Resources;
import android.content.res.TypedArray;
import android.content.res.XmlResourceParser;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Paint.Align;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.inputmethodservice.Keyboard;
import android.inputmethodservice.Keyboard.Key;
import android.inputmethodservice.Keyboard.Row;
import android.media.AudioManager;
import android.provider.Settings;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.widget.FrameLayout;
import android.widget.ImageView;

import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

public class LeanbackKeyboardView extends FrameLayout {

    private static final String TAG = "LbKbView";
    private static final boolean DEBUG = false;

    private static final int NOT_A_KEY = -1;

    public static final int SHIFT_OFF = 0;
    public static final int SHIFT_ON = 1;
    public static final int SHIFT_LOCKED = 2;
    private int mShiftState;

    private final float mFocusedScale;
    private final float mClickedScale;
    private final int mClickAnimDur;
    private final int mUnfocusStartDelay;
    private final int mInactiveMiniKbAlpha;

    private Keyboard mKeyboard;
    private KeyHolder[] mKeys;
    private ImageView[] mKeyImageViews;

    private int mFocusIndex;
    private boolean mFocusClicked;
    private View mCurrentFocusView;
    private boolean mMiniKeyboardOnScreen;

    /**
     * Special keycodes
     */
    public static final int ASCII_SPACE = 32;
    public static final int ASCII_PERIOD = 46;
    public static final int KEYCODE_SHIFT = -1;
    public static final int KEYCODE_SYM_TOGGLE = -2;
    public static final int KEYCODE_LEFT = -3;
    public static final int KEYCODE_RIGHT = -4;
    public static final int KEYCODE_DELETE = -5;
    public static final int KEYCODE_CAPS_LOCK = -6;
    public static final int KEYCODE_VOICE = -7;
    public static final int KEYCODE_DISMISS_MINI_KEYBOARD = -8;

    private int mBaseMiniKbIndex = -1;

    private Paint mPaint;
    private Rect mPadding;
    private int mModeChangeTextSize;
    private int mKeyTextSize;
    private int mKeyTextColor;
    private int mRowCount;
    private int mColCount;

    private class KeyHolder {
        public boolean isInMiniKb = false;
        public boolean isInvertible = false;
        public Key key;

        public KeyHolder(Key key) {
            this.key = key;
        }
    }

    public LeanbackKeyboardView(Context context, AttributeSet attrs) {
        super(context, attrs);

        final Resources res = context.getResources();
        TypedArray a = context.getTheme()
                .obtainStyledAttributes(attrs, R.styleable.LeanbackKeyboardView, 0, 0);
        mRowCount = a.getInteger(R.styleable.LeanbackKeyboardView_rowCount, -1);
        mColCount = a.getInteger(R.styleable.LeanbackKeyboardView_columnCount, -1);

        mKeyTextSize = (int) res.getDimension(R.dimen.key_font_size);

        mPaint = new Paint();
        mPaint.setAntiAlias(true);
        mPaint.setTextSize(mKeyTextSize);
        mPaint.setTextAlign(Align.CENTER);
        mPaint.setAlpha(255);

        mPadding = new Rect(0, 0, 0, 0);

        mModeChangeTextSize = (int) res.getDimension(R.dimen.function_key_mode_change_font_size);

        mKeyTextColor = res.getColor(R.color.key_text_default);

        mFocusIndex = -1;

        mShiftState = SHIFT_OFF;

        mFocusedScale = res.getFraction(R.fraction.focused_scale, 1, 1);
        mClickedScale = res.getFraction(R.fraction.clicked_scale, 1, 1);
        mClickAnimDur = res.getInteger(R.integer.clicked_anim_duration);
        mUnfocusStartDelay = res.getInteger(R.integer.unfocused_anim_delay);

        mInactiveMiniKbAlpha = res.getInteger(R.integer.inactive_mini_kb_alpha);
    }

    /**
     * Get the total rows of the keyboard
     */
    public int getRowCount() {
        return mRowCount;
    }

    /**
     * Get the total columns of the keyboard
     */
    public int getColCount() {
        return mColCount;
    }

    /**
     * Get the key at the specified index
     *
     * @param index
     * @return null if the keyboardView has not been assigned a keyboard
     */
    public Key getKey(int index) {
        if (mKeys == null || mKeys.length == 0 || index < 0 || index > mKeys.length) {
            return null;
        }
        return mKeys[index].key;
    }

    /**
     * Get the current focused key
     */
    public Key getFocusedKey() {
        return mFocusIndex == -1 ? null : mKeys[mFocusIndex].key;
    }

    /**
     * Get the keyboard that's attached to the keyboardView
     */
    public Keyboard getKeyboard() {
        return mKeyboard;
    }

    /**
     * Get the key that's the nearest to the given position
     *
     * @param x position in pixels
     * @param y position in pixels
     */
    public int getNearestIndex(float x, float y) {
        if (mKeys == null || mKeys.length == 0) {
            return 0;
        }
        x -= getPaddingLeft();
        y -= getPaddingTop();
        float height = getMeasuredHeight() - getPaddingTop() - getPaddingBottom();
        float width = getMeasuredWidth() - getPaddingLeft() - getPaddingRight();
        int rows = getRowCount();
        int cols = getColCount();
        int row = (int) (y / height * rows);
        if (row < 0) {
            row = 0;
        } else if (row >= rows) {
            row = rows - 1;
        }
        int col = (int) (x / width * cols);
        if (col < 0) {
            col = 0;
        } else if (col >= cols) {
            col = cols - 1;
        }
        int index = mColCount * row + col;

        // at space key (space key is 7 keys wide)
        if (index > 46 && index < 53) {
            index = 46;
        }

        // beyond space, remove 6 extra slots for space
        if (index >= 53) {
            index -= 6;
        }

        if (index < 0) {
            index = 0;
        } else if (index >= mKeys.length) {
            index = mKeys.length - 1;
        }

        return index;
    }

    /**
     * Attaches a keyboard to this view. The keyboard can be switched at any
     * time and the view will re-layout itself to accommodate the keyboard.
     *
     * @see Keyboard
     * @see #getKeyboard()
     * @param keyboard the keyboard to display in this view
     */
    public void setKeyboard(Keyboard keyboard) {
        // Remove any pending messages
        removeMessages();
        mKeyboard = keyboard;
        setKeys(mKeyboard.getKeys());

        // reset shift state
        int shiftState = mShiftState;
        mShiftState = -1;
        setShiftState(shiftState);

        requestLayout();
        invalidateAllKeys();
        // computeProximityThreshold(keyboard); // TODO
    }

    private ImageView createKeyImageView(int keyIndex) {

        final Rect padding = mPadding;
        final int kbdPaddingLeft = getPaddingLeft();
        final int kbdPaddingTop = getPaddingTop();
        final KeyHolder keyHolder = mKeys[keyIndex];
        final Key key = keyHolder.key;

        // Switch the character to uppercase if shift is pressed
        adjustCase(keyHolder);
        String label = key.label == null ? null : key.label.toString();
        if (Log.isLoggable(TAG, Log.VERBOSE)) {
            Log.d(TAG, "LABEL: " + key.label + "->" + label);
        }

        Bitmap bitmap = Bitmap.createBitmap(key.width, key.height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        final Paint paint = mPaint;
        paint.setColor(mKeyTextColor);

        canvas.drawARGB(0, 0,  0,  0);

        if (key.icon != null) {
            if (key.codes[0] == Keyboard.KEYCODE_SHIFT) {
                switch (mShiftState) {
                    case SHIFT_OFF:
                        key.icon = getContext().getResources().getDrawable(R.drawable.ic_ime_shift_off);
                        break;
                    case SHIFT_ON:
                        key.icon = getContext().getResources().getDrawable(R.drawable.ic_ime_shift_on);
                        break;
                    case SHIFT_LOCKED:
                        key.icon = getContext().getResources()
                                .getDrawable(R.drawable.ic_ime_shift_lock_on);
                        break;
                }
            }
            final int drawableX = (key.width - padding.left - padding.right
                    - key.icon.getIntrinsicWidth()) / 2 + padding.left;
            final int drawableY = (key.height - padding.top - padding.bottom
                    - key.icon.getIntrinsicHeight()) / 2 + padding.top;
            canvas.translate(drawableX, drawableY);
            key.icon.setBounds(0, 0,
                    key.icon.getIntrinsicWidth(), key.icon.getIntrinsicHeight());
            key.icon.draw(canvas);
            canvas.translate(-drawableX, -drawableY);
        } else if (label != null) {
            // For characters, use large font. For labels like "Done", use
            // small font.
            if (label.length() > 1) {
                paint.setTextSize(mModeChangeTextSize);
                paint.setTypeface(Typeface.create("sans-serif", Typeface.NORMAL));
            } else {
                paint.setTextSize(mKeyTextSize);
                paint.setTypeface(Typeface.create("sans-serif-light", Typeface.NORMAL));
            }
            // Draw the text
            canvas.drawText(label,
                    (key.width - padding.left - padding.right) / 2
                    + padding.left,
                    (key.height - padding.top - padding.bottom) / 2
                    + (paint.getTextSize() - paint.descent()) / 2 + padding.top,
                    paint);
            // Turn off drop shadow
            paint.setShadowLayer(0, 0, 0, 0);
        }

        ImageView view = new ImageView(getContext());
        view.setImageBitmap(bitmap);
        view.setContentDescription(label);
        addView(view, new ViewGroup.LayoutParams(LayoutParams.WRAP_CONTENT,
                LayoutParams.WRAP_CONTENT));

        view.setX(key.x + kbdPaddingLeft);
        view.setY(key.y + kbdPaddingTop);
        view.setImageAlpha(mMiniKeyboardOnScreen && !keyHolder.isInMiniKb ?
                mInactiveMiniKbAlpha : 255);
        view.setVisibility(View.VISIBLE);

        return view;
    }

    private void createKeyImageViews(KeyHolder[] keys) {
        int totalKeys = keys.length;
        if (mKeyImageViews != null) {
            for (ImageView view : mKeyImageViews) {
                this.removeView(view);
            }
            mKeyImageViews = null;
        }

        for (int keyIndex = 0; keyIndex < totalKeys; keyIndex++) {
            if (mKeyImageViews == null) {
                mKeyImageViews = new ImageView[totalKeys];
            } else if (mKeyImageViews[keyIndex] != null) {
                removeView(mKeyImageViews[keyIndex]);
            }
            mKeyImageViews[keyIndex] = createKeyImageView(keyIndex);
        }
    }

    private void removeMessages() {
        // TODO create mHandler and remove all messages here
    }

    /**
     * Requests a redraw of the entire keyboard. Calling {@link #invalidate} is
     * not sufficient because the keyboard renders the keys to an off-screen
     * buffer and an invalidate() only draws the cached buffer.
     *
     * @see #invalidateKey(int)
     */
    public void invalidateAllKeys() {
        createKeyImageViews(mKeys);
    }

    public void invalidateKey(int keyIndex) {
        if (mKeys == null)
            return;
        if (keyIndex < 0 || keyIndex >= mKeys.length) {
            return;
        }
        if (mKeyImageViews[keyIndex] != null) {
            removeView(mKeyImageViews[keyIndex]);
        }
        mKeyImageViews[keyIndex] = createKeyImageView(keyIndex);
    }

    @Override
    public void onDraw(Canvas canvas) {
        super.onDraw(canvas);
    }

    private CharSequence adjustCase(KeyHolder keyHolder) {
        CharSequence label = keyHolder.key.label;

        if (label != null && label.length() < 3) {
            // if we're adjusting the case of a basic letter in the mini keyboard,
            // we want the opposite case
            boolean invert = keyHolder.isInMiniKb && keyHolder.isInvertible;
            if (mKeyboard.isShifted() ^ invert) {
                label = label.toString().toUpperCase();
            } else {
                label = label.toString().toLowerCase();
            }

            keyHolder.key.label = label;
        }

        return label;
    }

    public void setShiftState(int state) {
        if (mShiftState == state) {
            return;
        }
        switch (state) {
            case SHIFT_OFF:
                mKeyboard.setShifted(false);
                break;
            case SHIFT_ON:
            case SHIFT_LOCKED:
                mKeyboard.setShifted(true);
                break;
        }
        mShiftState = state;
        invalidateAllKeys();
    }

    public int getShiftState() {
        return mShiftState;
    }

    public boolean isShifted() {
        return mShiftState == SHIFT_ON || mShiftState == SHIFT_LOCKED;
    }

    public void setFocus(int index, boolean clicked) {
        setFocus(index, clicked, true);
    }

    public void setFocus(int index, boolean clicked, boolean showFocusScale) {
        if (mKeyImageViews == null || mKeyImageViews.length == 0) {
            return;
        }
        if (index < 0 || index >= mKeyImageViews.length) {
            index = NOT_A_KEY;
        }

        if (index != mFocusIndex || clicked != mFocusClicked) {
            if (index != mFocusIndex) {
                if (mFocusIndex != NOT_A_KEY) {
                    LeanbackUtils.sendAccessibilityEvent(mKeyImageViews[mFocusIndex], false);
                }
                if (index != NOT_A_KEY) {
                    LeanbackUtils.sendAccessibilityEvent(mKeyImageViews[index], true);
                }
            }

            if (mCurrentFocusView != null) {
                mCurrentFocusView.animate().scaleX(1f).scaleY(1f)
                        .setInterpolator(LeanbackKeyboardContainer.sMovementInterpolator)
                        .setStartDelay(mUnfocusStartDelay);
                mCurrentFocusView.animate().setDuration(mClickAnimDur)
                        .setInterpolator(LeanbackKeyboardContainer.sMovementInterpolator)
                        .setStartDelay(mUnfocusStartDelay);
            }
            if (index != NOT_A_KEY) {
                float scale = clicked ? mClickedScale : (showFocusScale ? mFocusedScale : 1.0f);
                mCurrentFocusView = mKeyImageViews[index];
                mCurrentFocusView.animate().scaleX(scale).scaleY(scale)
                        .setInterpolator(LeanbackKeyboardContainer.sMovementInterpolator)
                        .setDuration(mClickAnimDur).start();
            }
            mFocusIndex = index;
            mFocusClicked = clicked;

            // if focusing on a non-mini kb key, dismiss minikb
            if (NOT_A_KEY != index && !mKeys[index].isInMiniKb) {
                dismissMiniKeyboard();
            }
        }
    }

    public boolean isMiniKeyboardOnScreen() {
        return mMiniKeyboardOnScreen;
    }

    public void onKeyLongPress() {
        int popupResId = mKeys[mFocusIndex].key.popupResId;
        if (popupResId != 0) {
            dismissMiniKeyboard();
            mMiniKeyboardOnScreen = true;
            Keyboard miniKeyboard = new Keyboard(getContext(), popupResId);
            List<Key> accentKeys = miniKeyboard.getKeys();
            int totalAccentKeys = accentKeys.size();
            int baseIndex = mFocusIndex;
            int currentRow = mFocusIndex / mColCount;
            int nextRow = (mFocusIndex + totalAccentKeys) / mColCount;
            // if all accent keys don't fit in a row when aligned with the popup
            // key, align the accent keys to the right boundary of that row
            if (currentRow != nextRow) {
                baseIndex = nextRow * mColCount - totalAccentKeys;
            }
            mBaseMiniKbIndex = baseIndex;
            for (int i = 0; i < totalAccentKeys; i++) {
                Key accentKey = accentKeys.get(i);
                // inherit the key position and edge flags. this way the xml files for the each
                // miniKb don't have to take into account the configuration of the keyboard
                // they're being inserted into.
                accentKey.x = mKeys[baseIndex + i].key.x;
                accentKey.y = mKeys[baseIndex + i].key.y;
                accentKey.edgeFlags = mKeys[baseIndex + i].key.edgeFlags;
                mKeys[baseIndex + i].key = accentKey;
                mKeys[baseIndex + i].isInMiniKb = true;
                mKeys[baseIndex + i].isInvertible = (i == 0);
            }

            invalidateAllKeys();
        }
    }

    public int getBaseMiniKbIndex() {
        return mBaseMiniKbIndex;
    }

    /**
     * @return  true if the minikeyboard was on-screen and is now dismissed, false otherwise.
     */
    public boolean dismissMiniKeyboard() {
        if (mMiniKeyboardOnScreen) {
            mMiniKeyboardOnScreen = false;
            setKeys(mKeyboard.getKeys());
            invalidateAllKeys();
            return true;
        }

        return false;
    }

    public void setFocus(int row, int col, boolean clicked) {
        setFocus(mColCount * row + col, clicked);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // For the kids, ya know?
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        // Round up a little
        if (mKeyboard == null) {
            setMeasuredDimension(getPaddingLeft() + getPaddingRight(),
                    getPaddingTop() + getPaddingBottom());
        } else {
            int width = mKeyboard.getMinWidth() + getPaddingLeft() + getPaddingRight();
            if (MeasureSpec.getSize(widthMeasureSpec) < width + 10) {
                width = MeasureSpec.getSize(widthMeasureSpec);
            }
            setMeasuredDimension(width, mKeyboard.getHeight() + getPaddingTop() + getPaddingBottom());
        }
    }

    private void setKeys(List<Key> keys) {
        mKeys = new KeyHolder[keys.size()];
        Iterator<Key> itt = keys.iterator();
        for (int i = 0; i < mKeys.length && itt.hasNext(); i++) {
            Key k = itt.next();
            mKeys[i] = new KeyHolder(k);
        }
    }
}
