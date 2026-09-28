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

### LIST OF TENSOR Warning

Lists of tensors WILL accumulate leaks if not freed:

```plantlang
CREATE L TO LIST OF TENSOR(...).
# Each new tensor leaks ~240 bytes until freed
```

## Best Practices

1. Free tensors immediately after use
2. Free lists when scope ends
3. Use `LIST_FREE` for nested lists (recursive)
4. Use `TENSOR_FREE` for tensors (flat free)
5. Avoid LIST OF TENSOR without explicit cleanup

## Performance

- `plant_tensor_to_string` allocates ~1KB per SHOW TENSOR
- Use `plant_tensor_to_string_static` for buffer-based output (internal API)
- Binary size: ~896KB (v0.51.1), growing <1% per release
