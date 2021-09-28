/*
 * Copyright (C) 2019 The Android Open Source Project
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

import static com.google.common.truth.Truth.assertThat;

import android.util.Log;

import junit.framework.Assert;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class EncryptionRunnerTest {

    private Key mClientKey;
    private Key mServerKey;
    private byte[] mData = "testData".getBytes();

    private interface RunnerFactory {
        EncryptionRunner newRunner();
    }

    private interface HandshakeVerifier {
        void verifyHandshake(EncryptionRunner clientRunner, EncryptionRunner serverRunner)
                throws Exception;
    }

    @Test
    public void happyFlow_dummyRunner() throws Exception {
        verifyRunners(EncryptionRunnerFactory::newDummyRunner,
                EncryptionRunnerTest::verifyHandshake);
    }

    @Test
    public void happyFlow_ukey2Runner() throws Exception {
        verifyRunners(EncryptionRunnerTest::newRunner, EncryptionRunnerTest::verifyHandshake);
    }

    @Test
    public void happyFlow_oobUkey2Runner() throws Exception {
        verifyRunners(EncryptionRunnerTest::newOobRunner, EncryptionRunnerTest::verifyOobHandshake);
    }

    @Test
    public void happyFlow_dummyRunner_reconnect() throws Exception {
        setUpFirstConnection(EncryptionRunnerFactory::newDummyRunner,
                EncryptionRunnerTest::verifyHandshake);
        verifyRunnersReconnect(EncryptionRunnerFactory::newDummyRunner);
    }

    @Test
    public void happyFlow_uKey2Runner_reconnect() throws Exception {
        setUpFirstConnection(EncryptionRunnerTest::newRunner,
                EncryptionRunnerTest::verifyHandshake);
        verifyRunnersReconnect(EncryptionRunnerTest::newRunner);
    }

    @Test
    public void happyFlow_oobUey2Runner_reconnect() throws Exception {
        setUpFirstConnection(EncryptionRunnerTest::newOobRunner,
                EncryptionRunnerTest::verifyOobHandshake);
        verifyRunnersReconnect(EncryptionRunnerTest::newOobRunner);
    }

    @Test
    public void uKey2Runner_reconnect_encrypt_and_decrypt() throws Exception {
        setUpFirstConnection(EncryptionRunnerTest::newRunner,
                EncryptionRunnerTest::verifyHandshake);
        setUpReconnection(EncryptionRunnerTest::newRunner, EncryptionRunnerTest::verifyHandshake);
        assertThat(mClientKey.decryptData(mServerKey.encryptData(mData))).isEqualTo(mData);
    }

    @Test
    public void dummyRunner_reconnect_encrypt_and_decrypt() throws Exception {
        setUpFirstConnection(EncryptionRunnerFactory::newDummyRunner,
                EncryptionRunnerTest::verifyHandshake);
        setUpReconnection(EncryptionRunnerFactory::newDummyRunner,
                EncryptionRunnerTest::verifyHandshake);
        assertThat(mClientKey.decryptData(mServerKey.encryptData(mData))).isEqualTo(mData);
    }

    @Test
    public void oobUkey2Runner_reconnect_encrypt_and_decrypt() throws Exception {
        setUpFirstConnection(EncryptionRunnerTest::newOobRunner,
                EncryptionRunnerTest::verifyOobHandshake);
        setUpReconnection(EncryptionRunnerTest::newOobRunner,
                EncryptionRunnerTest::verifyOobHandshake);
        assertThat(mClientKey.decryptData(mServerKey.encryptData(mData))).isEqualTo(mData);
    }

    private static EncryptionRunner newRunner() {
        return EncryptionRunnerFactory.newRunner(
                EncryptionRunnerFactory.EncryptionRunnerType.UKEY2);
    }

    private static EncryptionRunner newOobRunner() {
        return EncryptionRunnerFactory.newRunner(
                EncryptionRunnerFactory.EncryptionRunnerType.OOB_UKEY2);
    }

    private void setUpFirstConnection(RunnerFactory runnerFactory,
            HandshakeVerifier handshakeVerifier) throws Exception {
        EncryptionRunner clientRunner = runnerFactory.newRunner();
        EncryptionRunner serverRunner = runnerFactory.newRunner();
        handshakeVerifier.verifyHandshake(clientRunner, serverRunner);
        HandshakeMessage finalServerMessage = serverRunner.verifyPin();
        HandshakeMessage finalClientMessage = clientRunner.verifyPin();
        mServerKey = finalServerMessage.getKey();
        mClientKey = finalClientMessage.getKey();
    }

    private void setUpReconnection(RunnerFactory runnerFactory, HandshakeVerifier handshakeVerifier)
            throws Exception {
        setUpFirstConnection(runnerFactory, handshakeVerifier);
        EncryptionRunner clientRunner = runnerFactory.newRunner();
        EncryptionRunner serverRunner = runnerFactory.newRunner();
        verifyHandshakeReconnect(clientRunner, serverRunner);
        HandshakeMessage nextClientMessage =
                clientRunner.initReconnectAuthentication(mClientKey.asBytes());
        HandshakeMessage finalServerMessage = serverRunner.authenticateReconnection(
                nextClientMessage.getNextMessage(), mServerKey.asBytes());
        HandshakeMessage finalClientMessage = clientRunner.authenticateReconnection(
                finalServerMessage.getNextMessage(), mServerKey.asBytes());
        mServerKey = finalServerMessage.getKey();
        mClientKey = finalClientMessage.getKey();
    }

    /**
     * Runs through a happy flow of encryption runners and verifies that they behave as expected.
     * Some * of the test is implementation specific because the interface doesn't specify how many
     * round * trips may be needed but this test makes assumptions( i.e. white box testing).
     */
    private void verifyRunners(RunnerFactory runnerFactory, HandshakeVerifier handshakeVerifier)
            throws Exception {
        EncryptionRunner clientRunner = runnerFactory.newRunner();
        EncryptionRunner serverRunner = runnerFactory.newRunner();

        handshakeVerifier.verifyHandshake(clientRunner, serverRunner);

        HandshakeMessage finalServerMessage = serverRunner.verifyPin();
        assertThat(finalServerMessage.getHandshakeState())
                .isEqualTo(HandshakeMessage.HandshakeState.FINISHED);
        assertThat(finalServerMessage.getKey()).isNotNull();
        assertThat(finalServerMessage.getNextMessage()).isNull();

        HandshakeMessage finalClientMessage = clientRunner.verifyPin();
        assertThat(finalClientMessage.getHandshakeState())
                .isEqualTo(HandshakeMessage.HandshakeState.FINISHED);
        assertThat(finalClientMessage.getKey()).isNotNull();
        assertThat(finalClientMessage.getNextMessage()).isNull();

        assertThat(finalServerMessage.getKey()
                .decryptData(finalClientMessage.getKey().encryptData(mData)))
                .isEqualTo(mData);
        assertThat(finalClientMessage.getKey()
                .decryptData(finalServerMessage.getKey().encryptData(mData)))
                .isEqualTo(mData);
    }

    private void verifyRunnersReconnect(RunnerFactory runnerFactory) throws Exception {
        EncryptionRunner clientRunner = runnerFactory.newRunner();
        EncryptionRunner serverRunner = runnerFactory.newRunner();
        verifyHandshakeReconnect(clientRunner, serverRunner);

        HandshakeMessage nextClientMessage =
                clientRunner.initReconnectAuthentication(mClientKey.asBytes());
        assertThat(nextClientMessage.getHandshakeState())
                .isEqualTo(HandshakeMessage.HandshakeState.RESUMING_SESSION);
        assertThat(nextClientMessage.getKey()).isNull();
        assertThat(nextClientMessage.getNextMessage()).isNotNull();

        HandshakeMessage finalServerMessage = serverRunner.authenticateReconnection(
                nextClientMessage.getNextMessage(), mServerKey.asBytes());
        assertThat(finalServerMessage.getHandshakeState())
                .isEqualTo(HandshakeMessage.HandshakeState.FINISHED);
        assertThat(finalServerMessage.getKey()).isNotNull();
        assertThat(finalServerMessage.getNextMessage()).isNotNull();

        HandshakeMessage finalClientMessage = clientRunner.authenticateReconnection(
                finalServerMessage.getNextMessage(), mServerKey.asBytes());
        assertThat(finalClientMessage.getHandshakeState())
                .isEqualTo(HandshakeMessage.HandshakeState.FINISHED);
        assertThat(finalClientMessage.getKey()).isNotNull();
        assertThat(finalClientMessage.getNextMessage()).isNull();
    }

    private static void verifyHandshake(EncryptionRunner clientRunner,
            EncryptionRunner serverRunner)
            throws Exception {
        HandshakeMessage initialClientMessage = clientRunner.initHandshake();

        assertThat(initialClientMessage.getHandshakeState())
                .isEqualTo(HandshakeMessage.HandshakeState.IN_PROGRESS);
        assertThat(initialClientMessage.getKey()).isNull();
        assertThat(initialClientMessage.getNextMessage()).isNotNull();

        // This and the following similar log statements are useful when running this test to
        // find the payload sizes.
        Log.i(EncryptionRunner.TAG,
                "initial client size:" + initialClientMessage.getNextMessage().length);

        HandshakeMessage initialServerMessage =
                serverRunner.respondToInitRequest(initialClientMessage.getNextMessage());

        assertThat(initialServerMessage.getHandshakeState())
                .isEqualTo(HandshakeMessage.HandshakeState.IN_PROGRESS);
        assertThat(initialServerMessage.getKey()).isNull();
        assertThat(initialServerMessage.getNextMessage()).isNotNull();

        Log.i(EncryptionRunner.TAG,
                "initial server message size:" + initialServerMessage.getNextMessage().length);

        HandshakeMessage clientMessage =
                clientRunner.continueHandshake(initialServerMessage.getNextMessage());

        assertThat(clientMessage.getHandshakeState())
                .isEqualTo(HandshakeMessage.HandshakeState.VERIFICATION_NEEDED);
        assertThat(clientMessage.getKey()).isNull();
        assertThat(clientMessage.getVerificationCode()).isNotEmpty();
        assertThat(clientMessage.getNextMessage()).isNotNull();

        Log.i(EncryptionRunner.TAG,
                "second client message size:" + clientMessage.getNextMessage().length);

        HandshakeMessage serverMessage =
                serverRunner.continueHandshake(clientMessage.getNextMessage());
        assertThat(serverMessage.getHandshakeState())
                .isEqualTo(HandshakeMessage.HandshakeState.VERIFICATION_NEEDED);
        assertThat(serverMessage.getKey()).isNull();
        assertThat(serverMessage.getNextMessage()).isNull();

        Log.i(EncryptionRunner.TAG,
                "last server message size:" + clientMessage.getNextMessage().length);
    }

    private static void verifyOobHandshake(
            EncryptionRunner clientRunner, EncryptionRunner serverRunner) throws Exception {
        HandshakeMessage initialClientMessage = clientRunner.initHandshake();

        assertThat(initialClientMessage.getHandshakeState())
                .isEqualTo(HandshakeMessage.HandshakeState.IN_PROGRESS);
        assertThat(initialClientMessage.getKey()).isNull();
        assertThat(initialClientMessage.getNextMessage()).isNotNull();

        HandshakeMessage initialServerMessage =
                serverRunner.respondToInitRequest(initialClientMessage.getNextMessage());

        assertThat(initialServerMessage.getHandshakeState())
                .isEqualTo(HandshakeMessage.HandshakeState.IN_PROGRESS);
        assertThat(initialServerMessage.getKey()).isNull();
        assertThat(initialServerMessage.getNextMessage()).isNotNull();

        HandshakeMessage clientMessage =
                clientRunner.continueHandshake(initialServerMessage.getNextMessage());

        assertThat(clientMessage.getHandshakeState())
                .isEqualTo(HandshakeMessage.HandshakeState.OOB_VERIFICATION_NEEDED);
        assertThat(clientMessage.getKey()).isNull();
        assertThat(clientMessage.getOobVerificationCode()).isNotEmpty();
        assertThat(clientMessage.getNextMessage()).isNotNull();

        HandshakeMessage serverMessage = serverRunner.continueHandshake(
                clientMessage.getNextMessage());
        assertThat(serverMessage.getHandshakeState())
                .isEqualTo(HandshakeMessage.HandshakeState.OOB_VERIFICATION_NEEDED);
        assertThat(serverMessage.getKey()).isNull();
        assertThat(serverMessage.getNextMessage()).isNull();
    }

    private void verifyHandshakeReconnect(
            EncryptionRunner clientRunner, EncryptionRunner serverRunner)
            throws HandshakeException {
        clientRunner.setIsReconnect(true);
        serverRunner.setIsReconnect(true);

        HandshakeMessage initialClientMessage = clientRunner.initHandshake();
        assertThat(initialClientMessage.getHandshakeState())
                .isEqualTo(HandshakeMessage.HandshakeState.IN_PROGRESS);
        assertThat(initialClientMessage.getKey()).isNull();
        assertThat(initialClientMessage.getNextMessage()).isNotNull();

        // This and the following similar log statements are useful when running this test to
        // find the payload sizes.
        Log.i(EncryptionRunner.TAG,
                "initial client size:" + initialClientMessage.getNextMessage().length);

        HandshakeMessage initialServerMessage =
                serverRunner.respondToInitRequest(initialClientMessage.getNextMessage());

        assertThat(initialServerMessage.getHandshakeState())
                .isEqualTo(HandshakeMessage.HandshakeState.IN_PROGRESS);
        assertThat(initialServerMessage.getKey()).isNull();
        assertThat(initialServerMessage.getNextMessage()).isNotNull();

        Log.i(EncryptionRunner.TAG,
                "initial server message size:" + initialServerMessage.getNextMessage().length);

        HandshakeMessage clientMessage =
                clientRunner.continueHandshake(initialServerMessage.getNextMessage());

        assertThat(clientMessage.getHandshakeState())
                .isEqualTo(HandshakeMessage.HandshakeState.RESUMING_SESSION);
        assertThat(clientMessage.getKey()).isNull();
        assertThat(clientMessage.getNextMessage()).isNotNull();

        HandshakeMessage serverMessage =
                serverRunner.continueHandshake(clientMessage.getNextMessage());
        assertThat(serverMessage.getHandshakeState())
                .isEqualTo(HandshakeMessage.HandshakeState.RESUMING_SESSION);
        assertThat(serverMessage.getKey()).isNull();
    }

    @Test
    public void invalidPin_ukey2() throws Exception {
        invalidPinTest(EncryptionRunnerTest::newRunner, EncryptionRunnerTest::verifyHandshake);
    }

    @Test
    public void invalidPin_dummy() throws Exception {
        invalidPinTest(EncryptionRunnerFactory::newDummyRunner,
                EncryptionRunnerTest::verifyHandshake);
    }

    @Test
    public void invalidPin_oobUkey2() throws Exception {
        invalidPinTest(EncryptionRunnerTest::newOobRunner,
                EncryptionRunnerTest::verifyOobHandshake);
    }

    private void invalidPinTest(RunnerFactory runnerFactory, HandshakeVerifier handshakeVerifier)
            throws Exception {
        EncryptionRunner clientRunner = runnerFactory.newRunner();
        EncryptionRunner serverRunner = runnerFactory.newRunner();

        handshakeVerifier.verifyHandshake(clientRunner, serverRunner);
        clientRunner.invalidPin();
        serverRunner.invalidPin();

        try {
            clientRunner.verifyPin();
            Assert.fail();
        } catch (Exception ignored) {
            // pass
        }

        try {
            serverRunner.verifyPin();
            Assert.fail();
        } catch (Exception ignored) {
            // pass
        }
    }
}
