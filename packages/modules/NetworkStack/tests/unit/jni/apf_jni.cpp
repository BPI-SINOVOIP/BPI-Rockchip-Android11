/*
 * Copyright 2018, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android/log.h>
#include <nativehelper/JNIHelp.h>
#include <nativehelper/ScopedUtfChars.h>
#include <jni.h>
#include <pcap.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "apf_interpreter.h"
#include "nativehelper/scoped_primitive_array.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define LOG_TAG "NetworkStackUtils-JNI"

// JNI function acting as simply call-through to native APF interpreter.
static jint com_android_server_ApfTest_apfSimulate(
        JNIEnv* env, jclass, jbyteArray jprogram, jbyteArray jpacket,
        jbyteArray jdata, jint filter_age) {

    ScopedByteArrayRO packet(env, jpacket);
    uint32_t packet_len = (uint32_t)packet.size();
    uint32_t program_len = env->GetArrayLength(jprogram);
    uint32_t data_len = jdata ? env->GetArrayLength(jdata) : 0;
    std::vector<uint8_t> buf(program_len + data_len, 0);

    env->GetByteArrayRegion(jprogram, 0, program_len, reinterpret_cast<jbyte*>(buf.data()));
    if (jdata) {
        // Merge program and data into a single buffer.
        env->GetByteArrayRegion(jdata, 0, data_len,
                                reinterpret_cast<jbyte*>(buf.data() + program_len));
    }

    jint result =
        accept_packet(buf.data(), program_len, program_len + data_len,
                        reinterpret_cast<const uint8_t*>(packet.get()), packet_len, filter_age);

    if (jdata) {
        env->SetByteArrayRegion(jdata, 0, data_len,
                                reinterpret_cast<jbyte*>(buf.data() + program_len));
    }

    return result;
}

class ScopedPcap {
  public:
    explicit ScopedPcap(pcap_t* pcap) : pcap_ptr(pcap) {}
    ~ScopedPcap() {
        pcap_close(pcap_ptr);
    }

    pcap_t* get() const { return pcap_ptr; };
  private:
    pcap_t* const pcap_ptr;
};

class ScopedFILE {
  public:
    explicit ScopedFILE(FILE* fp) : file(fp) {}
    ~ScopedFILE() {
        fclose(file);
    }

    FILE* get() const { return file; };
  private:
    FILE* const file;
};

static void throwException(JNIEnv* env, const std::string& error) {
    jclass newExcCls = env->FindClass("java/lang/IllegalStateException");
    if (newExcCls == 0) {
      abort();
      return;
    }
    env->ThrowNew(newExcCls, error.c_str());
}

static jstring com_android_server_ApfTest_compileToBpf(JNIEnv* env, jclass, jstring jfilter) {
    ScopedUtfChars filter(env, jfilter);
    std::string bpf_string;
    ScopedPcap pcap(pcap_open_dead(DLT_EN10MB, 65535));
    if (pcap.get() == NULL) {
        throwException(env, "pcap_open_dead failed");
        return NULL;
    }

    // Compile "filter" to a BPF program
    bpf_program bpf;
    if (pcap_compile(pcap.get(), &bpf, filter.c_str(), 0, PCAP_NETMASK_UNKNOWN)) {
        throwException(env, "pcap_compile failed");
        return NULL;
    }

    // Translate BPF program to human-readable format
    const struct bpf_insn* insn = bpf.bf_insns;
    for (uint32_t i = 0; i < bpf.bf_len; i++) {
        bpf_string += bpf_image(insn++, i);
        bpf_string += "\n";
    }

    return env->NewStringUTF(bpf_string.c_str());
}

static jboolean com_android_server_ApfTest_compareBpfApf(JNIEnv* env, jclass, jstring jfilter,
        jstring jpcap_filename, jbyteArray japf_program) {
    ScopedUtfChars filter(env, jfilter);
    ScopedUtfChars pcap_filename(env, jpcap_filename);
    ScopedByteArrayRO apf_program(env, japf_program);

    // Open pcap file for BPF filtering
    ScopedFILE bpf_fp(fopen(pcap_filename.c_str(), "rb"));
    char pcap_error[PCAP_ERRBUF_SIZE];
    ScopedPcap bpf_pcap(pcap_fopen_offline(bpf_fp.get(), pcap_error));
    if (bpf_pcap.get() == NULL) {
        throwException(env, "pcap_fopen_offline failed: " + std::string(pcap_error));
        return false;
    }

    // Open pcap file for APF filtering
    ScopedFILE apf_fp(fopen(pcap_filename.c_str(), "rb"));
    ScopedPcap apf_pcap(pcap_fopen_offline(apf_fp.get(), pcap_error));
    if (apf_pcap.get() == NULL) {
        throwException(env, "pcap_fopen_offline failed: " + std::string(pcap_error));
        return false;
    }

    // Compile "filter" to a BPF program
    bpf_program bpf;
    if (pcap_compile(bpf_pcap.get(), &bpf, filter.c_str(), 0, PCAP_NETMASK_UNKNOWN)) {
        throwException(env, "pcap_compile failed");
        return false;
    }

    // Install BPF filter on bpf_pcap
    if (pcap_setfilter(bpf_pcap.get(), &bpf)) {
        throwException(env, "pcap_setfilter failed");
        return false;
    }

    while (1) {
        pcap_pkthdr bpf_header, apf_header;
        // Run BPF filter to the next matching packet.
        const uint8_t* bpf_packet = pcap_next(bpf_pcap.get(), &bpf_header);

        // Run APF filter to the next matching packet.
        const uint8_t* apf_packet;
        do {
            apf_packet = pcap_next(apf_pcap.get(), &apf_header);
        } while (apf_packet != NULL && !accept_packet(
                reinterpret_cast<uint8_t*>(const_cast<int8_t*>(apf_program.get())),
                apf_program.size(), 0 /* data_len */,
                apf_packet, apf_header.len, 0 /* filter_age */));

        // Make sure both filters matched the same packet.
        if (apf_packet == NULL && bpf_packet == NULL)
            break;
        if (apf_packet == NULL || bpf_packet == NULL)
            return false;
        if (apf_header.len != bpf_header.len ||
                apf_header.ts.tv_sec != bpf_header.ts.tv_sec ||
                apf_header.ts.tv_usec != bpf_header.ts.tv_usec ||
                memcmp(apf_packet, bpf_packet, apf_header.len))
            return false;
    }
    return true;
}

