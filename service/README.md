# CodeWords Compiler Service

A standalone HTTP API that lets external clients (a Web REPL UI, curl, CI pipelines, other tools) submit PlantLang source code and get back execution output, type-check diagnostics, compiled binaries, or VERIFY test results — without needing PlantLang or Node.js installed locally.

This is a plain Node.js service (not written in PlantLang) because it needs to manage OS processes, enforce timeouts on arbitrary user code, and isolate untrusted execution — none of which PlantLang can safely do to its own host process.

## Running

```bash
node service/codewords-server.js --port 8420
```

## Endpoints

### `GET /health`

```json
{ "ok": true, "service": "CodeWords Compiler Service", "version": "1.0.0" }
```

### `POST /run`

Executes the program with the interpreter and returns captured `SHOW` output.

```bash
curl -X POST http://localhost:8420/run \
  -H "Content-Type: application/json" \
  -d '{"source": "MISSION: SAFE.\n1\\ SHOW \"Hello!\"."}'
```

```json
{ "ok": true, "output": "Hello!", "truncated": false, "elapsedMs": 42 }
```

On a runtime storm:
```json
{ "ok": false, "error": "...", "stormType": "ZERO_STORM", "line": 3, "column": 4, "output": "..." }
```

### `POST /check`

Runs the static type checker (no execution) and returns diagnostics.

```json
{
  "ok": false,
  "diagnostics": [
    { "severity": "error", "code": "TYPE_MISMATCH", "message": "...", "line": 4, "column": 6 }
  ],
  "elapsedMs": 12
}
```

### `POST /verify`

Runs a VERIFY/SUITE test file and returns pass/fail counts plus formatted output.

```json
{ "ok": false, "output": "...", "passed": 8, "failed": 1, "elapsedMs": 55 }
```

### `POST /compile`

Generates C code, compiles with `gcc -O2 -lm`, runs the binary, and returns stdout.

```json
{ "ok": true, "cCode": "#include ...", "output": "42\n", "elapsedMs": 210 }
```

If the program uses constructs the C generator doesn't support yet:
```json
{ "ok": false, "cCode": "...", "diagnostics": [{ "message": "Unsupported construct...", "line": 2 }] }
```

## Safety Model

| Protection | Detail |
|---|---|
| **Process isolation** | Every request runs in a freshly forked child process (`service/sandbox-runner.js`), never in the main server process |
| **Timeout** | 5s wall-clock limit per request; the child is `SIGKILL`ed on expiry, the server stays responsive to other requests the whole time |
| **Output cap** | 64KB max captured output — runaway `SHOW` loops can't exhaust memory |
| **Body size cap** | 128KB max HTTP request body, 64KB max submitted source |
| **Network blocking** | `LISTEN BRANCH` and `HARVEST` are rejected before execution — submitted code cannot bind ports or make outbound requests from the host |
| **Compile timeouts** | `gcc` (8s) and the resulting binary (3s) each have their own timeout inside `/compile` |

## Testing

```bash
node service/test_service.js
```

15 automated tests covering every endpoint, error handling, the timeout mechanism, network blocking, and concurrent-request isolation.

## Architecture

```
Client (Web REPL UI / curl / CI)
   │  HTTP POST /run, /check, /verify, /compile
   ▼
codewords-server.js  (long-lived, handles routing/validation/timeouts)
   │  fork() + IPC — one child per request
   ▼
sandbox-runner.js    (disposable, does the actual PlantLang work)
   │
   ├── core/interpreter.js   (for /run, /verify)
   ├── core/typechecker.js   (for /check)
   └── core/codegen.js + gcc (for /compile)
```
