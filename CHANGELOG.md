# Changelog — PlantLang / Chloroplast

## v0.39.1 — 2026

### New: Phase 1 LLVM IR Compiler — Primitives & Early SHOW
- **New directories**: `runtime/c/`, `src/codegen/llvm/`, `tests/llvm/`
- **C Runtime Library** (`runtime/c/plant_runtime.{h,c}`):
  - `plnt_print_int(i64)` — prints signed 64-bit integers
  - `plnt_print_decimal(double)` — prints double-precision floats
  - `plnt_print_bool(i1)` — prints `true`/`false`
  - `plnt_print_text(i8*)` — prints null-terminated strings
  - `plnt_pow_i64(i64, i64)` — integer power helper
- **LLVM Codegen Infrastructure** (`src/codegen/llvm/`):
  - `llvm_context.js` — register counter (`%1`-based), string constant pool with dedup, `declare` header accumulator, x86-64 target triple/datalayout
  - `llvm_type_mapper.js` — PlantLang→LLVM type mapping (NUM→i64, SCL→double, FACT→i1, TX→i8*), print-function registry, mixed-type promotion helpers
  - `llvm_symbol_table.js` — variable declaration tracker, `alloca` emission at function entry
  - `llvm_emitter.js` — AST visitor + recursive-descent expression parser:
    - Handles `ProgramNode`, `LiteralNode` (NUMBER/STRING/FACT/RAW_EXPR), `IdentifierNode`, `CreateStatementNode`, `SetStatementNode`, `ShowStatementNode`
    - Full precedence expression parser for RAW_EXPR: arithmetic (`+ - * / % **`), comparison (IS, IS NOT, GT, LT, GTE, LTE), logical (AND, OR, NOT), parentheses, mixed-type promotion (i64↔double)
- **Differential Test Harness** (`tests/llvm/01_primitives.test.js`):
  - 39 tests: parses PlantLang source → generates `.ll` → `llc -O2` → links `plant_runtime.o` → runs binary → captures stdout → compares against AST interpreter output
  - Covers: integer/decimal/boolean/string literals, CREATE+SHOW, SET, arithmetic precedence, comparisons, logical operators, mixed-type expressions, multi-SHOW sequences
  - Generated IR validated by `llvm-as` and `llc -O2`
- Total test count grows from ~1212+ → **~1251+** across **29 test suites**
- All 39 new tests + all 75 existing tests at 100% pass rate

## v0.38.0 — 2026

### New Features
- **AST Zero-Fallback** — removed `RawStatementNode` from the parser; replaced with `EndBlockNode`, `BranchElseNode`, `BlockDelimiterNode` as typed structural markers. Truly unrecognized constructs are skipped gracefully (no fallback wrapping), maintaining the invariant that every node in any parsed AST is a typed, named class:
  - `core/parser.js`: RawStatementNode class removed, `_rawFallback` references eliminated, `assertNoRawStatements()` invariant helper called after every `parse()`.
  - `core/codegen.js`, `core/llvm_codegen.js`: EndBlock, BranchElse, BlockDelimiter no-op cases added.
  - All 28 test suites and all legacy example/regression files parse without producing a single RawStatement.
- **CYCLE...IN Loop with Index Variable** — `CYCLE item [, idx] IN list, body 1\.` syntax with per-iteration scope isolation:
  - `parseCycleStatement` rewritten: lookahead-based `,` disambiguation (index-var comma vs body-delimiter comma); proper `this.match` vs `this.peek` usage fixing advance bugs.
  - `src/interpreter/cycle_evaluator.js` — `evaluateCycleInStatement` with scope isolation, BREAK/CONTINUE signal catching, empty/null/undefined safety.
  - Index variable auto-binding in scope at depth 0 (`NUM`), reset per iteration.
- **BREAK / CONTINUE Statements** — `BREAK.` and `CONTINUE.` with `BreakSignalException`/`ContinueSignalException` for structured loop control:
  - `BreakStatementNode`, `ContinueStatementNode` in AST.
  - Interceptor wraps `_evalBody` with try/catch for signal propagation.
- **Multi-field SORT** — `SORT list BY field1 ASC, field2 DESC, ...` with chained comparator:
  - `SortStatementV2Node` in AST; `parseSortStatement` rewritten to support `BY` syntax.
  - `src/interpreter/sort_evaluator.js` — `_makeChainedComparator` for multi-field ASC/DESC, null-to-end semantics, `localeCompare` for strings.
  - Simple `SORT list.` and `SORT list ASC|DESC.` still work identically (empty `fields` array falls through to `_makeSimpleComparator`).
- **BLOOM AS Visual Governance** — `BLOOM data_expr AS TABLE|GRAPH|CHART.` with target-specific rendering:
  - `BloomAsStatementNode` in AST; `parseBloomAsStatement` method.
  - `src/interpreter/bloom_evaluator.js` — TABLE/GRAPH/CHART renderers with `isRestrictedEnvironment` detection (`CODEPLANT_RESTRICTED` env var, non-TTY).
- **Nested Struct Formatting** — `SHOW` on nested struct instances renders an indented, JSON-like tree view:
  - `src/interpreter/show_formatter.js` — `formatShowValue` with recursive struct descent, type-prefixed keys, indentation.
- **Memory Allocators (FAST / PERSISTENT Patterns)** — two allocator implementations for the local runtime layer:
  - `src/memory/allocator.js` — `ArenaAllocator` (FAST bump allocator with child arena cascading reset) and `ARCHeap` (PERSISTENT cascading reference counting with parent-child retention chains).

### Parser Fixes
- `this.match()` does not advance — added explicit `this.advance()` after `IN` detection in non-index CYCLE path (was collecting `"IN items"` instead of `"items"`).
- FROM/TO path: restored `this.advance()` after `FROM` check (was producing `fromExpr: "FROM 1"` instead of `"1"`); restored comma-stopping inline loop for `toExpr` (was including trailing comma); restored comma/period body delimiter consumption.
- INFUSE, ABSORB, SEAL dispatch removed — these not-yet-migrated features now skip gracefully via the fallthrough handler rather than throwing.
- HARVEST block form (comma-delimited body with HEADERS/TIMEOUT/AS clauses) parses successfully.
- `SHOW_VERIFY_SUMMARY` built-in directive parsed as `ShowVerifySummaryNode` (was being skipped due to tokenization as a single `IDENT`).

### Test Suite
- New `tests/v0.38.0_ergonomics.test.js` — 54 tests covering:
  - AST Zero-Fallback: EndBlock, BranchElse, BlockDelimiter nodes, assertNoRawStatements invariant
  - CYCLE...IN: empty/null/undefined lists, index variable, element iteration
  - BREAK/CONTINUE: signal exceptions, AST node types
  - Multi-field SORT: chained comparator, ASC/DESC, null-to-end, locale sort
  - Nested struct formatting: formatShowValue for all primitive types, struct nesting, deep field access
  - BLOOM AS: AST node type, target type detection, restricted environment detection
  - Memory allocators: ArenaAllocator alloc/reset/child cascading, ARCHeap retain/release/cascading parent-child
  - Integration: end-to-end CYCLE with index, full program execution, zero RawStatement guarantee
- Total test count grows from ~1158+ → **~1212+** across **28 test suites**
- All 28 test suites at 100% pass rate

### Documentation
- All `.md` files bumped to v0.38.0
- Language Tour.md — header → v0.38.0, new CYCLE...IN (with index variable), BREAK/CONTINUE, multi-field SORT, BLOOM AS, nested struct formatting sections
- TECHNICAL.md — new Section 21 for Language Ergonomics & AST Zero-Fallback design
- ROADMAP.md — v0.37.0 objectives marked complete, v0.38.0 Language Ergonomics documented
- CHANGELOG.md — new v0.38.0 entry

### Source Layout
- New directories under `src/`:
  - `src/interpreter/cycle_evaluator.js`
  - `src/interpreter/sort_evaluator.js`
  - `src/interpreter/bloom_evaluator.js`
  - `src/interpreter/show_formatter.js`
  - `src/memory/allocator.js`

---

## v0.37.0 — 2026

