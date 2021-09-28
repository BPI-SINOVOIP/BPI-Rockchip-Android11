// © 2020 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html#License

package android.icu.extratest.number;

import android.icu.dev.test.TestFmwk;
import android.icu.number.FormattedNumberRange;
import android.icu.number.NumberRangeFormatter;
import android.icu.number.NumberRangeFormatter.RangeIdentityResult;
import android.icu.testsharding.MainTestShard;
import android.icu.util.ULocale;
import java.math.BigDecimal;
import java.math.BigInteger;
import java.text.AttributedCharacterIterator;
import java.text.CharacterIterator;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@MainTestShard
@RunWith(JUnit4.class)
public class FormattedNumberRangeTest extends TestFmwk {

    @Test
    public void test_enUS() {
        FormattedNumberRange range = NumberRangeFormatter.withLocale(ULocale.US)
            .formatRange(-99.99d, 123.45d);
        String expected = "-99.99 – 123.45";
        assertEquals("Test toString()", expected, range.toString());
        assertEquals("Test charAt()", '-', range.charAt(0));
        assertEquals("Test length()", 15, range.length());
        assertEquals("Test subSequence()", "9.9", range.subSequence(2,5));
        assertEquals("Test getIdentityResult()", RangeIdentityResult.NOT_EQUAL,
            range.getIdentityResult());
        assertEquals("Test getFirstBigDecimal()", new BigDecimal(BigInteger.valueOf(-9999), 2),
            range.getFirstBigDecimal());
        assertEquals("Test getSecondBigDecimal()", new BigDecimal(BigInteger.valueOf(12345), 2),
            range.getSecondBigDecimal());

        AttributedCharacterIterator iter = range.toCharacterIterator();
        for (char c = iter.first(); c != CharacterIterator.DONE; c = iter.next()) {
            assertEquals("Test toCharacterIterator():" + iter.getIndex(),
                expected.charAt(iter.getIndex()), c);
        }
    }
}
