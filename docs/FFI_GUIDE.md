# FFI Guide — PlantLang Chloroplast

## FFI Safety Rules

### DO NOT Use Internal APIs

The following functions are for internal runtime use only. FFI code must NOT call them directly:

| Function | Why Unsafe |
|----------|------------|
| `plant_tensor_to_string_static` | Requires caller-provided buffer; size must be pre-calculated |
| `_tensor_to_string_recursive` | Internal recursive helper |
| `plant_raw_free` | Low-level free wrapper; use `plant_free` instead |

### Safe APIs for FFI

| Function | Description |
|----------|-------------|
| `plant_free(obj)` | Generic type-aware dispatcher |
| `plant_list_free(list)` | Recursive list free |
| `plant_tensor_free(tensor)` | Tensor free |
| `plant_tensor_to_string(tensor)` | Heap-allocated string (must free result) |
| `plant_is_list(obj)` | Type detection |
| `plant_is_tensor(obj)` | Type detection |

### Example: Safe FFI Code

```c
#include "plant_memory.h"

void my_ffi_function(tx_t list_arg) {
    PlantArray* list = (PlantArray*)list_arg;
    
    // Process the list...
    
    // Safe cleanup
    plant_list_free(list);
}
```

### Example: Unsafe FFI Code (DO NOT)

```c
#include "plant_tensor.h"

void unsafe_ffi(tx_t tensor_arg) {
    PlantTensor* t = (PlantTensor*)tensor_arg;
    char buf[1024];
    
    // UNSAFE: plant_tensor_to_string_static is internal
    plant_tensor_to_string_static(t, buf, sizeof(buf));
}
```

### Type Detection

Use `plant_is_list()` and `plant_is_tensor()` to check types before casting:

```c
void safe_dispatch(void* obj) {
    if (plant_is_list(obj)) {
        PlantArray* list = (PlantArray*)obj;
        // handle list
    } else if (plant_is_tensor(obj)) {
        PlantTensor* t = (PlantTensor*)obj;
        // handle tensor
    }
}
```

## Compile-time Safety

The `ASSERT_MAGIC_FIRST(type)` macro ensures `magic` is always the first field in PlantArray and PlantTensor structs. This is verified at compile time:

```c
ASSERT_MAGIC_FIRST(PlantArray);   // OK
ASSERT_MAGIC_FIRST(PlantTensor);  // OK
```

If someone moves `magic` to a non-first position, the build will fail with a negative array size error.
