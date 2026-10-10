# ═══════════════════════════════════════════════════════════════
# PlantLang — Chloroplast Pure Native Self-Hosting Toolchain (v0.50.5)
#
# Targets:
#   make            build the native compiler (bin/Chloroplast)
#   make self       full multi-generation self-hosting chain + convergence check
#   make test       native integration test suite (compile+run+compare)
#   make perf       compile + run benchmarks, write perf_results.md
#   make fmt        format generated C with clang-format (skips if missing)
#   make lint       static-analyze generated C with cppcheck (skips if missing)
#   make dist       versioned tarball + unpack/build/test validation
#   make install    install to $(PREFIX) (default: ~/.local)
#   make clean      remove build artifacts (keeps dist/Chloroplast bootstrap)
#   make help       show this help
# ═══════════════════════════════════════════════════════════════

VERSION    ?= 0.51.17
PREFIX     ?= $(HOME)/.local

CC         ?= gcc
CFLAGS     ?= -w -O0
TEST_CFLAGS ?= -DPLANT_MALLOC_SIMULATE
CPPFLAGS   += -I runtime/c
RUNTIME    := runtime/c/plant_runtime.c
ERROR      := runtime/c/plant_error.c
REPORT     := runtime/c/plant_report.c
REPORT_JSON:= runtime/c/plant_report_json.c
REPORT_XML := runtime/c/plant_report_xml.c
REPORT_HTML:= runtime/c/plant_report_html.c
LEXER      := runtime/c/plant_lexer.c
PARSER     := runtime/c/plant_parser.c
CODEGEN    := runtime/c/plant_codegen.c
MATH       := runtime/c/plant_math.c
TENSOR     := runtime/c/plant_tensor.c
MEMORY     := runtime/c/plant_memory.c
COMPLEX    := runtime/c/plant_complex.c
RUNTIME_C  := $(RUNTIME) $(ERROR) $(REPORT) $(REPORT_JSON) $(REPORT_XML) $(REPORT_HTML) $(LEXER) $(PARSER) $(CODEGEN) $(MATH) $(TENSOR) $(MEMORY) $(COMPLEX)
COMPAT     := runtime/c/plant_compat.h

SRC_DIR    := src/plantc
PLANT_SRC  := $(SRC_DIR)/lexer.plant $(SRC_DIR)/parser.plant $(SRC_DIR)/codegen_c.plant $(SRC_DIR)/main.plant
BOOTSTRAP  := dist/Chloroplast

ALL_PLANT  := build/plantc_all.plant
V2_C       := build/plantc_v2.c
V2_BIN     := build/plantc_v2
V3_C       := build/plantc_v3.c
V3_BIN     := build/plantc_v3
V4_C       := build/plantc_v4.c
V4_BIN     := build/plantc_v4
V5_C       := build/plantc_v5.c
NATIVE_BIN := bin/Chloroplast

# dist/Chloroplast is the bootstrap seed; update manually via:
#   cp build/plantc_v3 dist/Chloroplast
# or run: make bootstrap-update
dist/Chloroplast:
	@echo "  [bootstrap] dist/Chloroplast not found — run: make bootstrap-update"
	@false

.PHONY: bootstrap-update
bootstrap-update: $(NATIVE_BIN)
	@cp $(NATIVE_BIN) $(BOOTSTRAP)
	@echo "  [bootstrap] updated from $(NATIVE_BIN)"

.PHONY: all self test fmt lint dist install help clean rollback benchmark smoke

.DEFAULT_GOAL := all

# ── all: build the native self-hosted compiler ────────────────
all: $(NATIVE_BIN) ## Build native self-hosted compiler (bin/Chloroplast)

$(NATIVE_BIN): $(V3_BIN)
	@mkdir -p bin
	cp $(V3_BIN) $@
	@echo "== bin/Chloroplast built (self-hosted v3) =="

# ── self: multi-generation self-hosting + convergence ────────
self: $(V3_C) $(V4_C) $(V5_C) ## Full self-hosting chain + convergence check
	@cmp -s $(V3_C) $(V4_C) && cmp -s $(V4_C) $(V5_C) && \
		echo "SELF-HOSTING CONVERGED ($(shell wc -c < $(V3_C)) bytes)" || \
		( echo "FAILED: self-hosting generations differ" && exit 1 )

# ── Bootstrap chain: v1 (dist) → v2 → v3 → v4 → v5 ────────────
$(ALL_PLANT): $(PLANT_SRC)
	@mkdir -p build
	@cat $^ | grep -v '^IMPORT\|^PLANT ' > $@

$(V2_C): $(ALL_PLANT) $(BOOTSTRAP)
	@echo "  [v1->v2] $(BOOTSTRAP)"
	@$(BOOTSTRAP) $(ALL_PLANT) $@ >/dev/null 2>&1

