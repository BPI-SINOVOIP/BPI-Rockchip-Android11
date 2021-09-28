package com.android.customization.testing;

import com.android.wallpaper.module.PackageStatusNotifier;

/**
 * Test implementation of {@link PackageStatusNotifier}.
 */
public class TestPackageStatusNotifier implements PackageStatusNotifier {
    @Override
    public void addListener(Listener listener, String action) {
        // Do nothing
    }

    @Override
    public void removeListener(Listener listener) {
        // Do nothing
    }
}
