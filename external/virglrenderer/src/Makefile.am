SUBDIRS := gallium/auxiliary
AM_LDFLAGS = -lm \
	$(GBM_LIBS) \
	$(EPOXY_LIBS) \
	$(X11_LIBS) \
	$(CODE_COVERAGE_LDFLAGS)

AM_CFLAGS = \
	-I$(top_srcdir)/src/gallium/include \
	-I$(top_srcdir)/src/gallium/auxiliary \
	$(DEFINES) \
	$(PIC_FLAGS) \
	$(LIBDRM_CFLAGS) \
	$(GBM_CFLAGS) \
	$(EPOXY_CFLAGS) \
	$(X11_CFLAGS) \
	$(VISIBILITY_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS)

libvrend_la_SOURCES = \
        virgl_hw.h \
        virgl_protocol.h \
        vrend_iov.h \
        vrend_renderer.c \
        vrend_renderer.h \
        vrend_shader.c \
        vrend_shader.h \
        vrend_object.c \
        vrend_object.h \
        vrend_decode.c \
        vrend_formats.c \
        vrend_blitter.c \
        vrend_blitter.h \
        iov.c

if HAVE_EPOXY_EGL
libvrend_la_SOURCES += \
        virgl_egl.h \
        virgl_egl_context.c
endif

if WITH_GLX
libvrend_la_SOURCES += \
	virgl_glx.h \
	virgl_glx_context.c
endif

lib_LTLIBRARIES = libvirglrenderer.la
noinst_LTLIBRARIES = libvrend.la

GM_LDFLAGS = -Wl,-Bsymbolic -version-number 0:2 -no-undefined

libvirglrenderer_la_SOURCES = virglrenderer.c
libvirglrenderer_ladir = $(libdir)
libvirglrenderer_la_LIBADD = libvrend.la gallium/auxiliary/libgallium.la
libvirglrenderer_la_LDFLAGS = $(GM_LDFLAGS) $(EPOXY_LDFLAGS) $(X11_LDFLAGS)

libvirglrendererincludedir = ${includedir}/virgl
libvirglrendererinclude_HEADERS = virglrenderer.h

EXTRA_DIST = gallium/include

-include $(top_srcdir)/git.mk
