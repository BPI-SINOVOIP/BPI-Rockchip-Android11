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

#include <atomic>
#include <inttypes.h>
#include <stdio.h>
#include <string>
#include <thread>
#include <time.h>
#include <vector>

//#define LOG_NDEBUG 0
#define LOG_TAG "NativeMidiManager-JNI"
#include <android/log.h>

#include <jni.h>

#include <amidi/AMidi.h>

extern "C" {

/*
 * Structures for storing data flowing through the echo server.
 */
#define SIZE_DATABUFFER 256
/*
 * Received Messages
 */
typedef struct {
    std::unique_ptr<uint8_t[]> dataBuff;
    size_t numDataBytes;
    int32_t opCode;
    int64_t timestamp;
    int64_t timeReceived;
} ReceivedMessageRecord;

/*
 * Sent Messages
 */
typedef struct {
    std::unique_ptr<uint8_t[]> dataBuff;
    size_t numDataBytes;
    int64_t timestamp;
    long timeSent;
} SentMessageRecord;

/*
 * Context
 * Holds the state of a given test and native MIDI I/O setup for that test.
 * NOTE: There is one of these per test (and therefore unique to each test thread).
 */
class TestContext {
private:
    // counters
    std::atomic<int> mNumSends;
    std::atomic<int> mNumBytesSent;
    std::atomic<int> mNumReceives;
    std::atomic<int> mNumBytesReceived;

    std::vector<ReceivedMessageRecord> mReceivedMsgs;
    std::vector<SentMessageRecord> mSentMsgs;

    // Java NativeMidiMessage class stuff, for passing messages back out to the Java client.
    jclass mClsNativeMidiMessage;
    jmethodID mMidNativeMidiMessage_ctor;
    jfieldID mFid_opcode;
    jfieldID mFid_buffer;
    jfieldID mFid_len;
    jfieldID mFid_timestamp;
    jfieldID mFid_timeReceived;

    std::mutex lock;

public:
    // read Thread
    std::unique_ptr<std::thread> mReadThread;
    std::atomic<bool> mReading;

    AMidiDevice* nativeDevice;
    std::atomic<AMidiOutputPort*> midiOutputPort;
    std::atomic<AMidiInputPort*> midiInputPort;

    TestContext() :
        mNumSends(0),
        mNumBytesSent(0),
        mNumReceives(0),
        mNumBytesReceived(0),
        mClsNativeMidiMessage(0),
        mMidNativeMidiMessage_ctor(0),
        mFid_opcode(0),
        mFid_buffer(0),
        mFid_len(0),
        mFid_timestamp(0),
        mFid_timeReceived(0),
        mReading(false),
        nativeDevice(nullptr),
        midiOutputPort(nullptr),
        midiInputPort(nullptr)
    {}

    bool initN(JNIEnv* env);

    int getNumSends() { return mNumSends; }
    void incNumSends() { mNumSends++; }

    int getNumBytesSent() { return mNumBytesSent; }
    void incNumBytesSent(int numBytes) { mNumBytesSent += numBytes; }

    int getNumReceives() { return mNumReceives; }
    void incNumReceives() { mNumReceives++; }

    int getNumBytesReceived() { return mNumBytesReceived; }
    void incNumBytesReceived(int numBytes) { mNumBytesReceived += numBytes; }

    void addSent(SentMessageRecord&& msg) { mSentMsgs.push_back(std::move(msg)); }
    size_t getNumSentMsgs() { return mSentMsgs.size(); }

    void addReceived(ReceivedMessageRecord&& msg) { mReceivedMsgs.push_back(std::move(msg)); }
    size_t getNumReceivedMsgs() { return mReceivedMsgs.size(); }

    jobject transferReceiveMsgAt(JNIEnv* env, int index);

    static const int COMPARE_SUCCESS = 0;
    static const int COMPARE_COUNTMISSMATCH = 1;
    static const int COMPARE_DATALENMISMATCH = 2;
    static const int COMPARE_DATAMISMATCH = 3;
    static const int COMPARE_TIMESTAMPMISMATCH = 4;
    int compareInsAndOuts();

    static const int CHECKLATENCY_SUCCESS = 0;
    static const int CHECKLATENCY_COUNTMISSMATCH = 1;
    static const int CHECKLATENCY_LATENCYEXCEEDED = 2;
    int checkInOutLatency(long maxLatencyNanos);
};

//
// Helpers
//
static long System_nanoTime() {
    // this code is the implementation of System.nanoTime()
    // from system/code/ojluni/src/main/native/System.
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * 1000000000LL + now.tv_nsec;
}

bool TestContext::initN(JNIEnv* env) {
    static const char* clsSigNativeMidiMessage = "android/nativemidi/cts/NativeMidiMessage";

    jclass cls = env->FindClass(clsSigNativeMidiMessage);
    if (cls == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                "JNI Error - couldn't find NativeMidiMessage class");
        return false; // we are doomed, so bail.
    }
    mClsNativeMidiMessage = (jclass)env->NewGlobalRef(cls);
    if (mClsNativeMidiMessage == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                "JNI Error - couldn't allocate NativeMidiMessage");
        return false; // we are doomed, so bail.
    }

    mMidNativeMidiMessage_ctor = env->GetMethodID(mClsNativeMidiMessage, "<init>", "()V");
    mFid_opcode = env->GetFieldID(mClsNativeMidiMessage, "opcode", "I");
    mFid_buffer = env->GetFieldID(mClsNativeMidiMessage, "buffer", "[B");
    mFid_len = env->GetFieldID( mClsNativeMidiMessage, "len", "I");
    mFid_timestamp = env->GetFieldID(mClsNativeMidiMessage, "timestamp", "J");
    mFid_timeReceived = env->GetFieldID(mClsNativeMidiMessage, "timeReceived", "J");
    if (mMidNativeMidiMessage_ctor == NULL ||
        mFid_opcode == NULL ||
        mFid_buffer == NULL ||
        mFid_len == NULL ||
        mFid_timestamp == NULL ||
        mFid_timeReceived == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                "JNI Error - couldn't load Field IDs");
        return false; // we are doomed, so bail.
    }

    return true;
}