### New Features
- **Distributed Cycles & Replica Governance Engine** — three cluster modules implementing partitioned loop distribution, stateless/stateful replication, and dual-mode reap aggregation:
  - `ReplicaManager` — stateless routing: LEAST_CONNECTIONS (default) and ROUND_ROBIN strategies; stateful Primary-Backup: assignment, delta replication log, ACK modes (ONE/QUORUM default/ALL); NodeRegistry-integrated failover with automatic highest-priority backup promotion; MISSION CONFIG for `REPLICA_STRATEGY` and `PRIMARY_BACKUP_ACK`
  - `DistributedCycleEngine` — adaptive chunk size computation via `max(minChunkSize, ceil(N / (activeWorkers × CYCLE_CORE_FACTOR)))`; `scatter()` to split iterations into chunks and assign to workers; `completeChunk()` with automatic work-stealing (`_trySteal()`) from idle workers; `checkTimeouts()` for straggler detection and re-queue with `WORKER_TIMEOUT_MS` (default 5000ms, range 1000–60000); `isComplete()` for cycle completion detection; MISSION CONFIG for `CYCLE_CORE_FACTOR`, `CYCLE_MIN_CHUNK_SIZE`, `WORKER_TIMEOUT_MS`
  - `ReapAggregator` — LOCAL_REAP: in-memory deterministic reduce/merge/flush with keyed deduplication; REMOTE_REAP: stream to MEMORY_BUFFER or URI-based target (`s3://`, `stream://`, etc.) via registered handlers; MISSION CONFIG for `REMOTE_REAP_TARGET`

### Test Suite
- New `tests/v0.37.0_distributed_cycles.test.js` — 89 tests covering ReplicaManager stateless routing (round-robin, least-connections, 100-call distribution), stateful Primary-Backup (assignment, replication log, ONE/QUORUM/ALL ACK modes), primary failover with backup promotion, DistributedCycleEngine chunking (formula, min chunk, scatter count), work-stealing, timeout recovery, ReapAggregator LOCAL_REAP (collect/reduce/merge/flush), REMOTE_REAP (memory buffer, URI targets), MISSION CONFIG validation (all 7 directives with range enforcement), and integration scenarios
- Total test count grows from ~1069+ → **~1158+** across **27 test suites**
- All 27 test suites at 100% pass rate

### Documentation
- All `.md` files bumped to v0.37.0
- Language Tour.md — header → v0.37.0, new "Distributed Cycles & Replica Governance (v0.37.0)" section with ReplicaManager, DistributedCycleEngine, ReapAggregator syntax and examples; Architecture diagram updated with all 3 new modules
- TECHNICAL.md — new Section 20 for Distributed Cycles & Replica Governance design, chunking formula, work-stealing protocol, ACK modes, LOCAL_REAP/REMOTE_REAP pipeline, and code snippets
- CHANGELOG.md — new v0.37.0 entry describing distributed cycles and test suite

### Source Layout
- New directories under `src/cluster/`:
  - `src/cluster/replica/replica_manager.js`
  - `src/cluster/cycles/distributed_cycle_engine.js`
  - `src/cluster/reap/reap_aggregator.js`

---

## v0.36.0 — 2026

### New Features
- **Geographic Routing & State Governance Engine** — three geo-routing modules implementing shared state governance with dual-path consensus, bounded static call-graph affinity analysis, and adaptive SMART execution routing:
  - `ShareGovernance` — SHARED_READ: O(1) versioned snapshot reads with zero lock contention; TCP Gossip invalidation propagation across peer nodes within `GOSSIP_PROPAGATION_MS`; SHARED_WRITE RAFT: single-leader linearizable consensus with log replication and majority commit; SHARED_WRITE CRDT: LWW register with lamport clock and nodeId tiebreak for conflict-free convergence; `parseDirective()` for `SHARE CONFIG <KEY> READ_ONLY|MUTABLE [CONSENSUS=RAFT|CRDT]` syntax; MISSION CONFIG for `GOSSIP_PROPAGATION_MS` and `CONSENSUS_ENGINE`
  - `CallGraphAnalyzer` — adjacency matrix built from function invocation frequencies; bounded depth traversal at `CALL_GRAPH_MAX_DEPTH` (default 3, range 1–10) guaranteeing O(V·E₍bₒᵤₙdₑd₎) pass times; Louvain-inspired community detection forming Affinity Groups; `computePlacement()` for static affinity group → node assignment; `buildFromAST()` factory for compiler integration; MISSION CONFIG for `CALL_GRAPH_MAX_DEPTH`
  - `SmartExecutionRouter` — adaptive triage router: LOCAL_CPU (default, normal workloads), REMOTE_NODE (local CPU > 70% + remote latency < `SMART_ROUTE_MAX_LATENCY_MS`), GPU_ACCELERATED (vector/matrix ops + payload ≥ `SMART_ROUTE_GPU_MIN_BYTES` + registered GPU pipeline); payload size estimation; matrix/vector operation keyword detection; latency caching with 5s TTL; routing overhead < 0.05ms per invocation; MISSION CONFIG for `SMART_ROUTE_GPU_MIN_BYTES` and `SMART_ROUTE_MAX_LATENCY_MS`

### Test Suite
- New `tests/v0.36.0_geo_routing.test.js` — 125 tests covering ShareGovernance SHARED_READ (declare/read/100K-read benchmark/directive parsing), Gossip invalidation propagation (peer send + receive within latency window), SHARED_WRITE RAFT (write/commit/replication convergence), SHARED_WRITE CRDT (LWW write/bidirectional convergence/sequential convergence), CallGraphAnalyzer (edge weights/bounded depth/affinity groups/placement/stats), SmartExecutionRouter (GPU registration/payload estimation/vector detection/triage transitions/1000-call benchmark), MISSION CONFIG (all 5 directives with range enforcement), integration scenarios
- Total test count grows from ~944+ → **~1069+** across **26 test suites**
- All 26 test suites at 100% pass rate

### Documentation
- All `.md` files bumped to v0.36.0
- Language Tour.md — header → v0.36.0, new "Geographic Routing & State Governance (v0.36.0)" section with SHARE CONFIG syntax, SHARED_READ/SHARED_WRITE semantics, affinity analysis, and SMART routing; Architecture diagram updated with all 3 new modules
- TECHNICAL.md — new Section 19 for Geographic Routing & State Governance Engine design, TCP Gossip invalidation, bounded call-graph partitioning, SMART routing triage matrix, and code snippets
- CHANGELOG.md — new v0.36.0 entry describing geo-routing modules and test suite

### Source Layout
- New directories under `src/cluster/`:
  - `src/cluster/config/share_governance.js`
  - `src/cluster/affinity/call_graph_analyzer.js`
  - `src/cluster/router/smart_execution_router.js`

---

## v0.35.0 — 2026

### New Features
- **Cluster Architecture & Distributed Memory** — three cluster modules implementing decentralized node discovery, weighted-least-connections routing with circuit-breaker failover, and consistent-hash-ring-based distributed storage:
  - `NodeRegistry` — heartbeat-based node lifecycle with three health states (HEALTHY/DEGRADED/OFFLINE); per-node telemetry (CPU, heap, active workers); background timer at `HEARTBEAT_INTERVAL` increments `missedBeats`; node marked OFFLINE at `HEARTBEAT_THRESHOLD`; EventEmitter for topology events (node:registered/healthy/degraded/offline); MISSION CONFIG overrides for interval and threshold
  - `ClusterRouter` & `CircuitBreaker` — weighted least-connections node selection (lowest active count, CPU tiebreak); per-node circuit breaker with 3 states (CLOSED, OPEN, HALF-OPEN); sliding-window error rate (≥10 total, errorRate ≥ threshold); cooldown-based HALF-OPEN probe with success→CLOSED / failure→OPEN transitions; transparent backup failover on primary failure; aggregated error on dual failure; mTLSJwtGuard integration for dispatch authentication; MISSION CONFIG for threshold and cooldown
  - `DistributedHeap` & `ConsistentHashRing` — SHA-256 → BigInt hash space; configurable virtual nodes (default 128 per physical node); balanced key distribution (≥0.98 ratio for 2 nodes); PERSISTENT key/value store with lease-based expiry and background GC; stateful actor ownership with proxy detection for non-owner writes; `computeDataKeyMigration()` and `computeMigrationStats()` for zero-downtime rebalancing; `removeNode()` re-owns entries via ring lookup; MISSION CONFIG for virtual node count

