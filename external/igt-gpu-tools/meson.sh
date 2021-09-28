#!/bin/bash

cat > Makefile <<EOF

.PHONY: default docs
default: all

build/build.ninja:
	mkdir -p build
	meson build

all: build/build.ninja
	ninja -C build

clean: build/build.ninja
	ninja -C build clean

test: build/build.ninja
	ninja -C build test

reconfigure: build/build.ninja
	ninja -C build reconfigure

check distcheck dist distclean:
	echo "This is the meson wrapper, not automake" && false

install: build/build.ninja
	ninja -C build install

uninstall: build/build.ninja
	ninja -C build uninstall

docs:
	ninja -C build igt-gpu-tools-doc

EOF

git config format.subjectprefix "PATCH i-g-t"

make $@
