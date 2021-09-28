/* GENERATED SOURCE. DO NOT MODIFY. */
package com.android.org.conscrypt;

import java.io.IOException;

/**
 * Utility for creating test client and server endpoints.
 */
interface EndpointFactory {
  ClientEndpoint newClient(ChannelType channelType, int port, String[] protocols,
      String[] ciphers) throws IOException;

  ServerEndpoint newServer(ChannelType channelType, int messageSize,
      String[] protocols, String[] ciphers) throws IOException;
}
