#!/bin/sh
# Copyright (c) 2018 Petr Vorel <pvorel@suse.cz>
set -e

apt remove -y \
	libacl1-dev \
	libaio-dev \
	libaio1 \
	libcap-dev \
	libcap2 \
	libkeyutils-dev \
	libkeyutils1 \
	libmm-dev \
	libnuma-dev \
	libnuma1 \
	libselinux1-dev \
	libsepol1-dev \
	libssl-dev \
	libtirpc-dev
