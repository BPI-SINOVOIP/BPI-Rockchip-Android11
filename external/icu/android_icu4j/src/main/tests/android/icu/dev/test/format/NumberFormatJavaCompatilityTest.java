/* GENERATED SOURCE. DO NOT MODIFY. */
// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html#License
package android.icu.dev.test.format;

import static org.junit.Assert.assertEquals;

import java.text.ParsePosition;
import java.util.Locale;
import java.util.Objects;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import android.icu.dev.test.TestUtil;
import android.icu.impl.number.DecimalFormatProperties.ParseMode;
import android.icu.text.DecimalFormat;
import android.icu.text.DecimalFormatSymbols;
import android.icu.util.ULocale;
import android.icu.testsharding.MainTestShard;

/**
 * Test for {@link DecimalFormat} in {@link ParseMode#JAVA_COMPATIBILITY} mode.
 */
@MainTestShard
@RunWith(JUnit4.class)
public class NumberFormatJavaCompatilityTest {

    @Test
    public void testIgnoreables() {
        // Test bidi characters
        assertParseError("0", "\u200e1");
        assertParsed("0", "1\u200e", 1);
        assertParseError("0%", "\u200e1%");
    }

    @Test
    public void testParseGroupingSeparator() {
        // Test that grouping separator is optional when the group separator is specified
        assertParsed("#,##0", "9,999", 9999);
        assertParsed("#,##0", "9999", 9999);
        assertParsed("#,###0", "9,9999", 99999);

        // Test that grouping size doesn't affect parsing at all
        assertParsed("#,##0", "9,9999", 99999);
        assertParsed("#,###0", "99,999", 99999);

        assertParsed("###0", "9999", 9999);
        assertParsed("###0", "99999", 99999);

        // Test that grouping separator must not be present when the group separator is NOT specified
        // Only the 1st character in front of separator , should be consumed.
        assertParsed("###0", "9,9999", 9);
        assertParsed("###0", "9,999", 9);
    }

    @Test
    public void testParseScienificNotation() {
        assertParsed("0.###E0", "1E-3", 0.001);
        assertParsed("0.###E0", "1E0", 1);
        assertParsed("0.###E0", "1E3", 1000);
        assertParsed("0.###E0", "1.111E3", 1111);
        assertParsed("0.###E0", "1.1E3", 1100);

        // "0.###E0" is engineering notation, i.e. the exponent should be a multiple of 3
        // for formatting. But it shouldn't affect parsing.
        assertParsed("0.###E0", "1E1", 10);

        // Test that exponent is not required for parsing
        assertParsed("0.###E0", "1.1", 1.1);
        assertParsed("0.###E0", "1100", 1100);

        // Test that the max of fraction, integer or signficant digits don't affect parsing
        // Note that the max of signficant digits is 4 = min integer digits (1)
        //   + max fraction digits (3)
        assertParsed("0.###E0", "1111.4E3", 1111400);
        assertParsed("0.###E0", "1111.9999E3", 1111999.9);
    }

    private void assertParseError(String pattern, String input) {
        assertParsed(pattern, input, null);
    }

    private void assertParsed(String pattern, String input, Number expected) {
        assertParsedICU4J(pattern, input, expected);

        // Skip the OpenJDK test if the runtime is not OpenJDK
        if (TestUtil.getJavaRuntimeName() != TestUtil.JavaRuntimeName.OpenJDK) {
            return;
        }

        assertParsedOpenJDK(pattern, input, expected);
    }

    private void assertParsedICU4J(String pattern, String input, Number expected) {
        DecimalFormat df = new DecimalFormat(pattern, new DecimalFormatSymbols(ULocale.US));
        df.setParseStrictMode(ParseMode.JAVA_COMPATIBILITY);
        ParsePosition ppos = new ParsePosition(0);
        Number actual = df.parse(input, ppos);
        assertEquals(String.format("pattern: %s input: %s", pattern, input),
                Objects.toString(expected), Objects.toString(actual));
    }

    private void assertParsedOpenJDK(String pattern, String input, Number expected) {
        java.text.DecimalFormat df = new java.text.DecimalFormat(pattern,
                new java.text.DecimalFormatSymbols(Locale.US));
        ParsePosition ppos = new ParsePosition(0);
        Number actual = df.parse(input, ppos);
        assertEquals(String.format("pattern: %s input: %s", pattern, input),
                Objects.toString(expected), Objects.toString(actual));
    }

}