jobject TestContext::transferReceiveMsgAt(JNIEnv* env, int index) {
    jobject msg = NULL;

    if (index < (int)mReceivedMsgs.size()) {
        ReceivedMessageRecord receiveRec = std::move(mReceivedMsgs.at(index));
        msg = env->NewObject(mClsNativeMidiMessage, mMidNativeMidiMessage_ctor);

        env->SetIntField(msg, mFid_opcode, receiveRec.opCode);
        env->SetIntField(msg, mFid_len, receiveRec.numDataBytes);
        jobject buffer_array = env->GetObjectField(msg, mFid_buffer);
        env->SetByteArrayRegion(reinterpret_cast<jbyteArray>(buffer_array), 0,
                receiveRec.numDataBytes, (jbyte*)receiveRec.dataBuff.get());
        env->SetLongField(msg, mFid_timestamp, receiveRec.timestamp);
        env->SetLongField(msg, mFid_timeReceived, receiveRec.timeReceived);
    }

    return msg;
}

int TestContext::compareInsAndOuts() {
    // Number of messages sent/received
    if (mReceivedMsgs.size() != mSentMsgs.size()) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "---- COMPARE_COUNTMISSMATCH r:%zu s:%zu",
                mReceivedMsgs.size(), mSentMsgs.size());
       return COMPARE_COUNTMISSMATCH;
    }

    // we know that both vectors have the same number of messages from the test above.
    size_t numMessages = mSentMsgs.size();
    for (size_t msgIndex = 0; msgIndex < numMessages; msgIndex++) {
        // Data Length?
        if (mReceivedMsgs[msgIndex].numDataBytes != mSentMsgs[msgIndex].numDataBytes) {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                    "---- COMPARE_DATALENMISMATCH r:%zu s:%zu",
                    mReceivedMsgs[msgIndex].numDataBytes, mSentMsgs[msgIndex].numDataBytes);
            return COMPARE_DATALENMISMATCH;
        }

        // Timestamps
        if (mReceivedMsgs[msgIndex].timestamp != mSentMsgs[msgIndex].timestamp) {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "---- COMPARE_TIMESTAMPMISMATCH");
            return COMPARE_TIMESTAMPMISMATCH;
        }

        // we know that the data in both messages have the same number of bytes from the test above.
        int dataLen = mReceivedMsgs[msgIndex].numDataBytes;
        for (int dataIndex = 0; dataIndex < dataLen; dataIndex++) {
            // Data Values?
            if (mReceivedMsgs[msgIndex].dataBuff[dataIndex] !=
                    mSentMsgs[msgIndex].dataBuff[dataIndex]) {
                __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                        "---- COMPARE_DATAMISMATCH r:%d s:%d",
                        (int)mReceivedMsgs[msgIndex].dataBuff[dataIndex],
                        (int)mSentMsgs[msgIndex].dataBuff[dataIndex]);
                return COMPARE_DATAMISMATCH;
            }
        }
    }

    return COMPARE_SUCCESS;
}

int TestContext::checkInOutLatency(long maxLatencyNanos) {
    if (mReceivedMsgs.size() != mSentMsgs.size()) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "  ---- CHECKLATENCY_COUNTMISSMATCH");
        return CHECKLATENCY_COUNTMISSMATCH;
    }

    // we know that both vectors have the same number of messages
    // from the test above.
    int numMessages = mSentMsgs.size();
    for (int msgIndex = 0; msgIndex < numMessages; msgIndex++) {
        long timeDelta =  mSentMsgs[msgIndex].timeSent - mReceivedMsgs[msgIndex].timestamp;
        if (timeDelta > maxLatencyNanos) {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                    "  ---- CHECKLATENCY_LATENCYEXCEEDED %ld", timeDelta);
            return CHECKLATENCY_LATENCYEXCEEDED;
        }
    }

    return CHECKLATENCY_SUCCESS;
}

