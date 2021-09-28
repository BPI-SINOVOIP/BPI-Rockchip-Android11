/* GENERATED SOURCE. DO NOT MODIFY. */
package com.android.org.conscrypt.testing;

import javax.net.ssl.SNIMatcher;
import javax.net.ssl.SNIServerName;

/**
 * @hide This class is not part of the Android public SDK API
 */
public class FailingSniMatcher extends SNIMatcher {
    private FailingSniMatcher() {
        super(0);
    }

    @Override
    public boolean matches(SNIServerName sniServerName) {
        return false;
    }

    public static SNIMatcher create() {
        return new FailingSniMatcher();
    }
}
