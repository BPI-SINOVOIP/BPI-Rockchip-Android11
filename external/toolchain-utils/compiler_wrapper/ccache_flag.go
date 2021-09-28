// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

func processCCacheFlag(sysroot string, builder *commandBuilder) {
	// We should be able to share the objects across compilers as
	// the pre-processed output will differ.  This allows boards
	// that share compiler flags (like x86 boards) to share caches.
	const ccacheDir = "/var/cache/distfiles/ccache"

	useCCache := true
	builder.transformArgs(func(arg builderArg) string {
		if arg.value == "-noccache" {
			useCCache = false
			return ""
		}
		return arg.value
	})

	if builder.cfg.useCCache && useCCache {
		// We need to get ccache to make relative paths from within the
		// sysroot.  This lets us share cached files across boards (if
		// all other things are equal of course like CFLAGS) as well as
		// across versions.  A quick test is something like:
		//   $ export CFLAGS='-O2 -g -pipe' CXXFLAGS='-O2 -g -pipe'
		//   $ BOARD=x86-alex
		//   $ cros_workon-$BOARD stop cros-disks
		//   $ emerge-$BOARD cros-disks
		//   $ cros_workon-$BOARD start cros-disks
		//   $ emerge-$BOARD cros-disks
		//   $ BOARD=amd64-generic
		//   $ cros_workon-$BOARD stop cros-disks
		//   $ emerge-$BOARD cros-disks
		//   $ cros_workon-$BOARD start cros-disks
		//   $ emerge-$BOARD cros-disks
		// All of those will get cache hits (ignoring the first one
		// which will seed the cache) due to this setting.
		builder.updateEnv("CCACHE_BASEDIR=" + sysroot)
		if _, present := builder.env.getenv("CCACHE_DISABLE"); present {
			// Portage likes to set this for us when it has FEATURES=-ccache.
			// The other vars we need to setup manually because of tools like
			// scons that scrubs the env before we get executed.
			builder.updateEnv("CCACHE_DISABLE=")
		}
		// If RESTRICT=sandbox is enabled, then sandbox won't be setup,
		// and the env vars won't be available for appending.
		if sandboxRewrite, present := builder.env.getenv("SANDBOX_WRITE"); present {
			builder.updateEnv("SANDBOX_WRITE=" + sandboxRewrite + ":" + ccacheDir)
		}

		// Make sure we keep the cached files group writable.
		builder.updateEnv("CCACHE_DIR="+ccacheDir, "CCACHE_UMASK=002")

		// ccache may generate false positive warnings.
		// Workaround bug https://crbug.com/649740
		if builder.target.compilerType == clangType {
			builder.updateEnv("CCACHE_CPP2=yes")
		}

		builder.wrapPath("/usr/bin/ccache")
	}
}
