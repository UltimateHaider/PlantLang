# Production Guide — PlantLang Chloroplast

## Memory Management

### Automatic vs Manual

PlantLang does NOT have garbage collection. All heap-allocated objects (lists, tensors) must be freed manually by the programmer.

### Built-in Functions

| Function | Description | Use Case |
|----------|-------------|----------|
| `LIST_FREE(x)` | Recursively frees a PlantArray | Free nested lists |
| `TENSOR_FREE(x)` | Frees a PlantTensor | Free tensors |
| `FREE x.` | Generic type-aware dispatcher | When type is unknown |

### Aliases (v0.51.2b+)

All three built-ins (`LIST_FREE`, `TENSOR_FREE`, `FREE`) are functionally identical since v0.51.2b. They all route through the `plant_free()` generic dispatcher, which detects the type via magic bytes and frees accordingly.

- Use `LIST_FREE(x)` when x is known to be a list
- Use `TENSOR_FREE(x)` when x is known to be a tensor
- Use `FREE x.` when the type is unknown or mixed

### MAP value ownership (v0.63.4)

A pair-list `MAP` **owns the values it stores**: `LIST_FREE(M)` releases them
recursively, and overwriting a key releases the **tensor or numeric** value
it replaces (container values — see the gap note below):

```plantlang
CREATE M (MAP) TO {}.
CREATE a (TENSOR) TO TENSOR([1, 2]).
CREATE b (TENSOR) TO TENSOR([3, 4]).
SET M["k"] TO a.
SET M["k"] TO b.   # a is released here — do NOT TENSOR_FREE(a) afterwards
LIST_FREE(M).      # releases b (and the key)
```

Do not free a value after the map replaced it — that is a double free
(`SIGABRT`). The same rule applies to `LINK … IN M` and to struct fields
written with `SET obj.field TO v`. Do not store the same object under two
keys: both entries would own it and `LIST_FREE(M)` would free it twice.

**Known gap:** overwriting a **list/map** value does not release the old
container yet (pre-existing leak, ~56 B per overwritten list) — containers
are still shared. Closing it requires reference counting, not `free()`
(see TD-077).

### Example

```plantlang
ACTION main(),
  LET data TO [1, 2, 3, 4, 5].
  LET tensor TO TENSOR([1, 2, 3, 4, 5, 6]).
  
  SHOW data.
  SHOW tensor.
  
  # Free when done
  LIST_FREE(data).
  TENSOR_FREE(tensor).
/ACTION.
```

### Loop Behavior

Loops reuse variable slots, so leaks do NOT accumulate:

```plantlang
LOOP i FROM 1 TO 100:
  CREATE T TO TENSOR([1, 2, 3]).
  # T is reused each iteration — only 1 tensor exists at exit
/LOOP
```

### LST OF TENSOR Warning

Lists of tensors WILL accumulate leaks if not freed:

```plantlang
CREATE L TO LST OF TENSOR(...).
# Each new tensor leaks ~240 bytes until freed
```

## Best Practices

1. Free tensors immediately after use
2. Free lists when scope ends
3. Use `LIST_FREE` for nested lists (recursive)
4. Use `TENSOR_FREE` for tensors (flat free)
5. Avoid LST OF TENSOR without explicit cleanup

## Performance

- `plant_tensor_to_string` allocates ~1KB per SHOW TENSOR
- Use `plant_tensor_to_string_static` for buffer-based output (internal API)
- Binary size: ~896KB (v0.51.1), growing <1% per release