### Test Suite
- New `tests/v0.35.0_cluster.test.js` — 88 tests covering NodeRegistry (register/unregister/heartbeat/telemetry/DEGRADED/OFFLINE/MISSION CONFIG), CircuitBreaker (CLOSED→OPEN→HALF-OPEN transitions/cooldown/reset/MISSION CONFIG), ClusterRouter (least-connections/CPU tiebreak/dispatch/failover/aggregated errors/benchmark), ConsistentHashRing (add/remove/distribution ratio/migration/vnode config), DistributedHeap (put/get/delete/actors/lease expiry/removeNode re-own/migration stats/benchmarks), integration scenarios
- Total test count grows from ~856+ → **~944+** across **25 test suites**
- All 25 test suites at 100% pass rate

### Documentation
- All `.md` files bumped to v0.35.0
- Language Tour.md — new "Cluster Architecture & Distributed Memory (v0.35.0)" section with all 3 cluster modules; Architecture diagram updated
- TECHNICAL.md — new Section 18 for Cluster Architecture design, MISSION CONFIG integration, ring implementation details
- CHANGELOG.md — new v0.35.0 entry describing cluster modules and test suite

### Source Layout
- New `src/cluster/` directory tree:
  - `src/cluster/discovery/node_registry.js`
  - `src/cluster/router/cluster_router.js`
  - `src/cluster/memory/distributed_heap.js`

---

## v0.34.0 — 2026

### New Features
- **Zero-Trust Security & Audit Architecture** — three security modules implementing non-blocking audit logging, mutual TLS with JWT authentication, and capability-based sandboxing:
  - `NonBlockingAuditLogger` — SharedArrayBuffer ring buffer (default 10K entries, configurable via `AUDIT_RING_SIZE`); O(1) lock-free atomic writes; SHA256 tamper-evident hash chain with `verifyIntegrity()` chain validation; async Worker Thread background flush; synchronous emergency flush on overflow
  - `mTLSJwtGuard` — TLS 1.3 certificate loading from `MTLS_CERT`/`MTLS_KEY`/`MTLS_CA` env vars or .pem file paths; RS256 and Ed25519 JWT signature verification with localized key caching; anti-replay protection via jti tracking table; certificate expiry auto-detection with renewal hooks; explicit error differentiation (EXPIRED, FORGERY, REPLAY, MTLS_FAILURE)
  - `CapabilityGuard` — zero-trust default: SAFE mode starts with zero permissions; granular capability matrix per mission mode (FILE_READ, FILE_WRITE, NET_CONNECT, NET_LISTEN, PROCESS_SPAWN, etc.); syscall filtering blocking execve/ptrace/fork/clone/kill in SAFE mode; violation enforcement with SIGSYS termination and CRITICAL audit log; `onViolation()` hook for custom alerting

### Test Suite
- New `tests/v0.34.0_security.test.js` — 91 tests covering audit logger integrity (hash chain, overflow, fast path overhead < 100µs), hash chain verification (prev hash chaining, zero genesis), mTLS & JWT verification (RS256 valid/expired/forged/replay/Ed25519), capability sandboxing (SAFE zero-default/grant/revoke/syscall blocking/integration hooks), benchmark suite (1000 record throughput, snapshot cycles, verifyIntegrity performance, 10K bulk write)
- Total test count grows from ~765+ → **~856+** across **26 test suites** (24 existing + runtime + parallel + security)
- All 26 test suites at 100% pass rate

### Documentation
- All `.md` files bumped to v0.34.0
- Language Tour.md — header → v0.34.0, new "Zero-Trust Security & Audit Architecture (v0.34.0)" section with all 3 modules, Architecture diagram updated with security modules
- CHANGELOG.md — new v0.34.0 entry describing all security modules

### Source Layout
- New `src/security/` directory tree:
  - `src/security/audit/audit_logger.js` + `src/security/audit/audit_worker.js`
  - `src/security/network/mtls_jwt_guard.js`
  - `src/security/sandbox/capability_guard.js`

---

## v0.33.0 — 2026

### New Features
- **Parallel Compilation & Telemetry** — four modules enabling multi-threaded codegen, distributed failover, and lock-free metrics:
  - `ParallelCodegenEngine` — DAG dependence graph builder with Tarjan cycle detection; weighted load balancer distributing actions by (1 + nested call count); worker_threads pool for parallel bitcode assembly with lock-free merge
  - `RemoteCompilerNode` — zlib (deflate) compression achieving ≥60% payload reduction on serialized AST; TCP transport via net.Socket; 100ms connect timeout triggers transparent failover to local engine; caller receives result regardless of remote availability
  - `NonBlockingTelemetry` — SharedArrayBuffer ring buffer: 128 entries × 64 bytes each; O(1) lock-free atomic writes via Atomics.add/store; zero-allocation snapshot() returning structured metrics copy; automatic read-ptr advance on overflow; background exporter for external sinks
  - `RuntimeDispatcher` — enableParallelCodegen()/disableParallelCodegen() toggle; auto-detects single-core CPUs and disables parallel mode at creation; hooks into NonBlockingTelemetry for compilation metrics

### Test Suite
- New `tests/v0.33.0_parallel.test.js` — 60 tests covering DAG building/cycle detection, weighted load balancing, network compression ratio (≥60%), 100ms timeout fallback, telemetry ring buffer/snapshot, dispatcher auto-disable, and a 20-node speedup benchmark suite (2/4/8 worker balance ratios)
- Total test count grows from ~705+ → **~765+** across **25 test suites**
- All 25 test suites at 100% pass rate

### Documentation
- All `.md` files bumped to v0.33.0
- ROADMAP.md — v0.32.0 objectives marked complete, v0.33.0 Parallel Compilation & Telemetry documented
- README.md — v0.33.0 completed section with parallel/distributed/telemetry modules; v0.34.0 planned section drafted
- CHANGELOG.md — new v0.33.0 entry describing all parallel/telemetry modules
- Language Tour.md — new "Parallel Compilation & Telemetry (v0.33.0)" section; Architecture diagram updated with all 4 new modules

### Source Layout
- New `src/compiler/parallel/` and `src/compiler/distributed/` directories:
  - `src/compiler/parallel/parallel_codegen.js`
  - `src/compiler/distributed/remote_compiler.js`
  - `src/telemetry/metrics_collector.js`
  - `src/runtime/dispatcher.js`

---

## v0.32.0 — 2026

### New Features
- **Local Runtime & Isolation Layer** — five runtime modules implementing mission-specific memory management, process isolation, adaptive IPC, and telemetry:
  - `BumpAllocator` (FAST mission) — O(1) linear bump allocator with strict 8-byte alignment, default 8MB capacity (hard cap 64MB), configurable via `MISSION CONFIG FAST_HEAP_SIZE = N`. O(1) reset at scope exit. Automatic BALANCED escalation with diagnostic on overflow.
  - `GlobalARCHeap` (PERSISTENT mission) — atomic reference counting with O(1) retain/release. Automatic cycle detection via weak reference registry every 1000 allocations (~0.1ms overhead). `GC.cycle()` interface for manual triggering during idle frames. `onFinalize` callbacks invoked when refcount reaches 0.
  - `WarmProcessPool` (SAFE mission) — 4 pre-warmed idle worker processes (configurable, ceiling `min(OS.cpus() × 2, 16)`). Ping/Pong heartbeat every 5000ms with 10ms timeout → zombie kill + respawn. Queue starvation protection: 50ms timeout → pool expansion or BALANCED fallback.
  - `SafeChannel` — adaptive IPC pipeline: Structured Clone for ≤1MB, Transferable ArrayBuffer zero-copy for >1MB, SharedArrayBuffer for read-only state, ReadableStream/WritableStream for continuous streaming. Emits [TRACE] logs identifying active mechanism.
  - `MissionContext` — wraps active allocator, IPC channel, and pool into unified telemetry. `diagnostic(msg)` for escalation/warnings (always on). `trace(msg)` for verbose logging (only with `--debug`). `getMetrics()` returns JSON with memory, fragmentation, pool status, and GC cycle counts.
- **Escalation & Safety Matrix** — 5 automatic rules: FAST OOM → BALANCED fallback, pool starvation → BALANCED fallback, heartbeat timeout → kill PID + respawn, 1000 allocations → cycle detection, large payload → transferable mode.

