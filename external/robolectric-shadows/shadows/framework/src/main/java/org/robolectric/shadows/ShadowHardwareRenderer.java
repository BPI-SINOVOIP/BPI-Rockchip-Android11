// BEGIN-INTERNAL
package org.robolectric.shadows;

import static android.os.Build.VERSION_CODES.Q;
import static android.os.Build.VERSION_CODES.R;

import android.graphics.HardwareRenderer;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

@Implements(value = HardwareRenderer.class, isInAndroidSdk = false, minSdk = Q)
public class ShadowHardwareRenderer {

  private static long nextCreateProxy = 0;

  @Implementation
  protected static long nCreateProxy(boolean translucent, long rootRenderNode) {
    return ++nextCreateProxy;
  }

  @Implementation(minSdk = R)
  protected static long nCreateProxy(
          boolean translucent, boolean isWideGamut, long rootRenderNode) {
    return nCreateProxy(translucent, rootRenderNode);
  }
}
// END-INTERNAL