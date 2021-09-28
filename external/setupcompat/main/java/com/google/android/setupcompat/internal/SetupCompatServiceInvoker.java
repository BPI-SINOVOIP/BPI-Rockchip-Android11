/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.android.setupcompat.internal;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Bundle;
import android.os.RemoteException;
import androidx.annotation.VisibleForTesting;
import android.util.Log;
import com.google.android.setupcompat.ISetupCompatService;
import com.google.android.setupcompat.logging.internal.SetupMetricsLoggingConstants.MetricType;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * This class is responsible for safely executing methods on SetupCompatService. To avoid memory
 * issues due to backed up queues, an upper bound of {@link
 * ExecutorProvider#SETUP_METRICS_LOGGER_MAX_QUEUED} is set on the logging executor service's queue
 * and {@link ExecutorProvider#SETUP_COMPAT_BINDBACK_MAX_QUEUED} on the overall executor service.
 * Once the upper bound is reached, metrics published after this event are dropped silently.
 *
 * <p>NOTE: This class is not meant to be used directly. Please use {@link
 * com.google.android.setupcompat.logging.SetupMetricsLogger} for publishing metric events.
 */
public class SetupCompatServiceInvoker {

  public void logMetricEvent(@MetricType int metricType, Bundle args) {
    try {
      loggingExecutor.execute(() -> invokeLogMetric(metricType, args));
    } catch (RejectedExecutionException e) {
      Log.e(TAG, String.format("Metric of type %d dropped since queue is full.", metricType), e);
    }
  }

  public void bindBack(String screenName, Bundle bundle) {
    try {
      setupCompatExecutor.execute(() -> invokeBindBack(screenName, bundle));
    } catch (RejectedExecutionException e) {
      Log.e(TAG, String.format("Screen %s bind back fail.", screenName), e);
    }
  }

  private void invokeLogMetric(
      @MetricType int metricType, @SuppressWarnings("unused") Bundle args) {
    try {
      ISetupCompatService setupCompatService =
          SetupCompatServiceProvider.get(
              context, waitTimeInMillisForServiceConnection, TimeUnit.MILLISECONDS);
      if (setupCompatService != null) {
        setupCompatService.logMetric(metricType, args, Bundle.EMPTY);
      } else {
        Log.w(TAG, "logMetric failed since service reference is null. Are the permissions valid?");
      }
    } catch (InterruptedException | TimeoutException | RemoteException e) {
      Log.e(TAG, String.format("Exception occurred while trying to log metric = [%s]", args), e);
    }
  }

  private void invokeBindBack(String screenName, Bundle bundle) {
    try {
      ISetupCompatService setupCompatService =
          SetupCompatServiceProvider.get(
              context, waitTimeInMillisForServiceConnection, TimeUnit.MILLISECONDS);
      if (setupCompatService != null) {
        setupCompatService.validateActivity(screenName, bundle);
      } else {
        Log.w(TAG, "BindBack failed since service reference is null. Are the permissions valid?");
      }
    } catch (InterruptedException | TimeoutException | RemoteException e) {
      Log.e(
          TAG,
          String.format("Exception occurred while %s trying bind back to SetupWizard.", screenName),
          e);
    }
  }

  private SetupCompatServiceInvoker(Context context) {
    this.context = context;
    this.loggingExecutor = ExecutorProvider.setupCompatServiceInvoker.get();
    this.setupCompatExecutor = ExecutorProvider.setupCompatExecutor.get();
    this.waitTimeInMillisForServiceConnection = MAX_WAIT_TIME_FOR_CONNECTION_MS;
  }

  private final Context context;

  private final ExecutorService loggingExecutor;
  private final ExecutorService setupCompatExecutor;
  private final long waitTimeInMillisForServiceConnection;

  public static synchronized SetupCompatServiceInvoker get(Context context) {
    if (instance == null) {
      instance = new SetupCompatServiceInvoker(context.getApplicationContext());
    }

    return instance;
  }

  @VisibleForTesting
  static void setInstanceForTesting(SetupCompatServiceInvoker testInstance) {
    instance = testInstance;
  }

  // The instance is coming from Application context which alive during the application activate and
  // it's not depend on the activities life cycle, so we can avoid memory leak. However linter
  // cannot distinguish Application context or activity context, so we add @SuppressLint to avoid
  // lint error.
  @SuppressLint("StaticFieldLeak")
  private static SetupCompatServiceInvoker instance;

  private static final long MAX_WAIT_TIME_FOR_CONNECTION_MS = TimeUnit.SECONDS.toMillis(10);
  private static final String TAG = "SucServiceInvoker";
}
