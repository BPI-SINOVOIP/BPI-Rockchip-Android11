#!/usr/bin/env bash

# e.x. convertFqName "android.hardware.foo@1.0::foo::IFoo" "\5" -> IFoo
function convertFqName() {
    # android.hardware.foo@1.0::foo::IFoo
    # \1 => android.hardware.foo
    # \3 => 1
    # \4 => 0
    # \5 => IFoo # optional capture group
    local FQNAME="^(([a-zA-Z0-9]+\.)*[a-zA-Z0-9]+)@([0-9]+).([0-9]+)(::([a-zA-Z0-9]+))?$"

    local fqName="$1"
    local pattern="$2"

    echo $1 | sed -E "s/$FQNAME/$2/g"
}
function assertInterfaceFqName() {
    if [ -n "$(convertFqName "$1")" ]; then
        echo "$1 is not an interface fqname"
        return 1
    fi
}
function javaTarget() { convertFqName "$1" "\1-V\3.\4-java"; }
function cppTarget()  { convertFqName "$1" "\1@\3.\4"; }

function hidl-gen-output() {
    local out="$1"

    while read -r fqName; do
        echo "Generating output for $fqName."
        hidl-gen -Lc++-headers -o "$out/cpp" "$fqName" || exit 1
        hidl-gen -Ljava -o "$out/java" "$fqName" 2>/dev/null
    done
}

# for a package root path + package, get unreleased interfaces
function all-interfaces() {
    [ $# = 2 ] || { echo "usage: all-interfaces <package root directory> <package root path>" && return 1; }

    local package_root="$1"
    [ -d "$package_root" ] || { echo "all-interfaces: directory $package_root does not exist" && return 1; }

    local package="$2"

    find $package_root -name '*.hal' -printf '%P\n' \
        | sed "s,^,$package.,g" \
        | sed s,/,.,g \
        | sed "s/\.\([0-9]\+\.[0-9]\+\)\.\([a-zA-Z0-9]\+\)\.hal$/@\1::\2/g" \
        | sort # should be unique
}

# for a package root path, get released interface names
function current-interfaces() {
    [ $# = 1 ] || { echo "usage: current-interfaces <package root directory>" && return 1; }

    local package_root="$1"
    [ -d "$package_root" ] || { echo "current-interfaces: directory $package_root does not exist" && return 1; }

    local current_file="$package_root/current.txt"
    [ -f "$current_file" ] || { echo "current-interfaces: current file $current_file does not exist" && return 1; }

    cat "$current_file" | cut -d '#' -f1 | awk '{print $2}' | sed "/^ *$/d" | sort | uniq
}

# for a package root path + package, get interfaces in tree but not in current.txt
function changed-interfaces() {
    [ $# = 2 ] || { echo "usage: changed-interfaces <package root directory> <package root path>" && return 1; }

    local package_root="$1"
    [ -d "$package_root" ] || { echo "current-interfaces: directory $package_root does not exist" && return 1; }

    local package="$2"

    echo "CHANGES FOR $2: ($1)"

    # not excluding '2' since 2 should be a subset of 1, but it might help catch some errors
    comm -3 <(all-interfaces "$package_root" "$package") <(current-interfaces "$package_root")
}

# see all of the interfaces which are unreleased
function aosp-changes-report() {
    echo "Warning: ignoring test/automotive interfaces."
    echo

    changed-interfaces $ANDROID_BUILD_TOP/hardware/interfaces android.hardware \
        | grep -v "android.hardware.tests" \
        | grep -v "android.hardware.automotive"
    changed-interfaces $ANDROID_BUILD_TOP/frameworks/hardware/interfaces android.frameworks
    changed-interfaces $ANDROID_BUILD_TOP/system/hardware/interfaces android.system
    changed-interfaces $ANDROID_BUILD_TOP/system/libhidl/transport android.hidl
}

function aosp-interfaces() {
    declare -A roots
    roots["android.hardware"]="hardware/interfaces"
    roots["android.system"]="system/hardware/interfaces"
    roots["android.frameworks"]="frameworks/hardware/interfaces"
    roots["android.hidl"]="system/libhidl/transport"

    for root in "${!roots[@]}"; do
        local path="${roots["$root"]}"
        all-interfaces "$ANDROID_BUILD_TOP/$path" "$root"
    done | sort -u
}

function aosp-released-interfaces() {
    local roots=(
        hardware/interfaces
        system/hardware/interfaces
        frameworks/hardware/interfaces
        system/libhidl/transport
    )

    for root in "${roots[@]}"; do
        current-interfaces "$ANDROID_BUILD_TOP/$root"
    done | sort -u
}

function aosp-released-packages() {
    aosp-released-interfaces | cut -d ':' -f1 | sort -u
}

function hidl-doc-generate-sources() {
    local outputDir="$1"
    [ -z "$1" ] && outputDir="gen"

    echo "Generating sources in $(realpath $outputDir)"

    aosp-released-packages | hidl-gen-output "$outputDir"

    echo "Deleting implementation-related files from $outputDir."
    rm $(find "$outputDir/cpp" -\( -name "IHw*.h" \
                                   -o -name "BnHw*.h" \
                                   -o -name "BpHw*.h" \
                                   -o -name "Bs*.h" \
                                   -o -name "hwtypes.h" \))

    echo "Done: generated sources are in $outputDir"
}

