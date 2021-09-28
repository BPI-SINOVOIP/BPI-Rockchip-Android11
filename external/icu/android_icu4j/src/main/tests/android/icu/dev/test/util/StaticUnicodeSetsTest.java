/* GENERATED SOURCE. DO NOT MODIFY. */
// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html#License
package android.icu.dev.test.util;

import static android.icu.impl.StaticUnicodeSets.get;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import android.icu.impl.StaticUnicodeSets.Key;
import android.icu.testsharding.MainTestShard;

/**
 * NOTE: The test testSetCoverage can be found in ExhaustiveNumberTest.java
 *
 * @author sffc
 */
@MainTestShard
public class StaticUnicodeSetsTest {

    @Test
    public void testFrozen() {
        for (Key key : Key.values()) {
            assertTrue(get(key).isFrozen());
        }
    }

    @Test
    public void testNonEmpty() {
        for (Key key : Key.values()) {
            // NOTE: No key EMPTY in Java
            assertTrue(get(key).isFrozen());
        }
    }
}
