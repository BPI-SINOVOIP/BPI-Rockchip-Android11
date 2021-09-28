# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

TMPDIR = `mktemp -d`
BINS = rowhammer_test
SRC = rowhammer_test.cc
OBJS = rowhammer_test.o

all: $(BINS)

rowhammer_test: $(OBJS)
	$(CXX) -static $(CXXFLAGS) -o $@ $^

.cc.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@rm $(BINS) $(OBJS) || true