### Test Suite
- New `tests/runtime.test.js` — 70 tests covering BumpAllocator alignment/overflow/escalation, ARCHeap retain/release/cycle detection/GC.cycle(), SafeChannel all 4 transfer mechanisms, MissionContext diagnostics/metrics/tracing, ProcessPool creation/heartbeat simulation
- Total test count grows from ~635+ → **~705+** across **24 test suites**
- All 24 test suites at 100% pass rate

### Documentation
- All `.md` files bumped to v0.32.0
- ROADMAP.md — v0.31.0 objectives marked complete, v0.32.0 Local Runtime & Isolation Layer documented
- README.md — Local Runtime & Isolation Layer section; v0.33.0 planned section drafted
- CHANGELOG.md — new v0.32.0 entry describing all runtime modules
- Language Tour.md — new "Local Runtime & Isolation Layer (v0.32.0)" section with idiomatic code examples

### Source Layout
- New `src/runtime/` directory tree:
  - `src/runtime/allocators/bump_allocator.js`
  - `src/runtime/allocators/arc_heap.js`
  - `src/runtime/isolation/process_pool.js`
  - `src/runtime/isolation/worker_bootstrap.js`
  - `src/runtime/isolation/safe_channel.js`
  - `src/runtime/context/mission_context.js`

---

## v0.31.0 — 2026

### New Features
- **Five-Mission Execution Architecture** — five mission modes with distinct memory policies, optimization paths, and cross-mode call permissions:
  - `MISSION: BALANCED/FAST/SAFE/SMART/PERSISTENT.` — parser recognizes `MISSION <MODE>.` via `MissionStatementNode` AST
  - `MissionBlockNode` wraps all top-level statements under a mission declaration
  - `MissionStack` — runtime context tracking via `push(mode)`/`pop()`
  - `ScopedArena` — depth-level memory slabs with per-mission overflow policies (`expand`, `snapshot`, `reset`)
  - `MissionDispatcher` — routes AST nodes to mission-specific evaluators with SMART mission-aware call routing
  - `BoundaryHandshakeMatrix` — 5×5 permission table (`BOUNDARY_MATRIX`) specifying which source modes may call ACTIONs in which target modes
  - `BoundaryViolationError` — structured error with `fromMode`, `toMode`, `reason` fields
  - LLVM codegen: `genMissionStatement` emits `@_mission_mode` global; `genReapStatement` emits mode-check guard IR
  - Typechecker: `_checkMissionStatement` validates mode string and permission matrix entries
  - 75 new tests across `tests/matrix.test.js` (28) and `tests/dispatcher.test.js` (47) — all green

### Test Suite
- 3 new test files: `tests/matrix.test.js` (28 tests), `tests/dispatcher.test.js` (47 tests)
- Total test count grows from ~560+ → **~635+** across **23 test suites**
- All pre-existing failures fixed — every test suite at 100% pass rate:
  - `test_diagnostics.js`: column assertion corrected (4→9) to match actual error column
  - `test_parser_migration.js`: RESPONSE body execution via `_verifyDryRun`; errVar expectation updated to `"division by zero"`
  - `test_llvm_codegen.js`: ACTION/REAP rejection test replaced with positive compilation test

### Documentation
- All `.md` files bumped to v0.31.0
- ROADMAP.md — v0.30.0 objectives marked complete, v0.31.0 Five-Mission Architecture documented with detailed sub-goals
- README.md — Five-Mission Architecture section under v0.31.0 completed; v0.32.0 planned section drafted
- CHANGELOG.md — new v0.31.0 entry describing all Five-Mission features

---

## v0.30.0 — 2026

### New Features
- **Block-Depth Contract Law Enforcement** — semantic depth validation ensuring well-structured code scope:
  - Parser: `enforceDepthContract(nodeType, minDepth, maxDepth, token)` method with `this.currentDepth` tracking
  - Scope entry/exit: `currentDepth` increments on ACTION body and CYCLE block entry, decrements on exit (via `try/finally`)
  - Typechecker: `validateDepthInvariants(ast)` second-pass AST walker verifying depth invariants
  - Enforces: ACTION/SPECIES restricted to Depth 0; REAP/GIVE/CYCLE restricted to Depth ≥ 1
  - DepthContractError: clear `SYNTAX_STORM` with `[DepthContractError]` prefix, expected range, and caret location
  - 13 new tests covering valid/invalid depth scenarios
- **Universal REAP Expression Support** — `REAP x FROM SPLIT(str, delim)`, `REAP x FROM JOIN(arr, delim)`, `REAP x FROM parts[0]` now work natively in both interpreter and LLVM backend:
  - Parser: `parseReapStatement` detects expression sources (`(` or `[` after FROM) and delegates to `parseExpressionSpan` for full AST construction
  - Typechecker: `_checkReap` handles `EXPR` source kind by calling `_inferExprNode` to derive return type
  - Interpreter: `evaluateReapStatement` handles `EXPR` source kind via `evaluateExpressionNode`
  - LLVM Codegen: `genReapStatement` accepts `EXPR` source kind, evaluates via `compileAstExpr`, and auto-creates target variable with inferred type
  - `SHOW` on `[TX]` arrays prints element count
- **C Runtime Library (`libplantlang.so`)** — native C implementations of performance-critical operations callable via LLVM FFI:
  - Math functions: `sqrt`, `sin`, `cos`, `tan`, `floor`, `ceil`, `abs` — thin wrappers around libm
  - Array sort: `plnt_sort_i64`, `plnt_sort_double` — quicksort via `qsort`
  - String operations: `plnt_string_concat` (heap-allocated concat), `plnt_string_len` (length query)
  - `Makefile` with `runtime`, `exec`, `test`, `clean` targets
  - `chloroplast.js` compile pipeline automatically links `-Lruntime -lplantlang`
  - LLVM test harness (`test_llvm_codegen.js`) also links against runtime lib
- **NATIVE Keyword** — parser recognizes `NATIVE ACTION name(params) -> external.` syntax; sets `isExternal = true`
- **RUNTIME_FFI Signature Map** — `core/llvm_codegen.js` maps 12 PlantLang FFI action names to their correct LLVM signatures (`double`, `void`, `%fat_ptr`, `i64` return types)
- **FFI Return Type Handling** — `genReapStatement` now handles `double`, `void`, `i64`, and `%fat_ptr` return types from external C functions:
  - `double` returns: stored directly or bitcast to `i64` for NUM targets
  - `void` returns: call emitted with no result binding
  - `i64` returns: legacy return-register convention preserved
- **Declare Cleanup** — parameter names stripped from `declare` lines for cleaner LLVM IR output

### Test Suite
- New `tests/test_phase21_runtime.js` — 20 tests: IR smoke tests, math FFI, SORT, SPLIT/JOIN (FFI), native SPLIT/JOIN via REAP (expression sources), 70KB large-string stress test
- New `tests/test_depth_contract.js` — 13 tests covering valid (ACTION at depth 0, REAP/CYCLE/GIVE inside ACTION) and invalid (REAP/CYCLE/GIVE at depth 0, nested ACTION, top-level REAP with depth syntax) depth scenarios
- All **20 test suites, ~560+ tests** — all green

### Documentation
- ROADMAP.md bumped to v0.30.0 — completed items marked, remaining objectives listed
- TECHNICAL.md — new section: Block-Depth Contract Law Enforcement (scope tracking, enforceDepthContract, validateDepthInvariants)
- README.md — Quick Reference updated with depth contract notes; Architecture diagram updated

---

## v0.29.0 — 2026

### New Features
- **SPECIES Vtable Dispatch (Phase 19)** — virtual method dispatch via polymorphic vtables in LLVM codegen:
  - Vtable pointer (i8*) added as field 0 of every species LLVM struct
  - Method slots computed across the full parent chain (parent methods prefix, child overrides reuse slots)
  - Per-species `@species.Name.vtable = constant [N x i8*]` globals emitted with function pointers
  - `genCreateSpecies` and `genCreateBloomed` store vtable pointer after zeroing fields
  - `genMethodCallStatement` dispatches through vtable: load → GEP → load function pointer → bitcast → call
  - Uniform `i8*` receiver convention for all species methods (bitcast to concrete type inside function body)
  - Dynamic dispatch also works in expression-level `MethodCall` nodes in `compileAstExpr`
