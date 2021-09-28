/*
 * Copyright 2017, OpenCensus Authors
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

package io.opencensus.contrib.agent.instrumentation;

import static com.google.common.truth.Truth.assertThat;

import io.grpc.Context;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.Semaphore;
import java.util.concurrent.atomic.AtomicBoolean;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Integration tests for {@link ExecutorInstrumentation}.
 *
 * <p>The integration tests are executed in a separate JVM that has the OpenCensus agent enabled via
 * the {@code -javaagent} command line option.
 */
@RunWith(JUnit4.class)
@SuppressWarnings("checkstyle:AbbreviationAsWordInName")
public class ExecutorInstrumentationIT {

  private static final Context.Key<String> KEY = Context.key("mykey");

  private ExecutorService executor;
  private Context previousContext;

  @Before
  public void beforeMethod() {
    executor = Executors.newCachedThreadPool();
  }

  @After
  public void afterMethod() {
    Context.current().detach(previousContext);
    executor.shutdown();
  }

  @Test(timeout = 60000)
  public void execute() throws Exception {
    final Thread callerThread = Thread.currentThread();
    final Context context = Context.current().withValue(KEY, "myvalue");
    previousContext = context.attach();

    final Semaphore tested = new Semaphore(0);

    executor.execute(
        new Runnable() {
          @Override
          public void run() {
            assertThat(Thread.currentThread()).isNotSameAs(callerThread);
            assertThat(Context.current()).isSameAs(context);
            assertThat(KEY.get()).isEqualTo("myvalue");
            tested.release();
          }
        });

    tested.acquire();
  }

  @Test(timeout = 60000)
  public void submit_Callable() throws Exception {
    final Thread callerThread = Thread.currentThread();
    final Context context = Context.current().withValue(KEY, "myvalue");
    previousContext = context.attach();

    final AtomicBoolean tested = new AtomicBoolean(false);

    executor
        .submit(
            new Callable<Void>() {
              @Override
              public Void call() throws Exception {
                assertThat(Thread.currentThread()).isNotSameAs(callerThread);
                assertThat(Context.current()).isSameAs(context);
                assertThat(KEY.get()).isEqualTo("myvalue");
                tested.set(true);

                return null;
              }
            })
        .get();

    assertThat(tested.get()).isTrue();
  }

  @Test(timeout = 60000)
  public void submit_Runnable() throws Exception {
    final Thread callerThread = Thread.currentThread();
    final Context context = Context.current().withValue(KEY, "myvalue");
    previousContext = context.attach();

    final AtomicBoolean tested = new AtomicBoolean(false);

    executor
        .submit(
            new Runnable() {
              @Override
              public void run() {
                assertThat(Thread.currentThread()).isNotSameAs(callerThread);
                assertThat(Context.current()).isSameAs(context);
                assertThat(KEY.get()).isEqualTo("myvalue");
                tested.set(true);
              }
            })
        .get();

    assertThat(tested.get()).isTrue();
  }

  @Test(timeout = 60000)
  public void submit_RunnableWithResult() throws Exception {
    final Thread callerThread = Thread.currentThread();
    final Context context = Context.current().withValue(KEY, "myvalue");
    previousContext = context.attach();

    final AtomicBoolean tested = new AtomicBoolean(false);
    Object result = new Object();

    Future<Object> future =
        executor.submit(
            new Runnable() {
              @Override
              public void run() {
                assertThat(Thread.currentThread()).isNotSameAs(callerThread);
                assertThat(Context.current()).isNotSameAs(Context.ROOT);
                assertThat(Context.current()).isSameAs(context);
                assertThat(KEY.get()).isEqualTo("myvalue");
                tested.set(true);
              }
            },
            result);

    assertThat(future.get()).isSameAs(result);
    assertThat(tested.get()).isTrue();
  }

  @Test(timeout = 60000)
  public void currentContextExecutor() throws Exception {
    final Thread callerThread = Thread.currentThread();
    final Context context = Context.current().withValue(KEY, "myvalue");
    previousContext = context.attach();

    final Semaphore tested = new Semaphore(0);

    Context.currentContextExecutor(executor)
        .execute(
            new Runnable() {
              @Override
              public void run() {
                StackTraceElement[] ste = new Exception().fillInStackTrace().getStackTrace();
                assertThat(ste[0].getClassName()).doesNotContain("Context");
                assertThat(ste[1].getClassName()).startsWith("io.grpc.Context$");
                // NB: Actually, we want the Runnable to be wrapped only once, but currently it is
                // still wrapped twice. The two places where the Runnable is wrapped are: (1) the
                // executor implementation itself, e.g. ThreadPoolExecutor, to which the Agent added
                // automatic context propagation, (2) CurrentContextExecutor.
                // ExecutorInstrumentation already avoids adding the automatic context propagation
                // to CurrentContextExecutor, but does not make it a no-op yet. Also see
                // ExecutorInstrumentation#createMatcher.
                assertThat(ste[2].getClassName()).startsWith("io.grpc.Context$");
                assertThat(ste[3].getClassName()).doesNotContain("Context");

                assertThat(Thread.currentThread()).isNotSameAs(callerThread);
                assertThat(Context.current()).isSameAs(context);
                assertThat(KEY.get()).isEqualTo("myvalue");

                tested.release();
              }
            });

    tested.acquire();
  }
}
