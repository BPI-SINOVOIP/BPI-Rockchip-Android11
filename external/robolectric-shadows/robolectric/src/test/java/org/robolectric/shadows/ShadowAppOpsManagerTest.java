package org.robolectric.shadows;

import static android.app.AppOpsManager.MODE_ALLOWED;
import static android.app.AppOpsManager.MODE_DEFAULT;
import static android.app.AppOpsManager.MODE_ERRORED;
import static android.app.AppOpsManager.OPSTR_FINE_LOCATION;
import static android.app.AppOpsManager.OPSTR_GPS;
import static android.app.AppOpsManager.OPSTR_SEND_SMS;
import static android.app.AppOpsManager.OP_FINE_LOCATION;
import static android.app.AppOpsManager.OP_GPS;
import static android.app.AppOpsManager.OP_SEND_SMS;
import static android.app.AppOpsManager.OP_VIBRATE;
import static android.os.Build.VERSION_CODES.KITKAT;
import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.robolectric.Shadows.shadowOf;

import android.app.AppOpsManager;
import android.app.AppOpsManager.OnOpChangedListener;
import android.app.AppOpsManager.OpEntry;
import android.app.AppOpsManager.PackageOps;
import android.content.Context;
import android.media.AudioAttributes;
import android.os.Binder;
import android.os.Build.VERSION_CODES;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowAppOpsManager.ModeAndException;

/** Unit tests for {@link ShadowAppOpsManager}. */
@RunWith(AndroidJUnit4.class)
@Config(minSdk = KITKAT)
public class ShadowAppOpsManagerTest {

  private static final String PACKAGE_NAME1 = "com.company1.pkg1";
  private static final String PACKAGE_NAME2 = "com.company2.pkg2";
  private static final int UID_1 = 10000;
  private static final int UID_2 = 10001;

  // Can be used as an argument of getOpsForPackage().
  private static final int[] NO_OP_FILTER = null;

  private AppOpsManager appOps;

  @Before
  public void setUp() {
    appOps =
        (AppOpsManager)
            ApplicationProvider.getApplicationContext().getSystemService(Context.APP_OPS_SERVICE);
  }

  @Test
  @Config(minSdk = VERSION_CODES.P)
  public void checkOpNoThrow_noModeSet_atLeastP_shouldReturnModeAllowed() {
    assertThat(appOps.checkOpNoThrow(OPSTR_GPS, UID_1, PACKAGE_NAME1)).isEqualTo(MODE_ALLOWED);
  }

  @Test
  @Config(minSdk = VERSION_CODES.P)
  public void setMode_withModeDefault_atLeastP_checkOpNoThrow_shouldReturnModeDefault() {
    appOps.setMode(OPSTR_GPS, UID_1, PACKAGE_NAME1, MODE_DEFAULT);
    assertThat(appOps.checkOpNoThrow(OPSTR_GPS, UID_1, PACKAGE_NAME1)).isEqualTo(MODE_DEFAULT);
  }

  @Test
  @Config(minSdk = VERSION_CODES.KITKAT)
  public void checkOpNoThrow_noModeSet_atLeastKitKat_shouldReturnModeAllowed() {
    assertThat(appOps.checkOpNoThrow(/* op= */ 2, UID_1, PACKAGE_NAME1)).isEqualTo(MODE_ALLOWED);
  }

  @Test
  @Config(minSdk = VERSION_CODES.KITKAT)
  public void setMode_withModeDefault_atLeastKitKat_checkOpNoThrow_shouldReturnModeDefault() {
    appOps.setMode(/* op= */ 2, UID_1, PACKAGE_NAME1, MODE_DEFAULT);
    assertThat(appOps.checkOpNoThrow(/* op= */ 2, UID_1, PACKAGE_NAME1)).isEqualTo(MODE_DEFAULT);
  }

  @Test
  @Config(maxSdk = VERSION_CODES.O_MR1)
  public void setMode_checkOpNoThrow_belowP() {
    assertThat(appOps.checkOpNoThrow(OP_GPS, UID_1, PACKAGE_NAME1)).isEqualTo(MODE_ALLOWED);
    appOps.setMode(OP_GPS, UID_1, PACKAGE_NAME1, MODE_ERRORED);
    assertThat(appOps.checkOpNoThrow(OP_GPS, UID_1, PACKAGE_NAME1)).isEqualTo(MODE_ERRORED);
  }

  @Test
  @Config(minSdk = VERSION_CODES.P)
  public void setMode_checkOpNoThrow_atLeastP() {
    assertThat(appOps.checkOpNoThrow(OPSTR_GPS, UID_1, PACKAGE_NAME1)).isEqualTo(MODE_ALLOWED);
    appOps.setMode(OPSTR_GPS, UID_1, PACKAGE_NAME1, MODE_ERRORED);
    assertThat(appOps.checkOpNoThrow(OPSTR_GPS, UID_1, PACKAGE_NAME1)).isEqualTo(MODE_ERRORED);
  }

