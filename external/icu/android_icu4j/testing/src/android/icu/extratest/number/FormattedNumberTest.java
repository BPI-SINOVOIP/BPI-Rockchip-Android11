// © 2020 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html#License

package android.icu.extratest.number;

import android.icu.dev.test.TestFmwk;
import android.icu.number.FormattedNumber;
import android.icu.number.NumberFormatter;
import android.icu.testsharding.MainTestShard;
import android.icu.util.ULocale;
import java.math.BigDecimal;
import java.math.BigInteger;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@MainTestShard
@RunWith(JUnit4.class)
public class FormattedNumberTest extends TestFmwk {

    @Test
    public void test_enUS() {
        FormattedNumber formattedNumber =
            NumberFormatter.withLocale(ULocale.US).format(123.45);
        assertEquals("Test toString()", "123.45", formattedNumber.toString());
        assertEquals("Test charAt()", '2', formattedNumber.charAt(1));
        assertEquals("Test length()", 6, formattedNumber.length());
        assertEquals("Test subSequence()", "3.4", formattedNumber.subSequence(2,5));
        assertEquals("Test toBigDecimal()", new BigDecimal(BigInteger.valueOf(12345), 2),
            formattedNumber.toBigDecimal());
    }
}
