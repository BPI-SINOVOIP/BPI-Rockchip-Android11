package com.android.helpers;

import java.util.Map;

public interface ICollectorHelper<T> {

    /**
     * This method will have setup to start collecting the metrics.
     */
    boolean startCollecting();

    /**
     * This method will retrieve the metrics.
     */
    Map<String, T> getMetrics();

    /**
     * This method will do the tear down to stop collecting the metrics.
     */
    boolean stopCollecting();

}