  @Test
  @Config(minSdk = VERSION_CODES.R)
  public void noModeSet_atLeastR_noteProxyOpNoThrow_shouldReturnModeAllowed() {
    assertThat(appOps.noteProxyOpNoThrow(OPSTR_GPS, PACKAGE_NAME1, UID_1, null, null))
            .isEqualTo(MODE_ALLOWED);
  }

  @Test
  @Config(minSdk = VERSION_CODES.R)
  public void setMode_withModeDefault_atLeastR_noteProxyOpNoThrow_shouldReturnModeDefault() {
    appOps.setMode(OP_GPS, Binder.getCallingUid(), PACKAGE_NAME1, MODE_DEFAULT);
    assertThat(appOps.noteProxyOpNoThrow(OPSTR_GPS, PACKAGE_NAME1, UID_1, null, null))
            .isEqualTo(MODE_DEFAULT);
  }

  @Test
  @Config(minSdk = VERSION_CODES.R)
  public void setMode_noteProxyOpNoThrow_atLeastR() {
    assertThat(appOps.noteProxyOpNoThrow(OPSTR_GPS, PACKAGE_NAME1, UID_1, null, null))
            .isEqualTo(MODE_ALLOWED);
    appOps.setMode(OP_GPS, Binder.getCallingUid(), PACKAGE_NAME1, MODE_ERRORED);
    assertThat(appOps.noteProxyOpNoThrow(OPSTR_GPS, PACKAGE_NAME1, UID_1, null, null))
            .isEqualTo(MODE_ERRORED);
  }

  @Test
  @Config(minSdk = VERSION_CODES.KITKAT)
  public void startStopWatchingMode() {
    OnOpChangedListener callback = mock(OnOpChangedListener.class);
    appOps.startWatchingMode(OPSTR_FINE_LOCATION, PACKAGE_NAME1, callback);
    appOps.setMode(OP_FINE_LOCATION, UID_1, PACKAGE_NAME1, MODE_ERRORED);
    verify(callback).onOpChanged(OPSTR_FINE_LOCATION, PACKAGE_NAME1);

    appOps.stopWatchingMode(callback);
    appOps.setMode(OP_FINE_LOCATION, UID_1, PACKAGE_NAME1, MODE_ALLOWED);
    verifyNoMoreInteractions(callback);
  }

  @Test
  @Config(maxSdk = VERSION_CODES.Q)
  public void noteOp() {
    assertThat(appOps.noteOp(OP_GPS, UID_1, PACKAGE_NAME1)).isEqualTo(MODE_ALLOWED);
    // Use same op more than once
    assertThat(appOps.noteOp(OP_GPS, UID_1, PACKAGE_NAME1)).isEqualTo(MODE_ALLOWED);

    assertThat(appOps.noteOp(OP_SEND_SMS, UID_1, PACKAGE_NAME1)).isEqualTo(MODE_ALLOWED);
  }

  @Test
  @Config(minSdk = VERSION_CODES.R)
  public void noteOp_atLeastR() {
    assertThat(appOps.noteOp(OPSTR_GPS, UID_1, PACKAGE_NAME1, null, null)).isEqualTo(MODE_ALLOWED);
    // Use same op more than once
    assertThat(appOps.noteOp(OPSTR_GPS, UID_1, PACKAGE_NAME1, null, null)).isEqualTo(MODE_ALLOWED);

    assertThat(appOps.noteOp(OPSTR_SEND_SMS, UID_1, PACKAGE_NAME1, null, null)).isEqualTo(MODE_ALLOWED);
  }

  @Test
  @Config(maxSdk = VERSION_CODES.Q)
  public void getOpsForPackage_noOps() {
    List<PackageOps> results = appOps.getOpsForPackage(UID_1, PACKAGE_NAME1, NO_OP_FILTER);
    assertOps(results);
  }

  @Test
  @Config(minSdk = VERSION_CODES.R)
  public void getOpsForPackage_noOps_atLeastR() {
    List<PackageOps> results = appOps.getOpsForPackage(UID_1, PACKAGE_NAME1);
    assertOps(results);
  }