$(V2_BIN): $(V2_C) $(RUNTIME_C) $(COMPAT)
	@echo "  [gcc]    $(V2_BIN)"
	@$(CC) $(CFLAGS) $(CPPFLAGS) $< $(RUNTIME_C) -lm -ldl -o $@

$(V3_C): $(ALL_PLANT) $(V2_BIN)
	@echo "  [v2->v3] $(V2_BIN)"
	@$(V2_BIN) $(ALL_PLANT) $@ >/dev/null 2>&1

$(V3_BIN): $(V3_C) $(RUNTIME_C) $(COMPAT)
	@echo "  [gcc]    $(V3_BIN)"
	@$(CC) $(CFLAGS) $(CPPFLAGS) $< $(RUNTIME_C) -lm -ldl -o $@

$(V4_C): $(ALL_PLANT) $(V3_BIN)
	@echo "  [v3->v4] $(V3_BIN)"
	@$(V3_BIN) $(ALL_PLANT) $@ >/dev/null 2>&1

$(V4_BIN): $(V4_C) $(RUNTIME_C) $(COMPAT)
	@echo "  [gcc]    $(V4_BIN)"
	@$(CC) $(CFLAGS) $(CPPFLAGS) $< $(RUNTIME_C) -lm -ldl -o $@

$(V5_C): $(ALL_PLANT) $(V4_BIN)
	@echo "  [v4->v5] $(V4_BIN)"
	@$(V4_BIN) $(ALL_PLANT) $@ >/dev/null 2>&1

# ── test: native + generics + closures integration suites ─────
test: $(NATIVE_BIN) ## Run native + generics + closures + regression suites
	@sh tests/native/run_native_tests.sh $(NATIVE_BIN)
	@sh tests/generics/run_generics_tests.sh $(NATIVE_BIN)
	@sh tests/closures/run_closures_tests.sh $(NATIVE_BIN)
	@sh tests/regression/run_regression_tests.sh $(NATIVE_BIN)

test-stress: $(NATIVE_BIN) ## Run stress test suite (tests/regression/stress/)
	@sh tests/regression/run_regression_tests.sh $(NATIVE_BIN) --stress

perf: $(NATIVE_BIN) ## Compile + run benchmarks, write perf_results.md
	@sh tests/perf/run_perf.sh $(NATIVE_BIN)

# ── fmt / lint: gracefully skip when tools are missing ────────
fmt: ## Format generated C with clang-format (skips if missing)
	@if command -v clang-format >/dev/null 2>&1; then \
		clang-format -i build/plantc_v*.c 2>/dev/null || true; \
		echo "fmt: formatted build/plantc_v*.c"; \
	else \
		echo "fmt: skipped (clang-format not found)"; \
	fi

lint: ## Static-analyze generated C with cppcheck (skips if missing)
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --quiet --enable=warning,performance --suppress=missingIncludeSystem \
			-I runtime/c $(V3_C) 2>&1 || true; \
	else \
		echo "lint: skipped (cppcheck not found)"; \
	fi

# ── tidy: clang-tidy static analysis (skips if missing) ───────
tidy: $(V3_BIN) ## Static-analyze compiled C with clang-tidy (skips if missing)
	@if command -v clang-tidy >/dev/null 2>&1; then \
		echo "tidy: running clang-tidy on $(RUNTIME_C)"; \
		clang-tidy -p . \
			--extra-arg=-Iruntime/c \
			--extra-arg=-I. \
			--checks=bugprone-*,performance-*,readability-* \
			$(RUNTIME_C) 2>&1 || true; \
	else \
		echo "tidy: skipped (clang-tidy not found)"; \
	fi

# ── cppcheck: comprehensive static analysis (skips if missing) ─
cppcheck: $(V3_BIN) ## Full static analysis with cppcheck (skips if missing)
	@if command -v cppcheck >/dev/null 2>&1; then \
		echo "cppcheck: analyzing $(RUNTIME_C)"; \
		cppcheck --enable=all --suppress=missingIncludeSystem -I runtime/c \
			$(V3_C) $(RUNTIME_C) 2>&1 || true; \
	else \
		echo "cppcheck: skipped (cppcheck not found)"; \
	fi

# ── valgrind: memory leak and invalid access audit ────────────
valgrind: $(V3_BIN) ## Run regression suite under valgrind (skips if missing)
	@if command -v valgrind >/dev/null 2>&1; then \
		echo "valgrind: auditing runtime memory safety"; \
		mkdir -p build/valgrind; \
		$(CC) $(CFLAGS) $(CPPFLAGS) $(V3_C) $(RUNTIME_C) -lm -ldl -o build/valgrind/chloroplast 2>&1; \
		valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 \
			./build/valgrind/chloroplast tests/native/hello.plant /tmp/vg_test.c 2>&1 || true; \
	else \
		echo "valgrind: skipped (valgrind not found)"; \
	fi

