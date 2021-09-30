set -e

# Grab libraries we'll need later at the beginning
sudo apt-get install libssl1.0-dev libcurl4-openssl-dev

# Grab clang prebuilt
git clone https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86 clang-prebuilt --filter=blob:none -n
git -C clang-prebuilt checkout 8a6de3cbfcd642b21ebaf9663596b3f7e942d1c5
CLANGDIR=${PWD}/clang-prebuilt/clang-r353983c/bin
export CC=${CLANGDIR}/clang
export CXX=${CLANGDIR}/clang++
export AR=${CLANGDIR}/llvm-ar
# Old rustc incorrectly assumed ar lived next to cc
cp ${AR} ${CLANGDIR}/ar

# Fetch rustc-1.19 and rustc-1.20
wget https://static.rust-lang.org/dist/rustc-1.19.0-src.tar.gz
wget https://static.rust-lang.org/dist/rustc-1.20.0-src.tar.gz
sha256sum -c rustc-shasums
export RUSTC_1_19=${PWD}/rustc-1.19.0-src.tar.gz
export RUSTC_1_20=${PWD}/rustc-1.20.0-src.tar.gz

# Build rustc-1.19 with mrustc
git clone https://github.com/thepowersgang/mrustc -b v0.8.0
git -C mrustc am ${PWD}/mrustc-patches/*
cd mrustc

# Build mrustc
make bin/mrustc

tar xf ${RUSTC_1_19}
# Build rustc/cargo
make output/rustc
make output/cargo

# Build prefix and stdlibs
make -C run_rustc

# Build rustc-1.20 with our mrustc-rustc 1.19
PREFIX=${PWD}/run_rustc/prefix/
MRUSTC_RUSTC=${PWD}/mrustc-rustc-rustc/
mkdir -p ${MRUSTC_RUSTC}
tar xf ${RUSTC_1_20} -C ${MRUSTC_RUSTC}
MRUSTC_RUSTC_SRC=${MRUSTC_RUSTC}/rustc-1.20.0-src
cat - > ${MRUSTC_RUSTC_SRC}/config.toml <<EOF
[build]
cargo = "${PREFIX}bin/cargo"
rustc = "${PREFIX}bin/rustc"
full-bootstrap = true
vendor = true
extended = true
[target.x86_64-unknown-linux-gnu]
cc = "${CC}"
cxx = "${CXX}"
ar = "${AR}"
EOF
mv ${MRUSTC_RUSTC_SRC} build
pushd build
./x.py build --stage 3 2>&1 > build.log
popd
mv build ${MRUSTC_RUSTC_SRC}

# Build rustc-1.20 with official 1.19 for comparison
OFFICIAL_RUSTC=${PWD}/official-rustc-rustc/
mkdir -p ${OFFICIAL_RUSTC}
tar xf ${RUSTC_1_20} -C ${OFFICIAL_RUSTC}
OFFICIAL_RUSTC_SRC=${OFFICIAL_RUSTC}/rustc-1.20.0-src
cat - > ${OFFICIAL_RUSTC_SRC}/config.toml <<EOF
[build]
full-bootstrap = true
vendor = true
extended = true
[target.x86_64-unknown-linux-gnu]
cc = "${CC}"
cxx = "${CXX}"
ar = "${AR}"
EOF
mv ${OFFICIAL_RUSTC_SRC} build
pushd build
./x.py build --stage 3 2>&1 > build.log 
popd
mv build ${OFFICIAL_RUSTC_SRC}

# Summarize differences
diff -r ${OFFICIAL_RUSTC_SRC}/build/x86_64-unknown-linux-gnu/stage3 ${MRUSTC_RUSTC_SRC}/build/x86_64-unknown-linux-gnu/stage3
