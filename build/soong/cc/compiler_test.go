// Copyright 2019 Google Inc. All rights reserved.
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

package cc

import (
	"testing"
)

func TestIsThirdParty(t *testing.T) {
	shouldFail := []string{
		"external/foo/",
		"vendor/bar/",
		"hardware/underwater_jaguar/",
	}
	shouldPass := []string{
		"vendor/google/cts/",
		"hardware/google/pixel",
		"hardware/interfaces/camera",
		"hardware/ril/supa_ril",
	}
	for _, path := range shouldFail {
		if !isThirdParty(path) {
			t.Errorf("Expected %s to be considered third party", path)
		}
	}
	for _, path := range shouldPass {
		if isThirdParty(path) {
			t.Errorf("Expected %s to *not* be considered third party", path)
		}
	}
}