  @Test
  @Config(maxSdk = VERSION_CODES.Q)
  public void getOpsForPackage_hasOps() {
    appOps.noteOp(OP_GPS, UID_1, PACKAGE_NAME1);
    appOps.noteOp(OP_SEND_SMS, UID_1, PACKAGE_NAME1);

    // PACKAGE_NAME2 has ops.
    List<PackageOps> results = appOps.getOpsForPackage(UID_1, PACKAGE_NAME1, NO_OP_FILTER);
    assertOps(results, OP_GPS, OP_SEND_SMS);

    // PACKAGE_NAME2 has no ops.
    results = appOps.getOpsForPackage(UID_2, PACKAGE_NAME2, NO_OP_FILTER);
    assertOps(results);
  }

  @Test
  @Config(minSdk = VERSION_CODES.R)
  public void getOpsForPackage_hasOps_atLeastR() {
    appOps.noteOp(OPSTR_GPS, UID_1, PACKAGE_NAME1, null, null);
    appOps.noteOp(OPSTR_SEND_SMS, UID_1, PACKAGE_NAME1, null, null);

    // PACKAGE_NAME2 has ops.
    List<PackageOps> results = appOps.getOpsForPackage(UID_1, PACKAGE_NAME1);
    assertOps(results, OP_GPS, OP_SEND_SMS);

    // PACKAGE_NAME2 has no ops.
    results = appOps.getOpsForPackage(UID_2, PACKAGE_NAME2);
    assertOps(results);
  }

  @Test
  @Config(maxSdk = VERSION_CODES.Q)
  public void getOpsForPackage_withOpFilter() {
    List<PackageOps> results = appOps.getOpsForPackage(UID_1, PACKAGE_NAME1, new int[] {OP_GPS});
    assertOps(results);

    appOps.noteOp(OP_SEND_SMS, UID_1, PACKAGE_NAME1);
    results = appOps.getOpsForPackage(UID_1, PACKAGE_NAME1, new int[] {OP_GPS});
    assertOps(results);

    appOps.noteOp(OP_GPS, UID_1, PACKAGE_NAME1);
    results = appOps.getOpsForPackage(UID_1, PACKAGE_NAME1, new int[] {OP_GPS});
    assertOps(results, OP_GPS);
  }

  @Test
  @Config(minSdk = VERSION_CODES.R)
  public void getOpsForPackage_withOpFilter_atLeastR() {
    List<PackageOps> results = appOps.getOpsForPackage(UID_1, PACKAGE_NAME1, OPSTR_GPS);
    assertOps(results);

    appOps.noteOp(OPSTR_SEND_SMS, UID_1, PACKAGE_NAME1, null, null);
    results = appOps.getOpsForPackage(UID_1, PACKAGE_NAME1, OPSTR_GPS);
    assertOps(results);

    appOps.noteOp(OPSTR_GPS, UID_1, PACKAGE_NAME1, null, null);
    results = appOps.getOpsForPackage(UID_1, PACKAGE_NAME1, OPSTR_GPS);
    assertOps(results, OP_GPS);
  }

  @Test
  @Config(minSdk = VERSION_CODES.LOLLIPOP)
  public void setRestrictions() {
    appOps.setRestriction(
        OP_VIBRATE, AudioAttributes.USAGE_NOTIFICATION, MODE_ERRORED, new String[] {PACKAGE_NAME1});

    ModeAndException modeAndException =
        shadowOf(appOps).getRestriction(OP_VIBRATE, AudioAttributes.USAGE_NOTIFICATION);
    assertThat(modeAndException.mode).isEqualTo(MODE_ERRORED);
    assertThat(modeAndException.exceptionPackages).containsExactly(PACKAGE_NAME1);
  }

  @Test
  public void checkPackage_doesntExist() {
    try {
      appOps.checkPackage(123, PACKAGE_NAME1);
      fail();
    } catch (SecurityException e) {
      // expected
    }
  }

  @Test
  public void checkPackage_doesntBelong() {
    shadowOf(ApplicationProvider.getApplicationContext().getPackageManager())
        .setPackagesForUid(111, PACKAGE_NAME1);
    try {
      appOps.checkPackage(123, PACKAGE_NAME1);
      fail();
    } catch (SecurityException e) {
      // expected
    }
  }

  @Test
  public void checkPackage_belongs() {
    shadowOf(ApplicationProvider.getApplicationContext().getPackageManager())
        .setPackagesForUid(123, PACKAGE_NAME1);
    appOps.checkPackage(123, PACKAGE_NAME1);
    // check passes without exception
  }

  /** Assert that the results contain the expected op codes. */
  private void assertOps(List<PackageOps> pkgOps, Integer... expectedOps) {
    Set<Integer> actualOps = new HashSet<>();
    for (PackageOps pkgOp : pkgOps) {
      for (OpEntry entry : pkgOp.getOps()) {
        actualOps.add(entry.getOp());
      }
    }

    assertThat(actualOps).containsAllIn(expectedOps);
  }
}
