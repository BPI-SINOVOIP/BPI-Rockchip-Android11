/*
 * Copyright 2016-17, OpenCensus Authors
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

package io.opencensus.trace;

import com.google.errorprone.annotations.MustBeClosed;
import io.opencensus.common.Scope;
import io.opencensus.internal.Utils;
import io.opencensus.trace.SpanBuilder.NoopSpanBuilder;
import java.util.concurrent.Callable;
import javax.annotation.Nullable;

/**
 * Tracer is a simple, thin class for {@link Span} creation and in-process context interaction.
 *
 * <p>Users may choose to use manual or automatic Context propagation. Because of that this class
 * offers APIs to facilitate both usages.
 *
 * <p>The automatic context propagation is done using {@link io.grpc.Context} which is a gRPC
 * independent implementation for in-process Context propagation mechanism which can carry
 * scoped-values across API boundaries and between threads. Users of the library must propagate the
 * {@link io.grpc.Context} between different threads.
 *
 * <p>Example usage with automatic context propagation:
 *
 * <pre>{@code
 * class MyClass {
 *   private static final Tracer tracer = Tracing.getTracer();
 *   void doWork() {
 *     try(Scope ss = tracer.spanBuilder("MyClass.DoWork").startScopedSpan()) {
 *       tracer.getCurrentSpan().addAnnotation("Starting the work.");
 *       doWorkInternal();
 *       tracer.getCurrentSpan().addAnnotation("Finished working.");
 *     }
 *   }
 * }
 * }</pre>
 *
 * <p>Example usage with manual context propagation:
 *
 * <pre>{@code
 * class MyClass {
 *   private static final Tracer tracer = Tracing.getTracer();
 *   void doWork(Span parent) {
 *     Span childSpan = tracer.spanBuilderWithExplicitParent("MyChildSpan", parent).startSpan();
 *     childSpan.addAnnotation("Starting the work.");
 *     try {
 *       doSomeWork(childSpan); // Manually propagate the new span down the stack.
 *     } finally {
 *       // To make sure we end the span even in case of an exception.
 *       childSpan.end();  // Manually end the span.
 *     }
 *   }
 * }
 * }</pre>
 *
 * @since 0.5
 */
public abstract class Tracer {
  private static final NoopTracer noopTracer = new NoopTracer();

  /**
   * Returns the no-op implementation of the {@code Tracer}.
   *
   * @return the no-op implementation of the {@code Tracer}.
   */
  static Tracer getNoopTracer() {
    return noopTracer;
  }

  /**
   * Gets the current Span from the current Context.
   *
   * <p>To install a {@link Span} to the current Context use {@link #withSpan(Span)} OR use {@link
   * SpanBuilder#startScopedSpan} methods to start a new {@code Span}.
   *
   * <p>startSpan methods do NOT modify the current Context {@code Span}.
   *
   * @return a default {@code Span} that does nothing and has an invalid {@link SpanContext} if no
   *     {@code Span} is associated with the current Context, otherwise the current {@code Span}
   *     from the Context.
   * @since 0.5
   */
  public final Span getCurrentSpan() {
    Span currentSpan = CurrentSpanUtils.getCurrentSpan();
    return currentSpan != null ? currentSpan : BlankSpan.INSTANCE;
  }

  /**
   * Enters the scope of code where the given {@link Span} is in the current Context, and returns an
   * object that represents that scope. The scope is exited when the returned object is closed.
   *
   * <p>Supports try-with-resource idiom.
   *
   * <p>Can be called with {@link BlankSpan} to enter a scope of code where tracing is stopped.
   *
   * <p>Example of usage:
   *
   * <pre>{@code
   * private static Tracer tracer = Tracing.getTracer();
   * void doWork() {
   *   // Create a Span as a child of the current Span.
   *   Span span = tracer.spanBuilder("my span").startSpan();
   *   try (Scope ws = tracer.withSpan(span)) {
   *     tracer.getCurrentSpan().addAnnotation("my annotation");
   *     doSomeOtherWork();  // Here "span" is the current Span.
   *   }
   *   span.end();
   * }
   * }</pre>
   *
   * <p>Prior to Java SE 7, you can use a finally block to ensure that a resource is closed
   * regardless of whether the try statement completes normally or abruptly.
   *
   * <p>Example of usage prior to Java SE7:
   *
   * <pre>{@code
   * private static Tracer tracer = Tracing.getTracer();
   * void doWork() {
   *   // Create a Span as a child of the current Span.
   *   Span span = tracer.spanBuilder("my span").startSpan();
   *   Scope ws = tracer.withSpan(span);
   *   try {
   *     tracer.getCurrentSpan().addAnnotation("my annotation");
   *     doSomeOtherWork();  // Here "span" is the current Span.
   *   } finally {
   *     ws.close();
   *   }
   *   span.end();
   * }
   * }</pre>
   *
   * @param span The {@link Span} to be set to the current Context.
   * @return an object that defines a scope where the given {@link Span} will be set to the current
   *     Context.
   * @throws NullPointerException if {@code span} is {@code null}.
   * @since 0.5
   */
  @MustBeClosed
  public final Scope withSpan(Span span) {
    return CurrentSpanUtils.withSpan(Utils.checkNotNull(span, "span"), /* endSpan= */ false);
  }

