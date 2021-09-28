# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ifndef SYSROOT
  $(error Define SYSROOT)
endif

OUT_DIR ?= .
PROTO_PATH = $(SYSROOT)/usr/include/chromeos/dbus/smbprovider
PROTO_DEFS = $(PROTO_PATH)/directory_entry.proto
PROTO_BINDINGS = $(OUT_DIR)/directory_entry_pb2.py

all: $(PROTO_BINDINGS)

$(PROTO_BINDINGS): $(PROTO_DEFS)
	protoc --proto_path=$(PROTO_PATH) --python_out=$(OUT_DIR) $(PROTO_DEFS)

clean:
	rm -f $(PROTO_BINDINGS)