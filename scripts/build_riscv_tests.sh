#!/usr/bin/bash

root_dir="$(cd $(dirname $0)/.. ; pwd)"
cd "$root_dir/third_party/riscv-tests/isa"

set +e
make rv64ui
set -e

rv64_elf_exe=()

while IFS= read -r line; do
    rv64_elf_exe+=("$line")
done < <(ls -lrt | awk '{print $9}'|xargs file|grep  ELF| awk '{print $1}'|tr -d ':')


for value in ${rv64_elf_exe[*]}
do 
  echo "generating $value.bin"
  riscv64-unknown-elf-objcopy -O binary $value $value.bin
  cp "$value.bin" "$root_dir/test/"
done
