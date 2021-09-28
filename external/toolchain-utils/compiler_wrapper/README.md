# Compiler wrapper

See the comments on the top of main.go.
Build is split into 2 steps via separate commands:
- bundle: copies the sources and the `build.py` file into
  a folder.
- build: builds the actual go binary, assuming it is executed
  from the folder created by `bundle.py`.

This allows to copy the sources to a Chrome OS / Android
package, including the build script, and then
build from there without a dependency on toolchain-utils
itself.

## Update Chrome OS

Copy over sources and `build.py` to Chrome OS:
```
(chroot) /mnt/host/source/src/third_party/chromiumos-overlay/sys-devel/llvm/files/update_compiler_wrapper.sh
```

`build.py` is called by these ebuilds:

- third_party/chromiumos-overlay/sys-devel/llvm/llvm-9.0_pre361749_p20190714.ebuild
- third_party/chromiumos-overlay/sys-devel/gcc/gcc-*.ebuild

Generated wrappers are stored here:

- Sysroot wrapper with ccache:
  `/usr/x86_64-pc-linux-gnu/<arch>/gcc-bin/4.9.x/sysroot_wrapper.hardened.ccache`
- Sysroot wrapper without ccache:
  `/usr/x86_64-pc-linux-gnu/<arch>/gcc-bin/4.9.x/sysroot_wrapper.hardened.noccache`
- Clang host wrapper:
  `/usr/bin/clang_host_wrapper`
- Gcc host wrapper:
  `/usr/x86_64-pc-linux-gnu/gcc-bin/4.9.x/host_wrapper`
