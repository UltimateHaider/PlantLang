# ═══════════════════════════════════════════════════════════════
# PlantLang — Chloroplast Pure Native Self-Hosting Toolchain
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

VERSION    ?= 0.49.58
PREFIX     ?= $(HOME)/.local

CC         ?= gcc
CFLAGS     ?= -w -O0
CPPFLAGS   += -I runtime/c
RUNTIME    := runtime/c/plant_runtime.c
ERROR      := runtime/c/plant_error.c
REPORT     := runtime/c/plant_report.c
REPORT_JSON:= runtime/c/plant_report_json.c
REPORT_XML := runtime/c/plant_report_xml.c
REPORT_HTML:= runtime/c/plant_report_html.c
RUNTIME_C  := $(RUNTIME) $(ERROR) $(REPORT) $(REPORT_JSON) $(REPORT_XML) $(REPORT_HTML)
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
	@cp $(RUNTIME_C) $(COMPAT) runtime/c/plant_runtime.h runtime/c/plant_types.h release/plantlang-$(VERSION)/runtime/c/
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

# ── smoke: rapid core feature validation ────────────────────────
smoke: $(NATIVE_BIN) ## Run smoke tests for core language features
	@sh tests/smoke/run_smoke_tests.sh $(NATIVE_BIN)

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
