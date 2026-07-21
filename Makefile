# PlantLang Build Pipeline
#
# Usage:
#   make              — build libplantlang.so only
#   make run FILE=x.plnt — compile + run a PlantLang source
#   make test         — run all JS test suites
#   make clean        — remove build artifacts

LLVM_BIN ?= $(shell which llc 2>/dev/null || which llc-18 2>/dev/null || which llc-17 2>/dev/null || which llc-16 2>/dev/null || which llc-15 2>/dev/null || which llc-14 2>/dev/null || echo "")
OPT_BIN  ?= $(shell which opt 2>/dev/null || echo "")

RUNTIME_DIR := runtime
RUNTIME_SRC := $(RUNTIME_DIR)/runtime.c
RUNTIME_LIB := $(RUNTIME_DIR)/libplantlang.so
RUNTIME_OBJ := $(RUNTIME_DIR)/runtime.o

BRIDGE_SRC := core/runtime_bridge.c
BRIDGE_OBJ := $(RUNTIME_DIR)/runtime_bridge.o

.PHONY: all runtime clean run test

all: runtime

# ── Build the shared runtime library ──────────────────────────
# NOTE: Use clang to match llc's struct-passing ABI for %fat_ptr.
CC ?= clang

runtime: $(RUNTIME_LIB)

$(RUNTIME_OBJ): $(RUNTIME_SRC)
	$(CC) -c -fPIC -O2 -Wall -Wextra -o $@ $< -lm

$(BRIDGE_OBJ): $(BRIDGE_SRC)
	$(CC) -c -fPIC -O2 -Wall -Wextra -o $@ $<

$(RUNTIME_LIB): $(RUNTIME_OBJ) $(BRIDGE_OBJ)
	$(CC) -shared -o $@ $^ -lm

# ── Compile and run a PlantLang file ─────────────────────────
run: $(RUNTIME_LIB)
	@if [ -z "$(FILE)" ]; then echo "Usage: make run FILE=examples/hello.plnt"; exit 1; fi
	@if [ -z "$(LLVM_BIN)" ]; then echo "Error: LLVM (llc) not found"; exit 1; fi
	node chloroplast.js compile "$(FILE)" --keep-c 2>&1 || true
	@# The compile command above handles the entire pipeline via chloroplast.js

# ── Development: manual compile-and-link (for testing) ───────
# Usage: make exec FILE=test.plnt
exec: $(RUNTIME_LIB)
	@if [ -z "$(FILE)" ]; then echo "Usage: make exec FILE=test.plnt"; exit 1; fi
	@if [ -z "$(LLVM_BIN)" ]; then echo "Error: LLVM (llc) not found"; exit 1; fi
	$(eval BASE := $(basename $(notdir $(FILE))))
	$(eval DIR := $(dir $(FILE)))
	@echo "Generating LLVM IR for $(FILE)..."
	node -e "
		const {parseFile} = require('./core/parser');
		const {generate} = require('./core/llvm_codegen');
		const prog = parseFile('$(FILE)');
		const {ir, errors} = generate(prog);
		if (errors.length) { console.log(errors.join('\n')); process.exit(1); }
		require('fs').writeFileSync('build/$(BASE).ll', ir, 'utf8');
	"
	@mkdir -p build
	@if [ -n "$(OPT_BIN)" ]; then \
		echo "Optimizing with $(OPT_BIN)..."; \
		$(OPT_BIN) build/$(BASE).ll -O2 -S -o build/$(BASE).opt.ll; \
		$(LLVM_BIN) build/$(BASE).opt.ll -O2 -o build/$(BASE).s; \
	else \
		$(LLVM_BIN) build/$(BASE).ll -O2 -o build/$(BASE).s; \
	fi
	gcc build/$(BASE).s -L$(RUNTIME_DIR) -lplantlang -no-pie -lm -o build/$(BASE)
	@echo "Running build/$(BASE)..."
	./build/$(BASE)

# ── Run JS test suites ───────────────────────────────────────
test:
	@echo "Running all PlantLang test suites..."
	@for f in tests/test_*.js; do \
		echo "=== $$(basename $$f) ==="; \
		node "$$f" 2>&1 | tail -2; \
	done

# ── Clean ────────────────────────────────────────────────────
clean:
	rm -rf build/*.ll build/*.opt.ll build/*.s build/*.o build/*.plnt
	rm -f $(RUNTIME_OBJ) $(BRIDGE_OBJ) $(RUNTIME_LIB)