static jboolean com_android_server_ApfTest_dropsAllPackets(JNIEnv* env, jclass, jbyteArray jprogram,
        jbyteArray jdata, jstring jpcap_filename) {
    ScopedUtfChars pcap_filename(env, jpcap_filename);
    ScopedByteArrayRO apf_program(env, jprogram);
    uint32_t apf_program_len = (uint32_t)apf_program.size();
    uint32_t data_len = env->GetArrayLength(jdata);
    pcap_pkthdr apf_header;
    const uint8_t* apf_packet;
    char pcap_error[PCAP_ERRBUF_SIZE];
    std::vector<uint8_t> buf(apf_program_len + data_len, 0);

    // Merge program and data into a single buffer.
    env->GetByteArrayRegion(jprogram, 0, apf_program_len, reinterpret_cast<jbyte*>(buf.data()));
    env->GetByteArrayRegion(jdata, 0, data_len,
                            reinterpret_cast<jbyte*>(buf.data() + apf_program_len));

    // Open pcap file
    ScopedFILE apf_fp(fopen(pcap_filename.c_str(), "rb"));
    ScopedPcap apf_pcap(pcap_fopen_offline(apf_fp.get(), pcap_error));

    if (apf_pcap.get() == NULL) {
        throwException(env, "pcap_fopen_offline failed: " + std::string(pcap_error));
        return false;
    }

    while ((apf_packet = pcap_next(apf_pcap.get(), &apf_header)) != NULL) {
        int result = accept_packet(buf.data(), apf_program_len,
                                    apf_program_len + data_len, apf_packet, apf_header.len, 0);

        // Return false once packet passes the filter
        if (result) {
            env->SetByteArrayRegion(jdata, 0, data_len,
                                    reinterpret_cast<jbyte*>(buf.data() + apf_program_len));
            return false;
         }
    }

    env->SetByteArrayRegion(jdata, 0, data_len,
                            reinterpret_cast<jbyte*>(buf.data() + apf_program_len));
    return true;
}

extern "C" jint JNI_OnLoad(JavaVM* vm, void*) {
    JNIEnv *env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "ERROR: GetEnv failed");
        return -1;
    }

    static JNINativeMethod gMethods[] = {
            { "apfSimulate", "([B[B[BI)I",
                    (void*)com_android_server_ApfTest_apfSimulate },
            { "compileToBpf", "(Ljava/lang/String;)Ljava/lang/String;",
                    (void*)com_android_server_ApfTest_compileToBpf },
            { "compareBpfApf", "(Ljava/lang/String;Ljava/lang/String;[B)Z",
                    (void*)com_android_server_ApfTest_compareBpfApf },
            { "dropsAllPackets", "([B[BLjava/lang/String;)Z",
                    (void*)com_android_server_ApfTest_dropsAllPackets },
    };

    jniRegisterNativeMethods(env, "android/net/apf/ApfTest",
            gMethods, ARRAY_SIZE(gMethods));

    return JNI_VERSION_1_6;
}
