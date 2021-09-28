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
#include <cstring>
#include <pthread.h>
#include <unistd.h>

#define TAG "MidiTestManager"
#include <android/log.h>
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#include "MidiTestManager.h"

static pthread_t readThread;

static const bool DEBUG = false;
static const bool DEBUG_MIDIDATA = false;

//
// MIDI Messages
//
// Channel Commands
static const uint8_t kMIDIChanCmd_KeyDown = 9;
static const uint8_t kMIDIChanCmd_KeyUp = 8;
static const uint8_t kMIDIChanCmd_PolyPress = 10;
static const uint8_t kMIDIChanCmd_Control = 11;
static const uint8_t kMIDIChanCmd_ProgramChange = 12;
static const uint8_t kMIDIChanCmd_ChannelPress = 13;
static const uint8_t kMIDIChanCmd_PitchWheel = 14;
// System Commands
static const uint8_t kMIDISysCmd_SysEx = 0xF0;
static const uint8_t kMIDISysCmd_EndOfSysEx =  0xF7;
static const uint8_t kMIDISysCmd_ActiveSensing = 0xFE;
static const uint8_t kMIDISysCmd_Reset = 0xFF;

static void* readThreadRoutine(void * context) {
    MidiTestManager* testManager = (MidiTestManager*)context;
    return reinterpret_cast<void*>(static_cast<intptr_t>(testManager->ProcessInput()));
}

/*
 * TestMessage
 */
#define makeMIDICmd(cmd, channel)  (uint8_t)((cmd << 4) | (channel & 0x0F))

class TestMessage {
public:
    uint8_t*   mMsgBytes;
    int     mNumMsgBytes;

    TestMessage()
        : mMsgBytes(NULL), mNumMsgBytes(0)
    {}

    ~TestMessage() {
        delete[] mMsgBytes;
    }

    bool set(uint8_t* msgBytes, int numMsgBytes) {
        if (msgBytes == NULL || numMsgBytes <= 0) {
            return false;
        }
        mNumMsgBytes = numMsgBytes;
        mMsgBytes = new uint8_t[numMsgBytes];
        memcpy(mMsgBytes, msgBytes, mNumMsgBytes * sizeof(uint8_t));
        return true;
    }
}; /* class TestMessage */

/*
 * MidiTestManager
 */
MidiTestManager::MidiTestManager()
    : mTestModuleObj(NULL),
      mTestStream(NULL), mNumTestStreamBytes(0),
      mReceiveStreamPos(0),
      mMidiSendPort(NULL), mMidiReceivePort(NULL),
      mTestMsgs(NULL), mNumTestMsgs(0)
{}

MidiTestManager::~MidiTestManager(){
    delete[] mTestStream;
}

void MidiTestManager::jniSetup(JNIEnv* env) {
    env->GetJavaVM(&mJvm);

    jclass clsMidiTestModule =
        env->FindClass("com/android/cts/verifier/audio/NDKMidiActivity$NDKMidiTestModule");
    if (DEBUG) {
        ALOGI("gClsMidiTestModule:%p", clsMidiTestModule);
    }

    // public void endTest(int endCode)
    mMidEndTest = env->GetMethodID(clsMidiTestModule, "endTest", "(I)V");
    if (DEBUG) {
        ALOGI("mMidEndTestgMidEndTest:%p", mMidEndTest);
    }
}

void MidiTestManager::buildTestStream() {
    // add up the total space
    mNumTestStreamBytes = 0;
    for(int msgIndex = 0; msgIndex < mNumTestMsgs; msgIndex++) {
        mNumTestStreamBytes += mTestMsgs[msgIndex].mNumMsgBytes;
    }

    delete[] mTestStream;
    mTestStream = new uint8_t[mNumTestStreamBytes];
    int streamIndex = 0;
    for(int msgIndex = 0; msgIndex < mNumTestMsgs; msgIndex++) {
        for(int byteIndex = 0; byteIndex < mTestMsgs[msgIndex].mNumMsgBytes; byteIndex++) {
            mTestStream[streamIndex++] = mTestMsgs[msgIndex].mMsgBytes[byteIndex];
        }
    }

    // Reset stream position
    mReceiveStreamPos = 0;
}

