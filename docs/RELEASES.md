# Release Timeline — PlantLang Chloroplast

## Cumulative Growth Table

# Auto-updated: 0.54.2c
| Version | Binary Size | plant_runtime.c | plant_tensor.c | plant_memory.c | plant_report.c | plant_math.c |
|---------|-------------|-----------------|----------------|----------------|----------------|--------------|
| v0.54.2c | 990072 | 8963 | 448 | 109 | 322 | 5361 |
| v0.51.2a | 896,816 | 8,940 | 416 | 72 | 322 | 5,361 |

## Release Timeline

| Version | Date | Feature | Status |
|---------|------|---------|--------|
| v0.51.0 | 2026 | malloc simulation + tensor infrastructure | Released |
| v0.51.0b | 2026 | plant_malloc exclusivity | Released |
| v0.51.1 | 2026 | TENSOR Core type | Released |
| v0.51.2a | 2026 | Core Memory Fixes (TD-001..004) | Released |
| v0.51.2b | 2026 | Introspection + Interface | Released |
| v0.51.2c | 2026 | Lifecycle Tracking + Release | Released |
| v0.51.2d.1 | 2026 | TDs Closure (TD-008/010/011/012/013) | Released |
| v0.51.2d.2 | 2026 | Advanced Scopes + JJ + 6D | Released |
| v0.51.3a | 2026 | GLOBAL Keyword | Released |
| v0.51.3b | 2026 | GLOBAL Complete | Released |
| v0.51.4 | 2026 | Type Registry | Released |
| v0.51.5 | 2026 | Fix IS Between Variables | Released |
| v0.51.6 | 2026 | BYT + UBT (8-bit pair) | Released |
| v0.51.7 | 2026 | SHR + USH (16-bit pair) | Released |
| v0.51.8 | 2026 | Rename NUM→LON + ULO | Released |
| v0.51.9 | 2026 | Rename FACT→NUM + UFACT→UNU + Remove UNUM | Released |
| v0.51.10 | 2026 | FLT + DBL + DCM (Float Type Set) | Released |
| v0.51.11 | 2026 | CMP (Complex Number) | Released |
| v0.51.12 | 2026 | BOL (C99 bool) + C89→C99 | Released |
| v0.51.13 | 2026 | CHA + TXT (Text Types) | Released |
| v0.51.14 | 2026 | VOI + TX Removal | Released |
| v0.51.15 | 2026 | LST (List Formalization) | Released |
| v0.51.16 | 2026 | List Operations (TD-020) | Released |
| v0.51.17 | 2026 | TD-027 Fix (str_eq) | Released |
| v0.51.18 | 2026 | TPL (Tuple) | Released |
| v0.51.19 | 2026 | Formalize MAP + QUE + STK + JSN | Released |
| v0.51.20 | 2026 | Formalize TENSOR | Released |
| v0.51.21 | 2026 | Formalize SET | Released |
| v0.51.22 | 2026 | Close TD-022 + TD-028 | Released |
| v0.51.23 | 2026 | Close TD-019 + TD-024 | Released |
| v0.52.0 | 2026 | Performance (Precompile Runtime) | Released |
| v0.52.1 | 2026 | Close TD-025 (CMP Operands) | Released |
| v0.52.2 | 2026 | Unknown Type Validation | Released |
| v0.52.3a | 2026 | Rename SPECIES → SPC + STRUCT → STR | Released |
| v0.52.3b | 2026 | SPECIES Comma SIGSEGV Fix | Released |
| v0.52.3c | 2026 | Fix TD-031 (Regression Runner) | Released |
| v0.52.3d | 2026 | Close TD-034 (Regression Suite Green) | Released |
| v0.52.3e | 2026 | TD-009 Partial (Struct Construction) | Released |
| v0.52.3f | 2026 | TD-009 Positional (Struct Construction) | Released |
| v0.52.3g | 2026 | Close TD-033 (Species Registration) | Released |
| v0.52.3h | 2026 | TD-037 (Tail Return) | Released |
| v0.52.4 | 2026 | Struct Methods + Dotted-decl | Released |
| v0.52.5 | 2026 | TD-038 (STR X Param) | Released |
| v0.52.6 | 2026 | TD-039 (STR X Return) | Released |
| v0.52.7 | 2026 | TD-035 (Fixture Brittleness) | Released |
| v0.54.0 | 2026 | GEO (Geographic Data Type) | Released |
| v0.54.1 | 2026 | TD-040 (CMP Hook Guard) | Released |
| v0.54.2a | 2026 | TD-015 Items 2+3 | Released |
| v0.54.2b | 2026 | TD-015 Item 4 | Released |
| v0.54.2c | 2026 | TD-015 Item 1 (Auto-Cleanup) | Released |

## Growth Chart (ASCII)

```
Binary Size (KB)
900 |                              *
    |                             /
895 |                            *   v0.51.1 (896,536)
    |                           /
890 |                          *     v0.51.0b (891,624)
    |                         /
885 |                        *       v0.51.0
    |                       /
880 |                      *         v0.50.7
    |                     /
875 |                    *           v0.50.5
    |                   /
870 |                  *             v0.50.3
    |                 /
865 |                *               v0.50.1
    |               /
860 |              *                 v0.49.56
    |____________/________________________________
      v0.49.56  v0.50.1  v0.50.3  v0.50.5  v0.50.7  v0.51.0  v0.51.0b  v0.51.1
```
