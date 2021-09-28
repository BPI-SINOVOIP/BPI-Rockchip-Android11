/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.google.android.exoplayer2.testutil;

import android.os.Handler.Callback;
import android.os.Looper;
import android.os.Message;
import androidx.annotation.GuardedBy;
import androidx.annotation.Nullable;
import com.google.android.exoplayer2.util.Clock;
import com.google.android.exoplayer2.util.HandlerWrapper;
import java.util.ArrayList;
import java.util.List;

/** Fake {@link Clock} implementation independent of {@link android.os.SystemClock}. */
public class FakeClock implements Clock {

  private final List<Long> wakeUpTimes;
  private final List<HandlerMessageData> handlerMessages;
  private final long bootTimeMs;

  @GuardedBy("this")
  private long timeSinceBootMs;

  /**
   * Creates a fake clock assuming the system was booted exactly at time {@code 0} (the Unix Epoch)
   * and {@code initialTimeMs} milliseconds have passed since system boot.
   *
   * @param initialTimeMs The initial elapsed time since the boot time, in milliseconds.
   */
  public FakeClock(long initialTimeMs) {
    this(/* bootTimeMs= */ 0, initialTimeMs);
  }

  /**
   * Creates a fake clock specifying when the system was booted and how much time has passed since
   * then.
   *
   * @param bootTimeMs The time the system was booted since the Unix Epoch, in milliseconds.
   * @param initialTimeMs The initial elapsed time since the boot time, in milliseconds.
   */
  public FakeClock(long bootTimeMs, long initialTimeMs) {
    this.bootTimeMs = bootTimeMs;
    this.timeSinceBootMs = initialTimeMs;
    this.wakeUpTimes = new ArrayList<>();
    this.handlerMessages = new ArrayList<>();
  }

  /**
   * Advance timestamp of {@link FakeClock} by the specified duration.
   *
   * @param timeDiffMs The amount of time to add to the timestamp in milliseconds.
   */
  public synchronized void advanceTime(long timeDiffMs) {
    timeSinceBootMs += timeDiffMs;
    for (Long wakeUpTime : wakeUpTimes) {
      if (wakeUpTime <= timeSinceBootMs) {
        notifyAll();
        break;
      }
    }
    for (int i = handlerMessages.size() - 1; i >= 0; i--) {
      if (handlerMessages.get(i).maybeSendToTarget(timeSinceBootMs)) {
        handlerMessages.remove(i);
      }
    }
  }

  @Override
  public synchronized long currentTimeMillis() {
    return bootTimeMs + timeSinceBootMs;
  }

  @Override
  public synchronized long elapsedRealtime() {
    return timeSinceBootMs;
  }

  @Override
  public long uptimeMillis() {
    return elapsedRealtime();
  }

  @Override
  public synchronized void sleep(long sleepTimeMs) {
    if (sleepTimeMs <= 0) {
      return;
    }
    Long wakeUpTimeMs = timeSinceBootMs + sleepTimeMs;
    wakeUpTimes.add(wakeUpTimeMs);
    while (timeSinceBootMs < wakeUpTimeMs) {
      try {
        wait();
      } catch (InterruptedException e) {
        // Ignore InterruptedException as SystemClock.sleep does too.
      }
    }
    wakeUpTimes.remove(wakeUpTimeMs);
  }

  @Override
  public HandlerWrapper createHandler(Looper looper, @Nullable Callback callback) {
    return new ClockHandler(looper, callback);
  }

  /** Adds a handler post to list of pending messages. */
  protected synchronized boolean addHandlerMessageAtTime(
      HandlerWrapper handler, Runnable runnable, long timeMs) {
    if (timeMs <= timeSinceBootMs) {
      return handler.post(runnable);
    }
    handlerMessages.add(new HandlerMessageData(timeMs, handler, runnable));
    return true;
  }

  /** Adds an empty handler message to list of pending messages. */
  protected synchronized boolean addHandlerMessageAtTime(
      HandlerWrapper handler, int message, long timeMs) {
    if (timeMs <= timeSinceBootMs) {
      return handler.sendEmptyMessage(message);
    }
    handlerMessages.add(new HandlerMessageData(timeMs, handler, message));
    return true;
  }

  /** Message data saved to send messages or execute runnables at a later time on a Handler. */
  private static final class HandlerMessageData {

    private final long postTime;
    private final HandlerWrapper handler;
    @Nullable private final Runnable runnable;
    private final int message;

    public HandlerMessageData(long postTime, HandlerWrapper handler, Runnable runnable) {
      this.postTime = postTime;
      this.handler = handler;
      this.runnable = runnable;
      this.message = 0;
    }

    public HandlerMessageData(long postTime, HandlerWrapper handler, int message) {
      this.postTime = postTime;
      this.handler = handler;
      this.runnable = null;
      this.message = message;
    }

    /** Sends the message and returns whether the message was sent to its target. */
    public boolean maybeSendToTarget(long currentTimeMs) {
      if (postTime <= currentTimeMs) {
        if (runnable != null) {
          handler.post(runnable);
        } else {
          handler.sendEmptyMessage(message);
        }
        return true;
      }
      return false;
    }
  }

  /** HandlerWrapper implementation using the enclosing Clock to schedule delayed messages. */
  private final class ClockHandler implements HandlerWrapper {

    private final android.os.Handler handler;

    public ClockHandler(Looper looper, @Nullable Callback callback) {
      handler = new android.os.Handler(looper, callback);
    }

    @Override
    public Looper getLooper() {
      return handler.getLooper();
    }

    @Override
    public Message obtainMessage(int what) {
      return handler.obtainMessage(what);
    }

    @Override
    public Message obtainMessage(int what, @Nullable Object obj) {
      return handler.obtainMessage(what, obj);
    }

    @Override
    public Message obtainMessage(int what, int arg1, int arg2) {
      return handler.obtainMessage(what, arg1, arg2);
    }

    @Override
    public Message obtainMessage(int what, int arg1, int arg2, @Nullable Object obj) {
      return handler.obtainMessage(what, arg1, arg2, obj);
    }

    @Override
    public boolean sendEmptyMessage(int what) {
      return handler.sendEmptyMessage(what);
    }

    @Override
    public boolean sendEmptyMessageAtTime(int what, long uptimeMs) {
      return addHandlerMessageAtTime(this, what, uptimeMs);
    }

    @Override
    public void removeMessages(int what) {
      handler.removeMessages(what);
    }

    @Override
    public void removeCallbacksAndMessages(@Nullable Object token) {
      handler.removeCallbacksAndMessages(token);
    }

    @Override
    public boolean post(Runnable runnable) {
      return handler.post(runnable);
    }

    @Override
    public boolean postDelayed(Runnable runnable, long delayMs) {
      return addHandlerMessageAtTime(this, runnable, uptimeMillis() + delayMs);
    }
  }
}