/**
 * Compares the supplied bytes against the sent message stream at the current postion
 * and advances the stream position.
 */
bool MidiTestManager::matchStream(uint8_t* bytes, int count) {
    if (DEBUG) {
        ALOGI("---- matchStream() count:%d", count);
    }
    bool matches = true;

    for (int index = 0; index < count; index++) {
        if (bytes[index] != mTestStream[mReceiveStreamPos]) {
            matches = false;
            if (DEBUG) {
                ALOGI("---- mismatch @%d [%d : %d]",
                        index, bytes[index], mTestStream[mReceiveStreamPos]);
            }
        }
        mReceiveStreamPos++;
    }

    if (DEBUG) {
        ALOGI("  returns:%d", matches);
    }
    return matches;
}

/**
 * Writes out the list of MIDI messages to the output port.
 * Returns total number of bytes sent.
 */
int MidiTestManager::sendMessages() {
    if (DEBUG) {
        ALOGI("---- sendMessages()...");
        for(int msgIndex = 0; msgIndex < mNumTestMsgs; msgIndex++) {
            if (DEBUG_MIDIDATA) {
            ALOGI("--------");
                for(int byteIndex = 0; byteIndex < mTestMsgs[msgIndex].mNumMsgBytes; byteIndex++) {
                    ALOGI("  0x%X", mTestMsgs[msgIndex].mMsgBytes[byteIndex]);
                }
            }
        }
    }

    int totalSent = 0;
    for(int msgIndex = 0; msgIndex < mNumTestMsgs; msgIndex++) {
        ssize_t numSent =
            AMidiInputPort_send(mMidiSendPort,
                    mTestMsgs[msgIndex].mMsgBytes, mTestMsgs[msgIndex].mNumMsgBytes);
        totalSent += numSent;
    }

    if (DEBUG) {
        ALOGI("---- totalSent:%d", totalSent);
    }

    return totalSent;
}

int MidiTestManager::ProcessInput() {
    uint8_t readBuffer[128];
    size_t totalNumReceived = 0;

    bool testRunning = true;
    int testResult = TESTSTATUS_NOTRUN;

    int32_t opCode;
    size_t numBytesReceived;
    int64_t timeStamp;
    while (testRunning) {
        // AMidiOutputPort_receive is non-blocking, so let's not burn up the CPU unnecessarily
        usleep(2000);

        numBytesReceived = 0;
        ssize_t numMessagesReceived =
            AMidiOutputPort_receive(mMidiReceivePort, &opCode, readBuffer, 128,
                        &numBytesReceived, &timeStamp);

        if (testRunning &&
            numBytesReceived > 0 &&
            opCode == AMIDI_OPCODE_DATA &&
            readBuffer[0] != kMIDISysCmd_ActiveSensing &&
            readBuffer[0] != kMIDISysCmd_Reset) {
            if (DEBUG) {
                ALOGI("---- msgs:%zd, bytes:%zu", numMessagesReceived, numBytesReceived);
            }
            // Process Here
            if (!matchStream(readBuffer, numBytesReceived)) {
                testResult = TESTSTATUS_FAILED_MISMATCH;
                testRunning = false;   // bail
            }
            totalNumReceived += numBytesReceived;
            if (totalNumReceived > mNumTestStreamBytes) {
                testResult = TESTSTATUS_FAILED_OVERRUN;
                testRunning = false;   // bail
            }
            if (totalNumReceived == mNumTestStreamBytes) {
                testResult = TESTSTATUS_PASSED;
                testRunning = false;   // done
            }
        }
    }

    return testResult;
}

