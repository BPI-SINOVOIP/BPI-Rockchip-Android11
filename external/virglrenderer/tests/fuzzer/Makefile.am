AM_CFLAGS = \
	-I$(top_srcdir)/src/gallium/drivers/virgl \
	-I$(top_srcdir)/src/gallium/include \
	-I$(top_srcdir)/src/gallium/auxiliary \
	-I$(top_srcdir)/src/gallium/drivers \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/src \
	$(DEFINES) \
	$(PIC_FLAGS) \
	$(LIBDRM_CFLAGS) \
	$(EPOXY_CFLAGS) \
	$(VISIBILITY_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS) \
	-fsanitize=address \
	-fsanitize=fuzzer

if FUZZER
noinst_PROGRAMS = virgl_fuzzer

virgl_fuzzer_SOURCES =				\
	virgl_fuzzer.c

virgl_fuzzer_LDADD = $(top_builddir)/src/libvirglrenderer.la $(EPOXY_LIBS)
endif