JNIEXPORT jlong JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_allocTestContext(
        JNIEnv* env, jclass) {
    TestContext* context = new TestContext;
    if (!context->initN(env)) {
        delete context;
        context = NULL;
    }

    return (jlong)context;
}

JNIEXPORT void JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_freeTestContext(
        JNIEnv*, jclass, jlong context) {
    delete (TestContext*)context;
}

/*
 * Receiving API
 */
//static void DumpDataMessage(ReceivedMessageRecord* msg) {
//    char midiDumpBuffer[SIZE_DATABUFFER * 4]; // more than enough
//    memset(midiDumpBuffer, 0, sizeof(midiDumpBuffer));
//    int pos = snprintf(midiDumpBuffer, sizeof(midiDumpBuffer),
//            "%" PRIx64 " ", msg->timestamp);
//    for (uint8_t *b = msg->buffer, *e = b + msg->numDataBytes; b < e; ++b) {
//        pos += snprintf(midiDumpBuffer + pos, sizeof(midiDumpBuffer) - pos,
//                "%02x ", *b);
//    }
//    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "---- DUMP %s", midiDumpBuffer);
//}

void readThreadRoutine(TestContext* context) {
    int32_t opCode;
    uint8_t inDataBuffer[SIZE_DATABUFFER];
    size_t numDataBytes;
    int64_t timestamp;

    while (context->mReading) {
        AMidiOutputPort* outputPort = context->midiOutputPort.load();
        if (outputPort != nullptr) {
            ssize_t numMessages =
                AMidiOutputPort_receive(outputPort, &opCode,
                    inDataBuffer, sizeof(inDataBuffer), &numDataBytes, &timestamp);

            if (numMessages > 0) {
                context->incNumReceives();
                context->incNumBytesReceived(numDataBytes);
                ReceivedMessageRecord receiveRec;
                receiveRec.timeReceived = System_nanoTime();
                receiveRec.numDataBytes = numDataBytes;
                receiveRec.dataBuff.reset(new uint8_t[receiveRec.numDataBytes]);
                memcpy(receiveRec.dataBuff.get(), inDataBuffer, receiveRec.numDataBytes);
                receiveRec.opCode = opCode;
                receiveRec.timestamp = timestamp;
                context->addReceived(std::move(receiveRec));
            }
        }
    }
}

static media_status_t commonDeviceOpen(JNIEnv *env, jobject midiDeviceObj, AMidiDevice** device) {
    media_status_t status = AMidiDevice_fromJava(env, midiDeviceObj, device);
    if (status == AMEDIA_OK) {
        // __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
        //      "---- Obtained device token for obj %p: dev %p", midiDeviceObj, device);
    } else {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                "---- Could not obtain device token for obj %p: status:%d", midiDeviceObj, status);
    }

    return status;
}

JNIEXPORT jint JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_openNativeMidiDevice(
        JNIEnv* env, jobject, jlong ctx, jobject deviceObj) {
    TestContext* context = (TestContext*)ctx;
    media_status_t status = commonDeviceOpen(env, deviceObj, &context->nativeDevice);
    return status;
}

JNIEXPORT jint JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_closeNativeMidiDevice(
        JNIEnv*, jobject, jlong ctx) {
    TestContext* context = (TestContext*)ctx;
    media_status_t status = AMidiDevice_release(context->nativeDevice);
    return status;
}

/*
 * Sending API
 */
JNIEXPORT jint JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_startWritingMidi(
        JNIEnv*, jobject, jlong ctx, jint portNumber) {

    TestContext* context = (TestContext*)ctx;

    AMidiInputPort* inputPort;
    media_status_t status = AMidiInputPort_open(context->nativeDevice, portNumber, &inputPort);
    if (status == AMEDIA_OK) {
        // __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
        //      "---- Opened INPUT port %d: token %p", portNumber, inputPort);
        context->midiInputPort.store(inputPort);
    } else {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "---- Could not open INPUT port %p:%d %d",
                context->nativeDevice, portNumber, status);
        return status;
    }

    return AMEDIA_OK;
}

JNIEXPORT jint JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_stopWritingMidi(
        JNIEnv*, jobject, jlong ctx) {

    TestContext* context = (TestContext*)ctx;

    AMidiInputPort* inputPort = context->midiInputPort.exchange(nullptr);
    if (inputPort == nullptr) {
        return -1;
    }

    AMidiInputPort_close(inputPort);

    return 0;
}