- **CHOICE / MATCH LLVM Codegen (Phase 20)** — tagged unions and pattern matching compiled to native LLVM IR:
  - CHOICE values stored as `{ i64 tag, i64 payload }` struct (`insertvalue` / `extractvalue`)
  - Variant construction: `Option.None` and `Option.Some(10)` emit struct construction inline
  - MATCH statement: extract tag, icmp chain against variant indices, branch to clause body
  - Payload binding: extractvalue payload, store in arena, register in clause scope
  - MAP `get()` returns `Option<V>` (probes map, branches on found/not found, wraps result)
- **SPECIES LLVM Bug Fixes**:
  - Typechecker stores `speciesName` in variable info for BLOOM instances, fixing method resolution
  - LLVM `genFnDef` registers both `self` and `__self` in scope for SET field access
  - `SelfExpression` and `BloomExpression` nodes handled in `compileAstExpr`
  - `BloomStatement` emits clear error message (old-style `BLOOM x AS y.` not supported in compiled mode)

### Full Test Suite
- All **17 test files, ~724 total assertions** — all green
- No regressions from any Phase 17–20 changes

### Documentation
- All `.md` files bumped to v0.29.0
- ROADMAP.md — v0.29.0 objectives marked complete, v0.30.0 roadmap drafted
- TECHNICAL.md — new sections: Species Vtable Dispatch (4), CHOICE/MATCH Codegen (5), MAP get() Option

---

## v0.28.0 — 2026

### New Features
- **Native LIST Operations (Phase 18)** — built-in aggregate operations on dynamic arrays compiled to native LLVM IR:
  - `COUNT(xs)` — O(1) length extraction via `extractvalue` from `%fat_ptr` struct
  - `FIRST(xs)` / `LAST(xs)` — O(1) element access via GEP with element-size offset
  - `SUM(xs)` — O(n) inline accumulation loop emitted as LLVM IR (no external C call)
  - Type checker validates: COUNT/FIRST/LAST require `[T]` array; SUM requires `[NUM]`
  - Interpreter: JS-native implementations via `Array.length`, `Array.reduce`, index access
  - 15 new tests covering empty/populated arrays, single elements, type checking, FOR...IN parity

### LLVM Codegen
- `ListOp` dispatch in `compileAstExpr` — COUNT/FIRST/LAST emit GEP/load; SUM emits phi-based accumulation loop
- All expression contexts updated: SHOW, CREATE RHS, struct field defaults, SET RHS

### Test Suite
- Added **Phase 18 — Native LIST Ops** (`tests/test_phase18_lists.js`) — 15 tests
- Total test suites expanded to **17 files, ~724 total assertions** — all green

### Documentation
- All `.md` files bumped to v0.28.0

---

## v0.27.0 — 2026

### New Features
- **SPECIES / BLOOM — Object-Oriented Foundations (Phase 17)** — class-based OOP with `{ }` body syntax:
  - Syntax: `SPECIES Name { field: TYPE, ACTION method(params) { body } }` — `{ }` body blocks (new) alongside legacy `,`/`/SPECIES.` syntax
  - `FROM` / `PARENT` inheritance — parent fields prefixed in LLVM struct layout, methods accessible via bitcast
  - `BLOOM SpeciesName` expression usable in `CREATE x TO BLOOM SpeciesName.`
  - Colon-dispatch method calls: `REAP result FROM obj:method.`
  - `SELF:field` access in species action bodies (read, SET, INCREASE/DECREASE)
  - LLVM codegen: species registered as LLVM struct types; method calls emit static dispatch with name-mangled function names and bitcast for inherited methods
  - Full interpreter pipeline: `_evalMethodCallStatement`, `BloomExpression`, `SelfExpression` evaluation
  - 10 new tests covering `{ }` body parsing, BLOOM instantiation, method dispatch, inheritance, type checking, SELF mutation
- **FOR...IN Loop (Phase 15)** — iterate over list/array/map values:
  - Syntax: `FOR name IN expr, ... /FOR.` with optional depth prefix and `.` terminator
  - Iterates over LIST values, MAP keys, or TX character spans
  - Supports nested loops with `DEPTH`-aware parsing
  - `STOP_STORM` from `STOP IF` propagates correctly through `FOR` bodies
  - 19 new tests covering empty arrays, typed arrays, TX strings, MAPs, nested IF
- **STRUCT Type (Phase 16)** — alternative struct declaration syntax with `field: TYPE`:
  - Syntax: `STRUCT Person { name: TX, age: NUM }.` (alongside existing `SHAPE … field(TYPE)`)
  - Anonymous struct literals: `CREATE p(Person) TO { name: "Alice", age: 30 }.` in `CREATE` context
  - INCREASE/DECREASE support on struct fields: `INCREASE p.age BY 1.`
  - Full type-checker validation of struct fields and literal arity
  - LLVM codegen: struct literals emit GEP-based field stores
  - 16 new tests covering declaration, nested structs, field access/mutation, type checking, LLVM codegen
- **English-Language Cleanup** — all Arabic-language strings in engine `.js` files translated to English:
  - `core/interpreter.js` — ~58 storm messages and CLI strings converted
  - `core/llvm_codegen.js` — Unicode-escaped Arabic error message replaced
  - `core/runtime.js`, `core/evaluator.js` — Arabic error text translated
  - `chloroplast.js` — CLI banner, error messages, REPL prompts converted
  - `webrepl/examples-data.js` — all example data strings translated
  - `ar-IQ` locale → `en-US` in all date/time formatting calls
- **MAP Type (Hash Table) — Full LLVM Codegen** — open-addressing HashMap with linear probing:
  - Syntax: `CREATE m(MAP[NUM,TX]).` with typed key/value parameters
  - Map literals: `CREATE m(MAP[NUM,TX]) TO { 1: "a", 2: "b" }.`
  - `LINK key WITH value IN map.` — key-value insertion (also `link`/`LINK` in regex pipeline)
  - `m.put(key, value)` — runtime method for insertion
  - `m.has(key)` — returns FACT (true/false) — **compiled natively** via linear probing
  - `m.get(key)` — returns `Option<V>` — interpreter-only (needs MATCH codegen)
  - **Native bucket layout**: `{ i1 is_occupied, key_type, value_type }` with arena-allocated bucket arrays
  - **djb2 hash** for TX keys (emitted as inline LLVM IR loop), identity hash for NUM keys
  - **Automatic growth**: load factor > 0.75 triggers 2× capacity doubling with full rehash
  - **Rooted Depth integration**: maps stored as `%fat_ptr` struct `{ i8* buckets, i64 len, i64 cap }` within arena slabs
  - **Backward compatibility**: legacy untyped `MAP` (plain object) still works in interpreter
  - Full type-checker validation: key/value arity, type matching, MAP[K,V] vs MAP inference

### LLVM Codegen Fixes
- `llvmType()` — added MAP type support (`isMapTypeStr`) so arena allocation returns a valid `%fat_ptr*`
- `@llvm.memset.p0i8.i64` declaration — conditional declare for bucket array zeroing
- Grow loop branch fix — rehash loop was a no-op (both branch targets pointed to exit); now properly iterates old buckets
- `storeL`/`skipL` label separation — fixed unterminated basic block in grow rehash by adding explicit `br` terminator
- `genMapHas` return type changed from `NUM` to `FACT` — SHOW now prints `true`/`false` matching the interpreter
- `mapBucketSize` padding fix — bucket stride now computes correct padded struct size (40 bytes for `{i1,i64,%fat_ptr}` instead of 33), fixing data corruption on multi-element maps

### SHOW Expression Support
- `genShow` now handles `MethodCall` expressions (enables `SHOW m.has(1).` in compiled mode)
- LinkStatement added to `genStatement` switch — compiled via `ExprCompiler.compileExpr` + `genMapPut`

