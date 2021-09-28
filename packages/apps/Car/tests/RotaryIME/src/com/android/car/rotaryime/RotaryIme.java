package com.android.car.rotaryime;

import static android.view.KeyEvent.KEYCODE_1;
import static android.view.KeyEvent.KEYCODE_A;
import static android.view.KeyEvent.KEYCODE_APOSTROPHE;
import static android.view.KeyEvent.KEYCODE_COMMA;
import static android.view.KeyEvent.KEYCODE_DEL;
import static android.view.KeyEvent.KEYCODE_PERIOD;
import static android.view.KeyEvent.KEYCODE_SEMICOLON;
import static android.view.KeyEvent.KEYCODE_SLASH;
import static android.view.KeyEvent.KEYCODE_SPACE;
import static android.view.KeyEvent.META_SHIFT_ON;

import android.inputmethodservice.InputMethodService;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import androidx.annotation.NonNull;

/**
 * Sample IME for rotary controllers. This is intentionally very basic so that it's easy to
 * understand the code. It doesn't support multiple locales / layouts. It doesn't support password
 * fields, numeric fields, etc.
 */
public class RotaryIme extends InputMethodService {

  /** Message requesting that the view in the {@code obj} field have its pressed state cleared. */
  private static final int MSG_CLEAR_PRESSED = 1;

  /** How many milliseconds a key should remain pressed after the user clicks it. */
  private static final long PRESSED_MS = 200;

  /** A handler for clearing the pressed state shortly after a key is pressed. */
  private final Handler mHandler = new ImeHandler(Looper.getMainLooper());

  @Override
  public boolean onEvaluateFullscreenMode() {
    // Don't go full-screen, even in landscape. This IME is very short so it fits easily.
    return false;
  }

  @Override
  public View onCreateInputView() {
    ViewGroup rootView = (ViewGroup) getLayoutInflater().inflate(R.layout.horizontal_keyboard,
        /* root= */ null);

    // Since the IME isn't in the application window, when the user presses the center button on the
    // rotary controller to press the focused key on this keyboard, ACTION_CLICK will be performed
    // on the key which will invoke its click handler. Long press will similarly result in the key's
    // long click handler being invoked.

    // The first row contains letters in alphabetical order and the delete key.
    ViewGroup letters = rootView.findViewById(R.id.letters);
    int aIndex = findChild(letters, rootView.findViewById(R.id.a));
    for (int i = 0; i < 26; i++) {
      TextView keyTextView = (TextView) letters.getChildAt(aIndex + i);
      int keyCode = KEYCODE_A + i;
      keyTextView.setOnClickListener(view -> handleKeyClick(view, keyCode));
      keyTextView.setOnLongClickListener(view -> handleKeyLongClick(view, keyCode));
    }

    rootView.findViewById(R.id.delete).setOnClickListener(
            view -> handleKeyClick(view, KEYCODE_DEL));

    // The second row contains the close key, symbols, space, and digits.
    rootView.findViewById(R.id.close).setOnClickListener(
            view -> requestHideSelf(/* flags= */ 0));
    rootView.findViewById(R.id.dash).setOnClickListener(
            view -> handleKeyClick(view, KeyEvent.KEYCODE_MINUS));
    rootView.findViewById(R.id.quote).setOnClickListener(
            view -> handleKeyClick(view, KEYCODE_APOSTROPHE, META_SHIFT_ON));
    rootView.findViewById(R.id.apostrophe).setOnClickListener(
            view -> handleKeyClick(view, KEYCODE_APOSTROPHE));
    rootView.findViewById(R.id.exclamation_mark).setOnClickListener(
            view -> handleKeyClick(view, KEYCODE_1, META_SHIFT_ON));
    rootView.findViewById(R.id.question_mark).setOnClickListener(
            view -> handleKeyClick(view, KEYCODE_SLASH, META_SHIFT_ON));
    rootView.findViewById(R.id.semicolon).setOnClickListener(
            view -> handleKeyClick(view, KEYCODE_SEMICOLON));
    rootView.findViewById(R.id.colon).setOnClickListener(
            view -> handleKeyClick(view, KEYCODE_SEMICOLON, META_SHIFT_ON));
    rootView.findViewById(R.id.comma).setOnClickListener(
            view -> handleKeyClick(view, KEYCODE_COMMA));
    rootView.findViewById(R.id.period).setOnClickListener(
            view -> handleKeyClick(view, KEYCODE_PERIOD));
    rootView.findViewById(R.id.space).setOnClickListener(
            view -> handleKeyClick(view, KEYCODE_SPACE));
    ViewGroup otherKeys = rootView.findViewById(R.id.other_keys);
    int zeroIndex = findChild(otherKeys, rootView.findViewById(R.id.zero));
    for (int i = 0; i < 10; i++) {
      int keyCode = KeyEvent.KEYCODE_0 + i;
      otherKeys.getChildAt(zeroIndex + i).setOnClickListener(
              view -> handleKeyClick(view, keyCode));
    }

    return rootView;
  }

