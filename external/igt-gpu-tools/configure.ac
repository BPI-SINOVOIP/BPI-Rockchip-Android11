#  Copyright 2005 Adam Jackson.
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  on the rights to use, copy, modify, merge, publish, distribute, sub
#  license, and/or sell copies of the Software, and to permit persons to whom
#  the Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
#  ADAM JACKSON BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
# Process this file with autoconf to produce a configure script

AC_PREREQ([2.60])
AC_INIT([igt-gpu-tools],
	[1.24],
	[https://bugs.freedesktop.org/enter_bug.cgi?product=DRI&component=IGT],
	[igt-gpu-tools])

AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_GNU_SOURCE
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([1.12 foreign subdir-objects dist-bzip2])
AM_PATH_PYTHON([3],, [:])

AC_PROG_CC
AC_PROG_LEX
AC_PROG_YACC

# Checks for functions, headers, structures, etc.
AC_HEADER_STDC
AC_CHECK_HEADERS([termios.h linux/kd.h sys/kd.h libgen.h sys/io.h])
AC_CHECK_MEMBERS([struct sysinfo.totalram],[],[],[AC_INCLUDES_DEFAULT
		  #include <sys/sysinfo.h>
		  ])
AC_CHECK_TYPES([sighandler_t],[],[],[AC_INCLUDES_DEFAULT
#include <signal.h>])
AC_CHECK_FUNCS([swapctl])
AC_CHECK_FUNCS([asprintf])
AX_GCC_FUNC_ATTRIBUTE([constructor])

dnl Check for POSIX timers
AC_CHECK_FUNCS(timer_create, [], [
	AC_CHECK_LIB(rt, timer_create, [
		TIMER_LIBS="-lrt"
		], [
		AC_CHECK_LIB(pthread, timer_create, [
			TIMER_LIBS="-lpthread"
		], [
		AC_MSG_ERROR([Failed to find library containing timer_create.])
		])
	])
])
AC_SUBST(TIMER_LIBS)

dnl Check for memfd_create
AC_CHECK_FUNCS(memfd_create)

dnl Check for CPUID
cpuid="yes"
AC_TRY_LINK([
	#include <cpuid.h>
	#include <stddef.h>
	],
	[
	int eax, ebx, ecx, edx;
	if (__get_cpuid_max(0, NULL) < 4)
		return 0;
	__cpuid_count(4, 0, eax, ebx, ecx, edx);
	],
	[cpuid="yes"],
	[cpuid="no"]
)
if test "x$cpuid" = "xyes"; then
	AC_DEFINE(HAVE_CPUID_H, 1, [Found a useable cpuid.h])
fi

# Initialize libtool
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

# Require X.Org macros 1.16 or later for XORG_TESTSET_CFLAG
m4_ifndef([XORG_MACROS_VERSION],
	  [m4_fatal([must install xorg-macros 1.16 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.16)
XORG_DEFAULT_OPTIONS

PKG_CHECK_MODULES(DRM, [libdrm >= 2.4.82])
PKG_CHECK_MODULES(PCIACCESS, [pciaccess >= 0.10])
PKG_CHECK_MODULES(KMOD, [libkmod])
PKG_CHECK_MODULES(PROCPS, [libprocps])
PKG_CHECK_MODULES(LIBUNWIND, [libunwind])
PKG_CHECK_MODULES(LIBDW, [libdw])
PKG_CHECK_MODULES(VALGRIND, [valgrind], [have_valgrind=yes], [have_valgrind=no])

if test x$have_valgrind = xyes; then
	AC_DEFINE(HAVE_VALGRIND, 1, [Enable valgrind annotation support.])
fi

case "$target_cpu" in
	x86*|i?86)
		build_x86="yes"
		;;
	*)
		build_x86="no"
		;;
esac

if test x"$build_x86" = xyes; then
	PKG_CHECK_MODULES(OVERLAY_XVLIB, [xv x11 xext dri2proto >= 2.6], enable_overlay_xvlib=yes, enable_overlay_xvlib=no)
	PKG_CHECK_MODULES(OVERLAY_XLIB, [cairo-xlib dri2proto >= 2.6], enable_overlay_xlib=yes, enable_overlay_xlib=no)

	AC_CHECK_TOOL([LEG], [leg])
	if test x"$LEG" != xleg; then
		enable_overlay_xvlib="no"
		enable_overlay_xlib="no"
		AC_MSG_NOTICE([Failed to find leg, required for overlay, try : apt-get install peg])
	fi
else
	enable_overlay_xvlib="no"
	enable_overlay_xlib="no"
fi
AM_CONDITIONAL(BUILD_X86, [test "x$build_x86" = xyes])

AM_CONDITIONAL(BUILD_OVERLAY_XVLIB, [test "x$enable_overlay_xvlib" = xyes])
AM_CONDITIONAL(BUILD_OVERLAY_XLIB, [test "x$enable_overlay_xlib" = xyes])
AM_CONDITIONAL(BUILD_OVERLAY, [test "x$enable_overlay_xlib" = xyes -o "x$enable_overlay_xvlib" = "xyes"])
if test x$enable_overlay_xvlib = xyes; then
	AC_DEFINE(HAVE_OVERLAY_XVLIB, 1, [Enable XV backend])
fi
if test x$enable_overlay_xlib = xyes; then
	AC_DEFINE(HAVE_OVERLAY_XLIB, 1, [Enable X backend])
fi
PKG_CHECK_MODULES(XRANDR, xrandr >= 1.3, AC_DEFINE(HAVE_XRANDR, 1, [Have libXrandr]), [have_xrandr=no])

# for testdisplay
PKG_CHECK_MODULES(CAIRO, [cairo >= 1.17.2], [],
	[AC_MSG_WARN([Cairo too old, HDR formats not available. Upgrade to cairo 1.17.2])
	PKG_CHECK_MODULES(CAIRO, [cairo >= 1.12.0])]
)
PKG_CHECK_MODULES(LIBUDEV, [libudev])
PKG_CHECK_MODULES(GLIB, [glib-2.0])
PKG_CHECK_MODULES(PIXMAN, [pixman-1 >= 0.36.0], [], [
	AC_MSG_WARN([Pixman too old, HDR formats not available. Upgrade to pixman 0.36.0])
	PKG_CHECK_MODULES(PIXMAN, [pixman-1])
])

PKG_CHECK_MODULES(GSL, [gsl], [gsl=yes], [gsl=no])
AM_CONDITIONAL(HAVE_GSL, [test "x$gsl" = xyes])

PKG_CHECK_MODULES(ALSA, [alsa], [alsa=yes], [alsa=no])
AM_CONDITIONAL(HAVE_ALSA, [test "x$alsa" = xyes])

# for chamelium
AC_ARG_ENABLE(chamelium, AS_HELP_STRING([--enable-chamelium],
	      [Enable building of chamelium libraries and tests (default: no)]),
	      [if test x$enableval = xyes; then enable_chamelium=yes; fi],
	      [enable_chamelium=no])
AM_CONDITIONAL(HAVE_CHAMELIUM, [test "x$enable_chamelium" = xyes])
if test "x$enable_chamelium" = xyes; then
	PKG_CHECK_MODULES(XMLRPC, xmlrpc xmlrpc_util xmlrpc_client,
		          [xmlrpc=yes], [xmlrpc=no])
	if test x"$xmlrpc" != xyes; then
		AC_CHECK_PROG([XMLRPC_C_CONFIG], [xmlrpc-c-config], [yes], [no])
		if test x"$XMLRPC_C_CONFIG" = xyes; then
			XMLRPC_CFLAGS=$(xmlrpc-c-config client --cflags)
			XMLRPC_LIBS=$(xmlrpc-c-config client --libs)
			AC_SUBST(XMLRPC_CFLAGS)
			AC_SUBST(XMLRPC_LIBS)
		else
			AC_MSG_ERROR([Failed to find xmlrpc, required by chamelium.])
		fi
	fi
	if test x"$gsl" != xyes; then
		AC_MSG_ERROR([Failed to find gsl, required by chamelium.])
	fi
	if test x"$alsa" != xyes; then
		AC_MSG_ERROR([Failed to find alsa, required by chamelium.])
	fi

	AC_DEFINE(HAVE_CHAMELIUM, 1, [Enable Chamelium support])
fi

# -----------------------------------------------------------------------------
#			Configuration options
# -----------------------------------------------------------------------------
AC_ARG_ENABLE(intel, AS_HELP_STRING([--disable-intel],
	      [Enable building of intel specific parts (default: auto)]),
	      [INTEL=$enableval], [INTEL=auto])
if test "x$INTEL" = xauto; then
	PKG_CHECK_EXISTS([libdrm_intel >= 2.4.75], [INTEL=yes], [INTEL=no])
fi
if test "x$INTEL" = xyes; then
	PKG_CHECK_MODULES(DRM_INTEL, [libdrm_intel >= 2.4.75])
	AC_DEFINE(HAVE_LIBDRM_INTEL, 1, [Have intel support])
else
	DRM_INTEL_CFLAGS=-I$\(top_srcdir\)/lib/stubs/drm/
	DRM_INTEL_LIBS=
fi
DRM_CFLAGS="$DRM_CFLAGS $DRM_INTEL_CFLAGS"
DRM_LIBS="$DRM_LIBS $DRM_INTEL_LIBS"
AC_SUBST([DRM_CFLAGS])
AC_SUBST([DRM_LIBS])

AM_CONDITIONAL(HAVE_LIBDRM_INTEL, [test "x$INTEL" = xyes])

AC_ARG_ENABLE(amdgpu, AS_HELP_STRING([--disable-amdgpu],
	      [Enable building of amdgpu tests (default: auto)]),
	      [AMDGPU=$enableval], [AMDGPU=auto])
if test "x$AMDGPU" = xauto; then
	PKG_CHECK_EXISTS([libdrm_amdgpu >= 2.4.81], [AMDGPU=yes], [AMDGPU=no])
fi
if test "x$AMDGPU" = xyes; then
	PKG_CHECK_MODULES(DRM_AMDGPU, [libdrm_amdgpu >= 2.4.81])
	AC_DEFINE(HAVE_LIBDRM_AMDGPU, 1, [Have amdgpu support])
fi
AM_CONDITIONAL(HAVE_LIBDRM_AMDGPU, [test "x$AMDGPU" = xyes])

# for dma-buf tests
AC_ARG_ENABLE(nouveau, AS_HELP_STRING([--disable-nouveau],
	      [Enable use of nouveau API for prime tests (default: auto)]),
	      [NOUVEAU=$enableval], [NOUVEAU=auto])
if test "x$NOUVEAU" = xauto; then
	PKG_CHECK_EXISTS([libdrm_nouveau >= 2.4.33], [NOUVEAU=yes], [NOUVEAU=no])
fi
if test "x$NOUVEAU" = xyes; then
	PKG_CHECK_MODULES(DRM_NOUVEAU, [libdrm_nouveau >= 2.4.33])
	AC_DEFINE(HAVE_LIBDRM_NOUVEAU, 1, [Have nouveau support])
fi
AM_CONDITIONAL(HAVE_LIBDRM_NOUVEAU, [test "x$NOUVEAU" = xyes])

# enable debug symbols
AC_ARG_ENABLE(debug,
	      AS_HELP_STRING([--disable-debug],
			     [Build tests without debug symbols]),
	      [], [enable_debug=yes])

AC_ARG_ENABLE(werror,
	      AS_HELP_STRING([--enable-werror],
			     [Fail on warnings]),
	      [], [enable_werror=no])

# disable "warning: pointer of type ‘void *’ used in arithmetic"
AS_COMPILER_FLAG([-Wno-pointer-arith], [CWARNFLAGS+=" -Wno-pointer-arith"], [])

if test "x$enable_debug" = xyes; then
	AS_COMPILER_FLAG([-g3], [DEBUG_CFLAGS="-g3"], [DEBUG_CFLAGS="-g"])
	AS_COMPILER_FLAG([-Og], [DEBUG_CFLAGS+=" -Og -Wno-maybe-uninitialized"], # disable maybe-uninitialized due to false positives
			 [DEBUG_CFLAGS+=" -O0"])
	AC_SUBST([DEBUG_CFLAGS])
fi

if test "x$enable_werror" = xyes; then
	AS_COMPILER_FLAG([-Werror], [WERROR_CFLAGS="-Werror"])
fi

# prevent relinking the world on every commit for developers
AC_ARG_ENABLE(git-hash,
	      AS_HELP_STRING([--disable-git-hash],
			     [Do not use git hash in version]),
	      [git_hash=$enableval], [git_hash=yes])
AC_SUBST(GIT_HASH, [$git_hash])

# -----------------------------------------------------------------------------

# To build multithread code, gcc uses -pthread, Solaris Studio cc uses -mt
XORG_TESTSET_CFLAG([THREAD_CFLAGS], [-pthread], [-mt])
AC_SUBST([THREAD_CFLAGS])

AC_ARG_ENABLE(tests,
	      AS_HELP_STRING([--disable-tests],
	      [Disable tests build (default: enabled)]),
	      [BUILD_TESTS=$enableval], [BUILD_TESTS="yes"])
if test "x$BUILD_TESTS" = xyes; then
	AC_DEFINE(BUILD_TESTS, 1, [Build tests])
fi
AM_CONDITIONAL(BUILD_TESTS, [test "x$BUILD_TESTS" = xyes])
AC_DEFINE_UNQUOTED(TARGET_CPU_PLATFORM, ["$host_cpu"], [Target platform])

files="broadwell cherryview haswell ivybridge sandybridge valleyview skylake"
for file in $files; do
	REGISTER_FILES="$REGISTER_FILES $file `cat $srcdir/tools/registers/$file`"
done
REGISTER_FILES=`echo $REGISTER_FILES | tr ' ' '\n' | sort -u | tr '\n' ' '`
AC_SUBST(REGISTER_FILES)

AC_CONFIG_FILES([
		 Makefile
		 benchmarks/Makefile
		 lib/Makefile
		 scripts/Makefile
		 tests/Makefile
		 tests/intel-ci/Makefile
		 tools/Makefile
		 tools/null_state_gen/Makefile
		 tools/registers/Makefile
		 overlay/Makefile
		 ])

AC_CONFIG_FILES([tools/intel_aubdump], [chmod +x tools/intel_aubdump])

AC_OUTPUT

# Print a summary of the compilation
echo ""
echo "Intel GPU tools"

echo ""
echo " • Tests:"
echo "       Build tests        : ${BUILD_TESTS}"
echo "       Chamelium tests    : ${enable_chamelium}"
echo "       Compile prime tests: ${NOUVEAU}"
echo "       Debug flags        : ${DEBUG_CFLAGS}"
echo ""
echo " • Tools:"
echo "       Overlay            : X: ${enable_overlay_xlib}, Xv: ${enable_overlay_xvlib}"
echo "       x86-specific tools : ${build_x86}"
echo "       Test runner        : ${BUILD_RUNNER}"
echo ""
echo " • Fail on warnings       : ${enable_werror}"
echo ""
echo " WARNING: automake is deprecated, please use meson"
echo ""

# vim: set ft=config ts=8 sw=8 tw=0 noet :
