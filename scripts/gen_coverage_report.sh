#!/bin/bash

PROJECT_DIR=$(xmake show | grep projectdir | awk '{print $3}' | sed -r "s/\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[m|K]//g")
BUILD_DIR=$(xmake show | grep buildir | awk '{print $3}' | sed -r "s/\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[m|K]//g")
PLATFORM=$(xmake show | grep plat | awk '{print $3}' | sed -r "s/\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[m|K]//g")
ARCH=$(xmake show | grep arch | awk '{print $3}' | sed -r "s/\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[m|K]//g")
MODE=$(xmake show | grep mode | awk '{print $3}' | sed -r "s/\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[m|K]//g")

# clean coverage files
paths=("./include" "./src" "./test")
for path in "${paths[@]}"
do
  find "$path" -name "*.gcno" -exec rm {} \;
  find "$path" -name "*.gcda" -exec rm {} \;
done

"$BUILD_DIR"/unit_test

OBJS_DIR="$PROJECT_DIR/$BUILD_DIR/.objs/unit_test/$PLATFORM/$ARCH/$MODE"
cd "$OBJS_DIR" || exit

SRC_DIR=($(find . -type d))
GCNO_FILES=($(find . -name "*.gcno"))
GCDA_FILES=($(find . -name "*.gcda"))

for((i=0;i<${#GCNO_FILES[@]};i++))
do
    cp "${GCNO_FILES[i]}" "$PROJECT_DIR/${GCNO_FILES[i]}"
done;

for((i=0;i<${#GCDA_FILES[@]};i++))
do
    cp "${GCDA_FILES[i]}" "$PROJECT_DIR/${GCDA_FILES[i]}"
done;

cd "$PROJECT_DIR" || exit
LCOV_CMD="lcov"

# 从索引 1 开始，排除 find 出来的第一项："."
for((i=1;i<${#SRC_DIR[@]};i++))
do
    LCOV_CMD="$LCOV_CMD -d ${SRC_DIR[i]}"
done;

LCOV_CMD="$LCOV_CMD -o $BUILD_DIR/all.info -c --rc lcov_branch_coverage=1"
$LCOV_CMD

TEMP_FILE="tmp.info"

cat "$BUILD_DIR/all.info" | sed 's/__cpp_//g' > "$BUILD_DIR/$TEMP_FILE"
cat "$BUILD_DIR/$TEMP_FILE" | sed 's/.cc.cc/.cc/g' > "$BUILD_DIR/all.info"

TMP_DIR="$BUILD_DIR/.objs/unit_test/$PLATFORM/$ARCH/$MODE/"
TMP_DIR_ESC=$(echo $TMP_DIR | sed 's#\/#\\\/#g')

cat "$BUILD_DIR/all.info" | sed "s/$TMP_DIR_ESC//g" > "$BUILD_DIR/$TEMP_FILE"
mv "$BUILD_DIR/$TEMP_FILE" "$BUILD_DIR/all.info"

genhtml -o "$BUILD_DIR/coverage" $BUILD_DIR/all.info --rc lcov_branch_coverage=1

lcov --extract "$BUILD_DIR"/all.info "$PROJECT_DIR/src/*" "$PROJECT_DIR/include/*" -o "$BUILD_DIR"/res.info --rc lcov_branch_coverage=1
genhtml -o "$BUILD_DIR/coverage" "$BUILD_DIR"/res.info --rc lcov_branch_coverage=1