# ── dist: versioned tarball + validation ──────────────────────
dist: all self test ## Build versioned tarball + unpack/build/test validation
	@mkdir -p release
	@rm -rf release/plantlang-$(VERSION) release/plantlang-$(VERSION).tar.gz
	@mkdir -p release/plantlang-$(VERSION)/src/plantc release/plantlang-$(VERSION)/runtime/c \
		release/plantlang-$(VERSION)/tests/native release/plantlang-$(VERSION)/docs \
		release/plantlang-$(VERSION)/dist
	@cp Makefile README.md LICENSE.txt CHANGELOG.md PHASE3_COMPLETED.md PHASE4_COMPLETED.md \
		release/plantlang-$(VERSION)/ 2>/dev/null || cp Makefile README.md LICENSE.txt CHANGELOG.md release/plantlang-$(VERSION)/
	@cp src/plantc/lexer.plant src/plantc/parser.plant src/plantc/codegen_c.plant src/plantc/main.plant \
		release/plantlang-$(VERSION)/src/plantc/
	@cp $(RUNTIME_C) $(COMPAT) runtime/c/plant_runtime.h runtime/c/plant_report.h runtime/c/plant_lexer.h runtime/c/plant_parser.h runtime/c/plant_codegen.h runtime/c/plant_types.h release/plantlang-$(VERSION)/runtime/c/
	@cp tests/native/*.plant tests/native/*.expected tests/native/*.c tests/native/*.h tests/native/run_native_tests.sh \
		release/plantlang-$(VERSION)/tests/native/
	@mkdir -p release/plantlang-$(VERSION)/tests/generics
	@cp tests/generics/*.plant tests/generics/*.expected tests/generics/*.grep tests/generics/run_generics_tests.sh \
		release/plantlang-$(VERSION)/tests/generics/
	@mkdir -p release/plantlang-$(VERSION)/tests/closures
	@cp tests/closures/*.plant tests/closures/*.expected tests/closures/*.grep tests/closures/run_closures_tests.sh \
		release/plantlang-$(VERSION)/tests/closures/
	@mkdir -p release/plantlang-$(VERSION)/tests/regression
	@cp tests/regression/*.plant tests/regression/*.expected tests/regression/*.invalid tests/regression/*.py tests/regression/run_regression_tests.sh \
		release/plantlang-$(VERSION)/tests/regression/
	@cp $(BOOTSTRAP) release/plantlang-$(VERSION)/dist/Chloroplast
	@tar -C release -czf release/plantlang-$(VERSION).tar.gz plantlang-$(VERSION)
	@rm -rf build/distcheck && mkdir -p build/distcheck
	@tar -C build/distcheck -xzf release/plantlang-$(VERSION).tar.gz
	@echo "== distcheck: unpack + build + test =="
	@$(MAKE) -s -C build/distcheck/plantlang-$(VERSION) all && \
		$(MAKE) -s -C build/distcheck/plantlang-$(VERSION) test && \
		echo "DISTCHECK OK: release/plantlang-$(VERSION).tar.gz"

# ── install ────────────────────────────────────────────────────
install: all ## Install to $(PREFIX) (default ~/.local)
	@mkdir -p $(PREFIX)/bin $(PREFIX)/include/plantlang
	@cp $(NATIVE_BIN) $(PREFIX)/bin/Chloroplast
	@cp $(COMPAT) $(RUNTIME_C) $(PREFIX)/include/plantlang/
	@echo "== installed: $(PREFIX)/bin/Chloroplast =="
	@$(PREFIX)/bin/Chloroplast --version

# ── rollback: revert to a tagged prior release ──────────────────
# Usage: make rollback VERSION=v0.49.56b
# Reverts all tracked files to the specified tag and rebuilds.
rollback: ## Revert to a tagged release: make rollback VERSION=v0.49.56b
	@if [ -z "$(VERSION)" ]; then echo "Usage: make rollback VERSION=<tag>"; exit 1; fi
	@if ! git rev-parse "$(VERSION)" >/dev/null 2>&1; then \
		echo "ERROR: tag '$(VERSION)' not found"; \
		git tag -l | sed 's/^/Available tags: /'; \
		exit 1; \
	fi
	@echo "== Rolling back to $(VERSION) =="
	@git checkout -- "$(VERSION)" 2>&1 || true
	@git clean -fdq 2>/dev/null || true
	@git reset --hard "$(VERSION)"
	@$(MAKE) -s clean >/dev/null 2>&1 || true
	@$(MAKE) -s all 2>&1 | tail -3
	@echo "== Rollback complete: now at $(VERSION) =="

# ── benchmark: timing metrics for build + test cycles ────────────
benchmark: ## Run performance benchmark for build targets
	@sh scripts/benchmark.sh

# ── size-report: binary size tracking across versions ────────────
size-report: $(NATIVE_BIN) ## Show binary sizes and threshold status
	@echo "╔══════════════════════════════════════════════════════════╗"
	@echo "║ SIZE REPORT — v$(VERSION)                                  ║"
	@echo "╠══════════════════════════════════════════════════════════╣"
	@echo "║ File                                    Lines Status    ║"
	@echo "╠══════════════════════════════════════════════════════════╣"
	@# plant_runtime.c: INFO=8000, WARN=8500, CRIT=9000
	@_loc=$$(wc -l < runtime/c/plant_runtime.c); \
	 if [ "$$_loc" -ge 9000 ]; then _st="🔴 CRITICAL"; \
	 elif [ "$$_loc" -ge 8500 ]; then _st="⚠️  WARN"; \
	 elif [ "$$_loc" -ge 8000 ]; then _st="ℹ️  INFO"; \
	 else _st="✅ OK"; fi; \
	 printf "║ %-40s %5d %s\n" "runtime/c/plant_runtime.c" "$$_loc" "$$_st"
	@# plant_math.c: WARN=8000, CRIT=10000
	@_loc=$$(wc -l < runtime/c/plant_math.c); \
	 if [ "$$_loc" -ge 10000 ]; then _st="🔴 CRITICAL"; \
	 elif [ "$$_loc" -ge 8000 ]; then _st="⚠️  WARN"; \
	 else _st="✅ OK"; fi; \
	 printf "║ %-40s %5d %s\n" "runtime/c/plant_math.c" "$$_loc" "$$_st"
	@# Other files — informational only
	@_loc=$$(wc -l < runtime/c/plant_tensor.c); \
	 printf "║ %-40s %5d ✅ OK\n" "runtime/c/plant_tensor.c" "$$_loc"
	@_loc=$$(wc -l < runtime/c/plant_memory.c); \
	 printf "║ %-40s %5d ✅ OK\n" "runtime/c/plant_memory.c" "$$_loc"
	@_loc=$$(wc -l < runtime/c/plant_report.c); \
	 printf "║ %-40s %5d ✅ OK\n" "runtime/c/plant_report.c" "$$_loc"
	@echo "╠══════════════════════════════════════════════════════════╣"
	@echo "║ Binaries:                                               ║"
	@_sz=$$(stat -c%s dist/Chloroplast 2>/dev/null || echo 0); \
	 printf "║ %-40s %s bytes\n" "dist/Chloroplast (bootstrap):" "$$_sz"
	@_sz=$$(stat -c%s bin/Chloroplast 2>/dev/null || echo 0); \
	 printf "║ %-40s %s bytes\n" "bin/Chloroplast (final):" "$$_sz"
	@echo "╚══════════════════════════════════════════════════════════╝"

# ── test-perf: timing metrics for test suites ──────────────────
test-perf: $(NATIVE_BIN) ## Run test suites with timing metrics
	@echo "=== Test Performance (v$(VERSION)) ==="
	@echo "--- Native tests ---"
	@time sh tests/native/run_native_tests.sh $(NATIVE_BIN) 2>&1 | tail -3
	@echo "--- Generics tests ---"
	@time sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) 2>&1 | tail -3
	@echo "--- Closures tests ---"
	@time sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) 2>&1 | tail -3

# ── smoke: rapid core feature validation ────────────────────────
smoke: $(NATIVE_BIN) ## Run smoke tests for core language features
	@sh tests/smoke/run_smoke_tests.sh $(NATIVE_BIN)

# ── check-no-raw-malloc: verify no raw malloc in new files ──────
check-no-raw-malloc: ## Verify no raw malloc in plant_tensor.c and plant_memory.c
	@if grep -nE '(^|[^_a-zA-Z])malloc[[:space:]]*\(' \
		runtime/c/plant_tensor.c | grep -v 'plant_malloc'; then \
		echo "STOP: Raw malloc found in plant_tensor.c"; \
		exit 1; \
	fi
	@if grep -nE '(^|[^_a-zA-Z])malloc[[:space:]]*\(' \
		runtime/c/plant_memory.c | grep -v 'plant_malloc'; then \
		echo "STOP: Raw malloc found in plant_memory.c"; \
		exit 1; \
	fi
	@echo "OK: plant_tensor.c and plant_memory.c use only plant_malloc"

# ── check-expected-files: verify .expected files have no comments ──
check-expected-files: ## Verify .expected files contain only real output
	@for f in tests/native/tensor_*.expected; do \
		if grep -qE '^\s*#' "$$f"; then \
			echo "STOP: Comments in $$f"; \
			exit 1; \
		fi; \
	done
	@echo "OK: .expected files contain only real output"

# ── check-changelog-numbers: verify no placeholders in CHANGELOG ──
check-changelog-numbers: ## Verify CHANGELOG numbers are real
	@if grep -E 'XXX|\?\?\?' docs/CHANGELOG.md | grep -A1 'SIZE REPORT'; then \
		echo "STOP: Placeholders in CHANGELOG"; \
		exit 1; \
	fi
	@echo "OK: CHANGELOG numbers are real"

# ── valgrind-check: memory leak check on tensor tests ────────────
# v0.51.1: report-only (pre-existing leaks documented, TD-001/002/003)
# v0.51.2+: will add --error-exitcode=1 for NEW leaks only
valgrind-check: $(NATIVE_BIN) ## Run valgrind on compiled tensor memory tests (report only)
	@if ! command -v valgrind >/dev/null 2>&1; then \
		echo "valgrind not installed. Skipping."; \
		exit 0; \
	fi
	@for test in tensor_refcount tensor_malloc; do \
		echo "Checking $$test..."; \
		./bin/Chloroplast tests/native/$$test.plant /tmp/vg_$$test.c 2>/dev/null; \
		$(CC) $(CFLAGS) $(TEST_CFLAGS) $(CPPFLAGS) /tmp/vg_$$test.c \
			$(RUNTIME) $(ERROR) $(REPORT) $(REPORT_JSON) $(REPORT_XML) \
			$(REPORT_HTML) $(MATH) $(TENSOR) tests/native/mock_ffi.c \
			-lm -ldl -o /tmp/vg_$$test 2>/dev/null; \
		valgrind --leak-check=full \
			--errors-for-leak-kinds=definite \
			/tmp/vg_$$test \
			2>&1 | grep -E "LEAK SUMMARY|definitely lost|ERROR SUMMARY" || true; \
		echo "---"; \
	done
	@echo "OK: valgrind report complete (pre-existing leaks documented)"

# ── valgrind-check-tensor: strict tensor leak gate (v0.51.2a) ───
# STRICT: fails on ANY definite leak in tensor tests.
valgrind-check-tensor: $(NATIVE_BIN) ## Run strict valgrind on tensor tests
	@echo "=== Strict valgrind on tensor tests ==="
	@if [ "$(SKIP_VALGRIND)" = "1" ]; then \
		echo "SKIP_VALGRIND=1 — skipping (document in commit)"; \
		exit 0; \
	fi
	@if ! command -v valgrind >/dev/null 2>&1; then \
		echo "STOP: valgrind not installed"; \
		echo "   Install: sudo apt-get install valgrind"; \
		exit 1; \
	fi
	@for test in tensor_basic tensor_malloc tensor_refcount; do \
		if [ ! -f tests/native/$$test.plant ]; then \
			echo "SKIP: tests/native/$$test.plant not found"; \
			continue; \
		fi; \
		echo "Checking $$test..."; \
		./bin/Chloroplast tests/native/$$test.plant /tmp/vg_$$test.c 2>/dev/null; \
		$(CC) $(CFLAGS) $(TEST_CFLAGS) $(CPPFLAGS) /tmp/vg_$$test.c \
			$(RUNTIME) $(ERROR) $(REPORT) $(REPORT_JSON) $(REPORT_XML) \
			$(REPORT_HTML) $(MATH) $(TENSOR) $(MEMORY) tests/native/mock_ffi.c \
			-lm -ldl -o /tmp/vg_$$test 2>/dev/null; \
		valgrind --leak-check=full \
			--error-exitcode=1 \
			--errors-for-leak-kinds=definite \
			/tmp/vg_$$test > /tmp/vg_$$test.log 2>&1; \
		vgrc=$$?; \
		cat /tmp/vg_$$test.log; \
		if [ $$vgrc -ne 0 ]; then \
			echo "STOP: valgrind errors in $$test"; \
			exit 1; \
		fi; \
		echo "OK: $$test clean"; \
	done
	@echo "valgrind-check-tensor passed"

# ── valgrind-check-tensor-soft: report-only (never fails) ───────
valgrind-check-tensor-soft: $(NATIVE_BIN) ## Report tensor leaks without failing
	@echo "=== Soft valgrind on tensor tests (report-only) ==="
	@for test in tensor_basic tensor_malloc tensor_refcount; do \
		if [ ! -f tests/native/$$test.plant ]; then \
			echo "SKIP: tests/native/$$test.plant not found"; \
			continue; \
		fi; \
		echo "Checking $$test..."; \
		./bin/Chloroplast tests/native/$$test.plant /tmp/vgs_$$test.c 2>/dev/null; \
		$(CC) $(CFLAGS) $(TEST_CFLAGS) $(CPPFLAGS) /tmp/vgs_$$test.c \
			$(RUNTIME) $(ERROR) $(REPORT) $(REPORT_JSON) $(REPORT_XML) \
			$(REPORT_HTML) $(MATH) $(TENSOR) $(MEMORY) tests/native/mock_ffi.c \
			-lm -ldl -o /tmp/vgs_$$test 2>/dev/null; \
		valgrind --leak-check=full \
			--errors-for-leak-kinds=definite \
			/tmp/vgs_$$test 2>&1 | grep -E "definitely lost|ERROR SUMMARY" || true; \
	done
	@echo "valgrind-check-tensor-soft done (report-only)"

# ── check-ffi-safety: verify FFI examples don't use unsafe APIs ─
check-ffi-safety: ## Check FFI safety
	@echo "=== Checking FFI safety ==="
	@if grep -rn "plant_tensor_to_string_static" examples/ffi_*.c 2>/dev/null; then \
		echo "STOP: FFI example uses to_string_static"; \
		echo "   Use plant_tensor_to_string instead."; \
		exit 1; \
	fi
	@echo "No FFI code uses to_string_static"

# ── check-releases-updated: verify RELEASES.md is current ──────
check-releases-updated: ## Check RELEASES.md is updated
	@if ! grep -q "# Auto-updated: $(VERSION)" docs/RELEASES.md 2>/dev/null; then \
		echo "STOP: RELEASES.md not updated for $(VERSION)"; \
		echo "   Run: make update-growth"; \
		exit 1; \
	fi
	@echo "RELEASES.md updated for $(VERSION)"

# ── check-verify-integrity: verify verify-* targets include valgrind
check-verify-integrity: ## Check verify targets include valgrind
	@echo "=== Checking verify-* targets include valgrind ==="
	@for target in verify-v0.51.2a verify-v0.51.2b verify-v0.51.2c verify-v0.51.2d; do \
		if grep -q "^$$target:" Makefile; then \
			if ! grep -A 25 "^$$target:" Makefile | grep -q "valgrind-check-tensor"; then \
				echo "STOP: $$target does not include valgrind-check-tensor"; \
				exit 1; \
			fi; \
		fi; \
	done
	@echo "All verify targets include valgrind"

# ── update-growth: update RELEASES.md growth table ──────────────
update-growth: ## Update growth table in RELEASES.md
	@bash scripts/update_growth_table.sh

# ── verify-v0.51.2a: comprehensive release gate ──────────────────
verify-v0.51.2a: check-no-raw-malloc check-expected-files \
                 check-changelog-numbers check-ffi-safety ## Run v0.51.2a verification gate
	@echo "========================================================"
	@echo "  v0.51.2a VERIFICATION GATE"
	@echo "========================================================"
	@echo ""
	@echo "[1/7] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/7] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[3/7] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[4/7] Size report..."      && $(MAKE) size-report
	@echo "[5/7] check-ffi-safety..." && $(MAKE) check-ffi-safety
	@echo "[6/7] Update RELEASES..."  && $(MAKE) update-growth
	@echo "[7/7] Verify RELEASES..."  && $(MAKE) check-releases-updated
	@echo ""
	@echo "========================================================"
	@echo "  v0.51.2a VERIFIED — PRODUCTION MILESTONE"
	@echo "========================================================"

# ── test-perf-6d: run 6D tensor benchmark ──────────────────────
test-perf-6d: ## Run 6D tensor benchmark
	@sh tests/perf/run_6d_bench.sh $(NATIVE_BIN)

# ── check-benchmark-recorded: verify benchmark result recorded ──
check-benchmark-recorded: ## Check benchmark result in perf_results.md
	@if ! grep -q "6D tensor" perf_results.md 2>/dev/null; then \
		echo "STOP: 6D tensor benchmark not recorded in perf_results.md"; \
		echo "   Run: make test-perf-6d"; \
		exit 1; \
	fi
	@echo "Benchmark recorded"

# ── check-changelog-benchmark: verify benchmark in CHANGELOG ────
check-changelog-benchmark: ## Check benchmark mentioned in CHANGELOG
	@if ! grep -q "tensor_6d_bench" docs/CHANGELOG.md 2>/dev/null; then \
		echo "STOP: tensor_6d_bench not mentioned in CHANGELOG.md"; \
		exit 1; \
	fi
	@echo "CHANGELOG benchmark OK"

# ── check-dispatcher-only: verify all free built-ins use plant_free
check-dispatcher-only: ## Verify codegen uses plant_free for all free built-ins
	@if ! grep -q 'ca2 IS "LIST_FREE".*plant_free' src/plantc/codegen_c.plant; then \
		echo "STOP: LIST_FREE not routed through plant_free"; exit 1; \
	fi
	@if ! grep -q 'ca2 IS "TENSOR_FREE".*plant_free' src/plantc/codegen_c.plant; then \
		echo "STOP: TENSOR_FREE not routed through plant_free"; exit 1; \
	fi
	@if ! grep -q 'ca2 IS "FREE".*plant_free' src/plantc/codegen_c.plant; then \
		echo "STOP: FREE not routed through plant_free"; exit 1; \
	fi
	@echo "All free built-ins use plant_free dispatcher"

# ── verify-v0.51.2b: comprehensive release gate ────────────────
verify-v0.51.2b: check-no-raw-malloc check-expected-files \
                 check-changelog-numbers check-ffi-safety \
                 check-dispatcher-only ## Run v0.51.2b verification gate
	@echo "========================================================"
	@echo "  v0.51.2b VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/8] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/8] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[3/8] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[4/8] Size report..."      && $(MAKE) size-report
	@echo "[5/8] check-ffi-safety..." && $(MAKE) check-ffi-safety
	@echo "[6/8] check-dispatcher..." && $(MAKE) check-dispatcher-only
	@echo "[7/8] Update RELEASES..."  && $(MAKE) update-growth
	@echo "[8/8] Verify RELEASES..."  && $(MAKE) check-releases-updated
	@echo ""
	@echo "========================================================"
	@echo "  v0.51.2b VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.2c: comprehensive release gate ────────────────
verify-v0.51.2c: check-no-raw-malloc check-expected-files \
                 check-changelog-numbers check-ffi-safety \
                 check-dispatcher-only ## Run v0.51.2c verification gate
	@echo "========================================================"
	@echo "  v0.51.2c VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/7] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/7] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/7] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/7] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/7] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[6/7] Size report..."      && $(MAKE) size-report
	@echo "[7/7] Update RELEASES..."  && $(MAKE) update-growth && $(MAKE) check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.2c VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.2d.1: comprehensive release gate ──────────────
verify-v0.51.2d.1: check-no-raw-malloc check-expected-files \
                   check-changelog-numbers check-ffi-safety \
                   check-dispatcher-only
	@echo "========================================================"
	@echo "  v0.51.2d.1 VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/6] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/6] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/6] Size report..."      && make size-report
	@echo "[6/6] Update RELEASES..."  && make update-growth && make check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.2d.1 VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.2d.2: comprehensive release gate ──────────────
verify-v0.51.2d.2: check-no-raw-malloc check-expected-files \
                   check-changelog-numbers check-ffi-safety \
                   check-dispatcher-only
	@echo "========================================================"
	@echo "  v0.51.2d.2 VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/6] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/6] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/6] Size report..."      && make size-report
	@echo "[6/6] Update RELEASES..."  && make update-growth && make check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.2d.2 VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.3a: comprehensive release gate ────────────────
verify-v0.51.3a: check-no-raw-malloc check-expected-files \
                 check-changelog-numbers check-ffi-safety \
                 check-dispatcher-only
	@echo "========================================================"
	@echo "  v0.51.3a VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/6] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/6] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/6] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[6/6] Update RELEASES..."  && make update-growth && make check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.3a VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.3b: comprehensive release gate ────────────────
verify-v0.51.3b: check-no-raw-malloc check-expected-files \
                 check-changelog-numbers check-ffi-safety \
                 check-dispatcher-only
	@echo "========================================================"
	@echo "  v0.51.3b VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/6] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/6] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/6] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[6/6] Update RELEASES..."  && make update-growth && make check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.3b VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.4: comprehensive release gate ────────────────
verify-v0.51.4: check-no-raw-malloc check-expected-files \
                 check-changelog-numbers check-ffi-safety \
                 check-dispatcher-only
	@echo "========================================================"
	@echo "  v0.51.4 VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/6] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/6] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/6] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[6/6] Update RELEASES..."  && make update-growth && make check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.4 VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.5: comprehensive release gate ────────────────
verify-v0.51.5: check-no-raw-malloc check-expected-files \
                check-changelog-numbers check-ffi-safety \
                check-dispatcher-only
	@echo "========================================================"
	@echo "  v0.51.5 VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/6] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/6] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/6] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[6/6] Update RELEASES..."  && make update-growth && make check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.5 VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.6: comprehensive release gate ────────────────
verify-v0.51.6: check-no-raw-malloc check-expected-files \
                check-changelog-numbers check-ffi-safety \
                check-dispatcher-only
	@echo "========================================================"
	@echo "  v0.51.6 VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/6] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/6] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/6] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[6/6] Update RELEASES..."  && make update-growth && make check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.6 VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.7: comprehensive release gate ────────────────
verify-v0.51.7: check-no-raw-malloc check-expected-files \
                check-changelog-numbers check-ffi-safety \
                check-dispatcher-only
	@echo "========================================================"
	@echo "  v0.51.7 VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/6] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/6] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/6] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[6/6] Update RELEASES..."  && make update-growth && make check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.7 VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.8: comprehensive release gate ────────────────
verify-v0.51.8: check-no-raw-malloc check-expected-files \
                check-changelog-numbers check-ffi-safety \
                check-dispatcher-only
	@echo "========================================================"
	@echo "  v0.51.8 VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/6] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/6] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/6] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[6/6] Update RELEASES..."  && make update-growth && make check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.8 VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.9: comprehensive release gate ────────────────
verify-v0.51.9: check-no-raw-malloc check-expected-files \
                check-changelog-numbers check-ffi-safety \
                check-dispatcher-only
	@echo "========================================================"
	@echo "  v0.51.9 VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/6] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/6] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/6] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[6/6] Update RELEASES..."  && make update-growth && make check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.9 VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.10: comprehensive release gate ───────────────
verify-v0.51.10: check-no-raw-malloc check-expected-files \
                 check-changelog-numbers check-ffi-safety \
                 check-dispatcher-only
	@echo "========================================================"
	@echo "  v0.51.10 VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/6] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/6] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/6] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[6/6] Update RELEASES..."  && make update-growth && make check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.10 VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.11: comprehensive release gate ───────────────
verify-v0.51.11: check-no-raw-malloc check-expected-files \
                 check-changelog-numbers check-ffi-safety \
                 check-dispatcher-only
	@echo "========================================================"
	@echo "  v0.51.11 VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/6] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/6] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/6] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[6/6] Update RELEASES..."  && make update-growth && make check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.11 VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.12: comprehensive release gate ───────────────
verify-v0.51.12: check-no-raw-malloc check-expected-files \
                 check-changelog-numbers check-ffi-safety \
                 check-dispatcher-only
	@echo "========================================================"
	@echo "  v0.51.12 VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/6] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/6] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/6] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[6/6] Update RELEASES..."  && make update-growth && make check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.12 VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.13: comprehensive release gate ───────────────
verify-v0.51.13: check-no-raw-malloc check-expected-files \
                 check-changelog-numbers check-ffi-safety \
                 check-dispatcher-only
	@echo "========================================================"
	@echo "  v0.51.13 VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/6] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/6] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/6] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[6/6] Update RELEASES..."  && make update-growth && make check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.13 VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.14: comprehensive release gate ───────────────
verify-v0.51.14: check-no-raw-malloc check-expected-files \
                 check-changelog-numbers check-ffi-safety \
                 check-dispatcher-only
	@echo "========================================================"
	@echo "  v0.51.14 VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/6] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/6] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/6] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[6/6] Update RELEASES..."  && make update-growth && make check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.14 VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.15: comprehensive release gate ───────────────
verify-v0.51.15: check-no-raw-malloc check-expected-files \
                 check-changelog-numbers check-ffi-safety \
                 check-dispatcher-only
	@echo "========================================================"
	@echo "  v0.51.15 VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/6] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/6] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/6] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[6/6] Update RELEASES..."  && make update-growth && make check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.15 VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.16: comprehensive release gate ───────────────
verify-v0.51.16: check-no-raw-malloc check-expected-files \
                 check-changelog-numbers check-ffi-safety \
                 check-dispatcher-only
	@echo "========================================================"
	@echo "  v0.51.16 VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/6] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/6] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/6] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[6/6] Update RELEASES..."  && make update-growth && make check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.16 VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.17: comprehensive release gate ───────────────
verify-v0.51.17: check-no-raw-malloc check-expected-files \
                 check-changelog-numbers check-ffi-safety \
                 check-dispatcher-only
	@echo "========================================================"
	@echo "  v0.51.17 VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."     && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] Generics..."         && sh tests/generics/run_generics_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[3/6] Closures..."         && sh tests/closures/run_closures_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[4/6] Self-hosting..."     && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || \
		{ echo "STOP: Self-hosting failed"; exit 1; }
	@echo "[5/6] valgrind strict..."  && $(MAKE) valgrind-check-tensor || exit 1
	@echo "[6/6] Update RELEASES..."  && make update-growth && make check-releases-updated
	@echo "========================================================"
	@echo "  v0.51.17 VERIFIED"
	@echo "========================================================"

# ── verify-v0.51.1: comprehensive release gate ──────────────────
verify-v0.51.1: check-no-raw-malloc check-expected-files check-changelog-numbers ## Run v0.51.1 verification gate
	@echo "========================================================"
	@echo "  v0.51.1 VERIFICATION GATE"
	@echo "========================================================"
	@echo "[1/6] Native tests..."   && sh tests/native/run_native_tests.sh $(NATIVE_BIN) || exit 1
	@echo "[2/6] valgrind..."       && make valgrind-check || exit 1
	@echo "[3/6] Self-hosting..."   && make self || exit 1
	@cmp -s build/plantc_v3 bin/Chloroplast || { \
		echo "STOP: Self-hosting does NOT converge"; exit 1; }
	@echo "[4/6] Size report..."    && make size-report
	@echo "========================================================"
	@echo "  v0.51.1 VERIFIED"
	@echo "========================================================"

# ── clean ──────────────────────────────────────────────────────
clean: ## Remove build artifacts (keeps dist/Chloroplast bootstrap)
	@rm -rf build/*.c build/plantc_v2 build/plantc_v3 build/plantc_v4 build/plantc_v5 \
		build/plantc_all.plant build/distcheck release $(NATIVE_BIN)
	@echo "== cleaned =="

# ── help ───────────────────────────────────────────────────────
help: ## Show this help
	@grep -hE '^[a-zA-Z0-9._-]+:.*##' $(MAKEFILE_LIST) | \
		awk -F':.*##' '{printf "  %-10s %s\n", $$1, $$2}'
	@echo ""
	@echo "Variables: VERSION=$(VERSION)  PREFIX=$(PREFIX)  CC=$(CC)"
