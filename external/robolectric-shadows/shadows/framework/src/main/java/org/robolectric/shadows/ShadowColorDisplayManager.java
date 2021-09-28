package org.robolectric.shadows;

import static android.os.Build.VERSION_CODES.Q;

import android.content.Context;
import android.hardware.display.ColorDisplayManager;
import android.hardware.display.ColorDisplayManager.AutoMode;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

@Implements(value = ColorDisplayManager.class, minSdk = Q)
public class ShadowColorDisplayManager {

  private boolean isNightDisplayActivated;
  private int nightDisplayTemperature;
  private int nightDisplayAutoMode;

  @Implementation
  protected void __constructor__(Context context) {
  }

  @Implementation
  public boolean setNightDisplayActivated(boolean activated) {
    isNightDisplayActivated = activated;
    return true;
  }

  @Implementation
  public boolean isNightDisplayActivated() {
    return isNightDisplayActivated;
  }

  @Implementation
  public boolean setNightDisplayColorTemperature(int temperature) {
    nightDisplayTemperature = temperature;
    return true;
  }

  @Implementation
  public int getNightDisplayColorTemperature() {
    return nightDisplayTemperature;
  }

  @Implementation
  public boolean setNightDisplayAutoMode(@AutoMode int autoMode) {
    nightDisplayAutoMode = autoMode;
    return true;
  }

  @Implementation
  public @AutoMode int getNightDisplayAutoMode() {
    return nightDisplayAutoMode;
  }
}
