// IBadProvider.aidl
package android.security.cts.CVE_2021_0327;

// Declare any non-default types here with import statements
import android.os.ParcelFileDescriptor;
interface IBadProvider {
    ParcelFileDescriptor takeBinder();

    oneway void exit();
}
