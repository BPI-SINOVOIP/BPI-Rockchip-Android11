
CFLAGS = -fno-short-enums -U_DEBUG -DARM_ARCH_7A -DLE_ENABLE -DUSE_SYSLOG -Iinc -Isrc -DDEFAULT_DOMAIN_ID=3
LDFLAGS = -fPIC -pie -Wl,--version-script=src/symbols.lst -ldl -lpthread -lm



prefix := /usr/local
bindir := $(prefix)/bin
libdir := $(prefix)/lib


LIBADSPRPC_SRC_FILES := src/fastrpc_apps_user.c \
		src/remotectl_stub.c \
		src/listener_android.c \
		src/adsp_current_process_stub.c \
		src/adsp_current_process1_stub.c \
		src/apps_std_skel.c \
		src/apps_std_imp.c \
		src/apps_mem_imp.c \
		src/apps_mem_skel.c \
		src/rpcmem_android.c \
		src/apps_remotectl_skel.c \
		src/std.c \
		src/std_path.c \
		src/std_mem.c \
		src/std_dtoa.c \
		src/std_strlprintf.c \
		src/BufBound.c \
		src/std_SwapBytes.c \
		src/smath.c \
		src/atomic.c \
		src/cae.c \
		src/adspmsgd_apps_skel.c \
		src/adspmsgd_adsp_stub.c \
		src/adspmsgd_adsp1_stub.c \
		src/adspmsgd_apps.c \
		src/platform_libs.c \
		src/pl_list.c \
		src/log_config.c \
		src/gpls.c \
		src/adsp_perf_stub.c \
		src/fastrpc_perf.c \
		src/mod_table.c

LIBADSPRPC_OBJ := $(LIBADSPRPC_SRC_FILES:.c=.o)

LIBDEFAULT_LISTENER_SRC_FILES := src/adsp_default_listener.c \
				src/adsp_default_listener_stub.c \
				src/std.c \
				src/std_mem.c

LIBDEFAULT_LISTENER_OBJ := $(LIBDEFAULT_LISTENER_SRC_FILES:.c=.o)

ADSPRPCD_SRC_FILES := src/cdsprpcd.c

ADSPRPCD_OBJ := $(ADSPRPCD_SRC_FILES:.c=.o)

%.o: %.c
	$(CC) -fpic -pie $(CFLAGS) -c $< -o $@

cdsprpcd: $(ADSPRPCD_OBJ) libcdsp_default_listener.so
	echo $(LD) -fpic -pie $(LDFLAGS) -o cdsprpcd
	$(CC) -fpic -pie -fPIC -pie -Wl,--version-script=src/symbols.lst -ldl -lpthread -lm $(ADSPRPCD_OBJ) -o cdsprpcd -lcdsp_default_listener -L.
	echo "cdsprpcd built successfully"

LDFLAGS = -fPIC -pie -Wl,--version-script=src/symbols.lst -shared -ldl -lpthread -lm    
    
libcdsp_default_listener.so: $(LIBDEFAULT_LISTENER_OBJ) libcdsprpc.so
	echo $(LD) -fpic -pie -shared $(LDFLAGS) -o libcdsp_default_listener.so
	$(CC) -fpic -pie -shared -fPIC -pie -Wl,--version-script=src/adsp_def_symbols.lst -ldl -lpthread -lm $(LIBDEFAULT_LISTENER_OBJ) -o libcdsp_default_listener.so -lcdsprpc -L.
	echo "libcdsp_default_listener built successfully"

libcdsprpc.so: $(LIBADSPRPC_OBJ)
	echo $(LD) -fpic -pie -shared $(LDFLAGS) -o libcdsprpc.so
	$(CC) -fpic -pie -shared $(LDFLAGS) $(LIBADSPRPC_OBJ) -o libcdsprpc.so
	echo "libcdsprpc built successfully"

all: libcdsprpc.so libcdsp_default_listener.so cdsprpcd
	echo "All binaries built"

install: libcdsprpc.so libcdsp_default_listener.so cdsprpcd
	@install -D -m 755 libcdsprpc.so $(DESTDIR)$(libdir)/libcdsprpc.so
	@install -D -m 755 libcdsp_default_listener.so $(DESTDIR)$(libdir)/libcdsp_default_listener.so
	@install -D -m 755 cdsprpcd $(DESTDIR)$(bindir)/cdsprpcd