# Copyright (c) 2018 Google, Inc.
# SPDX-License-Identifier: GPL-2.0-or-later

top_srcdir	?= ../../../..
include $(top_srcdir)/include/mk/testcases.mk

LDLIBS += -lpthread

FILTER_OUT_MAKE_TARGETS := trace_parse util
include $(top_srcdir)/include/mk/generic_leaf_target.mk
$(MAKE_TARGETS): %: %.o trace_parse.o util.o
