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

/**
 * An encryption runner that doesn't actually do encryption. Useful for debugging out of band
 * association. Do not use in production environments.
 */
public class OobFakeEncryptionRunner extends FakeEncryptionRunner {

    @Override
    public HandshakeMessage continueHandshake(byte[] response) throws HandshakeException {
        if (getState() != HandshakeMessage.HandshakeState.IN_PROGRESS) {
            throw new HandshakeException("not waiting for response but got one");
        }

        @HandshakeMessage.HandshakeState int newState =
                HandshakeMessage.HandshakeState.OOB_VERIFICATION_NEEDED;
        switch (getMode()) {
            case Mode.SERVER:
                if (!CLIENT_RESPONSE.equals(new String(response))) {
                    throw new HandshakeException("unexpected response: " + new String(response));
                }
                setState(newState);
                return HandshakeMessage.newBuilder()
                        .setOobVerificationCode(VERIFICATION_CODE.getBytes())
                        .setHandshakeState(newState)
                        .build();
            case Mode.CLIENT:
                if (!INIT_RESPONSE.equals(new String(response))) {
                    throw new HandshakeException("unexpected response: " + new String(response));
                }
                setState(newState);
                return HandshakeMessage.newBuilder()
                        .setHandshakeState(newState)
                        .setNextMessage(CLIENT_RESPONSE.getBytes())
                        .setOobVerificationCode(VERIFICATION_CODE.getBytes())
                        .build();
            default:
                throw new IllegalStateException("unexpected role: " + getMode());
        }
    }

    @Override
    public HandshakeMessage verifyPin() throws HandshakeException {
        @HandshakeMessage.HandshakeState int state = getState();
        if (state != HandshakeMessage.HandshakeState.OOB_VERIFICATION_NEEDED) {
            throw new IllegalStateException("asking to verify pin, state = " + state);
        }
        state = HandshakeMessage.HandshakeState.FINISHED;
        return HandshakeMessage.newBuilder().setKey(new FakeKey()).setHandshakeState(
                state).build();
    }
}
