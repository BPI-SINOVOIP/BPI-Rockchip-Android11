/*
 * Copyright 2020 The Android Open Source Project
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

package android.car.encryptionrunner;

import com.google.security.cryptauth.lib.securegcm.Ukey2Handshake;

/**
 * An {@link EncryptionRunner} that uses Ukey2 as the underlying implementation, and generates a
 * longer token for the out of band verification step.
 *
 * <p>To use this class:
 *
 * <p>1. As a client.
 *
 * <p>{@code
 * HandshakeMessage initialClientMessage = clientRunner.initHandshake();
 * sendToServer(initialClientMessage.getNextMessage());
 * byte message = getServerResponse();
 * HandshakeMessage message = clientRunner.continueHandshake(message);
 * }
 *
 * <p>If it is a first-time connection,
 *
 * <p>{@code message.getHandshakeState()} should be OOB_VERIFICATION_NEEDED. Wait for an encrypted
 * message sent from the server, and decrypt that message with an out of band key that was generated
 * before the start of the handshake.
 *
 * <p>After confirming that the decrypted message matches the verification code, send an encrypted
 * message back to the server, and call {@code HandshakeMessage lastMessage =
 * clientRunner.verifyPin();} otherwise {@code clientRunner.invalidPin(); }
 *
 * <p>Use {@code lastMessage.getKey()} to get the key for encryption.
 *
 * <p>If it is a reconnection,
 *
 * <p>{@code message.getHandshakeState()} should be RESUMING_SESSION, PIN has been verified blindly,
 * send the authentication message over to server, then authenticate the message from server.
 *
 * <p>{@code
 * clientMessage = clientRunner.initReconnectAuthentication(previousKey)
 * sendToServer(clientMessage.getNextMessage());
 * HandshakeMessage lastMessage = clientRunner.authenticateReconnection(previousKey, message)
 * }
 *
 * <p>{@code lastMessage.getHandshakeState()} should be FINISHED if reconnection handshake is done.
 *
 * <p>2. As a server.
 *
 * <p>{@code
 * byte[] initialMessage = getClientMessageBytes();
 * HandshakeMessage message = serverRunner.respondToInitRequest(initialMessage);
 * sendToClient(message.getNextMessage());
 * byte[] clientMessage = getClientResponse();
 * HandshakeMessage message = serverRunner.continueHandshake(clientMessage);}
 *
 * <p>if it is a first-time connection,
 *
 * <p>{@code message.getHandshakeState()} should be OOB_VERIFICATION_NEEDED, send the verification
 * code to the client, encrypted using an out of band key generated before the start of the
 * handshake, and wait for a response from the client.
 * If the decrypted message from the client matches the verification code, call {@code
 * HandshakeMessage lastMessage = serverRunner.verifyPin}, otherwise
 * {@code clientRunner.invalidPin(); }
 * Use {@code lastMessage.getKey()} to get the key for encryption.
 *
 * <p>If it is a reconnection,
 *
 * <p>{@code message.getHandshakeState()} should be RESUMING_SESSION,PIN has been verified blindly,
 * waiting for client message.
 * After client message been received,
 * {@code serverMessage = serverRunner.authenticateReconnection(previousKey, message);
 * sendToClient(serverMessage.getNextMessage());}
 * {@code serverMessage.getHandshakeState()} should be FINISHED if reconnection handshake is done.
 *
 * <p>Also see {@link EncryptionRunnerTest} for examples.
 */
public final class OobUkey2EncryptionRunner extends Ukey2EncryptionRunner {
    // Choose max verification string length supported by Ukey2
    private static final int VERIFICATION_STRING_LENGTH = 32;

    @Override
    public HandshakeMessage continueHandshake(byte[] response) throws HandshakeException {
        checkInitialized();

        Ukey2Handshake uKey2Client = getUkey2Client();

        try {
            if (uKey2Client.getHandshakeState() != Ukey2Handshake.State.IN_PROGRESS) {
                throw new IllegalStateException(
                        "handshake is not in progress, state =" + uKey2Client.getHandshakeState());
            }
            uKey2Client.parseHandshakeMessage(response);

            // Not obvious from ukey2 api, but getting the next message can change the state.
            // calling getNext message might go from in progress to verification needed, on
            // the assumption that we already send this message to the peer.
            byte[] nextMessage = null;
            if (uKey2Client.getHandshakeState() == Ukey2Handshake.State.IN_PROGRESS) {
                nextMessage = uKey2Client.getNextHandshakeMessage();
            }

            byte[] verificationCode = null;
            if (uKey2Client.getHandshakeState() == Ukey2Handshake.State.VERIFICATION_NEEDED) {
                // getVerificationString() needs to be called before notifyPinVerified().
                verificationCode = uKey2Client.getVerificationString(VERIFICATION_STRING_LENGTH);
                if (isReconnect()) {
                    HandshakeMessage handshakeMessage = verifyPin();
                    return HandshakeMessage.newBuilder()
                            .setHandshakeState(handshakeMessage.getHandshakeState())
                            .setNextMessage(nextMessage)
                            .build();
                }
            }

            return HandshakeMessage.newBuilder()
                    .setHandshakeState(HandshakeMessage.HandshakeState.OOB_VERIFICATION_NEEDED)
                    .setNextMessage(nextMessage)
                    .setOobVerificationCode(verificationCode)
                    .build();
        } catch (com.google.security.cryptauth.lib.securegcm.HandshakeException
                | Ukey2Handshake.AlertException e) {
            throw new HandshakeException(e);
        }
    }
}
