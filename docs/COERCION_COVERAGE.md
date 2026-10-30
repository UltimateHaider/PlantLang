# Coercion Coverage (v0.62.6)

## Leading-call coercion (`_coerce_lead_call`)

An explicit `-> LON` (also `-> DBL`/`FLT`/`NUM`) ACTION call that **leads** an
expression is coerced to a number:

| Context | Status | Notes |
|---|---|---|
| `GIVE` | ✅ v0.62.5b | re-boxed with `_from_long(...)` (text ABI preserved) |
| `CREATE` | ✅ v0.62.5b | numeric target uses the expression directly |
| `SET` | ✅ v0.62.6 | also gained the missing **bare-call** coercion |
| `LET` | ✅ v0.62.6 | |
| `IF` condition | ✅ v0.62.6 | numeric comparison |
| ACTION arguments | ❌ **TD-075** | no structured arg list at emission |

## Limitation (documented — TD-075)

Only the **leading** call is coerced:

```plant
inner() + 1        # ✅ coerced
1 + inner()        # ✗ not coerced (non-leading)
f(inner() + 1)     # ✗ not coerced (arguments)
f(g() + 1)         # ✗ not coerced (nested in arguments)
```

**Workaround** — bind first:

```plant
CREATE tmp (LON) TO inner().
CREATE v   (LON) TO 1 + tmp.     # 11
```

## Notes

- Bare calls are coerced exactly once (`inner()`, no double wrapping).
- The helper is **fail-safe**: any expression that is not
  `<action>(...)<more>` is returned unchanged, so programs without the
  pattern generate identical C.
- Numeric values produced by the release path are magic-checked and freed
  (`_to_long_release`), so multi-digit ACTION returns no longer leak.

## Related

- **TD-071** — CLOSED for `GIVE`/`CREATE` (v0.62.5b) and `SET`/`LET`/`IF` (v0.62.6).
- **TD-075** — arguments + non-leading calls (future cycle).
