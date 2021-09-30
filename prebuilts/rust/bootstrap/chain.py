#!/usr/bin/env python3

import subprocess
from os.path import abspath
import hashlib
import urllib3
import tarfile

def rustc_url(version):
    return f"https://static.rust-lang.org/dist/rustc-{version}-src.tar.gz"

version_sequence = ["1.21.0", "1.22.1", "1.23.0", "1.24.1", "1.25.0", "1.26.2", "1.27.2", "1.28.0", "1.29.2", "1.30.1", "1.31.1", "1.32.0", "1.33.0", "1.34.2"]

bootstrap_version = "1.20.0"
bootstrap_path = abspath("mrustc-bootstrap/mrustc/mrustc-rustc-rustc/rustc-1.20.0-src/")
clang_prebuilt_path = abspath("mrustc-bootstrap/clang-prebuilt/clang-r353983c/bin")
cc = clang_prebuilt_path + "/clang"
cxx = clang_prebuilt_path + "/clang++"
ar = clang_prebuilt_path + "/llvm-ar"

class RustBuild(object):
    def __init__(self, version, path, stage0):
        self.version = version
        self.stage0 = stage0
        self.path = path
        self.built = False

    def configure(self):
        minor = self.version.split('.')[1]
        remap = ""
        if int(minor) > 30:
            remap = "[rust]\nremap-debuginfo = true"
        config_toml = f"""\
[build]
cargo = "{self.stage0.cargo()}"
rustc = "{self.stage0.rustc()}"
full-bootstrap = true
vendor = true
extended = true
docs = false
{remap}
[target.x86_64-unknown-linux-gnu]
cc = "{cc}"
cxx = "{cxx}"
"""
        with open(self.path + "/config.toml", "w+") as f:
            f.write(config_toml)

    def build(self):
        self.configure()
        subprocess.run(["./x.py", "--stage", "3", "build"], check=True, cwd=self.path)
        self.built = True

    def rustc(self):
        if not self.built:
            self.build()
        return self.path + "/build/x86_64-unknown-linux-gnu/stage3/bin/rustc"

    def cargo(self):
        if not self.built:
            self.build()
        return self.path + "/build/x86_64-unknown-linux-gnu/stage3-tools/x86_64-unknown-linux-gnu/release/cargo"


class RustPrebuilt(RustBuild):
    def __init__(self, version, path):
        super().__init__(version, path, None)
        self.built = True
    def build(self): pass


def fetch_rustc():
    http = urllib3.PoolManager()
    for version in version_sequence:
        rust_src_resp = http.request("GET", rustc_url(version), preload_content=False)
        rust_src_tar_path = f"rustc-{version}-src.tar.gz"
        hasher = hashlib.sha256()
        with open(rust_src_tar_path, "wb+") as tar_file:
            for chunk in rust_src_resp.stream():
                hasher.update(chunk)
                tar_file.write(chunk)
        rust_src_resp.release_conn()
        print(f"rustc-{version}-src.tar.gz {hasher.hexdigest()}")
        tarfile.open(rust_src_tar_path).extractall()


def main():
    fetch_rustc()
    build = RustPrebuilt(bootstrap_version, bootstrap_path)
    for version in version_sequence:
        build = RustBuild(version, abspath(f"rustc-{version}-src"), build)
    print(f"rustc: {build.rustc()}")
    print(f"cargo: {build.rustc()}")


if __name__ == "__main__":
    main()