  /**
   * Returns a {@link Runnable} that runs the given task with the given {@code Span} in the current
   * context.
   *
   * <p>Users may consider to use {@link SpanBuilder#startSpanAndRun(Runnable)}.
   *
   * <p>Any error will end up as a {@link Status#UNKNOWN}.
   *
   * <p>IMPORTANT: Caller must manually propagate the entire {@code io.grpc.Context} when wraps a
   * {@code Runnable}, see the examples.
   *
   * <p>IMPORTANT: Caller must manually end the {@code Span} within the {@code Runnable}, or after
   * the {@code Runnable} is executed.
   *
   * <p>Example with Executor wrapped with {@link io.grpc.Context#currentContextExecutor}:
   *
   * <pre><code>
   * class MyClass {
   *   private static Tracer tracer = Tracing.getTracer();
   *   void handleRequest(Executor executor) {
   *     Span span = tracer.spanBuilder("MyRunnableSpan").startSpan();
   *     executor.execute(tracer.withSpan(span, new Runnable() {
   *      {@literal @}Override
   *       public void run() {
   *         try {
   *           sendResult();
   *         } finally {
   *           span.end();
   *         }
   *       }
   *     }));
   *   }
   * }
   * </code></pre>
   *
   * <p>Example without Executor wrapped with {@link io.grpc.Context#currentContextExecutor}:
   *
   * <pre><code>
   * class MyClass {
   *   private static Tracer tracer = Tracing.getTracer();
   *   void handleRequest(Executor executor) {
   *     Span span = tracer.spanBuilder("MyRunnableSpan").startSpan();
   *     executor.execute(Context.wrap(tracer.withSpan(span, new Runnable() {
   *      {@literal @}Override
   *       public void run() {
   *         try {
   *           sendResult();
   *         } finally {
   *           span.end();
   *         }
   *       }
   *     })));
   *   }
   * }
   * </code></pre>
   *
   * @param span the {@code Span} to be set as current.
   * @param runnable the {@code Runnable} to withSpan in the {@code Span}.
   * @return the {@code Runnable}.
   * @since 0.11.0
   */
  public final Runnable withSpan(Span span, Runnable runnable) {
    return CurrentSpanUtils.withSpan(span, /* endSpan= */ false, runnable);
  }

  /**
   * Returns a {@link Callable} that runs the given task with the given {@code Span} in the current
   * context.
   *
   * <p>Users may consider to use {@link SpanBuilder#startSpanAndCall(Callable)}.
   *
   * <p>Any error will end up as a {@link Status#UNKNOWN}.
   *
   * <p>IMPORTANT: Caller must manually propagate the entire {@code io.grpc.Context} when wraps a
   * {@code Callable}, see the examples.
   *
   * <p>IMPORTANT: Caller must manually end the {@code Span} within the {@code Callable}, or after
   * the {@code Callable} is executed.
   *
   * <p>Example with Executor wrapped with {@link io.grpc.Context#currentContextExecutor}:
   *
   * <pre><code>
   * class MyClass {
   *   private static Tracer tracer = Tracing.getTracer();
   *   void handleRequest(Executor executor) {
   *     Span span = tracer.spanBuilder("MyRunnableSpan").startSpan();
   *     executor.execute(tracer.withSpan(span, {@code new Callable<MyResult>()} {
   *      {@literal @}Override
   *       public MyResult call() throws Exception {
   *         try {
   *           return sendResult();
   *         } finally {
   *           span.end();
   *         }
   *       }
   *     }));
   *   }
   * }
   * </code></pre>
   *
   * <p>Example without Executor wrapped with {@link io.grpc.Context#currentContextExecutor}:
   *
   * <pre><code>
   * class MyClass {
   *   private static Tracer tracer = Tracing.getTracer();
   *   void handleRequest(Executor executor) {
   *     Span span = tracer.spanBuilder("MyRunnableSpan").startSpan();
   *     executor.execute(Context.wrap(tracer.withSpan(span, {@code new Callable<MyResult>()} {
   *      {@literal @}Override
   *       public MyResult call() throws Exception {
   *         try {
   *           return sendResult();
   *         } finally {
   *           span.end();
   *         }
   *       }
   *     })));
   *   }
   * }
   * </code></pre>
   *
   * @param span the {@code Span} to be set as current.
   * @param callable the {@code Callable} to run in the {@code Span}.
   * @return the {@code Callable}.
   * @since 0.11.0
   */
  public final <C> Callable<C> withSpan(Span span, final Callable<C> callable) {
    return CurrentSpanUtils.withSpan(span, /* endSpan= */ false, callable);
  }

