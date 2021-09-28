package com.android.nn.benchmark.app;

import android.app.Application;

import com.android.nn.benchmark.core.TestModelsListLoader;

import java.io.IOException;

public class BenchmarkApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        try {
            TestModelsListLoader.parseFromAssets(getAssets());
        } catch (IOException e) {
            throw new IllegalStateException("Failed to load test models json", e);
        }

    }
}
