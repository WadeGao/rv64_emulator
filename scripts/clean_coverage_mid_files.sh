#!/bin/bash

# clean coverage files
paths=("./include" "./src" "./test")
for path in "${paths[@]}"; do
    find "$path" -name "*.gcno" -exec rm {} \;
    find "$path" -name "*.gcda" -exec rm {} \;
done
