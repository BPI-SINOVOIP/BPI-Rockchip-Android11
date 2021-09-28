/* GENERATED SOURCE. DO NOT MODIFY. */
/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.org.conscrypt.java.security;

import java.security.KeyPair;
import java.security.NoSuchAlgorithmException;
import java.security.interfaces.ECPrivateKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.ECPrivateKeySpec;
import java.security.spec.ECPublicKeySpec;
import java.security.spec.InvalidKeySpecException;
import java.util.Arrays;
import java.util.List;
import libcore.junit.util.EnableDeprecatedBouncyCastleAlgorithmsRule;
import org.junit.ClassRule;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import tests.util.ServiceTester;

/**
 * @hide This class is not part of the Android public SDK API
 */
@RunWith(JUnit4.class)
public class KeyFactoryTestEC extends
    AbstractKeyFactoryTest<ECPublicKeySpec, ECPrivateKeySpec> {

  // BEGIN Android-Added: Allow access to deprecated BC algorithms.
  // Allow access to deprecated BC algorithms in this test, so we can ensure they
  // continue to work
  @ClassRule
  public static TestRule enableDeprecatedBCAlgorithmsRule =
          EnableDeprecatedBouncyCastleAlgorithmsRule.getInstance();
  // END Android-Added: Allow access to deprecated BC algorithms.

  public KeyFactoryTestEC() {
    super("EC", ECPublicKeySpec.class, ECPrivateKeySpec.class);
  }

  @Override
  public ServiceTester customizeTester(ServiceTester tester) {
    // BC's EC keys always use explicit params, even though it's a bad idea, and we don't support
    // those, so don't test BC keys
    return tester.skipProvider("BC");
  }

  @Override
  protected void check(KeyPair keyPair) throws Exception {
    new SignatureHelper("SHA256withECDSA").test(keyPair);
  }

  @Override
  protected List<KeyPair> getKeys() throws NoSuchAlgorithmException, InvalidKeySpecException {
      return Arrays.asList(
              new KeyPair(DefaultKeys.getPublicKey("EC"), DefaultKeys.getPrivateKey("EC")),
              new KeyPair(new TestPublicKey(DefaultKeys.getPublicKey("EC")),
                      new TestPrivateKey(DefaultKeys.getPrivateKey("EC"))),
              new KeyPair(new TestECPublicKey((ECPublicKey) DefaultKeys.getPublicKey("EC")),
                      new TestECPrivateKey((ECPrivateKey) DefaultKeys.getPrivateKey("EC"))));
  }
}
