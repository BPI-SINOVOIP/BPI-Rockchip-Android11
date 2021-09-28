#!/bin/bash
# Find builds here:
# https://android-build.googleplex.com/builds/branches/aosp-emu-master-dev/grid?
set -e

function update_binaries {
	local src="$1"
	local dst="$2"

	rm -rf "$dst"
	rm -rf "emulator"
	unzip "$src"
	rm -f "./emulator/emulator64-crash-service"
	rm -f "./emulator/emulator64-mips"
	rm -f "./emulator/qemu/linux-x86_64/qemu-system-mipsel"
	rm -f "./emulator/qemu/linux-x86_64/qemu-system-mips64el"
	mv "emulator" "$dst"
	git add "$dst"
}

if [ $# == 1 ]
then
build=$1
else
	echo  Usage: $0 build
	exit 1
fi

linux_zip="sdk-repo-linux-emulator-$build.zip"

echo Fetching Linux $build
/google/data/ro/projects/android/fetch_artifact --bid $build --target sdk_tools_linux "$linux_zip"
update_binaries "$linux_zip" "linux-x86_64"

printf "Upgrade emulator to emu-master-dev build $build\n\n" > emulator.commitmsg

git commit -s -t emulator.commitmsg

rm -f "emulator.commitmsg"
rm -f "$linux_zip"
