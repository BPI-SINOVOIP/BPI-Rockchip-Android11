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

package io.opencensus.trace.propagation;

import io.opencensus.common.ExperimentalApi;
import io.opencensus.internal.Utils;
import io.opencensus.trace.SpanContext;
import java.util.Collections;
import java.util.List;
import javax.annotation.Nullable;

/*>>>
import org.checkerframework.checker.nullness.qual.NonNull;
*/

/**
 * Injects and extracts {@link SpanContext trace identifiers} as text into carriers that travel
 * in-band across process boundaries. Identifiers are often encoded as messaging or RPC request
 * headers.
 *
 * <p>When using http, the carrier of propagated data on both the client (injector) and server
 * (extractor) side is usually an http request. Propagation is usually implemented via library-
 * specific request interceptors, where the client-side injects span identifiers and the server-side
 * extracts them.
 *
 * <p>Example of usage on the client:
 *
 * <pre>{@code
 * private static final Tracer tracer = Tracing.getTracer();
 * private static final TextFormat textFormat = Tracing.getPropagationComponent().getTextFormat();
 * private static final TextFormat.Setter setter = new TextFormat.Setter<HttpURLConnection>() {
 *   public void put(HttpURLConnection carrier, String key, String value) {
 *     carrier.setRequestProperty(field, value);
 *   }
 * }
 *
 * void makeHttpRequest() {
 *   Span span = tracer.spanBuilder("Sent.MyRequest").startSpan();
 *   try (Scope s = tracer.withSpan(span)) {
 *     HttpURLConnection connection =
 *         (HttpURLConnection) new URL("http://myserver").openConnection();
 *     textFormat.inject(span.getContext(), connection, httpURLConnectionSetter);
 *     // Send the request, wait for response and maybe set the status if not ok.
 *   }
 *   span.end();  // Can set a status.
 * }
 * }</pre>
 *
 * <p>Example of usage on the server:
 *
 * <pre>{@code
 * private static final Tracer tracer = Tracing.getTracer();
 * private static final TextFormat textFormat = Tracing.getPropagationComponent().getTextFormat();
 * private static final TextFormat.Getter<HttpRequest> getter = ...;
 *
 * void onRequestReceived(HttpRequest request) {
 *   SpanContext spanContext = textFormat.extract(request, getter);
 *   Span span = tracer.spanBuilderWithRemoteParent("Recv.MyRequest", spanContext).startSpan();
 *   try (Scope s = tracer.withSpan(span)) {
 *     // Handle request and send response back.
 *   }
 *   span.end()
 * }
 * }</pre>
 *
 * @since 0.11
 */
@ExperimentalApi
public abstract class TextFormat {
  private static final NoopTextFormat NOOP_TEXT_FORMAT = new NoopTextFormat();

  /**
   * The propagation fields defined. If your carrier is reused, you should delete the fields here
   * before calling {@link #inject(SpanContext, Object, Setter)}.
   *
   * <p>For example, if the carrier is a single-use or immutable request object, you don't need to
   * clear fields as they couldn't have been set before. If it is a mutable, retryable object,
   * successive calls should clear these fields first.
   *
   * @since 0.11
   */
  // The use cases of this are:
  // * allow pre-allocation of fields, especially in systems like gRPC Metadata
  // * allow a single-pass over an iterator (ex OpenTracing has no getter in TextMap)
  public abstract List<String> fields();

  /**
   * Injects the span context downstream. For example, as http headers.
   *
   * @param spanContext possibly not sampled.
   * @param carrier holds propagation fields. For example, an outgoing message or http request.
   * @param setter invoked for each propagation key to add or remove.
   * @since 0.11
   */
  public abstract <C /*>>> extends @NonNull Object*/> void inject(
      SpanContext spanContext, C carrier, Setter<C> setter);

  /**
   * Class that allows a {@code TextFormat} to set propagated fields into a carrier.
   *
   * <p>{@code Setter} is stateless and allows to be saved as a constant to avoid runtime
   * allocations.
   *
   * @param <C> carrier of propagation fields, such as an http request
   * @since 0.11
   */
  public abstract static class Setter<C> {

    /**
     * Replaces a propagated field with the given value.
     *
     * <p>For example, a setter for an {@link java.net.HttpURLConnection} would be the method
     * reference {@link java.net.HttpURLConnection#addRequestProperty(String, String)}
     *
     * @param carrier holds propagation fields. For example, an outgoing message or http request.
     * @param key the key of the field.
     * @param value the value of the field.
     * @since 0.11
     */
    public abstract void put(C carrier, String key, String value);
  }

  /**
   * Extracts the span context from upstream. For example, as http headers.
   *
   * @param carrier holds propagation fields. For example, an outgoing message or http request.
   * @param getter invoked for each propagation key to get.
   * @throws SpanContextParseException if the input is invalid
   * @since 0.11
   */
  public abstract <C /*>>> extends @NonNull Object*/> SpanContext extract(
      C carrier, Getter<C> getter) throws SpanContextParseException;

  /**
   * Class that allows a {@code TextFormat} to read propagated fields from a carrier.
   *
   * <p>{@code Getter} is stateless and allows to be saved as a constant to avoid runtime
   * allocations.
   *
   * @param <C> carrier of propagation fields, such as an http request
   * @since 0.11
   */
  public abstract static class Getter<C> {

    /**
     * Returns the first value of the given propagation {@code key} or returns {@code null}.
     *
     * @param carrier carrier of propagation fields, such as an http request
     * @param key the key of the field.
     * @return the first value of the given propagation {@code key} or returns {@code null}.
     * @since 0.11
     */
    @Nullable
    public abstract String get(C carrier, String key);
  }

  /**
   * Returns the no-op implementation of the {@code TextFormat}.
   *
   * @return the no-op implementation of the {@code TextFormat}.
   */
  static TextFormat getNoopTextFormat() {
    return NOOP_TEXT_FORMAT;
  }

  private static final class NoopTextFormat extends TextFormat {

    private NoopTextFormat() {}

    @Override
    public List<String> fields() {
      return Collections.emptyList();
    }

    @Override
    public <C /*>>> extends @NonNull Object*/> void inject(
        SpanContext spanContext, C carrier, Setter<C> setter) {
      Utils.checkNotNull(spanContext, "spanContext");
      Utils.checkNotNull(carrier, "carrier");
      Utils.checkNotNull(setter, "setter");
    }

    @Override
    public <C /*>>> extends @NonNull Object*/> SpanContext extract(C carrier, Getter<C> getter) {
      Utils.checkNotNull(carrier, "carrier");
      Utils.checkNotNull(getter, "getter");
      return SpanContext.INVALID;
    }
  }
}
