package android.signature.cts;

import java.io.BufferedReader;
import java.io.IOException;
import java.util.Spliterator;
import java.util.function.BiFunction;
import java.util.function.Consumer;
import java.util.function.Function;

/**
 * A spliterator breaking up a ByteBuffer into lines of text. A converter will translate a line to
 * an arbitrary type. This spliterator allows splitting.
 *
 * This class assumes that there is no issue looking for just '\n'. That is not true for all of
 * unicode and must be guaranteed by the caller.
 */
public class BufferedReaderLineSpliterator<T> implements Spliterator<T> {
    private final BufferedReader mReader;
    private int mLineNum;
    private final Function<String, T> mConverter;
    private final BiFunction<String, Integer, T> mLineNumConverter;

    public BufferedReaderLineSpliterator(BufferedReader reader, Function<String, T> converter) {
        mReader = reader;
        mLineNum = 0;
        mConverter = converter;
        mLineNumConverter = null;
    }

    public BufferedReaderLineSpliterator(BufferedReader reader,
            BiFunction<String, Integer, T> converter) {
        mReader = reader;
        mLineNum = 0;
        mConverter = null;
        mLineNumConverter = converter;
    }

    @Override
    public Spliterator<T> trySplit() {
        return null;
    }

    @Override
    public long estimateSize() {
        return Long.MAX_VALUE;
    }

    @Override
    public int characteristics() {
        return ORDERED | DISTINCT | NONNULL | IMMUTABLE;
    }

    private String nextLine() {
        try {
            String line = mReader.readLine();
            if (line != null) {
                mLineNum = mLineNum + 1;
            }
            return line;
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public boolean tryAdvance(Consumer<? super T> action) {
        String nextLine = nextLine();
        if (nextLine == null) {
            return false;
        }
        if (mConverter != null) {
            action.accept(mConverter.apply(nextLine));
        } else {
            action.accept(mLineNumConverter.apply(nextLine, mLineNum));
        }
        return true;
    }
}