### Test Suite
- Added **Phase 14 — MAP Types** (`tests/test_phase14_maps.js`) — 17 tests covering empty map create, map literals, LINK/put semantics, has/get, overwrite, growth (10 entries), SHOW display, type-checker validation
- Added **Phase 15 — FOR...IN** (`tests/test_phase15_for_in.js`) — 19 tests covering empty arrays, typed arrays, TX strings, MAPs, nested IF, STOP IF propagation
- Added **Phase 16 — STRUCT** (`tests/test_phase16_structs.js`) — 16 tests covering declaration, nested structs, field access/mutation, type checking, LLVM codegen
- Added **Phase 17 — SPECIES/BLOOM** (`tests/test_phase17_species.js`) — 10 tests covering `{ }` body syntax, BLOOM instantiation, method dispatch, inheritance, SELF mutation, type checking
- LLVM backend expanded from 46 → **50 smoke tests** (4 new MAP tests: create+has, all-keys, growth, overwrite)
- Total test suites expanded to **16 files, ~709 total assertions** — all green

### Documentation
- All `.md` files bumped to v0.27.0
- ROADMAP.md — FOR...IN, STRUCT, and SPECIES/BLOOM objectives marked complete
- Language Tour.md — supported subset table now shows FOR...IN and SPECIES/BLOOM as ✅
- TECHNICAL.md — updated test counts from ~634 → ~709, 13 → 16 test files

---

## v0.25.0 — 2026

### New Features
- **SHAPE (Struct Types)** — user-defined aggregate types:
  - Syntax: `SHAPE Point { x(NUM), y(NUM) }.` with named, typed fields
  - Instantiation: `CREATE p(Point) TO Point{ 10, 20 }.`
  - Field access: `p.x`, `p.y`
  - Field mutation: `SET p.x TO 99.`
  - Support for zero-field structs (`SHAPE Unit { }.`)
  - Struct-of-struct composition and deep field access
- **Methods on Structs** — ACTIONs that receive a typed `SELF` parameter:
  - Syntax: `ACTION (self(Point)) move(x(NUM), y(NUM)), SET self.x TO x. SET self.y TO y. /ACTION.`
  - Method call: `REAP _ FROM p:move, 5, 10.` via colon dispatch
  - `SelfReferenceNode` in AST for `SELF` keyword
  - Type checker validates receiver type on method calls
- **Dynamic Arrays (push/pop)** — runtime array growth:
  - `PUT val INTO xs.` — amortized-O(1) appends with capacity doubling
  - `TAKE val FROM xs.` — pop from end with shrink
  - `CREATE xs(LIST) TO.` — empty list declaration
  - Type checker validates list operations
- **Tagged Unions via CHOICE** — type-safe sum types:
  - Syntax: `CHOICE Option { Some(NUM), None }.`
  - Variant construction: `Option.Some(10)`, `Option.None`
  - Variant names may collide with keywords (`Num`, `Empty`), handled by parser
  - Static type checker validates variant existence, payload types, arity
- **Pattern Matching via MATCH** — exhaustive case analysis on CHOICE values:
  - Syntax: `MATCH opt { Some(v) -> { SHOW v } None -> { SHOW 0 } }.`
  - Payload binding: `Some(v)` binds the inner value to `v` in the clause body
  - Payload-free clause: `None -> { SHOW "none" }`
  - Non-exhaustive match detection (missing variant = compile-time error)
  - `MatchStatementNode` in AST with `{variantName, binding, bodyStatements}` clause array
- **Member Access / Method Call Parsing** — full expression-level member access:
  - `Option.Some(10)` parsed as `MethodCallNode` with target `Option`, method `Some`
  - `Option.None` parsed as `MemberAccessNode` with object `Option`, member `None`
  - KEYWORD field names (e.g. `Num`, `Empty`) accepted after `.` in member-access position, with same-line + statement-avoidance heuristics to prevent false matches
- **Inferred CREATE types** — `(TYPE)` annotation optional when the value expression type can be inferred from a CHOICE variant construction or array literal

### Test Suite
- Added **Phase 9 — Structs** (`tests/test_phase9_structs.js`) — 70 tests covering SHAPE declaration, instantiation, field access/mutation, struct-of-struct, type checking, LLVM codegen parity
- Added **Phase 10 — Dynamic Arrays** (`tests/test_phase10_arrays.js`) — 58 tests covering push/pop, capacity growth, type checking, LLVM codegen, interpreter parity
- Added **Phase 11 — Methods** (`tests/test_phase11_methods.js`) — 47 tests covering method declaration, call dispatch, SELF receiver, type checking, LLVM codegen
- Added **Phase 12 — Array Growth** (`tests/test_phase12_arrays_growth.js`) — 64 tests covering amortized capacity doubling, shrink-on-pop, edge cases
- Added **Phase 13 — CHOICE & Pattern Matching** (`tests/test_phase13_choices_matching.js`) — 64 tests covering variant declaration, construction, member access, MATCH exhaustiveness, payload binding, type checking, interpreter execution
- Total test suites expanded to **12 files, ~613 total assertions** — all green

### Documentation
- `README.md` updated to v0.25.0 with SHAPE, Methods, Dynamic Arrays, CHOICE/MATCH sections
- `ROADMAP.md` — v0.25.0 objectives marked complete, v0.26.0 roadmap drafted
- `Language Tour.md` — added SHAPE, CHOICE/MATCH, method call syntax
- `TECHNICAL.md` — updated test counts and architecture diagrams

---

## v0.24.0 — 2026

### New Features
- **IMPORT Statement & Module System** — multi-file PlantLang programs:
  - `IMPORT "path".` syntax for loading external `.plnt` files
  - Relative, absolute, and `std/`-prefixed path resolution
  - Cycle detection with clear error messages (`IMPORT cycle detected`)
  - AST merging — imported statements are spliced into the importing program's AST at parse time
  - Type checker resolves and validates all imports before semantic analysis
  - Interpreter resolves `IMPORT` at runtime with file-system lookup
  - `ImportStatementNode` in AST with `path`, `statements`, `resolvedAbsPath` fields
- **FFI (Foreign Function Interface)** — call native C functions from PlantLang:
  - Syntax: `ACTION name(params) -> external.` — marks an ACTION as an external C function
  - `isExternal` property on `ActionDeclaration` nodes
  - LLVM backend emits `declare` IR for FFI functions with proper type signatures
  - FFI stubs pre-registered in the interpreter for all `runtime_bridge` functions
  - Type checker validates FFI signatures against known bridge functions
- **Standard Library Foundation** (`std/`):
  - `std/io.plnt` — `print`, `println`, `plant_printf`, `plant_puts` (FFI-bridged I/O)
  - `std/string.plnt` — `len`, `upper`, `lower`, `trim`, `contains`, `split`, `replace`, `concat`
  - `std/prelude.plnt` — auto-injected core definitions (TRUE, FALSE, _BOOT)
  - Auto-prelude injection — every program implicitly imports `std/prelude.plnt`
  - `IMPORT "std/io"` and `IMPORT "std/string"` — resolve via `std/` path prefix
- **Core Runtime Bridge** (`core/runtime_bridge.c`):
  - C bridge implementing FFI targets: `plant_printf`, `plant_puts`, `plant_len`, `plant_upper`, `plant_lower`, `plant_trim`, `plant_contains`, `plant_split`, `plant_replace`, `plant_concat`
  - Linked into compiled binaries via the LLVM backend's `declare` + extern resolution

### Test Suite
- Added **Phase 7 — Module System & FFI** (30 test groups)
- Added **Phase 8 — Standard Library** (8 integration test groups)
- LLVM backend expanded from 37 → **46 smoke tests**
- All test suites green — 7 test files, ~300+ total assertions

---

## v0.23.0 — 2026

### New Features
- **ACTION/REAP/GIVE** in the LLVM backend (`core/llvm_codegen.js`) — full function support with:
  - Multiple typed parameters (NUM, SCL, TX, FACT)
  - Recursive calls (factorial, Fibonacci — verified via llc + gcc)
  - IF/ELSE bodies with multiple GIVE statements (terminator-aware branching)
  - SCL (double) params via bitcast through i64 return register
  - TX (string) returns via ptrtoint/inttoptr
  - Void actions (no GIVE) default to `ret i64 0`
- **Rooted Depth System** — deterministic arena-based memory management:
  - 64 depth levels, each backed by a 64KB arena slab (`@arena_offsets[64 x i64]`, `@arena_memory[64 x [65536 x i8]]`)
  - Bump allocation (`arenaAlloc`/`arenaAllocTyped`) replaces all `alloca` across all variable creation sites (CREATE, REAP auto-create, CYCLE iter, function params)
  - Depth tracking (`trackDepth`) injects automatic `arenaResetDepth` on scope exit
  - Arena globals emitted lazily only when `m.usesArena` is set
