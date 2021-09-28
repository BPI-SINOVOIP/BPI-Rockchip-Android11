AUTOMAKE_OPTIONS = subdir-objects

include Makefile.sources
include $(top_srcdir)/src/gallium/Automake.inc

noinst_LTLIBRARIES = libgallium.la

AM_CFLAGS = \
	-I$(top_srcdir)/src/gallium/auxiliary/util \
	-I$(top_srcdir)/src/gallium/auxiliary/ \
	$(GALLIUM_CFLAGS) \
	$(PTHREAD_CFLAGS) \
	$(VISIBILITY_CFLAGS)

AM_CXXFLAGS = $(VISIBILITY_CXXFLAGS)

libgallium_la_LIBADD = \
	$(PTHREAD_LIBS)

libgallium_la_SOURCES = \
	$(C_SOURCES) \
	$(GENERATED_SOURCES) \
	util/rgtc.h \
	util/u_atomic.h \
	util/u_bitmask.h \
	util/u_box.h \
	util/u_cpu_detect.h \
	util/u_debug.h \
	util/u_debug_describe.h \
	util/u_debug_refcnt.h \
	util/u_double_list.h \
	util/u_dual_blend.h \
	util/u_format.h \
	util/u_format_s3tc.h \
	util/u_half.h \
	util/u_hash_table.h \
	util/u_inlines.h \
	util/u_math.h \
	util/u_memory.h \
	util/u_pack_color.h \
	util/u_pointer.h \
	util/u_prim.h \
	util/u_rect.h \
	util/u_string.h \
	util/u_surface.h \
	util/u_texture.h \
	cso_cache/cso_cache.h \
	cso_cache/cso_hash.h \
	tgsi/tgsi_build.h \
	tgsi/tgsi_dump.h \
	tgsi/tgsi_info.h \
	tgsi/tgsi_iterate.h \
	tgsi/tgsi_opcode_tmp.h \
	tgsi/tgsi_parse.h \
	tgsi/tgsi_sanity.h \
	tgsi/tgsi_scan.h \
	tgsi/tgsi_strings.h \
	tgsi/tgsi_text.h \
	tgsi/tgsi_transform.h \
	tgsi/tgsi_ureg.h \
	tgsi/tgsi_util.h \
	os/os_memory.h \
	os/os_memory_stdc.h \
	os/os_memory_aligned.h \
	os/os_misc.h \
	os/os_thread.h

util/u_format_table.c: $(srcdir)/util/u_format_table.py $(srcdir)/util/u_format_pack.py $(srcdir)/util/u_format_parse.py $(srcdir)/util/u_format.csv
	$(AM_V_at)$(MKDIR_P) util
	$(AM_V_GEN) $(PYTHON) $(srcdir)/util/u_format_table.py $(srcdir)/util/u_format.csv > $@

-include $(top_srcdir)/git.mk

EXTRA_DIST = util/u_format_table.py \
             util/u_format_pack.py \
             util/u_format_parse.py \
             util/u_format.csv
