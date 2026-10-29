# Globals Recipe (v0.62.0 — TD-055)

## Overview

Top-level `CREATE` variables are visible inside `ACTION` bodies:
**read**, **write-through**, across **nested calls**, with **parameter
shadowing**. Hoisting is **surgical** — only names actually used in an
ACTION body are lifted to file scope, so programs that do not use the
pattern generate byte-identical C (bootstrap-inert).

Mechanism: the hoisted name becomes `static <ctype> <name>;` at file scope
and its top-level `CREATE` emits an **assignment only**
(`g = 42;`), the same code path an ACTION-body `CREATE` uses.

## Read

```plant
CREATE g (LON) TO 42.
ACTION f()                     # implicit return: raw LON, no coercion
GIVE g.
/ACTION.
CREATE v (LON) TO f().
SHOW v.                        # 42
```

> Prefer the implicit-return form for large numbers: an explicit
> `-> LON` return goes through `_from_long` and multi-digit values leak
> (TD-072). Single-digit values are safe in either form.

## Write-through (shared state)

```plant
CREATE counter (LON) TO 0.
ACTION inc() -> LON
SET counter TO counter + 1.
GIVE counter.
/ACTION.
CREATE a (LON) TO inc().
CREATE b (LON) TO inc().
SHOW a.                        # 1
SHOW b.                        # 2
```

## Tensor globals

```plant
CREATE W (TENSOR) TO TENSOR([1.0, 2.0]).
ACTION f() -> TENSOR
GIVE TENSOR_ADD(W, W).
/ACTION.
CREATE v (TENSOR) TO f().
SHOW v.                        # [2, 4]
TENSOR_FREE(v).
TENSOR_FREE(W).
```

A hoisted tensor is declared `PlantTensor*` at file scope, so the normal
`TENSOR_FREE` / `SET` ownership rules still apply.

## Nested calls

```plant
CREATE g (LON) TO 7.
ACTION inner() -> LON
GIVE g.
/ACTION.
ACTION outer() -> LON
GIVE inner().
/ACTION.
CREATE v (LON) TO outer().
SHOW v.                        # 7
```

All ACTIONs see the same file-scope static — no transitive capture analysis
is needed.

> ⚠️ **TD-071**: an explicit `-> LON` ACTION call *inside an expression*
> (e.g. `GIVE inner() + 1.`) is **not coerced** — the `tx_t` pointer is
> used as a number. Use a variable in between:
> ```plant
> CREATE t (LON) TO inner().
> SET t TO t + 1.
> GIVE t.
> ```

## Shadowing

```plant
CREATE g (LON) TO 9.
ACTION f(g (LON)) -> LON
GIVE g.                        # the parameter wins
/ACTION.
CREATE v (LON) TO f(3).
SHOW v.                        # 3
SHOW g.                        # 9  (global untouched)
```

## Hoisting rules

1. Candidate = a **top-level** `CREATE` (not inside `SUITE`/`ACTION`);
   `ARRAY[...]` declarations are skipped.
2. Hoisted iff the name occurs **word-bounded** in the expression strings
   of some ACTION body (`value`, `target`, `condition`, `expr`, `args`,
   `items`, `message`, `text`, `range`, `iterable`, `pattern`, `key`,
   recursing into `body` / `elseBody` / `clauses.bodyStatements`).
3. Skipped if the name is an ACTION name or an existing `GLOBAL`
   (a static would collide with the C function / already exist).
4. Numeric hoisted names join `NUM_GLOBALS`, so ACTION-side numeric
   handling stays correct.
5. Empty set ⇒ current emission (no output change).

## Limitations

- Only **top-level** `CREATE`s are hoisted (a `CREATE` inside a top-level
  `SUITE` is not).
- **TD-071** (`-> LON` call inside an expression) and **TD-072**
  (`_to_long` of a multi-digit ACTION return leaks 11 B) are open.
- Dev-only: do not serialise raw AST nodes with `plant_map_to_string` —
  some nodes have back-references and the serializer overflows the stack
  (the compiler uses a safe field scanner instead).
- `TYPEOF(x)` returns `string` / `list` / `map` (not `TXT`/`LST`).

## See also

- `docs/NN_EXAMPLE.md`
- `docs/DENSE_TRAINING_RECIPE.md`
- `docs/ACTIVATIONS_RECIPE.md`
