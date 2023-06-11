#!/bin/bash

paths=("./include" "./src" "./test")
ext_name=("*.h" "*.hh" "*.hpp" "*.c" "*.cc" "*.cpp")

for path in "${paths[@]}"; do
  for ext in "${ext_name[@]}"; do
    find "$path" -type f -name "$ext" -print | xargs clang-format -style=file --sort-includes -i
  done
done