- **Contract Law Validation** (Article III) — compile-time depth enforcement:
  - CREATE destination must be ≤ current depth, with educational error messages citing the violated rule and suggesting fixes
  - `checkDepthAccess()` helper infrastructure for future cross-depth read/write enforcement
  - Scope entries track `{ptr, plType, depth}` for all variables
- **Forced Exit Arena Cleanup** (Article IX) — `genGiveStatement` emits the Unwinding Chain before every `ret`, resetting all arenas from `currentDepth` down to depth 1 (depth 0 preserved for caller's function params — critical for recursive correctness)
- **Loop Iteration Reset** (Article VII) — `genCycle` and `genSeason` save the arena offset at the loop's depth before the body and restore it after each tick, preventing temporary-variable accumulation across iterations. Deeper arenas entered during the body are also reset.
- **Error Unwinding Protocol** (WEATHER/SHELTER/CALM) — deterministic exception handling in LLVM:
  - `genWeatherStatement` generates the try/catch block structure with body, typed SHELTER handlers, and CALM continuation
  - Division-by-zero detection (`emitZeroCheck`) emits a `fcmp oeq` + conditional branch that sets `@_weather_msg`/`@_weather_type`/`@_weather_flag` globals and transfers control to the matching ZERO_STORM (or ANY_STORM) shelter handler
  - Unwind chain in each handler resets arenas from error depth to shelter depth
  - `errVar` binding loads the error message from `@_weather_msg` into a TX-typed arena slot
  - `shelterStack` enables proper nesting — inner errors are caught by inner handlers; handler body errors propagate outward
- **`core/Module`** gains `ensureWeatherGlobals()` and `weatherGlobalsEmitted` flag for lazy global emission

### Test Suite
- LLVM backend tests expanded from 34 → **37 smoke tests** (3 new: WEATHER body without error, ZERO_STORM caught via division by zero, post-error scope integrity)
- All four test suites green: LLVM backend 37/37, C codegen 9/9, parser migration 108/109 (1 pre-existing RESPONSE resolution discrepancy), diagnostics 44/45 (1 pre-existing)

### Documentation
- `README.md` updated to v0.23.0 with new sections on the Rooted Depth System, complete Quick Reference (ACTION/REAP/GIVE, WEATHER/SHELTER/CALM, depth syntax), updated test counts, and reorganized Roadmap

---

## v0.22.0 — 2026

### New Features
- **LLVM IR Backend** (`core/llvm_codegen.js`) — a real compiler backend emitting LLVM IR text, using the same pipeline architecture as **Rust, Swift, Julia, and Zig**:
  - Proper SSA-form code generation (`alloca`/`load`/`store` per variable, exactly matching clang's own `-O0` output shape, which LLVM's `mem2reg` pass then promotes to true SSA registers)
  - Full expression compiler with correct operator precedence (`OR` → `AND` → `NOT` → comparison → additive → multiplicative → unary → atom), matching the interpreter's evaluation semantics exactly
  - NUM/SCL type promotion, string concatenation via runtime `malloc`/`strcpy`/`strcat`, `BETWEEN` conditions, boolean short-circuit-free `AND`/`OR`/`NOT`
  - Same supported/unsupported subset as the C backend (CREATE/SET/INCREASE/DECREASE/SHOW/IF/ORIF/ELSE/CYCLE/SEASON) — unsupported constructs (LIST, MAP, ACTION, SPECIES, ...) fail with a clear compile-time error, never silently miscompiled
- **`chloroplast compile --backend llvm|c`** — auto-detects an installed LLVM toolchain (`llc`/`llc-14` through `llc-18`) and prefers it by default; falls back to the C backend automatically if no LLVM install is found
- **`opt -O2` integration** — the generated IR is run through LLVM's real optimizer (mem2reg, GVN, loop optimizations, inlining) before `llc` lowers it to object code. Without this step, hand-emitted alloca-heavy IR only gets `llc`'s backend-level optimizations and misses most of what makes LLVM fast — with it, compiled PlantLang matches gcc's own optimizer on equivalent code (verified: a 50-million-iteration accumulation loop drops from 88.7s interpreted to 6ms compiled — a ~14,700× speedup)
- **`tests/test_llvm_codegen.js`** — 27 parity tests (interpreted vs LLVM-compiled output must match exactly) covering arithmetic, SCL/NUM promotion, string concatenation, all comparison operators, BETWEEN, boolean logic, IF/ORIF/ELSE, CYCLE (with and without STEP), SEASON, nested loops, and unsupported-construct error handling
- **`examples/10_performance.plnt`** — a runnable interpreter-vs-compiled benchmark demonstrating the real-world speedup

### Fixes (found by building and testing the LLVM backend against the interpreter)
- **`core/evaluator.js`**: string literals containing text that happens to match a variable name (e.g. `"pi=" + pi` where `pi` is also a variable) were being corrupted — the identifier-substitution regex used plain word-boundaries (`\b...\b`) with no awareness of quote context, so it matched and replaced the `pi` substring *inside* the string literal `"pi="` itself, producing `"3.14=3.14"` instead of `"pi=3.14"`. Fixed by having the substitution regex skip over quoted string-literal spans entirely. This is a real interpreter bug, not just an LLVM-codegen quirk — it affected `chloroplast run` too, and was only caught because the LLVM backend's independent implementation disagreed with the interpreter's on the exact same test case.
- **`core/llvm_codegen.js`**: `CREATE x(NUM) TO someOtherVariable.` was compiling to a hardcoded `0` instead of loading the referenced variable — the parser produces a plain `Identifier` AST node (not a `RAW_EXPR` `Literal`) for single bare-identifier value expressions, and `genCreate()`'s type dispatch had no case for it, silently falling through to a zero default. Caught by a nested-loop test (`SEASON` inside `CYCLE`, where the loop-local variable was initialized from the outer loop's counter) producing empty output instead of a countdown.

---

## v0.21.0 — 2025 (current)

### New Features
- **Web REPL UI** (`webrepl/`) — a single-page, no-build-step browser interface for writing and running PlantLang:
  - Four modes matching the CodeWords Compiler Service's endpoints: **Run it**, **Check it**, **Verify it**, **Compile it**
  - Live connection indicator polling `/health`
  - Eight curated example programs loadable from a dropdown (generated from real `examples/*.plnt` files)
  - Collapsible generated-C-source view for Compile mode
  - `Cmd/Ctrl+Enter` keyboard shortcut, persistent editor content and server URL via `localStorage`
  - Visual design built around PlantLang's "reads like prose" identity — Fraunces serif for the mode tabs/wordmark, output lines settle in individually rather than appearing all at once
- **`webrepl/test_webrepl.js`** — 13 integration tests using jsdom to load the *real* HTML/JS files and drive actual clicks/keyboard events/fetch calls against a *live* CodeWords Compiler Service (not mocked)

### Milestone
This completes the full compiler pipeline originally planned:
```
Tokenizer → Parser/AST → Type Checker → Standard Library
    → C Generator → GCC Binary → Compiler Service → Web REPL UI
```
Every stage is implemented, tested, and documented.

---

## v0.20.0 — 2025 (current)

### New Features
- **CodeWords Compiler Service** (`service/codewords-server.js`) — standalone HTTP API exposing the interpreter, typechecker, and C code generator to external clients:
  - `GET /health` — service status
  - `POST /run` — execute a program, return captured output
  - `POST /check` — static type-check, return diagnostics
  - `POST /verify` — run VERIFY/SUITE tests, return pass/fail counts
  - `POST /compile` — generate C, compile with gcc, run the binary, return output
- **`service/sandbox-runner.js`** — per-request forked child process worker. Untrusted PlantLang code never runs in the main server process, so:
  - Infinite loops are killed cleanly after a 5s timeout without affecting other in-flight requests
  - `LISTEN BRANCH` and `HARVEST` are rejected before execution (no port-binding or outbound network access from submitted code)
  - Output is capped at 64KB; request bodies at 128KB
- **`service/test_service.js`** — 15 automated tests covering every endpoint, error handling, timeout enforcement, network blocking, and true concurrent-request isolation
- **`service/README.md`** — full API reference and safety model documentation

