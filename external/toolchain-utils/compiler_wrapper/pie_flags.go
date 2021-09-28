// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

func processPieFlags(builder *commandBuilder) {
	fpieMap := map[string]bool{"-D__KERNEL__": true, "-fPIC": true, "-fPIE": true, "-fno-PIC": true, "-fno-PIE": true,
		"-fno-pic": true, "-fno-pie": true, "-fpic": true, "-fpie": true, "-nopie": true,
		"-nostartfiles": true, "-nostdlib": true, "-pie": true, "-static": true}

	pieMap := map[string]bool{"-D__KERNEL__": true, "-A": true, "-fno-PIC": true, "-fno-PIE": true, "-fno-pic": true, "-fno-pie": true,
		"-nopie": true, "-nostartfiles": true, "-nostdlib": true, "-pie": true, "-r": true, "--shared": true,
		"-shared": true, "-static": true}

	pie := false
	fpie := false
	if builder.target.abi != "eabi" {
		for _, arg := range builder.args {
			if arg.fromUser {
				if fpieMap[arg.value] {
					fpie = true
				}
				if pieMap[arg.value] {
					pie = true
				}
			}
		}
	}
	builder.transformArgs(func(arg builderArg) string {
		// Remove -nopie as it is a non-standard flag.
		if arg.value == "-nopie" {
			return ""
		}
		if fpie && !arg.fromUser && arg.value == "-fPIE" {
			return ""
		}
		if pie && !arg.fromUser && arg.value == "-pie" {
			return ""
		}
		return arg.value
	})
}
