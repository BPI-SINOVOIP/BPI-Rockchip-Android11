package android.signature.cts;

import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.Spliterator;
import java.util.function.Consumer;
import java.util.function.Function;

/**
 * A spliterator breaking up a ByteBuffer into lines of text. A converter will translate a line to
 * an arbitrary type. This spliterator allows splitting.
 *
 * This class assumes that there is no issue looking for just '\n'. That is not true for all of
 * unicode and must be guaranteed by the caller.
 */
public class ByteBufferLineSpliterator<T> implements Spliterator<T> {
    private final ByteBuffer mBuffer;
    private final int mLineLengthEstimate;
    private final Function<String, T> mConverter;

    private static final Charset CHAR_SET = Charset.defaultCharset();

    public ByteBufferLineSpliterator(ByteBuffer buffer, int lineLengthEstimate,
            Function<String, T> converter) {
        mBuffer = buffer;
        mLineLengthEstimate = lineLengthEstimate;
        mConverter = converter;
    }

    @Override
    public Spliterator<T> trySplit() {
        // Approach: jump into the middle of the remaining segment and look
        // for the next line break. If there is none, return null. Otherwise
        // initialize a return value with a subsequence, and limit the
        // current buffer.
        //
        // This relies heavily on lines not having outliers (wrt/ length).

        // Only attempt if there's enough "work" left.
        if (mBuffer.remaining() < 10 * mLineLengthEstimate) {
            return null;
        }

        int curPos = mBuffer.position();
        mBuffer.position(curPos + mBuffer.remaining() / 2);
        int nextNewLine = findNextNewLine();
        if (nextNewLine == -1) {
            return null;
        }

        mBuffer.position(mBuffer.position() + nextNewLine + 1);
        Spliterator<T> retValue = new ByteBufferLineSpliterator<T>(mBuffer.slice(),
                mLineLengthEstimate, mConverter);

        // Reset this buffer and set limit.
        mBuffer.position(curPos).limit(curPos + mBuffer.remaining() / 2 + nextNewLine);

        return retValue;
    }

    @Override
    public long estimateSize() {
        return mBuffer.remaining() / mLineLengthEstimate;
    }

    @Override
    public int characteristics() {
        return ORDERED | DISTINCT | NONNULL | IMMUTABLE;
    }

    private int findNextNewLine() {
        if (!mBuffer.hasRemaining()) {
            return -1;
        }
        mBuffer.mark();
        int index = 0;
        try {
            while (mBuffer.hasRemaining()) {
                if (mBuffer.get() == '\n') {
                    return index;
                }
                index++;
            }
            return -1;
        } finally {
            mBuffer.reset();
        }
    }

    private ByteBuffer subsequence(int start, int end) {
        int curPos = mBuffer.position();
        mBuffer.position(curPos + start);
        int curLimit = mBuffer.limit();
        mBuffer.limit(curPos + end);
        ByteBuffer retValue = mBuffer.slice();
        mBuffer.position(curPos).limit(curLimit);
        return retValue;
    }

    protected String nextLine() {
        if (!mBuffer.hasRemaining()) {
            return null;
        }

        int nextLineBreakIndex = findNextNewLine();
        if (nextLineBreakIndex == 0) {
            throw new IllegalStateException("Empty line.");
        }
        String line;
        if (nextLineBreakIndex > 0) {
            line = CHAR_SET.decode(subsequence(0, nextLineBreakIndex)).toString();
            mBuffer.position(mBuffer.position() + nextLineBreakIndex + 1);
        } else {
            line = CHAR_SET.decode(mBuffer).toString();
            mBuffer.position(mBuffer.limit()); // Should not be necessary?
        }
        return line;
    }

    @Override
    public boolean tryAdvance(Consumer<? super T> action) {
        String nextLine = nextLine();
        if (nextLine == null) {
            return false;
        }
        action.accept(mConverter.apply(nextLine));
        return true;
    }
}