JNIEXPORT jint JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_writeMidiWithTimestamp(
        JNIEnv* env, jobject,
        jlong ctx, jbyteArray data, jint offset, jint numBytes, jlong timestamp) {

    TestContext* context = (TestContext*)ctx;
    context->incNumSends();
    context->incNumBytesSent(numBytes);

    jbyte* bufferPtr = env->GetByteArrayElements(data, NULL);
    if (bufferPtr == NULL) {
        return -1;
    }

    int numWritten =  AMidiInputPort_sendWithTimestamp(
            context->midiInputPort, (uint8_t*)bufferPtr + offset, numBytes, timestamp);
    if (numWritten > 0) {
        // Don't save a send record if we didn't send!
        SentMessageRecord sendRec;
        sendRec.numDataBytes = numBytes;
        sendRec.dataBuff.reset(new uint8_t[sendRec.numDataBytes]);
        memcpy(sendRec.dataBuff.get(), (uint8_t*)bufferPtr + offset, numBytes);
        sendRec.timestamp = timestamp;
        sendRec.timeSent = System_nanoTime();
        context->addSent(std::move(sendRec));
    }

    env->ReleaseByteArrayElements(data, bufferPtr, JNI_ABORT);

    return numWritten;
}

JNIEXPORT jint JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_writeMidi(
        JNIEnv* env, jobject j_object, jlong ctx, jbyteArray data, jint offset, jint numBytes) {
    return Java_android_nativemidi_cts_NativeMidiEchoTest_writeMidiWithTimestamp(
            env, j_object, ctx, data, offset, numBytes, 0L);
}

JNIEXPORT jint JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_flushSentMessages(
        JNIEnv*, jobject, jlong ctx) {
    TestContext* context = (TestContext*)ctx;
    return AMidiInputPort_sendFlush(context->midiInputPort);
}

JNIEXPORT jint JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_getNumSends(
        JNIEnv*, jobject, jlong ctx) {
    return ((TestContext*)ctx)->getNumSends();
}

JNIEXPORT jint JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_getNumBytesSent(
        JNIEnv*, jobject, jlong ctx) {
    return ((TestContext*)ctx)->getNumBytesSent();
}

JNIEXPORT jint JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_getNumReceives(
        JNIEnv*, jobject, jlong ctx) {
    return ((TestContext*)ctx)->getNumReceives();
}

JNIEXPORT jint JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_getNumBytesReceived(
        JNIEnv*, jobject, jlong ctx) {
    return ((TestContext*)ctx)->getNumBytesReceived();
}

JNIEXPORT jint JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_startReadingMidi(
        JNIEnv*, jobject, jlong ctx, jint portNumber) {

    // __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "++++ startReadingMidi()");
    TestContext* context = (TestContext*)ctx;

    AMidiOutputPort* outputPort;
    media_status_t status = AMidiOutputPort_open(context->nativeDevice, portNumber, &outputPort);
    if (status == AMEDIA_OK) {
        context->midiOutputPort.store(outputPort);
    } else {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                "---- Could not open OUTPUT port %p : %d %d",
                context->nativeDevice, portNumber, status);
        return status;
    }

    // Start read thread
    context->mReading = true;
    context->mReadThread.reset(new std::thread(readThreadRoutine, context));
    std::this_thread::yield(); // let the read thread startup.

    return status;
}

JNIEXPORT jint JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_stopReadingMidi(
        JNIEnv*, jobject, jlong ctx) {

    // __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "++++ stopReadingMidi()");
    TestContext* context = (TestContext*)ctx;
    context->mReading = false;

    context->mReadThread->join();

    AMidiOutputPort* outputPort = context->midiOutputPort.exchange(nullptr);
    if (outputPort == nullptr) {
        return -1;
    }

    AMidiOutputPort_close(outputPort);

    return 0;
}

/*
 * Messages
 */
JNIEXPORT jint JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_getNumReceivedMessages(
        JNIEnv*, jobject, jlong ctx) {
    return ((TestContext*)ctx)->getNumReceivedMsgs();
}

JNIEXPORT jobject JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_getReceivedMessageAt(
        JNIEnv* env, jobject, jlong ctx, jint index) {
    return ((TestContext*)ctx)->transferReceiveMsgAt(env, index);
}

JNIEXPORT jint JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_matchNativeMessages(
        JNIEnv*, jobject, jlong ctx) {
    return ((TestContext*)ctx)->compareInsAndOuts();
}

JNIEXPORT jint JNICALL Java_android_nativemidi_cts_NativeMidiEchoTest_checkNativeLatency(
        JNIEnv*, jobject, jlong ctx, jlong maxLatencyNanos) {
    return ((TestContext*)ctx)->checkInOutLatency(maxLatencyNanos);
}

} // extern "C"