  /** Returns {@code child}'s index within {@code parent} or -1 if not found. */
  private static int findChild(ViewGroup parent, View child) {
    for (int i = 0; i < parent.getChildCount(); i++) {
      if (parent.getChildAt(i) == child) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Handles a click on the key {@code view} by sending events with the given {@code keyCode}. Use
   * this convenience method for unshifted keys.
   */
  private void handleKeyClick(View view, int keyCode) {
    handleKeyClick(view, keyCode, /* metaState= */ 0);
  }

  /**
   * Handles a click on the key {@code view} by sending events with the given {@code keyCode}
   * and {@code metaState}. Use {@link KeyEvent#META_SHIFT_ON} to access shifted keys such as
   * question mark (shift slash). Use zero for unshifted keys.
   */
  private void handleKeyClick(View view, int keyCode, int metaState) {
    animatePressed(view);
    sendDownUpKeyEvents(keyCode, metaState);
  }

  /**
   * Handles a long click on the key {@code view} by sending events with the given {@code keyCode}
   * and {@link KeyEvent#META_SHIFT_ON} to produce shifted keys such as capital letters.
   */
  private boolean handleKeyLongClick(View view, int keyCode) {
    animatePressed(view);
    sendDownUpKeyEvents(keyCode, META_SHIFT_ON);
    return true;
  }

  /**
   * Sends an {@link KeyEvent#ACTION_DOWN} event followed by an {@link KeyEvent#ACTION_UP} event
   * with the given {@code keyCode} and {@code metaState}.
   */
  private void sendDownUpKeyEvents(int keyCode, int metaState) {
    long uptimeMillis = SystemClock.uptimeMillis();
    KeyEvent downEvent = new KeyEvent(uptimeMillis, uptimeMillis, KeyEvent.ACTION_DOWN,
        keyCode, /* repeat= */ 0, metaState, KeyCharacterMap.VIRTUAL_KEYBOARD, /* scancode= */ 0,
        KeyEvent.FLAG_SOFT_KEYBOARD, InputDevice.SOURCE_KEYBOARD);
    KeyEvent upEvent = new KeyEvent(uptimeMillis, uptimeMillis, KeyEvent.ACTION_UP,
        keyCode, /* repeat= */ 0, metaState, KeyCharacterMap.VIRTUAL_KEYBOARD, /* scancode= */ 0,
        KeyEvent.FLAG_SOFT_KEYBOARD, InputDevice.SOURCE_KEYBOARD);
    getCurrentInputConnection().sendKeyEvent(downEvent);
    getCurrentInputConnection().sendKeyEvent(upEvent);
  }

  /** Sets {@code view}'s pressed state and clears it {@link #PRESSED_MS} later. */
  private void animatePressed(View view) {
    view.setPressed(true);
    Message message = mHandler.obtainMessage(MSG_CLEAR_PRESSED, view);
    mHandler.removeMessages(MSG_CLEAR_PRESSED);
    mHandler.sendMessageDelayed(message, PRESSED_MS);
  }

  /** A handler for clearing the pressed state shortly after a key is pressed. */
  private static class ImeHandler extends Handler {
    ImeHandler(@NonNull Looper looper) {
      super(looper);
    }

    @Override
    public void handleMessage(@NonNull Message msg) {
      if (msg.what == MSG_CLEAR_PRESSED) {
        ((View) msg.obj).setPressed(false);
      }
    }
  }
}
