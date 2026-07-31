# ═══════════════════════════════════════════════════════════════
# PlantLang — Native Self-Hosting Toolchain Makefile
#
# Targets:
#   make            build the native compiler (bin/plantc)
#   make self       full multi-generation self-hosting chain + convergence check
#   make test       native integration test suite (compile+run+compare)
#   make test:js    legacy JS test suites (requires node)
#   make fmt        format generated C with clang-format (skips if missing)
#   make lint       static-analyze generated C with cppcheck (skips if missing)
#   make dist       versioned tarball + unpack/build/test validation
#   make install    install to $(PREFIX) (default: ~/.local)
#   make clean      remove build artifacts (keeps dist/plantc bootstrap)
#   make help       show this help
# ═══════════════════════════════════════════════════════════════

VERSION    ?= 1.0.0-phase5
PREFIX     ?= $(HOME)/.local

CC         ?= gcc
CFLAGS     ?= -w -O0
CPPFLAGS   += -I runtime/c
RUNTIME    := runtime/c/plant_runtime.c
COMPAT     := runtime/c/plant_compat.h

SRC_DIR    := src/plantc
PLANT_SRC  := $(SRC_DIR)/lexer.plant $(SRC_DIR)/parser.plant $(SRC_DIR)/codegen_c.plant $(SRC_DIR)/main.plant
BOOTSTRAP  := dist/plantc

ALL_PLANT  := build/plantc_all.plant
V2_C       := build/plantc_v2.c
V2_BIN     := build/plantc_v2
V3_C       := build/plantc_v3.c
V3_BIN     := build/plantc_v3
V4_C       := build/plantc_v4.c
V4_BIN     := build/plantc_v4
V5_C       := build/plantc_v5.c
NATIVE_BIN := bin/plantc

# dist/plantc is the pre-built bootstrap compiler — never rebuilt
dist/plantc:
	@true

.PHONY: all self test test-js fmt lint dist install help clean

.DEFAULT_GOAL := all

# ── all: build the native self-hosted compiler ────────────────
all: $(NATIVE_BIN) ## Build native self-hosted compiler (bin/plantc)

$(NATIVE_BIN): $(V3_BIN)
	@mkdir -p bin
	cp $(V3_BIN) $@
	@echo "== bin/plantc built (self-hosted v3) =="

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

$(V2_BIN): $(V2_C) $(RUNTIME) $(COMPAT)
	@echo "  [gcc]    $(V2_BIN)"
	@$(CC) $(CFLAGS) $(CPPFLAGS) $< $(RUNTIME) -o $@

$(V3_C): $(ALL_PLANT) $(V2_BIN)
	@echo "  [v2->v3] $(V2_BIN)"
	@$(V2_BIN) $(ALL_PLANT) $@ >/dev/null 2>&1

$(V3_BIN): $(V3_C) $(RUNTIME) $(COMPAT)
	@echo "  [gcc]    $(V3_BIN)"
	@$(CC) $(CFLAGS) $(CPPFLAGS) $< $(RUNTIME) -o $@

$(V4_C): $(ALL_PLANT) $(V3_BIN)
	@echo "  [v3->v4] $(V3_BIN)"
	@$(V3_BIN) $(ALL_PLANT) $@ >/dev/null 2>&1

$(V4_BIN): $(V4_C) $(RUNTIME) $(COMPAT)
	@echo "  [gcc]    $(V4_BIN)"
	@$(CC) $(CFLAGS) $(CPPFLAGS) $< $(RUNTIME) -o $@

$(V5_C): $(ALL_PLANT) $(V4_BIN)
	@echo "  [v4->v5] $(V4_BIN)"
	@$(V4_BIN) $(ALL_PLANT) $@ >/dev/null 2>&1

# ── test: native integration suite ────────────────────────────
test: $(NATIVE_BIN) ## Run native integration test suite
	@sh tests/native/run_native_tests.sh $(NATIVE_BIN)

test-js: ## Run legacy JS test suites (requires node)
	@for f in tests/test_*.js; do \
		echo "=== $$(basename $$f) ==="; \
		node "$$f" 2>&1 | tail -2; \
	done

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

# ── dist: versioned tarball + validation ──────────────────────
dist: all self test ## Build versioned tarball + unpack/build/test validation
	@mkdir -p release
	@rm -rf release/plantlang-$(VERSION) release/plantlang-$(VERSION).tar.gz
	@mkdir -p release/plantlang-$(VERSION)/src/plantc release/plantlang-$(VERSION)/runtime/c \
		release/plantlang-$(VERSION)/tests/native release/plantlang-$(VERSION)/docs \
		release/plantlang-$(VERSION)/dist
	@cp Makefile README.md LICENSE.txt CHANGELOG.md PHASE3_COMPLETED.md \
		release/plantlang-$(VERSION)/ 2>/dev/null || cp Makefile README.md LICENSE.txt CHANGELOG.md release/plantlang-$(VERSION)/
	@cp src/plantc/lexer.plant src/plantc/parser.plant src/plantc/codegen_c.plant src/plantc/main.plant \
		release/plantlang-$(VERSION)/src/plantc/
	@cp $(RUNTIME) $(COMPAT) runtime/c/plant_runtime.h release/plantlang-$(VERSION)/runtime/c/
	@cp tests/native/*.plant tests/native/*.expected tests/native/run_native_tests.sh \
		release/plantlang-$(VERSION)/tests/native/
	@cp $(BOOTSTRAP) release/plantlang-$(VERSION)/dist/plantc
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
	@cp $(NATIVE_BIN) $(PREFIX)/bin/plantc
	@cp $(COMPAT) $(RUNTIME) $(PREFIX)/include/plantlang/
	@echo "== installed: $(PREFIX)/bin/plantc =="
	@$(PREFIX)/bin/plantc --version

# ── clean ──────────────────────────────────────────────────────
clean: ## Remove build artifacts (keeps dist/plantc bootstrap)
	@rm -rf build/*.c build/plantc_v2 build/plantc_v3 build/plantc_v4 build/plantc_v5 \
		build/plantc_all.plant build/distcheck release $(NATIVE_BIN)
	@echo "== cleaned =="

# ── help ───────────────────────────────────────────────────────
help: ## Show this help
	@grep -hE '^[a-zA-Z0-9._-]+:.*##' $(MAKEFILE_LIST) | \
		awk -F':.*##' '{printf "  %-10s %s\n", $$1, $$2}'
	@echo ""
	@echo "Variables: VERSION=$(VERSION)  PREFIX=$(PREFIX)  CC=$(CC)"
