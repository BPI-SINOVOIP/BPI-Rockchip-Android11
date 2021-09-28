/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <jni.h>
#include <nativehelper/JNIHelp.h>
#include <nativehelper/ScopedLocalRef.h>
#include <nativehelper/ScopedUtfChars.h>
#include <selinux/selinux.h>
#include <android/log.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <memory>

struct SecurityContext_Delete {
    void operator()(security_context_t p) const {
        freecon(p);
    }
};
typedef std::unique_ptr<char[], SecurityContext_Delete> Unique_SecurityContext;

/**
 * Function: checkNetlinkRouteGetlink
 * Purpose: Checks to see if RTM_GETLINK is allowed on a netlink route socket.
 * Returns: 13 (expected) if RTM_GETLINK fails with permission denied.
 *          0 if socket creation fails
 *          -1 if bind() succeeds.
 */
static jint checkNetlinkRouteGetlink() {
    int sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sock < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "SELLinuxTargetSdkTest", "socket creation failed.");
        return 0;
    }
    struct NetlinkMessage {
        nlmsghdr hdr;
        rtgenmsg msg;
    } request;
    memset(&request, 0, sizeof(request));
    request.hdr.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
    request.hdr.nlmsg_type = RTM_GETLINK;
    request.hdr.nlmsg_len = sizeof(request);
    request.msg.rtgen_family = AF_UNSPEC;

    int ret = send(sock, &request, sizeof(request), 0);
    if (ret < 0) {
        return errno;
    }

    return -1;
}

/**
 * Function: checkNetlinkRouteBind
 * Purpose: Checks to see if bind() is allowed on a netlink route socket.
 * Returns: 13 (expected) if bind() fails with permission denied.
 *          0 if socket creation fails
 *          -1 if bind() succeeds.
 */
static jint checkNetlinkRouteBind() {
    int sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sock < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "SELLinuxTargetSdkTest", "socket creation failed.");
        return 0;
    }

    struct sockaddr_nl addr;
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = RTMGRP_LINK|RTMGRP_IPV4_IFADDR|RTMGRP_IPV6_IFADDR;

    int ret = bind(sock,(struct sockaddr *)&addr,sizeof(addr));
    if (ret < 0) {
        return errno;
    }

    return -1;
}

/*
 * Function: getFileContext
 * Purpose: retrieves the context associated with the given path in the file system
 * Parameters:
 *        path: given path in the file system
 * Returns:
 *        string representing the security context string of the file object
 *        the string may be NULL if an error occured
 * Exceptions: NullPointerException if the path object is null
 */
static jstring getFileContext(JNIEnv *env, jobject, jstring pathStr) {
    ScopedUtfChars path(env, pathStr);
    if (path.c_str() == NULL) {
        return NULL;
    }

    security_context_t tmp = NULL;
    int ret = getfilecon(path.c_str(), &tmp);
    Unique_SecurityContext context(tmp);

    ScopedLocalRef<jstring> securityString(env, NULL);
    if (ret != -1) {
        securityString.reset(env->NewStringUTF(context.get()));
    }

    return securityString.release();
}

static JNINativeMethod gMethods[] = {
    { "getFileContext", "(Ljava/lang/String;)Ljava/lang/String;", (void*) getFileContext },
    { "checkNetlinkRouteBind", "()I", (void*) checkNetlinkRouteBind },
    { "checkNetlinkRouteGetlink", "()I", (void*) checkNetlinkRouteGetlink },
};

int register_android_security_SELinuxTargetSdkTest(JNIEnv* env)
{
    jclass clazz = env->FindClass("android/security/SELinuxTargetSdkTestBase");

    return env->RegisterNatives(clazz, gMethods,
            sizeof(gMethods) / sizeof(JNINativeMethod));
}
