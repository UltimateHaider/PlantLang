# PlantLang Web REPL

A single-page, no-build-step web interface for writing and running PlantLang in the browser. Talks to a [CodeWords Compiler Service](../service/README.md) over HTTP.

## Running

You need two things running:

**1. The compiler service** (does the actual work):
```bash
node ../service/codewords-server.js --port 8420
```

**2. This static site** (any static file server works):
```bash
python3 -m http.server 8850
# or: npx serve .
```

Then open `http://localhost:8850` in a browser. The server URL field in the top bar defaults to `http://localhost:8420` — change it if your service runs elsewhere (it's remembered across reloads).

## Features

- **Four modes**, matching the service's endpoints:
  - **Run it** — executes the program, shows `SHOW` output
  - **Check it** — static type check, lists diagnostics with line/column
  - **Verify it** — runs `VERIFY`/`SUITE` tests, shows pass/fail counts
  - **Compile it** — generates C, compiles with `gcc`, runs the binary, shows the output plus a collapsible view of the generated C source
- **Example dropdown** — eight real programs pulled from `examples/*.plnt` (auto-generated into `examples-data.js`; see below)
- **Connection indicator** — pings `/health` every 15s so it's obvious when the service isn't reachable
- **Keyboard shortcut** — `Cmd/Ctrl+Enter` runs the current mode
- **Persistence** — your code and server URL are remembered in `localStorage` across reloads
- **No build step** — plain HTML/CSS/JS, open `index.html` directly or serve it as static files

## Regenerating examples-data.js

`examples-data.js` is generated from the real files in `../examples/`. If those change, regenerate it:

```bash
cd ..
python3 - << 'EOF'
import json
files = [
    ("Hello, PlantLang", "examples/01_basics.plnt"),
    ("Species & Objects", "examples/02_species.plnt"),
    ("Storms (error handling)", "examples/03_storms.plnt"),
    ("FLOW & PULSE", "examples/04_flow_pulse.plnt"),
    ("Grade System", "examples/05_grade_system.plnt"),
    ("BRAID (zip lists)", "examples/06_braid.plnt"),
    ("Compile to C (FizzBuzz-style)", "examples/09_compile.plnt"),
]
entries = [{"name": l, "source": open(p, encoding='utf-8').read()} for l, p in files]
js = "window.PLANTLANG_EXAMPLES = " + json.dumps(entries, indent=2, ensure_ascii=False) + ";\n"
open('webrepl/examples-data.js', 'w', encoding='utf-8').write(js)
EOF
```

Note: `HARVEST`/`LISTEN BRANCH` examples are intentionally excluded — the compiler service blocks those constructs for safety, so they'd only produce an error in the browser.

## Testing

```bash
node test_webrepl.js
```

13 integration tests that load the **real** `index.html`/`app.js`/`examples-data.js` into a jsdom document, start a **real** CodeWords Compiler Service, and drive actual button clicks / keyboard events / fetch calls end-to-end — not mocked.

## Design Notes

The visual language leans into PlantLang's core idea — code that reads like prose — rather than a generic dark-terminal look:

- **Fraunces** (a display serif with organic, almost hand-set letterforms) for the wordmark, mode tabs, and empty-state copy
- **IBM Plex Mono** for the editor and output, where precision matters
- Mode switching reads as a sentence: *"i want to run it / check its types / verify its tests / compile it to C"*
- Output lines settle in one at a time with a small stagger and fade — like ink taking hold — rather than appearing all at once
