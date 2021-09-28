/*
 * Copyright (C) 2020 The Android Open Source Project
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

import androidx.annotation.NonNull;

/**
 * A generalized interface that allows for generating shared secrets as well as encrypting
 * messages.
 *
 * To use this interface:
 *
 * <p>1. As a client.
 *
 * {@code
 * HandshakeMessage initialClientMessage = clientRunner.initHandshake();
 * sendToServer(initialClientMessage.getNextMessage());
 * byte message = getServerResponse();
 * HandshakeMessage message = clientRunner.continueHandshake(message);
 * }
 *
 * <p>If it is a first-time connection,
 *
 * {@code message.getHandshakeState()} should be VERIFICATION_NEEDED, show user the verification
 * code and ask to verify.
 * After user confirmed, {@code HandshakeMessage lastMessage = clientRunner.verifyPin();} otherwise
 * {@code clientRunner.invalidPin(); }
 *
 * Use {@code lastMessage.getKey()} to get the key for encryption.
 *
 * <p>If it is a reconnection,
 *
 * {@code message.getHandshakeState()} should be RESUMING_SESSION, PIN has been verified blindly,
 * send the authentication message over to server, then authenticate the message from server.
 *
 * {@code
 * clientMessage = clientRunner.initReconnectAuthentication(previousKey)
 * sendToServer(clientMessage.getNextMessage());
 * HandshakeMessage lastMessage = clientRunner.authenticateReconnection(previousKey, message)
 * }
 *
 * {@code lastMessage.getHandshakeState()} should be FINISHED if reconnection handshake is done.
 *
 * <p>2. As a server.
 *
 * {@code
 * byte[] initialMessage = getClientMessageBytes();
 * HandshakeMessage message = serverRunner.respondToInitRequest(initialMessage);
 * sendToClient(message.getNextMessage());
 * byte[] clientMessage = getClientResponse();
 * HandshakeMessage message = serverRunner.continueHandshake(clientMessage);}
 *
 * <p>if it is a first-time connection,
 *
 * {@code message.getHandshakeState()} should be VERIFICATION_NEEDED, show user the verification
 * code and ask to verify.
 * After PIN is confirmed, {@code HandshakeMessage lastMessage = serverRunner.verifyPin}, otherwise
 * {@code clientRunner.invalidPin(); }
 * Use {@code lastMessage.getKey()} to get the key for encryption.
 *
 * <p>If it is a reconnection,
 *
 * {@code message.getHandshakeState()} should be RESUMING_SESSION,PIN has been verified blindly,
 * waiting for client message.
 * After client message been received,
 * {@code serverMessage = serverRunner.authenticateReconnection(previousKey, message);
 * sendToClient(serverMessage.getNextMessage());}
 * {@code serverMessage.getHandshakeState()} should be FINISHED if reconnection handshake is done.
 *
 * Also see {@link EncryptionRunnerTest} for examples.
 */
public interface EncryptionRunner {

    String TAG = "EncryptionRunner";

    /**
     * Starts an encryption handshake.
     *
     * @return A handshake message with information about the handshake that is started.
     */
    @NonNull
    HandshakeMessage initHandshake();

    /**
     * Starts an encryption handshake where the device that is being communicated with already
     * initiated the request.
     *
     * @param initializationRequest the bytes that the other device sent over.
     * @return a handshake message with information about the handshake.
     * @throws HandshakeException if initialization request is invalid.
     */
    @NonNull
    HandshakeMessage respondToInitRequest(@NonNull byte[] initializationRequest)
            throws HandshakeException;

    /**
     * Continues a handshake after receiving another response from the connected device.
     *
     * @param response the response from the other device.
     * @return a message that can be used to continue the handshake.
     * @throws HandshakeException if unexpected bytes in response.
     */
    @NonNull
    HandshakeMessage continueHandshake(@NonNull byte[] response) throws HandshakeException;

    /**
     * Verifies the pin shown to the user. The result is the next handshake message and will
     * typically contain an encryption key.
     *
     * @throws HandshakeException if not in state to verify pin.
     */
    @NonNull
    HandshakeMessage verifyPin() throws HandshakeException;

    /**
     * Notifies the encryption runner that the user failed to validate the pin. After calling this
     * method the runner should not be used, and will throw exceptions.
     */
    void invalidPin();

    /**
     * Verifies the reconnection message.
     *
     * <p>The message passed to this method should have been generated by
     * {@link #initReconnectAuthentication(byte[] previousKey)}.
     *
     * <p>If the message is valid, then a {@link HandshakeMessage} will be returned that contains
     * the encryption key and a handshake message which can be used to verify the other side of the
     * connection.
     *
     * @param previousKey previously stored key.
     * @param message     message from the client
     * @return a handshake message with an encryption key if verification succeed.
     * @throws HandshakeException if the message does not match.
     */
    @NonNull
    HandshakeMessage authenticateReconnection(@NonNull byte[] message, @NonNull byte[] previousKey)
            throws HandshakeException;

    /**
     * Initiates the reconnection verification by generating a message that should be sent to the
     * device that is being reconnected to.
     *
     * @param previousKey previously stored key.
     * @return a handshake message with client's message which will be sent to server.
     * @throws HandshakeException when get encryption key's unique session fail.
     */
    @NonNull
    HandshakeMessage initReconnectAuthentication(@NonNull byte[] previousKey)
            throws HandshakeException;

    /**
     * De-serializes a previously serialized key generated by an instance of this encryption runner.
     *
     * @param serialized the serialized bytes of the key.
     * @return the Key object used for encryption.
     */
    @NonNull
    Key keyOf(@NonNull byte[] serialized);

    /**
     * Set the signal if it is a reconnection process.
     *
     * @param isReconnect {@code true} if it is a reconnect.
     */
    void setIsReconnect(boolean isReconnect);
}
