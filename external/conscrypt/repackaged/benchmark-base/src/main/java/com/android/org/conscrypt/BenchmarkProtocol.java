/* GENERATED SOURCE. DO NOT MODIFY. */
package com.android.org.conscrypt;

/**
 * @hide This class is not part of the Android public SDK API
 */
@SuppressWarnings("ImmutableEnumChecker")
public enum BenchmarkProtocol {

    TLSv13("TLSv1.3"),
    TLSv12("TLSv1.2");

    private final String[] protocols;

    BenchmarkProtocol(String... protocols) {
        this.protocols = protocols;
    }

    public String[] getProtocols() {
        return protocols.clone();
    }
}
