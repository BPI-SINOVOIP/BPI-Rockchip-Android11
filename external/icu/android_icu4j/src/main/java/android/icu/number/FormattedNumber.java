/* GENERATED SOURCE. DO NOT MODIFY. */
// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html#License
package android.icu.number;

import java.math.BigDecimal;
import java.text.AttributedCharacterIterator;
import java.text.FieldPosition;
import java.util.Arrays;

import android.icu.impl.FormattedStringBuilder;
import android.icu.impl.FormattedValueStringBuilderImpl;
import android.icu.impl.Utility;
import android.icu.impl.number.DecimalQuantity;
import android.icu.text.ConstrainedFieldPosition;
import android.icu.text.FormattedValue;
import android.icu.text.PluralRules.IFixedDecimal;

/**
 * The result of a number formatting operation. This class allows the result to be exported in several
 * data types, including a String, an AttributedCharacterIterator, and a BigDecimal.
 *
 * Instances of this class are immutable and thread-safe.
 *
 * @see NumberFormatter
 */
public class FormattedNumber implements FormattedValue {
    final FormattedStringBuilder string;
    final DecimalQuantity fq;

    FormattedNumber(FormattedStringBuilder nsb, DecimalQuantity fq) {
        this.string = nsb;
        this.fq = fq;
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
    public char charAt(int index) {
        return string.charAt(index);
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
     * @hide unsupported on Android
     */
    @Override
    public <A extends Appendable> A appendTo(A appendable) {
        return Utility.appendTo(string, appendable);
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
     * {@inheritDoc}
     */
    @Override
    public AttributedCharacterIterator toCharacterIterator() {
        return FormattedValueStringBuilderImpl.toCharacterIterator(string, null);
    }

    /**
     * Determines the start (inclusive) and end (exclusive) indices of the next occurrence of the
     * given <em>field</em> in the output string. This allows you to determine the locations of,
     * for example, the integer part, fraction part, or symbols.
     * <p>
     * This is a simpler but less powerful alternative to {@link #nextPosition}.
     * <p>
     * If a field occurs just once, calling this method will find that occurrence and return it. If a
     * field occurs multiple times, this method may be called repeatedly with the following pattern:
     *
     * <pre>
     * FieldPosition fpos = new FieldPosition(NumberFormat.Field.GROUPING_SEPARATOR);
     * while (formattedNumber.nextFieldPosition(fpos, status)) {
     *     // do something with fpos.
     * }
     * </pre>
     * <p>
     * This method is useful if you know which field to query. If you want all available field position
     * information, use {@link #nextPosition} or {@link #toCharacterIterator()}.
     *
     * @param fieldPosition
     *            Input+output variable. On input, the "field" property determines which field to look
     *            up, and the "beginIndex" and "endIndex" properties determine where to begin the search.
     *            On output, the "beginIndex" is set to the beginning of the first occurrence of the
     *            field with either begin or end indices after the input indices, "endIndex" is set to
     *            the end of that occurrence of the field (exclusive index). If a field position is not
     *            found, the method returns FALSE and the FieldPosition may or may not be changed.
     * @return true if a new occurrence of the field was found; false otherwise.
     * @see android.icu.text.NumberFormat.Field
     * @see NumberFormatter
     * @hide draft / provisional / internal are hidden on Android
     */
    public boolean nextFieldPosition(FieldPosition fieldPosition) {
        fq.populateUFieldPosition(fieldPosition);
        return FormattedValueStringBuilderImpl.nextFieldPosition(string, fieldPosition);
    }

    /**
     * Export the formatted number as a BigDecimal. This endpoint is useful for obtaining the exact
     * number being printed after scaling and rounding have been applied by the number formatting
     * pipeline.
     *
     * @return A BigDecimal representation of the formatted number.
     * @see NumberFormatter
     */
    public BigDecimal toBigDecimal() {
        return fq.toBigDecimal();
    }

    /**
     * @deprecated This API is ICU internal only.
     * @hide draft / provisional / internal are hidden on Android
     */
    @Deprecated
    public IFixedDecimal getFixedDecimal() {
        return fq;
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
        return Arrays.hashCode(string.toCharArray())
                ^ Arrays.hashCode(string.toFieldArray())
                ^ fq.toBigDecimal().hashCode();
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
        if (!(other instanceof FormattedNumber))
            return false;
        // FormattedStringBuilder and BigDecimal are mutable, so we can't call
        // #equals() or #hashCode() on them directly.
        FormattedNumber _other = (FormattedNumber) other;
        return Arrays.equals(string.toCharArray(), _other.string.toCharArray())
                && Arrays.equals(string.toFieldArray(), _other.string.toFieldArray())
                && fq.toBigDecimal().equals(_other.fq.toBigDecimal());
    }
}