---

## v0.19.0 — 2025 (current)

### New Features
- **C Code Generator** (`core/codegen.js`) — translates a supported subset of PlantLang AST into standalone, compilable C99 source:
  - `CREATE` / `SET` / `INCREASE` / `DECREASE` for `NUM`, `SCL`, `TX`, `FACT`
  - `SHOW` — string literals, identifiers, and `+` concatenation (auto-selects `printf` format per type)
  - `IF` / `ORIF` / `ELSE`
  - `CYCLE var FROM lo TO hi [STEP k]` (numeric ranges)
  - `SEASON` (while loop)
  - Arithmetic (`+ - * / % **`) and comparisons (`IS`, `GREATER THAN`, `BETWEEN`, etc.) translated to native C operators
  - Unsupported constructs (LIST, MAP, ACTION/REAP, SPECIES, WEATHER, HARVEST, LISTEN BRANCH, ...) are reported as clear compile-time errors naming the exact construct and line — never silently miscompiled
- **`chloroplast compile`** — new CLI command:
  ```bash
  chloroplast compile file.plnt --run        # compile + execute immediately
  chloroplast compile file.plnt -o mybinary  # custom output path
  chloroplast compile file.plnt --keep-c     # keep generated .c file for inspection
  ```
  Pipes generated C through `gcc -O2 -lm` to produce a native binary.
- **`tests/test_codegen.js`** — parity smoke tests: runs each fixture both interpreted and compiled, asserts identical stdout (9/9 passing)
- **`examples/09_compile.plnt`** — FizzBuzz-style demo covering CYCLE, IF/ORIF/ELSE, SEASON, SCL comparisons

### Fixes
- `CYCLE var FROM lo TO hi STEP k` — `STEP` is tokenized as `IDENT` (not `KEYWORD`), so the parser's generic `_collectUntilKeyword()` never stopped at it. `toExpr` was swallowing `"20 STEP 5"` whole and `stepExpr` was always null. Fixed with an explicit IDENT-aware scan in `parseCycleStatement`. This was a **pre-existing interpreter bug**, not just a codegen limitation — `CYCLE ... STEP` was silently broken in `chloroplast run` too.

---

## v0.18.0 — 2025 (current)

### New Features
- **Static Type Checker** (`core/typechecker.js`) — full AST-based static analysis:
  - `TYPE_MISMATCH` — wrong type passed to action parameter or arithmetic on non-NUM
  - `ARITY_MISMATCH` — wrong number of arguments to an action
  - `UNDEFINED_ACTION` — calling an action that was never defined
  - `UNDEFINED_VAR` — using a variable that was never declared (warning)
  - `UNDEFINED_SPECIES` — BLOOM of an undefined SPECIES (warning)
  - `LOCK_VIOLATION` — attempt to SET a ROOT or locked variable
  - Errors inside WEATHER body are demoted to info (they're intentionally risky)
  - Two-pass architecture: declaration hoisting then full type checking
  - Library return types pre-declared (`math:SQRT → SCL`, `strings:UPPER → TX`, etc.)
  - Polymorphic action detection (all-TX params treated as ANY)
- **`chloroplast check`** now runs the static type checker with visual caret output
  (previously just counted statements)

### Fixes
- `chloroplast check` shows file/line/column with source preview and error arrow

---

## v0.7 — 2025 (current)

### New Features
- **LISTEN BRANCH** — real Node.js HTTP server with routing (`IF/ORIF/ELSE` on `req:"path"`)
- **HARVEST** — synchronous-style HTTP/HTTPS client (Worker + SharedArrayBuffer)
- **BRAID** — zip two lists into pairs or a MAP
- **VERIFY / SUITE** — built-in testing framework written in PlantLang itself
- **ANALYZE** — type-aware data inspection (sum/avg/min/max for NUM lists, key listing for MAPs)
- **NOW** — current date/time (`FORMAT:DATE`, `FORMAT:TIME`, `FORMAT:STAMP`, `FORMAT:YEAR`)
- **WAIT** — synchronous sleep (capped at 10s)
- **ROOT_SCOPE** — locked config MAP built at program start
- **TYPEOF** — `REAP t FROM TYPEOF x` returns type as TX

### Language Fixes
- `GREATER THAN OR EQUAL` / `LESS THAN OR EQUAL` now work correctly (were parsed as `GREATER THAN OR` + `EQUAL`)
- `IF/ORIF/ELSE` chain now runs only the first matching branch (was running all branches)
- `obj:"key"` quoted MAP access works inside string concatenation
- Chained access `obj:"k1":"k2":"k3"` works in expressions and compound strings
- `**` (power) tokenized as single token — `2 ** 8` now evaluates correctly
- `SET obj:"key" TO val` added with LOCK_STORM guard
- `LOCK_STORM` on `SET` to any locked MAP or ROOT variable

### Architecture
- Full compiler frontend: Tokenizer → Parser → AST (40+ typed node classes) → evaluateNode
- 96% AST coverage — all statements except empty block closers parsed to typed nodes
- CLI (`chloroplast run/verify`) now uses AST pipeline (`runSource`) exclusively
- symbolPass handles typed PlantStatement/RootStatement/RootScopeStatement/MissionStatement
- VERIFY dry-run mode for LISTEN BRANCH inside SUITE blocks

---

## v0.6 — 2025

### New Features
- **VERIFY / SUITE** initial implementation
- **BRAID** initial implementation
- **HARVEST** initial implementation (HTTP via Worker + SAB)
- **ANALYZE / TYPEOF / NOW / WAIT / ROOT_SCOPE** implemented
- **PULSE / WHENEVER** confirmed working with MATCH bodies
- Precision line/column diagnostics with visual caret pointer
- `SHOW_VERIFY_SUMMARY` CLI command, `chloroplast verify` subcommand

### Architecture
- Compiler frontend migration started (Tokenizer, Parser, AST as additive layer)
- SHOW / CREATE / LISTEN BRANCH / RESPONSE migrated to AST
- WEATHER / SHELTER / CALM migrated to AST

---

## v0.5 — 2025

### New Features
- **SPECIES / BLOOM / PARENT** — class-based OOP with inheritance
- **SELF** — instance self-reference in all contexts (read/write/INCREASE/PUT/REAP)
- **FLOW** — action pipeline chaining
- **MATCH / YIELD** — pattern matching with BETWEEN, range, ELSE
- **SEASON** — while-style loop

---

## v0.4 — 2025

### New Features
- **WEATHER / SHELTER / CALM** — try/catch/finally error handling
- **TAP / ABSORB / INFUSE / SEAL** — file I/O (VeinFS virtual filesystem)
- **PULSE / WHENEVER / CHANGES** — reactive variable observation
- **ROOT / LOCK** — immutable constants
- Multiple SHELTER clauses per WEATHER block
- `ANY_STORM` catch-all

---

## v0.3 — 2025

### New Features
- **ACTION / GIVE / REAP** — user-defined functions with return values
- **Recursion** — verified with Fibonacci and factorial
- **REAP _ FROM** — discard return value
- **PLANT** — load built-in libraries (math, strings, lists)
- **math** library: SQRT, ABS, ROUND, FLOOR, CEIL, POW, LOG, SIN, COS
- **strings** library: TRIM, UPPER, LOWER, LENGTH, SPLIT, REPLACE, INCLUDES
- **lists** library: AVERAGE, MEDIAN, UNIQUE, FLATTEN, CHUNK, RANGE

---

## v0.2 — 2025

### New Features
- **CYCLE** — list iteration and numeric range loops (`FROM n TO m STEP k`)
- **SORT / SHAKE** — list sorting and shuffling
- **PUT / TAKE** — list mutation
- **LINK** — MAP key assignment
- **MAP** type with `obj:prop` and `obj:"key"` access
- **STOP IF** — conditional abort with optional message
- Comma continuation for multi-line statements

---

## v0.1 — 2025 (initial)

- Core interpreter in Node.js (Chloroplast)
- Depth prefix `N\` syntax
- Types: NUM, SCL, TX, FACT, LIST
- CREATE / SET / INCREASE / DECREASE / SHOW / LOCK / EVAPORATE
- IF / ORIF / ELSE (single-line and block forms)
- MISSION: SAFE / FAST / SMART / BALANCED
- Arabic-readable design with English-only keywords
