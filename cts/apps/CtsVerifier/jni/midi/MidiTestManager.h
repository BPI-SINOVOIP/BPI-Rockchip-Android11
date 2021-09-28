/*
 * Copyright (C) 2018 The Android Open Source Project
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
#include <amidi/AMidi.h>

#include <jni.h>

class TestMessage;

class MidiTestManager {
public:
    MidiTestManager();
    ~MidiTestManager();

    void jniSetup(JNIEnv* env);

    bool RunTest(jobject testModuleObj, AMidiDevice* sendDevice, AMidiDevice* receiveDevice);
    void EndTest(int testCode);

    // Called by the thread routine.
    int ProcessInput();

private:
   void buildTestStream();
    bool matchStream(uint8_t* bytes, int count);

    int sendMessages();

    jobject mTestModuleObj;

    // The send messages in a linear stream for matching.
    uint8_t*   mTestStream;
    int     mNumTestStreamBytes;
    int     mReceiveStreamPos;

    AMidiInputPort* mMidiSendPort;
    AMidiOutputPort* mMidiReceivePort;

    // The array of messages to send/receive
    TestMessage*    mTestMsgs;
    int             mNumTestMsgs;

    // JNI
    JavaVM* mJvm;
    jmethodID mMidEndTest;

    // Test result codes
    static const int TESTSTATUS_NOTRUN = 0;
    static const int TESTSTATUS_PASSED = 1;
    static const int TESTSTATUS_FAILED_MISMATCH = 2;
    static const int TESTSTATUS_FAILED_TIMEOUT = 3;
    static const int TESTSTATUS_FAILED_OVERRUN = 4;
    static const int TESTSTATUS_FAILED_DEVICE = 5;
    static const int TESTSTATUS_FAILED_JNI = 6;

    bool StartReading(AMidiDevice* nativeReadDevice);
    bool StartWriting(AMidiDevice* nativeWriteDevice);
};
