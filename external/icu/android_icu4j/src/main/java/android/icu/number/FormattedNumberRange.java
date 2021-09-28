/* GENERATED SOURCE. DO NOT MODIFY. */
// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html#License
package android.icu.number;

import java.io.IOException;
import java.math.BigDecimal;
import java.text.AttributedCharacterIterator;
import java.text.FieldPosition;
import java.util.Arrays;

import android.icu.impl.FormattedStringBuilder;
import android.icu.impl.FormattedValueStringBuilderImpl;
import android.icu.impl.number.DecimalQuantity;
import android.icu.number.NumberRangeFormatter.RangeIdentityResult;
import android.icu.text.ConstrainedFieldPosition;
import android.icu.text.FormattedValue;
import android.icu.util.ICUUncheckedIOException;

/**
 * The result of a number range formatting operation. This class allows the result to be exported in several data types,
 * including a String, an AttributedCharacterIterator, and a BigDecimal.
 *
 * Instances of this class are immutable and thread-safe.
 *
 * @author sffc
 * @see NumberRangeFormatter
 */
public class FormattedNumberRange implements FormattedValue {
    final FormattedStringBuilder string;
    final DecimalQuantity quantity1;
    final DecimalQuantity quantity2;
    final RangeIdentityResult identityResult;

    FormattedNumberRange(FormattedStringBuilder string, DecimalQuantity quantity1, DecimalQuantity quantity2,
            RangeIdentityResult identityResult) {
        this.string = string;
        this.quantity1 = quantity1;
        this.quantity2 = quantity2;
        this.identityResult = identityResult;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public String toString() {
        return string.toString();
    }

    /**
     * {@inheritDoc}
     *
     * @hide unsupported on Android
     */
    @Override
    public <A extends Appendable> A appendTo(A appendable) {
        try {
            appendable.append(string);
        } catch (IOException e) {
            // Throw as an unchecked exception to avoid users needing try/catch
            throw new ICUUncheckedIOException(e);
        }
        return appendable;
    }

    /**
     * {@inheritDoc}
     *
     * @hide draft / provisional / internal are hidden on Android
     */
    @Override
    public char charAt(int index) {
        return string.charAt(index);
    }

    /**
     * {@inheritDoc}
     *
     * @hide draft / provisional / internal are hidden on Android
     */
    @Override
    public int length() {
        return string.length();
    }

    /**
     * {@inheritDoc}
     *
     * @hide draft / provisional / internal are hidden on Android
     */
    @Override
    public CharSequence subSequence(int start, int end) {
        return string.subString(start, end);
    }

    /**
     * {@inheritDoc}
     *
     * @hide draft / provisional / internal are hidden on Android
     */
    @Override
    public boolean nextPosition(ConstrainedFieldPosition cfpos) {
        return FormattedValueStringBuilderImpl.nextPosition(string, cfpos, null);
    }

    /**
     * Determines the start (inclusive) and end (exclusive) indices of the next occurrence of the given
     * <em>field</em> in the output string. This allows you to determine the locations of, for example,
     * the integer part, fraction part, or symbols.
     * <p>
     * If both sides of the range have the same field, the field will occur twice, once before the range separator and
     * once after the range separator, if applicable.
     * <p>
     * If a field occurs just once, calling this method will find that occurrence and return it. If a field occurs
     * multiple times, this method may be called repeatedly with the following pattern:
     *
     * <pre>
     * FieldPosition fpos = new FieldPosition(NumberFormat.Field.INTEGER);
     * while (formattedNumberRange.nextFieldPosition(fpos, status)) {
     *     // do something with fpos.
     * }
     * </pre>
     * <p>
     * This method is useful if you know which field to query. If you want all available field position information, use
     * {@link #toCharacterIterator()}.
     *
     * @param fieldPosition
     *            Input+output variable. See {@link FormattedNumber#nextFieldPosition(FieldPosition)}.
     * @return true if a new occurrence of the field was found; false otherwise.
     * @see android.icu.text.NumberFormat.Field
     * @see NumberRangeFormatter
     * @hide draft / provisional / internal are hidden on Android
     */
    public boolean nextFieldPosition(FieldPosition fieldPosition) {
        return FormattedValueStringBuilderImpl.nextFieldPosition(string, fieldPosition);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public AttributedCharacterIterator toCharacterIterator() {
        return FormattedValueStringBuilderImpl.toCharacterIterator(string, null);
    }

    /**
     * Export the first formatted number as a BigDecimal. This endpoint is useful for obtaining the exact number being
     * printed after scaling and rounding have been applied by the number range formatting pipeline.
     *
     * @return A BigDecimal representation of the first formatted number.
     * @see NumberRangeFormatter
     * @see #getSecondBigDecimal
     */
    public BigDecimal getFirstBigDecimal() {
        return quantity1.toBigDecimal();
    }

    /**
     * Export the second formatted number as a BigDecimal. This endpoint is useful for obtaining the exact number being
     * printed after scaling and rounding have been applied by the number range formatting pipeline.
     *
     * @return A BigDecimal representation of the second formatted number.
     * @see NumberRangeFormatter
     * @see #getFirstBigDecimal
     */
    public BigDecimal getSecondBigDecimal() {
        return quantity2.toBigDecimal();
    }

    /**
     * Returns whether the pair of numbers was successfully formatted as a range or whether an identity fallback was
     * used. For example, if the first and second number were the same either before or after rounding occurred, an
     * identity fallback was used.
     *
     * @return A RangeIdentityType indicating the resulting identity situation in the formatted number range.
     * @see NumberRangeFormatter
     * @see NumberRangeFormatter.RangeIdentityFallback
     */
    public RangeIdentityResult getIdentityResult() {
        return identityResult;
    }

    /**
     * {@inheritDoc}
     *
     * @hide draft / provisional / internal are hidden on Android
     */
    @Override
    public int hashCode() {
        // FormattedStringBuilder and BigDecimal are mutable, so we can't call
        // #equals() or #hashCode() on them directly.
        return Arrays.hashCode(string.toCharArray()) ^ Arrays.hashCode(string.toFieldArray())
                ^ quantity1.toBigDecimal().hashCode() ^ quantity2.toBigDecimal().hashCode();
    }

    /**
     * {@inheritDoc}
     *
     * @hide draft / provisional / internal are hidden on Android
     */
    @Override
    public boolean equals(Object other) {
        if (this == other)
            return true;
        if (other == null)
            return false;
        if (!(other instanceof FormattedNumberRange))
            return false;
        // FormattedStringBuilder and BigDecimal are mutable, so we can't call
        // #equals() or #hashCode() on them directly.
        FormattedNumberRange _other = (FormattedNumberRange) other;
        return Arrays.equals(string.toCharArray(), _other.string.toCharArray())
                && Arrays.equals(string.toFieldArray(), _other.string.toFieldArray())
                && quantity1.toBigDecimal().equals(_other.quantity1.toBigDecimal())
                && quantity2.toBigDecimal().equals(_other.quantity2.toBigDecimal());
    }
}
