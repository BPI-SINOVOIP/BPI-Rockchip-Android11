package android.location.cts.common;

import android.location.Location;
import android.os.CancellationSignal;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.function.Consumer;

public class GetCurrentLocationCapture implements Consumer<Location>, AutoCloseable {

    private final CancellationSignal mCancellationSignal;
    private final CountDownLatch mLatch;
    private Location mLocation;

    public GetCurrentLocationCapture() {
        mCancellationSignal = new CancellationSignal();
        mLatch = new CountDownLatch(1);
    }

    public CancellationSignal getCancellationSignal() {
        return mCancellationSignal;
    }

    public boolean hasLocation(long timeoutMs) throws InterruptedException {
        return mLatch.await(timeoutMs, TimeUnit.MILLISECONDS);
    }

    public Location getLocation(long timeoutMs) throws InterruptedException, TimeoutException {
        if (mLatch.await(timeoutMs, TimeUnit.MILLISECONDS)) {
            return mLocation;
        } else {
            throw new TimeoutException("no location received before timeout");
        }
    }

    @Override
    public void accept(Location location) {
        if (mLatch.getCount() == 0) {
            throw new AssertionError("callback received multiple locations");
        }

        mLocation = location;
        mLatch.countDown();
    }

    @Override
    public void close() {
        mCancellationSignal.cancel();
    }
}