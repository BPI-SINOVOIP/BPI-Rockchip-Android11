// BEGIN-INTERNAL
package org.robolectric.shadows;

import static com.google.common.truth.Truth.assertThat;

import static android.os.Build.VERSION_CODES.Q;
import static com.google.common.truth.Truth.assertThat;

import android.icu.util.TimeZone;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.util.List;
import java.util.stream.Collectors;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;
import org.robolectric.util.ReflectionHelpers.ClassParameter;

/** Unit tests for {@link ShadowTimeZoneFinderQ}. */
@RunWith(AndroidJUnit4.class)
public class ShadowTimeZoneFinderQTest {

  @Test
  @Config(minSdk = Q, maxSdk = Q)
  public void lookupTimeZonesByCountry_shouldReturnExpectedTimeZones() throws Exception {
    Class<?> cls = Class.forName("libcore.timezone.TimeZoneFinder");
    Object timeZoneFinder = ReflectionHelpers.callStaticMethod(cls, "getInstance");
    List<TimeZone> timezones =
            ReflectionHelpers.callInstanceMethod(
                    cls,
                    timeZoneFinder,
                    "lookupTimeZonesByCountry",
                    ClassParameter.from(String.class, "us"));

    assertThat(timezones.stream().map(TimeZone::getID).collect(Collectors.toList()))
            .containsAllOf("America/Los_Angeles", "America/New_York", "Pacific/Honolulu");
  }
}
// END-INTERNAL
