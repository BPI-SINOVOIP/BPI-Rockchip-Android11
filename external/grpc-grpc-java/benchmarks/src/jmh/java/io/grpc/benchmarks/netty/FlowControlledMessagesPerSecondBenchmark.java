/*
 * Copyright 2015 The gRPC Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package io.grpc.benchmarks.netty;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;
import java.util.logging.Logger;
import org.openjdk.jmh.annotations.AuxCounters;
import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.Fork;
import org.openjdk.jmh.annotations.Level;
import org.openjdk.jmh.annotations.Param;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.Setup;
import org.openjdk.jmh.annotations.State;
import org.openjdk.jmh.annotations.TearDown;

/**
 * Benchmark measuring messages per second received from a streaming server. The server
 * is obeying outbound flow-control.
 */
@State(Scope.Benchmark)
@Fork(1)
public class FlowControlledMessagesPerSecondBenchmark extends AbstractBenchmark {

  private static final Logger logger =
      Logger.getLogger(FlowControlledMessagesPerSecondBenchmark.class.getName());

  @Param({"1", "2", "4"})
  public int channelCount = 1;

  @Param({"1", "2", "10", "100"})
  public int maxConcurrentStreams = 1;

  @Param
  public ExecutorType clientExecutor = ExecutorType.DIRECT;

  @Param({"SMALL"})
  public MessageSize responseSize = MessageSize.SMALL;

  private static AtomicLong callCounter;
  private AtomicBoolean completed;
  private AtomicBoolean record;
  private CountDownLatch latch;

  /**
   * Use an AuxCounter so we can measure that calls as they occur without consuming CPU
   * in the benchmark method.
   */
  @AuxCounters
  @State(Scope.Thread)
  public static class AdditionalCounters {

    @Setup(Level.Iteration)
    public void clean() {
      callCounter.set(0);
    }

    public long messagesPerSecond() {
      return callCounter.get();
    }
  }

  /**
   * Setup with direct executors, small payloads and the default flow-control window.
   */
  @Setup(Level.Trial)
  public void setup() throws Exception {
    super.setup(clientExecutor,
        ExecutorType.DIRECT,
        MessageSize.SMALL,
        responseSize,
        FlowWindowSize.MEDIUM,
        ChannelType.NIO,
        maxConcurrentStreams,
        channelCount);
    callCounter = new AtomicLong();
    completed = new AtomicBoolean();
    record = new AtomicBoolean();
    latch =
        startFlowControlledStreamingCalls(maxConcurrentStreams, callCounter, record, completed, 1);
  }

  /**
   * Stop the running calls then stop the server and client channels.
   */
  @Override
  @TearDown(Level.Trial)
  public void teardown() throws Exception {
    completed.set(true);
    if (!latch.await(5, TimeUnit.SECONDS)) {
      logger.warning("Failed to shutdown all calls.");
    }

    super.teardown();
  }

  /**
   * Measure the rate of messages received. The calls are already running, we just observe a counter
   * of received responses.
   */
  @Benchmark
  public void stream(AdditionalCounters counters) throws Exception {
    record.set(true);
    // No need to do anything, just sleep here.
    Thread.sleep(1001);
    record.set(false);
  }

  /**
   * Useful for triggering a subset of the benchmark in a profiler.
   */
  public static void main(String[] argv) throws Exception {
    FlowControlledMessagesPerSecondBenchmark bench = new FlowControlledMessagesPerSecondBenchmark();
    bench.setup();
    Thread.sleep(30000);
    bench.teardown();
    System.exit(0);
  }
}
