/*
 * Copyright 2018 The gRPC Authors
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

package io.grpc.internal;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import io.grpc.CallOptions;
import io.grpc.Metadata;
import io.grpc.Status;
import io.grpc.internal.ClientStreamListener.RpcProgress;
import io.grpc.testing.TestMethodDescriptors;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Unit tests for {@link FailingClientTransport}.
 */
@RunWith(JUnit4.class)
public class FailingClientTransportTest {

  @Test
  public void newStreamStart() {
    Status error = Status.UNAVAILABLE;
    RpcProgress rpcProgress = RpcProgress.DROPPED;
    FailingClientTransport transport = new FailingClientTransport(error, rpcProgress);
    ClientStream stream = transport
        .newStream(TestMethodDescriptors.voidMethod(), new Metadata(), CallOptions.DEFAULT);
    ClientStreamListener listener = mock(ClientStreamListener.class);
    stream.start(listener);

    verify(listener).closed(eq(error), eq(rpcProgress), any(Metadata.class));
  }
}
