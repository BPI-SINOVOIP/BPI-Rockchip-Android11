package android.view.textclassifier.cts;

import static com.google.common.truth.Truth.assertWithMessage;

import android.content.pm.PackageManager;
import android.text.TextUtils;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Test;

@SmallTest
public class TextClassifierPolicyTest {

    @Test
    public void isDefaultTextClassifierPackageKnown() {
        PackageManager pm = ApplicationProvider.getApplicationContext().getPackageManager();
        String defaultTextClassifierPackageName = pm.getDefaultTextClassifierPackageName();
        String extensionServicesPackageName = pm.getServicesSystemSharedLibraryPackageName();
        assertWithMessage(
                "The default text classifier package must be either empty or has the same value "
                        + "as config_servicesExtensionPackage.")
                .that(TextUtils.isEmpty(defaultTextClassifierPackageName)
                        || defaultTextClassifierPackageName.equals(extensionServicesPackageName))
                .isTrue();
    }

    @Test
    public void defaultTextClassifierIsNotTheSameAsSystemTextClassifier() {
        PackageManager pm = ApplicationProvider.getApplicationContext().getPackageManager();
        String defaultTextClassifierPackageName = pm.getDefaultTextClassifierPackageName();
        String systemTextClassifierPackageName = pm.getSystemTextClassifierPackageName();
        assertWithMessage(
                "The default text classifier package should not be the same as the system text "
                        + "classifier package, just leave config_defaultTextClassifierPackage "
                        + "empty if they are the same package.")
                .that(defaultTextClassifierPackageName)
                .isNotEqualTo(systemTextClassifierPackageName);
    }
}
