// BEGIN-INTERNAL
package org.robolectric.shadows;

import android.graphics.ColorSpace;

import static android.os.Build.VERSION_CODES.Q;
import static android.os.Build.VERSION_CODES.O;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

/**
 * Container for implementations of ColorSpace
 */
public class ShadowColorSpace {
  @SuppressWarnings({"UnusedDeclaration"})
  @Implements(value = ColorSpace.Rgb.class, minSdk = O)
  public static class ShadowRgb {

    @Implementation(minSdk = Q)
    protected static long nativeCreate(float a, float b, float c, float d,
                float e, float f, float g, float[] xyz) {
      return 1;
    }
  }
}
// END-INTERNAL