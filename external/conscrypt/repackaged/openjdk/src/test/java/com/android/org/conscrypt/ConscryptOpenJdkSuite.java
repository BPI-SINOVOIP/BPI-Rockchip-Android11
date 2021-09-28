/* GENERATED SOURCE. DO NOT MODIFY. */
package com.android.org.conscrypt;

import static com.android.org.conscrypt.TestUtils.installConscryptAsDefaultProvider;

import org.junit.BeforeClass;
import org.junit.runner.RunWith;
import org.junit.runners.Suite;

/**
 * @hide This class is not part of the Android public SDK API
 */
@RunWith(Suite.class)
@Suite.SuiteClasses({
  AddressUtilsTest.class,
  ApplicationProtocolSelectorAdapterTest.class,
  ClientSessionContextTest.class,
  ConscryptSocketTest.class,
  ConscryptTest.class,
  DuckTypedPSKKeyManagerTest.class,
  FileClientSessionCacheTest.class,
  NativeCryptoTest.class,
  NativeRefTest.class,
  NativeSslSessionTest.class,
  OpenSSLKeyTest.class,
  OpenSSLX509CertificateTest.class,
  PlatformTest.class,
  ServerSessionContextTest.class,
  SSLUtilsTest.class,
  TestSessionBuilderTest.class,
})
public class ConscryptOpenJdkSuite {

  @BeforeClass
  public static void setupStatic() {
    installConscryptAsDefaultProvider();
  }

}
