# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
AC_INIT([cras], [0.1], [dgreid@chromium.org],
             [cras], [http://www.chromium.org/])
AC_PREREQ([2.59])

AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([1.10 -Wall no-define])
#AC_CONFIG_HEADERS([config.h])

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_LIBTOOL
AC_PROG_CC
# c++ unit test (gtest).
AC_PROG_CXX
AC_LANG_C
AM_PROG_CC_C_O
PKG_PROG_PKG_CONFIG
AC_CONFIG_FILES([Makefile src/Makefile libcras.pc])

PKG_CHECK_MODULES([LIBSPEEX], [ speexdsp >= 1.2 ])
PKG_CHECK_MODULES([ASOUNDLIB], [ alsa >= 1.1.0 ])

AC_ARG_ENABLE([dbus], AS_HELP_STRING([--disable-dbus], [Disable all DBUS uses]), have_dbus=$enableval, have_dbus=yes)
AM_CONDITIONAL(HAVE_DBUS, test "$have_dbus" = "yes")
if test "$have_dbus" = "yes"; then
    PKG_CHECK_MODULES([DBUS], [ dbus-1 >= 1.4.12 ])
    AS_VAR_APPEND([DBUS_CFLAGS], [" -DCRAS_DBUS"])
else
    DBUS_CFLAGS=
    DBUS_LIBS=
fi
AC_SUBST(DBUS_LIBS)
AC_SUBST(DBUS_CFLAGS)

AC_ARG_ENABLE([selinux], AS_HELP_STRING([--enable-selinux], [Enable SELinux calls]), have_selinux=$enableval, have_selinux=no)
AM_CONDITIONAL(HAVE_SELINUX, test "$have_selinux" = "yes")
if test "$have_selinux" = "yes"; then
    PKG_CHECK_MODULES([SELINUX], [ libselinux ])
    AS_VAR_APPEND([SELINUX_CFLAGS], [" -DCRAS_SELINUX"])
else
    SELINUX_CFLAGS=
    SELINUX_LIBS=
fi
AC_SUBST(SELINUX_CFLAGS)
AC_SUBST(SELINUX_LIBS)

# WEBRTC APM support
AC_ARG_ENABLE([webrtc-apm], AS_HELP_STRING([--disable-webrtc-apm], [Disable webrtc-apm uses]), have_webrtc_apm=$enableval, have_webrtc_apm=yes)
AM_CONDITIONAL(HAVE_WEBRTC_APM, test "$have_webrtc_apm" = "yes")
if test "$have_webrtc_apm" = "yes"; then
    PKG_CHECK_MODULES([WEBRTC_APM], [ libwebrtc_apm ])
    AC_DEFINE(HAVE_WEBRTC_APM, 1, [Define to use webrtc-apm.])
else
    WEBRTC_APM_LIBS=
fi
AC_SUBST(WEBRTC_APM_LIBS)

PKG_CHECK_MODULES([SBC], [ sbc >= 1.0 ])
AC_CHECK_LIB(asound, snd_pcm_ioplug_create,,
	     AC_ERROR([*** libasound has no external plugin SDK]), -ldl)

AC_ARG_ENABLE([alsa-plugin], AS_HELP_STRING([--disable-alsa-plugin],
                                            [Disable building of ALSA plugin]))

AC_ARG_ENABLE([metrics], AS_HELP_STRING([--enable-metrics], [Enable metrics uses]), have_metrics=$enableval, have_metrics=no)
if test "$have_metrics" = "yes"; then
    AC_DEFINE(HAVE_LIB_METRICS, 1, [Define to use libmetrics])
    METRICS_LIBS=-lmetrics-${BASE_VER}
else
    METRICS_LIBS=
fi
AC_SUBST(METRICS_LIBS)

# Determine ALSA plugin directory.
test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix=$prefix

AC_ARG_WITH(plugindir,
    AS_HELP_STRING([--with-plugindir=dir],
	[path where ALSA plugin files are stored]),
    plugindir="$withval", plugindir="")
if test -z "$plugindir"; then
    eval dir="$libdir"
    case "$dir" in
    /*) ;;
    *) dir="$dir"
    esac
    plugindir="$dir/alsa-lib"
fi
AC_DEFINE_UNQUOTED(ALSA_PLUGIN_DIR, "$plugindir",
                   [directory containing ALSA add-on modules])
ALSA_PLUGIN_DIR="$plugindir"
AC_SUBST(ALSA_PLUGIN_DIR)

# Determine CRAS configuration directory.
eval cras_config_file_dir="$sysconfdir/cras"
AC_DEFINE_UNQUOTED(CRAS_CONFIG_FILE_DIR, "$cras_config_file_dir",
                   [directory containing CRAS configuration])

# CRAS socket dir
AC_ARG_WITH(socketdir,
    AS_HELP_STRING([--with-socketdir=dir],
        [path where CRAS stores its sockets]),
    socketdir="$withval",
    socketdir="/run/cras")
AC_DEFINE_UNQUOTED(CRAS_SOCKET_FILE_DIR, "$socketdir",
                   [directory containing CRAS socket files])

# SSE4_2 support
AC_ARG_ENABLE(sse42, [AS_HELP_STRING([--enable-sse42],[enable SSE42 optimizations])], have_sse42=$enableval, have_sse42=yes)
if  test "x$host_cpu" != xx86_64; then
	have_sse42=no
fi
if test "$have_sse42" = "yes"; then
        AC_DEFINE(HAVE_SSE42,1,[Define to enable SSE42 optimizations.])
	SSE42_CFLAGS="-DOPS_SSE42 -msse4.2 -ffast-math"
fi
AM_CONDITIONAL(HAVE_SSE42, test "$have_sse42" = "yes")
AC_SUBST(SSE42_CFLAGS)

# AVX support
AC_ARG_ENABLE(avx, [AS_HELP_STRING([--enable-avx],[enable AVX optimizations])], have_avx=$enableval, have_avx=yes)
if  test "x$host_cpu" != xx86_64; then
	have_avx=no
fi
if test "$have_avx" = "yes"; then
        AC_DEFINE(HAVE_AVX,1,[Define to enable AVX optimizations.])
	AVX_CFLAGS="-DOPS_AVX -mavx -ffast-math"
fi
AM_CONDITIONAL(HAVE_AVX, test "$have_avx" = "yes")
AC_SUBST(AVX_CFLAGS)

# AVX2 support
AC_ARG_ENABLE(avx2, [AS_HELP_STRING([--enable-avx2],[enable AVX2 optimizations])], have_avx2=$enableval, have_avx2=yes)
if  test "x$host_cpu" != xx86_64; then
	have_avx2=no
fi
if test "$have_avx2" = "yes"; then
        AC_DEFINE(HAVE_AVX2,1,[Define to enable AVX2 optimizations.])
	AVX2_CFLAGS="-DOPS_AVX2 -mavx2 -ffast-math"
fi
AM_CONDITIONAL(HAVE_AVX2, test "$have_avx2" = "yes")
AC_SUBST(AVX2_CFLAGS)

# FMA support
AC_ARG_ENABLE(fma, [AS_HELP_STRING([--enable-fma],[enable FMA optimizations])], have_fma=$enableval, have_fma=yes)
if  test "x$host_cpu" != xx86_64; then
	have_fma=no
fi
if test "$have_fma" = "yes"; then
        AC_DEFINE(HAVE_FMA,1,[Define to enable FMA optimizations.])
	FMA_CFLAGS="-DOPS_FMA -mavx2 -mfma -ffast-math"
fi
AM_CONDITIONAL(HAVE_FMA, test "$have_fma" = "yes")
AC_SUBST(FMA_CFLAGS)

AC_OUTPUT

AC_MSG_NOTICE([

Enable SSE42:                  ${have_sse42}
Enable AVX:                    ${have_avx}
Enable AVX2:                   ${have_avx2}
Enable FMA:                    ${have_fma}
])
