// Copyright 2016 Google Inc. All rights reserved.
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

package phony

import (
	"fmt"
	"io"
	"strings"

	"android/soong/android"
)

func init() {
	android.RegisterModuleType("phony", PhonyFactory)
}

type phony struct {
	android.ModuleBase
	requiredModuleNames       []string
	hostRequiredModuleNames   []string
	targetRequiredModuleNames []string
}

func PhonyFactory() android.Module {
	module := &phony{}

	android.InitAndroidArchModule(module, android.HostAndDeviceSupported, android.MultilibCommon)
	return module
}

func (p *phony) GenerateAndroidBuildActions(ctx android.ModuleContext) {
	p.requiredModuleNames = ctx.RequiredModuleNames()
	p.hostRequiredModuleNames = ctx.HostRequiredModuleNames()
	p.targetRequiredModuleNames = ctx.TargetRequiredModuleNames()
	if len(p.requiredModuleNames) == 0 &&
		len(p.hostRequiredModuleNames) == 0 && len(p.targetRequiredModuleNames) == 0 {
		ctx.PropertyErrorf("required", "phony must not have empty required dependencies "+
			"in order to be useful(and therefore permitted).")
	}
}

func (p *phony) AndroidMk() android.AndroidMkData {
	return android.AndroidMkData{
		Custom: func(w io.Writer, name, prefix, moduleDir string, data android.AndroidMkData) {
			fmt.Fprintln(w, "\ninclude $(CLEAR_VARS)")
			fmt.Fprintln(w, "LOCAL_PATH :=", moduleDir)
			fmt.Fprintln(w, "LOCAL_MODULE :=", name)
			if p.Host() {
				fmt.Fprintln(w, "LOCAL_IS_HOST_MODULE := true")
			}
			if len(p.requiredModuleNames) > 0 {
				fmt.Fprintln(w, "LOCAL_REQUIRED_MODULES :=",
					strings.Join(p.requiredModuleNames, " "))
			}
			if len(p.hostRequiredModuleNames) > 0 {
				fmt.Fprintln(w, "LOCAL_HOST_REQUIRED_MODULES :=",
					strings.Join(p.hostRequiredModuleNames, " "))
			}
			if len(p.targetRequiredModuleNames) > 0 {
				fmt.Fprintln(w, "LOCAL_TARGET_REQUIRED_MODULES :=",
					strings.Join(p.targetRequiredModuleNames, " "))
			}
			fmt.Fprintln(w, "include $(BUILD_PHONY_PACKAGE)")
		},
	}
}