  /**
   * Returns a {@link SpanBuilder} to create and start a new child {@link Span} as a child of to the
   * current {@code Span} if any, otherwise creates a root {@code Span}.
   *
   * <p>See {@link SpanBuilder} for usage examples.
   *
   * <p>This <b>must</b> be used to create a {@code Span} when automatic Context propagation is
   * used.
   *
   * <p>This is equivalent with:
   *
   * <pre>{@code
   * tracer.spanBuilderWithExplicitParent("MySpanName",tracer.getCurrentSpan());
   * }</pre>
   *
   * @param spanName The name of the returned Span.
   * @return a {@code SpanBuilder} to create and start a new {@code Span}.
   * @throws NullPointerException if {@code spanName} is {@code null}.
   * @since 0.5
   */
  public final SpanBuilder spanBuilder(String spanName) {
    return spanBuilderWithExplicitParent(spanName, CurrentSpanUtils.getCurrentSpan());
  }

  /**
   * Returns a {@link SpanBuilder} to create and start a new child {@link Span} (or root if parent
   * is {@code null} or has an invalid {@link SpanContext}), with parent being the designated {@code
   * Span}.
   *
   * <p>See {@link SpanBuilder} for usage examples.
   *
   * <p>This <b>must</b> be used to create a {@code Span} when manual Context propagation is used OR
   * when creating a root {@code Span} with a {@code null} parent.
   *
   * @param spanName The name of the returned Span.
   * @param parent The parent of the returned Span. If {@code null} the {@code SpanBuilder} will
   *     build a root {@code Span}.
   * @return a {@code SpanBuilder} to create and start a new {@code Span}.
   * @throws NullPointerException if {@code spanName} is {@code null}.
   * @since 0.5
   */
  public abstract SpanBuilder spanBuilderWithExplicitParent(String spanName, @Nullable Span parent);

  /**
   * Returns a {@link SpanBuilder} to create and start a new child {@link Span} (or root if parent
   * is {@link SpanContext#INVALID} or {@code null}), with parent being the remote {@link Span}
   * designated by the {@link SpanContext}.
   *
   * <p>See {@link SpanBuilder} for usage examples.
   *
   * <p>This <b>must</b> be used to create a {@code Span} when the parent is in a different process.
   * This is only intended for use by RPC systems or similar.
   *
   * <p>If no {@link SpanContext} OR fail to parse the {@link SpanContext} on the server side, users
   * must call this method with a {@code null} remote parent {@code SpanContext}.
   *
   * @param spanName The name of the returned Span.
   * @param remoteParentSpanContext The remote parent of the returned Span.
   * @return a {@code SpanBuilder} to create and start a new {@code Span}.
   * @throws NullPointerException if {@code spanName} is {@code null}.
   * @since 0.5
   */
  public abstract SpanBuilder spanBuilderWithRemoteParent(
      String spanName, @Nullable SpanContext remoteParentSpanContext);

  // No-Op implementation of the Tracer.
  private static final class NoopTracer extends Tracer {

    @Override
    public SpanBuilder spanBuilderWithExplicitParent(String spanName, @Nullable Span parent) {
      return NoopSpanBuilder.createWithParent(spanName, parent);
    }

    @Override
    public SpanBuilder spanBuilderWithRemoteParent(
        String spanName, @Nullable SpanContext remoteParentSpanContext) {
      return NoopSpanBuilder.createWithRemoteParent(spanName, remoteParentSpanContext);
    }

    private NoopTracer() {}
  }

  protected Tracer() {}
}
