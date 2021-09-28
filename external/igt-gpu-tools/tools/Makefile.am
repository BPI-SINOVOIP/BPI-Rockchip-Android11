include Makefile.sources

bin_PROGRAMS = $(tools_prog_lists)

if HAVE_LIBDRM_INTEL
bin_PROGRAMS += $(LIBDRM_INTEL_BIN)
intel_error_decode_LDFLAGS = -lz
endif

bin_PROGRAMS += intel_dp_compliance
intel_dp_compliance_CFLAGS = $(AM_CFLAGS) $(GLIB_CFLAGS)
intel_dp_compliance_LDADD = $(top_builddir)/lib/libintel_tools.la

SUBDIRS = null_state_gen registers

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/include/drm-uapi -I$(top_srcdir)/lib \
	      -I$(top_srcdir)/lib/stubs/syscalls
AM_CFLAGS = $(DEBUG_CFLAGS) $(DRM_CFLAGS) $(PCIACCESS_CFLAGS) $(CWARNFLAGS) \
	    $(CAIRO_CFLAGS) $(LIBUNWIND_CFLAGS) -DIGT_DATADIR=\"$(pkgdatadir)\" \
	    $(WERROR_CFLAGS) -D_GNU_SOURCE
LDADD = $(top_builddir)/lib/libintel_tools.la
AM_LDFLAGS = -Wl,--as-needed

# aubdumper

module_LTLIBRARIES = intel_aubdump.la
moduledir = $(libdir)
intel_aubdump_la_LDFLAGS = -module -avoid-version -no-undefined
intel_aubdump_la_SOURCES = aubdump.c
intel_aubdump_la_LIBADD = $(top_builddir)/lib/libintel_tools.la -ldl

intel_gpu_top_LDADD = $(top_builddir)/lib/libigt_perf.la

bin_SCRIPTS = intel_aubdump
CLEANFILES = $(bin_SCRIPTS)