bool MidiTestManager::StartReading(AMidiDevice* nativeReadDevice) {
    ALOGI("StartReading()...");

    media_status_t m_status =
        AMidiOutputPort_open(nativeReadDevice, 0, &mMidiReceivePort);
    if (m_status != 0) {
        ALOGE("Can't open MIDI device for reading err:%d", m_status);
        return false;
    }

    // Start read thread
    int status = pthread_create(&readThread, NULL, readThreadRoutine, this);
    if (status != 0) {
        ALOGE("Can't start readThread: %s (%d)", strerror(status), status);
    }
    return status == 0;
}

bool MidiTestManager::StartWriting(AMidiDevice* nativeWriteDevice) {
    ALOGI("StartWriting()...");

    media_status_t status =
        AMidiInputPort_open(nativeWriteDevice, 0, &mMidiSendPort);
    if (status != 0) {
        ALOGE("Can't open MIDI device for writing err:%d", status);
        return false;
    }
    return true;
}

uint8_t msg0[] = {makeMIDICmd(kMIDIChanCmd_KeyDown, 0), 64, 120};
//uint8_t msg0Alt[] = {makeMIDICmd(kMIDIChanCmd_KeyDown, 0), 65, 120};
uint8_t msg1[] = {makeMIDICmd(kMIDIChanCmd_KeyUp, 0), 64, 35};

bool MidiTestManager::RunTest(jobject testModuleObj, AMidiDevice* sendDevice,
        AMidiDevice* receiveDevice) {
    if (DEBUG) {
        ALOGI("RunTest(%p, %p, %p)", testModuleObj, sendDevice, receiveDevice);
    }

    JNIEnv* env;
    mJvm->AttachCurrentThread(&env, NULL);
    if (env == NULL) {
        EndTest(TESTSTATUS_FAILED_JNI);
    }

    mTestModuleObj = env->NewGlobalRef(testModuleObj);

    // Call StartWriting first because StartReading starts a thread.
    if (!StartWriting(sendDevice) || !StartReading(receiveDevice)) {
        // Test call to EndTest will close any open devices.
        EndTest(TESTSTATUS_FAILED_DEVICE);
        return false; // bail
    }

    // setup messages
    delete[] mTestMsgs;
    mNumTestMsgs = 3;
    mTestMsgs = new TestMessage[mNumTestMsgs];

    int sysExSize = 8;
    uint8_t* sysExMsg = new uint8_t[sysExSize];
    sysExMsg[0] = kMIDISysCmd_SysEx;
    for(int index = 1; index < sysExSize-1; index++) {
        sysExMsg[index] = (uint8_t)index;
    }
    sysExMsg[sysExSize-1] = kMIDISysCmd_EndOfSysEx;

    if (!mTestMsgs[0].set(msg0, sizeof(msg0)) ||
        !mTestMsgs[1].set(msg1, sizeof(msg1)) ||
        !mTestMsgs[2].set(sysExMsg, sysExSize)) {
        return false;
    }
    delete[] sysExMsg;

    buildTestStream();

    // Inject an error
    // mTestMsgs[0].set(msg0Alt, 3);

    sendMessages();
    void* threadRetval = (void*)TESTSTATUS_NOTRUN;
    int status = pthread_join(readThread, &threadRetval);
    if (status != 0) {
        ALOGE("Failed to join readThread: %s (%d)", strerror(status), status);
    }
    EndTest(static_cast<int>(reinterpret_cast<intptr_t>(threadRetval)));
    return true;
}

void MidiTestManager::EndTest(int endCode) {

    JNIEnv* env;
    mJvm->AttachCurrentThread(&env, NULL);
    if (env == NULL) {
        ALOGE("Error retrieving JNI Env");
    }

    env->CallVoidMethod(mTestModuleObj, mMidEndTest, endCode);
    env->DeleteGlobalRef(mTestModuleObj);

    // EndTest() will ALWAYS be called, so we can close the ports here.
    if (mMidiSendPort != NULL) {
        AMidiInputPort_close(mMidiSendPort);
        mMidiSendPort = NULL;
    }
    if (mMidiReceivePort != NULL) {
        AMidiOutputPort_close(mMidiReceivePort);
        mMidiReceivePort = NULL;
    }
}
