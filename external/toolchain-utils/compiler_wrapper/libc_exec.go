// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// +build libc_exec

package main

// #include <errno.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
//
// int libc_exec(const char *pathname, char *const argv[], char *const envp[]) {
//	if (execve(pathname, argv, envp) != 0) {
//		return errno;
//	}
//	return 0;
//}
import "C"
import (
	"os/exec"
	"unsafe"
)

// Replacement for syscall.Execve that uses the libc.
// This allows tools that rely on intercepting syscalls via
// LD_PRELOAD to work properly (e.g. gentoo sandbox).
// Note that this changes the go binary to be a dynamically linked one.
// See crbug.com/1000863 for details.
// To use this version of exec, please add '-tags libc_exec' when building Go binary.
// Without the tags, libc_exec.go will be used.

func execCmd(env env, cmd *command) error {
	freeList := []unsafe.Pointer{}
	defer func() {
		for _, ptr := range freeList {
			C.free(ptr)
		}
	}()

	goStrToC := func(goStr string) *C.char {
		cstr := C.CString(goStr)
		freeList = append(freeList, unsafe.Pointer(cstr))
		return cstr
	}

	goSliceToC := func(goSlice []string) **C.char {
		// len(goSlice)+1 as the c array needs to be null terminated.
		cArray := C.malloc(C.size_t(len(goSlice)+1) * C.size_t(unsafe.Sizeof(uintptr(0))))
		freeList = append(freeList, cArray)

		// Convert the C array to a Go Array so we can index it.
		// Note: Storing pointers to the c heap in go pointer types is ok
		// (see https://golang.org/cmd/cgo/).
		cArrayForIndex := (*[1<<30 - 1]*C.char)(cArray)
		for i, str := range goSlice {
			cArrayForIndex[i] = goStrToC(str)
		}
		cArrayForIndex[len(goSlice)] = nil

		return (**C.char)(cArray)
	}

	execCmd := exec.Command(cmd.Path, cmd.Args...)
	mergedEnv := mergeEnvValues(env.environ(), cmd.EnvUpdates)
	if errno := C.libc_exec(goStrToC(execCmd.Path), goSliceToC(execCmd.Args), goSliceToC(mergedEnv)); errno != 0 {
		return newErrorwithSourceLocf("exec error: %d", errno)
	}

	return nil
}
