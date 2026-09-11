#!/usr/bin/env bash
set -e

# --- ⚙️ إعدادات المتغيرات المرنة ---
PLANTC_SRC="src/plantc/main.plant"
ORIGINAL_PLANTC="dist/plantc"
NATIVE_PLANTC="dist/plantc_native"
OUTPUT_DIR="dist"
RUNTIME_DIR="runtime/c"
TEST_FIXTURE="tests/fixtures/hello_world.plant"
BUILD_LOG="build.log"

echo "[$(date)] 🔍 Starting Pre-flight Verification..."
file "$ORIGINAL_PLANTC"
ls -l "$ORIGINAL_PLANTC"
ls -l "$PLANTC_SRC"
ls -l "$RUNTIME_DIR/plant_runtime.c"
ls -l "$TEST_FIXTURE"
echo "[$(date)] ✓ Pre-flight checks passed successfully."

echo "[$(date)] 🚀 Starting Phase 1: Self-Compilation & Strict Verification..."

# 1. التجميع الذاتي وتوليد شيفرة C
"$ORIGINAL_PLANTC" "$PLANTC_SRC" "$OUTPUT_DIR/plantc_v2.c"
ls -lh "$OUTPUT_DIR/plantc_v2.c"
echo "[$(date)] ✓ Self-compilation C generation passed." >> "$BUILD_LOG"

# 2. التجميع الأصلي عبر GCC
gcc -O2 -I"$RUNTIME_DIR" "$OUTPUT_DIR/plantc_v2.c" "$RUNTIME_DIR/plant_runtime.c" -o "$NATIVE_PLANTC"
file "$NATIVE_PLANTC"
echo "[$(date)] ✓ GCC compilation of plantc_native passed." >> "$BUILD_LOG"

# اختبار استجابة السريع للثنائي الجديد
echo "[$(date)] -> Testing native binary responsiveness..."
"$NATIVE_PLANTC" --version || true

# 3. فحص المطابقة الصارم (Diff Enforcement)
"$ORIGINAL_PLANTC" "$TEST_FIXTURE" "$OUTPUT_DIR/hello_old.c"
echo "[$(date)] -> Running native binary on test fixture..."
"$NATIVE_PLANTC" "$TEST_FIXTURE" "$OUTPUT_DIR/hello_new.c" 2>&1 || true

if [ ! -f "$OUTPUT_DIR/hello_new.c" ]; then
    echo "[$(date)] ✗ Critical Error: Self-compiled binary produced no output file." | tee -a "$BUILD_LOG"
    echo "[$(date)] Reason: main.plant imports modules (lexer, parser, codegen_c) whose PlantLang logic"
    echo "         is inlined into main() rather than compiled as separate C functions."
    echo "         Self-compilation of the full self-hosted compiler requires the bootstrap compiler"
    echo "         to handle function definitions — a Phase 2 capability."
    exit 1
fi

if diff -u "$OUTPUT_DIR/hello_old.c" "$OUTPUT_DIR/hello_new.c" > "$OUTPUT_DIR/codegen_diff.patch"; then
    echo "[$(date)] ✓ AST/Codegen exact match verified via diff." >> "$BUILD_LOG"
    rm -f "$OUTPUT_DIR/codegen_diff.patch"
else
    echo "[$(date)] ✗ Critical Error: AST/Codegen drift detected! Check $OUTPUT_DIR/codegen_diff.patch" >> "$BUILD_LOG"
    cat "$OUTPUT_DIR/codegen_diff.patch"
    exit 1
fi

# 4. التحقق النهائي من التشغيل الحي
gcc -O2 -I"$RUNTIME_DIR" "$OUTPUT_DIR/hello_new.c" "$RUNTIME_DIR/plant_runtime.c" -o "$OUTPUT_DIR/hello_v2"
file "$OUTPUT_DIR/hello_v2"
"$OUTPUT_DIR/hello_v2"

# 5. تنظيف الملفات المؤقتة (Workspace Cleanup)
echo "[$(date)] -> Cleaning up temporary test files..."
rm -f "$OUTPUT_DIR/hello_old.c" "$OUTPUT_DIR/hello_new.c" "$OUTPUT_DIR/hello_v2"

echo "[$(date)] ✓ Phase 1 fully completed successfully." >> "$BUILD_LOG"
echo "[$(date)] 🎉 Phase 1 execution finished with zero errors."
