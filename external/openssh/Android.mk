LOCAL_PATH:= $(call my-dir)

openssh_common_cflags := \
    -Wall \
    -Werror \
    -Wno-error=implicit-function-declaration \
    -Wno-pointer-sign \
    -Wno-sign-compare \
    -Wno-type-limits \
    -Wno-unused-parameter \
    -Wno-unused-variable \
    -Wno-error \

# Use -Wno-error to allow at least the following warnings:
# (1) bsd-openpty.c calls to 'ptsname' declared with attribute warning:
#     ptsname is not thread-safe; use ptsname_r instead [-Werror]
# (2) external/boringssl/src/include/openssl/opensslfeatures.h:
#     error: "OPENSSL_NO_BF" redefined [-Werror]

openssh_common_clang_cflags := \
    -Wno-incompatible-pointer-types \
    -Wno-macro-redefined \

###################### libssh ######################
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
    addrmatch.c \
    atomicio.c \
    authfd.c \
    authfile.c \
    bitmap.c \
    blocks.c \
    bufaux.c \
    bufbn.c \
    bufec.c \
    buffer.c \
    canohost.c \
    chacha.c \
    channels.c \
    cipher-aes.c \
    cipher-aesctr.c \
    cipher-chachapoly.c \
    cipher-ctr.c \
    cipher.c \
    cleanup.c \
    compat.c \
    crc32.c \
    deattack.c \
    dh.c \
    digest-openssl.c \
    dispatch.c \
    dns.c \
    ed25519.c \
    entropy.c \
    fatal.c \
    fe25519.c \
    ge25519.c \
    gss-genr.c \
    hash.c \
    hmac.c \
    hostfile.c \
    kex.c \
    kexc25519.c \
    kexc25519c.c \
    kexdh.c \
    kexdhc.c \
    kexecdh.c \
    kexecdhc.c \
    kexgex.c \
    kexgexc.c \
    key.c \
    krl.c \
    log.c \
    mac.c \
    match.c \
    md-sha256.c \
    misc.c \
    moduli.c \
    monitor_fdpass.c \
    msg.c \
    nchan.c \
    opacket.c \
    openbsd-compat/bcrypt_pbkdf.c \
    openbsd-compat/bindresvport.c \
    openbsd-compat/blowfish.c \
    openbsd-compat/bsd-closefrom.c \
    openbsd-compat/bsd-getpeereid.c \
    openbsd-compat/bsd-misc.c \
    openbsd-compat/bsd-openpty.c \
    openbsd-compat/bsd-statvfs.c \
    openbsd-compat/explicit_bzero.c \
    openbsd-compat/fmt_scaled.c \
    openbsd-compat/getopt_long.c \
    openbsd-compat/glob.c \
    openbsd-compat/openssl-compat.c \
    openbsd-compat/port-linux.c \
    openbsd-compat/port-tun.c \
    openbsd-compat/pwcache.c \
    openbsd-compat/readpassphrase.c \
    openbsd-compat/reallocarray.c \
    openbsd-compat/rresvport.c \
    openbsd-compat/setproctitle.c \
    openbsd-compat/strmode.c \
    openbsd-compat/strtonum.c \
    openbsd-compat/timingsafe_bcmp.c \
    openbsd-compat/vis.c \
    packet.c \
    platform-pledge.c \
    platform-tracing.c \
    poly1305.c \
    readpass.c \
    rijndael.c \
    rsa.c \
    sc25519.c \
    smult_curve25519_ref.c \
    ssh-dss.c \
    ssh-ecdsa.c \
    ssh-ed25519.c \
    ssh-rsa.c \
    sshbuf-getput-basic.c \
    sshbuf-getput-crypto.c \
    sshbuf-misc.c \
    sshbuf.c \
    ssherr.c \
    sshkey.c \
    ttymodes.c \
    uidswap.c \
    umac.c \
    umac128.c \
    utf8.c \
    uuencode.c \
    verify.c \
    xmalloc.c

LOCAL_C_INCLUDES := \
    external/zlib \
    external/openssl/include \
    external/openssh/openbsd-compat

LOCAL_SHARED_LIBRARIES += libssl libcrypto libdl libz

LOCAL_MODULE := libssh

LOCAL_CFLAGS += -O3 $(openssh_common_cflags)
LOCAL_CLANG_CFLAGS += $(openssh_common_clang_cflags)

LOCAL_CFLAGS += -DGCE_PLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)
ifneq ($(filter gce_x86 gce_x86_64 calypso, $(TARGET_DEVICE)),)
LOCAL_CFLAGS += -DANDROID_GCE -DSSHDIR=\"/var/run/ssh\"
endif

ifneq (,$(SSHDIR))
LOCAL_CFLAGS += -DSSHDIR=\"$(SSHDIR)\"
endif

include $(BUILD_SHARED_LIBRARY)

