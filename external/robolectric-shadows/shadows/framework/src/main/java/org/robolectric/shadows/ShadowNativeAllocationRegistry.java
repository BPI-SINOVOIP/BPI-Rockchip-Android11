package org.robolectric.shadows;

import static android.os.Build.VERSION_CODES.N;
// BEGIN-INTERNAL
import static android.os.Build.VERSION_CODES.Q;
// END-INTERNAL

import libcore.util.NativeAllocationRegistry;

// BEGIN-INTERNAL
import org.robolectric.annotation.Implementation;
// END-INTERNAL
import org.robolectric.annotation.Implements;
// BEGIN-INTERNAL
import org.robolectric.shadow.api.Shadow;
import org.robolectric.util.ReflectionHelpers.ClassParameter;
// END-INTERNAL

@Implements(value = NativeAllocationRegistry.class, callThroughByDefault = false, minSdk = N, isInAndroidSdk = false)
public class ShadowNativeAllocationRegistry {
    // BEGIN-INTERNAL
    @Implementation(minSdk = Q)
    protected static NativeAllocationRegistry createMalloced(
            ClassLoader classLoader, long freeFunction, long size) {
        return Shadow.directlyOn(NativeAllocationRegistry.class, "createMalloced",
                ClassParameter.from(ClassLoader.class, classLoader),
                ClassParameter.from(long.class, freeFunction),
                ClassParameter.from(long.class, size));
    }

    @Implementation(minSdk = Q)
    protected static NativeAllocationRegistry createMalloced(
            ClassLoader classLoader, long freeFunction) {
        return Shadow.directlyOn(NativeAllocationRegistry.class, "createMalloced",
                ClassParameter.from(ClassLoader.class, classLoader),
                ClassParameter.from(long.class, freeFunction));
    }
    // END-INTERNAL
}
