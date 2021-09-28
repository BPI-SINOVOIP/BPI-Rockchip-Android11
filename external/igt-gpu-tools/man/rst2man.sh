#!/bin/bash

rst2man=$1
input=$2
output=$3

out_dir=$(dirname "${output}")
in_file=$(basename "${input}")

# rst2man doesn't handle multiple source directories well, and since defs.rst is
# generated we first need to move it all into the build dir
cp "$input" "$out_dir"

${rst2man} "$out_dir/$in_file" "${output%.gz}"

rm -f "${output}"
gzip "${output%.gz}"