###################### ssh ######################

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
    ssh.c readconf.c clientloop.c sshtty.c \
    sshconnect.c sshconnect1.c sshconnect2.c mux.c

LOCAL_MODULE := ssh


LOCAL_CFLAGS += $(openssh_common_cflags)
LOCAL_CLANG_CFLAGS += $(openssh_common_clang_cflags)

LOCAL_C_INCLUDES := \
    external/zlib \
    external/openssl/include \
    external/openssh/openbsd-compat

LOCAL_SHARED_LIBRARIES += libssh libssl libcrypto libdl libz

include $(BUILD_EXECUTABLE)

###################### sftp ######################

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
    sftp.c sftp-client.c sftp-common.c sftp-glob.c progressmeter.c

LOCAL_MODULE := sftp

LOCAL_CFLAGS += $(openssh_common_cflags)
LOCAL_CLANG_CFLAGS += $(openssh_common_clang_cflags)

LOCAL_C_INCLUDES := \
    external/zlib \
    external/openssl/include \
    external/openssh/openbsd-compat

LOCAL_SHARED_LIBRARIES += libssh libssl libcrypto libdl libz

include $(BUILD_EXECUTABLE)

###################### scp ######################

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
    scp.c progressmeter.c

LOCAL_MODULE := scp

LOCAL_CFLAGS += $(openssh_common_cflags)
LOCAL_CLANG_CFLAGS += $(openssh_common_clang_cflags)

LOCAL_C_INCLUDES := \
    external/zlib \
    external/openssl/include \
    external/openssh/openbsd-compat

LOCAL_SHARED_LIBRARIES += libssh libssl libcrypto libdl libz

include $(BUILD_EXECUTABLE)

###################### sshd ######################

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
    audit-bsm.c \
    audit-linux.c \
    audit.c \
    auth-bsdauth.c \
    auth-krb5.c \
    auth-options.c \
    auth-pam.c \
    auth-rhosts.c \
    auth-shadow.c \
    auth-sia.c \
    auth-skey.c \
    auth.c \
    auth2-chall.c \
    auth2-gss.c \
    auth2-hostbased.c \
    auth2-kbdint.c \
    auth2-none.c \
    auth2-passwd.c \
    auth2-pubkey.c \
    auth2.c \
    groupaccess.c \
    gss-serv-krb5.c \
    gss-serv.c \
    kexc25519s.c \
    kexdhs.c \
    kexecdhs.c \
    kexgexs.c \
    loginrec.c \
    md5crypt.c \
    monitor.c \
    monitor_wrap.c \
    platform.c \
    sandbox-null.c \
    sandbox-rlimit.c \
    sandbox-systrace.c \
    servconf.c \
    serverloop.c \
    session.c \
    sftp-common.c \
    sftp-server.c \
    sshd.c \
    sshlogin.c \
    sshpty.c

LOCAL_MODULE := sshd

LOCAL_CFLAGS += $(openssh_common_cflags)
LOCAL_CLANG_CFLAGS += $(openssh_common_clang_cflags)
ifneq ($(filter gce_x86 gce_x86_64 calypso, $(TARGET_DEVICE)),)
LOCAL_CFLAGS += -DANDROID_GCE $(GCE_VERSION_CFLAGS)
endif

LOCAL_C_INCLUDES := \
    external/zlib \
    external/openssl/include \
    external/openssh/openbsd-compat

LOCAL_SHARED_LIBRARIES += libssh libssl libcrypto libdl libz libcutils

# libc.bootstrap is available/required since Q
ifneq ($(PLATFORM_VERSION_CODENAME)|$(call math_lt,$(PLATFORM_SDK_VERSION),29),REL|true)
# This filter is for old branches that does not have math_lt macro
# This is equivalently PLATFORM_SDK_VERSION > 27
ifeq ($(filter 14 15 16 17 18 19 20 21 22 23 24 25 26 27,$(PLATFORM_SDK_VERSION)),)
LOCAL_SHARED_LIBRARIES += libc.bootstrap
endif
endif

include $(BUILD_EXECUTABLE)

###################### ssh-keygen ######################

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
    ssh-keygen.c

LOCAL_MODULE := ssh-keygen

LOCAL_CFLAGS += $(openssh_common_cflags)
LOCAL_CLANG_CFLAGS += $(openssh_common_clang_cflags)

LOCAL_C_INCLUDES := \
    external/zlib \
    external/openssl/include \
    external/openssh/openbsd-compat

LOCAL_SHARED_LIBRARIES += libssh libssl libcrypto libdl libz

include $(BUILD_EXECUTABLE)

###################### sshd_config ######################

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := sshd_config
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/ssh
LOCAL_SRC_FILES := sshd_config.android
include $(BUILD_PREBUILT)

###################### start-ssh ######################

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := start-ssh
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := start-ssh
include $(BUILD_PREBUILT)
