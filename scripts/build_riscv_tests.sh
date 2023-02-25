#!/bin/bash

root_dir="$(cd $(dirname $0)/.. ; pwd)"
cd "$root_dir/third_party/riscv-tests/isa"

set +e
make clean
make "$1" -j $(nproc)
set -e

rv64_elf_exe=()

while IFS= read -r line; do
    rv64_elf_exe+=("$line")
done < <(ls -lrt | awk '{print $9}'|xargs file|grep  ELF| awk '{print $1}'|tr -d ':')

mkdir -p "$root_dir/test/bin"
mkdir -p "$root_dir/test/elf"
mkdir -p "$root_dir/test/dump"

for value in ${rv64_elf_exe[*]}
do
    riscv64-unknown-elf-objcopy -O binary "$value" "$root_dir/test/bin/$value.bin"
    cp "$value" "$root_dir/test/elf"
    cp "$value.dump" "$root_dir/test/dump"
done
