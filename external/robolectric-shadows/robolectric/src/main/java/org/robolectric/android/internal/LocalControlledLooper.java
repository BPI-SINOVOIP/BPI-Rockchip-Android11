package org.robolectric.android.internal;

import static org.robolectric.Shadows.shadowOf;
import static org.robolectric.util.ReflectionHelpers.ClassParameter.from;

import android.view.View;
import android.view.ViewRootImpl;
import android.os.Looper;
import androidx.test.internal.platform.os.ControlledLooper;
import org.robolectric.util.ReflectionHelpers;

public class LocalControlledLooper implements ControlledLooper {

  @Override
  public void drainMainThreadUntilIdle() {
    shadowOf(Looper.getMainLooper()).idle();
  }

  @Override
  public void simulateWindowFocus(View decorView) {
    ViewRootImpl viewRoot = ReflectionHelpers.callInstanceMethod(decorView, "getViewRootImpl");
    if (viewRoot != null) {
      ReflectionHelpers.callInstanceMethod(
          viewRoot,
          "windowFocusChanged",
          from(boolean.class, true), /* hasFocus */
          from(boolean.class, false) /* inTouchMode */);
    }
  }
}
