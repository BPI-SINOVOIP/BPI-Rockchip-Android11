// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package icu

import (
	"android/soong/android"
)

func init() {
	host_whitelist := []string{
		"art/build/apex/",
		"device/google/cuttlefish/host/commands/",
		"external/skia",
		"frameworks/base/libs/hwui",
	}

	device_whitelist := []string{
		"art/",
		"external/chromium-libpac",
		"external/icu/",
		"external/v8/",
		"libcore/",
	}

	android.AddNeverAllowRules(
		android.NeverAllow().
			InDirectDeps("libandroidicu").
			WithOsClass(android.Host).
			NotIn(host_whitelist...).
			Because("libandroidicu is not intended to be used on host"),
		android.NeverAllow().
			InDirectDeps("libicuuc").
			WithOsClass(android.Device).
			NotIn(device_whitelist...).
			Because("libicuuc is not intended to be used on device"),
		android.NeverAllow().
			InDirectDeps("libicui18n").
			WithOsClass(android.Device).
			NotIn(device_whitelist...).
			Because("libicui18n is not intended to be used on device"),
	)
}
